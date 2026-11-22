/* Cameras: the battle camera that follows both fighters and lifts over stage camera walls, the
 * first-person (POV) camera, and the victory, continue and Slash-clear scene cameras. */

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <inline_o.h>

#include "fighter.h"
#include "game_state.h"
#include "camera.h"
#include "stage_cull.h"
#include "stage_id.h"
#include "angle.h"
#include "cloth.h"
#include "scene_math.h"
#include "globals.h"
#include "match_setup.h"

#define BATTLE_CAMERA_SCRATCH ((BattleCameraScratch *)getScratchAddr(0))

static SVECTOR gOcclusionMarkers[2];

/* One-sided XZ segment-vs-wall intersection; wall endpoint winding sets the blocking side. */
static int IntersectAxisAlignedWall(StageCameraWall *wall, VECTOR *start, VECTOR *end, VECTOR *outHit)
{
    int isAscending;
    int hitCoord;

    if (wall->x0 == wall->x1) {
        isAscending = wall->z0 < wall->z1;
        if ((isAscending && start->vx > wall->x0) || (!isAscending && end->vx > wall->x0)) {
            int startX = start->vx;
            int wallX = wall->x0;
            if (startX <= wallX && end->vx <= wallX)
                return 0;
            if (startX > wallX && end->vx > wallX)
                return 0;

            hitCoord = start->vz + (wall->x0 - start->vx) * (start->vz - end->vz) / (start->vx - end->vx);
            if (!isAscending) {
                if (hitCoord <= wall->z1 || hitCoord >= wall->z0)
                    return 0;
            } else {
                if (hitCoord <= wall->z0 || hitCoord >= wall->z1)
                    return 0;
            }
            outHit->vx = wall->x0;
            outHit->vy = wall->topY;
            outHit->vz = hitCoord;
            return 1;
        }
    } else {
        int wallZ;
        isAscending = wall->x0 < wall->x1;
        wallZ = wall->z0;
        if ((isAscending && end->vz > wallZ) || (!isAscending && start->vz > wallZ)) {
            int startZ = start->vz;
            int wallZCheck = wall->z0;
            if (startZ <= wallZCheck && end->vz <= wallZCheck)
                return 0;
            if (startZ > wallZCheck && end->vz > wallZCheck)
                return 0;

            hitCoord = start->vx + (wall->z0 - start->vz) * (start->vx - end->vx) / (start->vz - end->vz);
            if (!isAscending) {
                if (hitCoord <= wall->x1 || hitCoord >= wall->x0)
                    return 0;
            } else {
                if (hitCoord <= wall->x0 || hitCoord >= wall->x1)
                    return 0;
            }
            outHit->vx = hitCoord;
            outHit->vy = wall->topY;
            outHit->vz = wall->z0;
            return 1;
        }
    }
    return 0;
}

static int DivideSignedAngle(int angle, int divisor)
{
    angle &= ANGLE_MASK;
    if (angle > ANGLE_HALF - 1) {
        angle -= ANGLE_FULL;
    }
    return angle / divisor;
}

/* kept: written in place, the record's base stays in a register; that the authors wrote helpers is not established */
static inline void ApproachCameraTarget(VECTOR *focus)
{
    int step;

    step = (focus->vx - gCameraState.target.vx) / 4;
    gCameraState.target.vx += step;
    step = (focus->vy - gCameraState.target.vy) / 4;
    gCameraState.target.vy += step;
    step = (focus->vz - gCameraState.target.vz) / 4;
    gCameraState.target.vz += step;
}

/* kept: written in place, the record's base stays in a register; that the authors wrote helpers is not established */
static inline void UpdateCameraDistance(int targetDist, int mode, int bladeLockState)
{
    int nextDistance = gCameraState.distance + (targetDist - gCameraState.distance) / 12;

    gCameraState.distance = nextDistance;
    if (mode == BATTLE_CAMERA_LIVE && bladeLockState == 0) {
        if (nextDistance <= 5000) {
            gCameraState.distance = 5000;
        }
    } else {
        if (nextDistance <= 3000) {
            gCameraState.distance = 3000;
        }
    }
    if (gCameraState.distance > 35000) {
        gCameraState.distance = 35000;
    }
}

/* kept: written in place, the record's base stays in a register; that the authors wrote helpers is not established */
static inline void ApproachCameraAngles(SVECTOR *targetAngles, int divisor)
{
    gCameraState.angles.vx += DivideSignedAngle(targetAngles->vx - gCameraState.angles.vx, divisor);
    gCameraState.angles.vy += DivideSignedAngle(targetAngles->vy - gCameraState.angles.vy, divisor);
    gCameraState.angles.vz += DivideSignedAngle(targetAngles->vz - gCameraState.angles.vz, divisor);
}

void UpdateBattleCamera(int mode, VECTOR *primaryTarget, VECTOR *secondaryTarget,
                        int frameIndex, int isRunning, int bladeLockState)
{
    BattleCameraScratch *scratch = BATTLE_CAMERA_SCRATCH;
    int *wallHeader;
    StageCameraWall *walls;
    VECTOR *targets[2];
    int wallLeading;
    int pairLeading;
    int replayFrame;
    int minPitch;
    int angleDivisor;
    int wallCount;
    int requiredPitch;
    int pitch;
    int i;
    int j;
    int dx;
    int dy;
    int dz;
    int wallDistOrFov;
    int pairDistance;
    int wallPitch;
    int primaryWeight;
    int secondaryWeight;
    int distancePitch;
    int yawOffset;
    int replayYaw;
    int evenLeading;

    replayFrame = frameIndex;
    wallHeader = gStageCameraWalls;
    wallCount = *wallHeader;
    walls = (StageCameraWall *)(wallHeader + 1);
    targets[0] = primaryTarget;
    targets[1] = secondaryTarget;
    gOcclusionMarkers[0].vx = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[0].vz = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[1].vx = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[1].vz = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[0].vy = 0;
    gOcclusionMarkers[1].vy = 0;
    angleDivisor = 8;
    if (gBattleCameraLocked != 0) {
        return;
    }

    scratch->cameraPos.vx = gCameraState.target.vx +
        (gCameraState.distance * gSinLutQ12[gCameraState.angles.vy & ANGLE_MASK] >> 12);
    scratch->cameraPos.vy = gCameraState.target.vy;
    scratch->cameraPos.vz = gCameraState.target.vz -
        (gCameraState.distance * gSinLutQ12[(gCameraState.angles.vy + ANGLE_QUARTER) & ANGLE_MASK] >> 12);

    scratch->fighterPos[0].vx = primaryTarget->vx;
    scratch->fighterPos[0].vy = primaryTarget->vy;
    scratch->fighterPos[0].vz = primaryTarget->vz;
    scratch->fighterPos[1].vx = secondaryTarget->vx;
    scratch->fighterPos[1].vy = secondaryTarget->vy;
    scratch->fighterPos[1].vz = secondaryTarget->vz;

    /* both arms are the same in the original */
    if (mode == 0) {
        minPitch = 0x80;
        requiredPitch = 0x80;
    } else {
        minPitch = 0x80;
        requiredPitch = 0x80;
    }
    if (gStage4LowerLevelActive != 0 || gStage10LowerLevelActive != 0) {
        minPitch = 0x40;
        requiredPitch = 0x40;
    }
    if (gCurrentStageId == STAGE_ID_MEIKYOKAN) {
        minPitch = 0x40;
        requiredPitch = 0x40;
    }
    if (gPlayMode == PLAY_MODE_SLASH) {
        minPitch = 0x20;
        requiredPitch = 0x20;
    }

    for (i = 0; i < wallCount; i++) {
        scratch->wall = walls[i];
        if ((gStage4LowerLevelActive != 0 || gStage10LowerLevelActive != 0) && scratch->wall.topY <= 4000) {
            continue;
        }
        for (j = 0; j < 2; j++) {
            if (IntersectAxisAlignedWall(&scratch->wall, &scratch->fighterPos[j], &scratch->cameraPos,
                                         &scratch->hitPos) != 0) {
                int wallDistSq;
                dx = targets[j]->vx - scratch->hitPos.vx;
                dz = targets[j]->vz - scratch->hitPos.vz;
                wallDistSq = dx * dx + dz * dz;
                if ((unsigned int)wallDistSq < 1024) {
                    wallDistOrFov = gSqrtLutQ3[wallDistSq] >> 3;
                } else {
                    unsigned int shift;
                    gte_ldlzc(wallDistSq);
                    gte_nop();
                    gte_nop();
                    gte_stlzc(&wallLeading);
                    evenLeading = wallLeading & ~1;
                    shift = 22 - evenLeading;
                    wallDistOrFov = ((unsigned int)gSqrtLutQ3[(unsigned int)wallDistSq >> shift] << 16) >>
                            (19 - (shift >> 1));
                }
                wallPitch = ratan2(targets[j]->vy - scratch->hitPos.vy, wallDistOrFov) & ANGLE_MASK;
                if (wallPitch >= ANGLE_HALF) {
                    wallPitch -= ANGLE_FULL;
                }
                if (wallPitch < ANGLE_QUARTER && requiredPitch < wallPitch + 64) {
                    requiredPitch = wallPitch + 64;
                    if (scratch->wall.x0 == scratch->wall.x1) {
                        gOcclusionMarkers[j].vx = (scratch->hitPos.vx < 0
                                                   ? scratch->hitPos.vx - STAGE_CELL_HALF_SIZE
                                                   : scratch->hitPos.vx + STAGE_CELL_HALF_SIZE) / STAGE_CELL_SIZE;
                        gOcclusionMarkers[j].vz = (scratch->hitPos.vz < 0
                                                   ? scratch->hitPos.vz - (STAGE_CELL_SIZE - 1)
                                                   : scratch->hitPos.vz) / STAGE_CELL_SIZE;
                    } else {
                        gOcclusionMarkers[j].vx = (scratch->hitPos.vx < 0
                                                   ? scratch->hitPos.vx - (STAGE_CELL_SIZE - 1)
                                                   : scratch->hitPos.vx) / STAGE_CELL_SIZE;
                        gOcclusionMarkers[j].vz = (scratch->hitPos.vz < 0
                                                   ? scratch->hitPos.vz - STAGE_CELL_HALF_SIZE
                                                   : scratch->hitPos.vz + STAGE_CELL_HALF_SIZE) / STAGE_CELL_SIZE;
                    }
                    if (scratch->wall.x0 == scratch->wall.x1) {
                        if (scratch->wall.z0 < scratch->wall.z1) {
                            gOcclusionMarkers[j].vx--;
                        }
                    } else if (scratch->wall.x1 < scratch->wall.x0) {
                        gOcclusionMarkers[j].vz--;
                    }
                }
            }
        }
    }

    pitch = minPitch;
    if (requiredPitch >= minPitch) {
        pitch = requiredPitch;
    }

    dx = secondaryTarget->vx - primaryTarget->vx;
    dy = secondaryTarget->vy - primaryTarget->vy;
    dz = secondaryTarget->vz - primaryTarget->vz;
    if (mode == BATTLE_CAMERA_LIVE) {
        scratch->focusTarget.vx = (primaryTarget->vx + secondaryTarget->vx) / 2;
        scratch->focusTarget.vy = (primaryTarget->vy + secondaryTarget->vy) / 2;
        scratch->focusTarget.vz = (primaryTarget->vz + secondaryTarget->vz) / 2;
    } else {
        primaryWeight = replayFrame + 192;
        secondaryWeight = 320 - replayFrame;
        scratch->focusTarget.vx = (primaryTarget->vx * primaryWeight + secondaryTarget->vx * secondaryWeight) / 512;
        scratch->focusTarget.vy = (primaryTarget->vy * primaryWeight + secondaryTarget->vy * secondaryWeight) / 512;
        scratch->focusTarget.vz = (primaryTarget->vz * primaryWeight + secondaryTarget->vz * secondaryWeight) / 512;
    }

    ApproachCameraTarget(&scratch->focusTarget);

    {
        int pairDistSq = dx * dx + dz * dz + dy * dy;
        if ((unsigned int)pairDistSq < 1024) {
            pairDistance = gSqrtLutQ3[pairDistSq] >> 3;
        } else {
            int leading = 0;
            unsigned int shift;
            if (pairDistSq >= 0) {
                gte_ldlzc(pairDistSq);
                gte_nop();
                gte_nop();
                gte_stlzc(&pairLeading);
                leading = pairLeading;
            }
            evenLeading = leading & ~1;
            shift = 22 - evenLeading;
            pairDistance = ((unsigned int)gSqrtLutQ3[(unsigned int)pairDistSq >> shift] << 16) >>
                    (19 - (shift >> 1));
        }
    }

    if (mode != BATTLE_CAMERA_LIVE) {
        wallDistOrFov = (replayFrame / 4 + 33) * 250 / 100;
    } else {
        wallDistOrFov = 165;
    }

    pairDistance = (pairDistance * gSinLutQ12[(wallDistOrFov + ANGLE_QUARTER) & ANGLE_MASK] /
                    gSinLutQ12[wallDistOrFov & ANGLE_MASK]) >> 1;
    if (dy < 0) {
        dy = -dy;
    }
    pairDistance += dy;
    if (isRunning != 0 && pairDistance < 16000) {
        pairDistance += (16000 - pairDistance) * 40 / 100;
    }

    UpdateCameraDistance(pairDistance, mode, bladeLockState);

    if (gCameraState.distance > 40000) {
        distancePitch = 0x80;
    } else {
        distancePitch = 0x200 - gCameraState.distance * 384 / 40000;
    }

    if ((gStage4LowerLevelActive != 0 || gStage10LowerLevelActive != 0) &&
        gCameraState.distance > 18000 &&
        distancePitch > 0x60) {
        distancePitch = 0x60;
    }
    if (distancePitch < pitch) {
        pitch = distancePitch;
    }

    if (mode == BATTLE_CAMERA_LIVE) {
        scratch->targetAngles.vx = pitch;
        scratch->targetAngles.vy = (ANGLE_QUARTER - ratan2(dx, dz)) & ANGLE_MASK;
        if (bladeLockState != 0) {
            yawOffset = (gFighters[1].bladeLockMeter - gFighters[0].bladeLockMeter) / 32;
            if (yawOffset > 896) {
                yawOffset = 896;
            } else if (yawOffset < -896) {
                yawOffset = -896;
            }
            scratch->targetAngles.vy += yawOffset;
        }
        scratch->targetAngles.vz = 0;
    } else {
        scratch->targetAngles.vx = pitch;
        replayYaw = ratan2(dx * (227 - replayFrame) / 195, dz * (227 - replayFrame) / 195) - 512;
        scratch->targetAngles.vz = 0;
        scratch->targetAngles.vy = (replayFrame * ANGLE_HALF / 195 - replayYaw) & ANGLE_MASK;
        if (replayFrame == 0) {
            angleDivisor = 1;
        }
    }

    ApproachCameraAngles(&scratch->targetAngles, angleDivisor);
}

void ApplyCameraOcclusionMarkers(void)
{
    int i;

    for (i = 0; i < 2; i++) {
        if (gOcclusionMarkers[i].vx != OCCLUSION_MARKER_NONE) {
            MarkCellSemiTrans(&gOcclusionMarkers[i]);
        }
    }
}

void UpdatePovCamera(Fighter *fighter)
{
    struct CameraState *camera = &gCameraState;
    int footSumY;
    int adjustment;
    int isStanding;
    int headY;
    int aim;
    int cameraY;

    gOcclusionMarkers[0].vx = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[0].vy = 0;
    gOcclusionMarkers[0].vz = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[1].vx = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[1].vy = 0;
    gOcclusionMarkers[1].vz = OCCLUSION_MARKER_NONE;
    footSumY = fighter->leftFootPos.y + fighter->rightFootPos.y;
    headY = fighter->headPos.y;
    camera->target.vx = fighter->headPos.x;
    adjustment = footSumY / 2 - headY;
    isStanding = adjustment >= 913;
    camera->target.vz = fighter->headPos.z;
    adjustment = fighter->headPos.y;
    if (!isStanding) {
        adjustment += (-(fighter->statureScale * 950)) / ONE;
    }
    cameraY = camera->target.vy + (adjustment - camera->target.vy) / 4;
    camera->target.vy = cameraY;

    adjustment = (-fighter->angleToOpponent - camera->angles.vy) & ANGLE_MASK;
    if (adjustment >= ANGLE_HALF)
        adjustment = ANGLE_FULL - adjustment;
    if (adjustment >= ANGLE_QUARTER)
        adjustment = ANGLE_QUARTER;
    aim = ratan2(fighter->opponent->position.vy - cameraY, gFighterDistanceXZ);
    adjustment = aim * (ANGLE_QUARTER - adjustment) >> 10;
    if (isStanding && fighter->opponent->isStationary != 0 && gFighterDistanceXZ >= 8000)
        adjustment = 0;
    camera->angles.vx += (adjustment - camera->angles.vx) / 8;
    camera->angles.vz = 0;

    aim = -fighter->angles.heading;
    if (fighter->climbState != 0 || fighter->flipHeadingNextTick != 0)
        adjustment = aim + ANGLE_HALF;
    else
        adjustment = aim;
    if (fighter->placement->facingMode == MOTION_FACING_INVERTED) {
        adjustment += ANGLE_HALF;
        camera->angles.vy += (adjustment - camera->angles.vy) / 8;
    } else {
        camera->angles.vy = adjustment;
    }
    camera->distance = 0;
}

/* Victory-scene camera transition around fighter; progress is Q12 (0..4096). */
void SetCameraTransitionFromFighter(Fighter *fighter, int progress)
{
    struct CameraState *camera;
    SVECTOR *markers = gOcclusionMarkers;
    int heightTravel, lift, targetX, pitch, heading, approach;
    short yaw;

    heightTravel = progress * -300;
    markers[0].vx = OCCLUSION_MARKER_NONE;
    markers[0].vy = 0;
    markers[0].vz = OCCLUSION_MARKER_NONE;
    markers[1].vx = OCCLUSION_MARKER_NONE;
    markers[1].vy = 0;
    markers[1].vz = OCCLUSION_MARKER_NONE;

    targetX = fighter->position.vx;
    camera = &gCameraState;
    camera->target.vx = targetX;
    lift = (heightTravel / ONE) * fighter->statureScale;
    camera->target.vy = fighter->position.vy + lift / ONE;
    camera->target.vz = fighter->position.vz;

    pitch = fighter->legsIntact ? progress * 768 : progress * 544;
    gCameraState.angles.vx = 512 - pitch / ONE;

    yaw = progress / 8 + 1792;
    approach = progress * 4500;
    heading = (unsigned short)fighter->angles.heading;
    camera->angles.vz = 0;
    yaw -= heading;
    camera->angles.vy = yaw;
    camera->distance = 6000 - approach / ONE;
}

/* Continue-screen orbit camera around defeated fighter; orbitProgress is Q12 (0..4096). */
void UpdateContinueOrbitCamera(Fighter *fighter, int orbitProgress, int baseYaw)
{
    int headZ;
    int positionZ;
    int yawStep;
    unsigned short prevYaw;
    int targetDistance;

    gOcclusionMarkers[0].vx = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[0].vy = 0;
    gOcclusionMarkers[0].vz = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[1].vx = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[1].vy = 0;
    gOcclusionMarkers[1].vz = OCCLUSION_MARKER_NONE;
    gCameraState.target.vx = (fighter->headPos.x + fighter->position.vx) / 2;
    gCameraState.target.vy = (fighter->headPos.y + fighter->position.vy) / 2;
    headZ = fighter->headPos.z;
    positionZ = fighter->position.vz;
    gCameraState.angles.vx = 0x200;
    gCameraState.target.vz = (headZ + positionZ) / 2;

    yawStep = DivideSignedAngle(baseYaw + orbitProgress / 4 - gCameraState.angles.vy, 32);
    prevYaw = (unsigned short)gCameraState.angles.vy;
    gCameraState.angles.vz = 0;
    gCameraState.angles.vy = prevYaw + yawStep;
    targetDistance = orbitProgress * 6000 / ONE + 1500;
    gCameraState.distance += (targetDistance - gCameraState.distance) / 32;
}

void AimSceneCameraAtFighter(Fighter *fighter, int progress)
{
    struct CameraState *camera = &gCameraState;
    int dx;
    int dz;
    int distanceSquared;
    int horizontalDist;
    int leadingZeros;
    int targetAngle;

    gOcclusionMarkers[0].vx = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[0].vy = 0;
    gOcclusionMarkers[0].vz = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[1].vx = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[1].vy = 0;
    gOcclusionMarkers[1].vz = OCCLUSION_MARKER_NONE;
    dx = fighter->position.vx - camera->target.vx;
    dz = fighter->position.vz - camera->target.vz;
    distanceSquared = dx * dx + dz * dz;
    if ((unsigned int)distanceSquared < 1024) {
        horizontalDist = gSqrtLutQ3[distanceSquared] >> 3;
    } else {
        unsigned int shift;
        gte_ldlzc(distanceSquared);
        gte_nop();
        gte_nop();
        gte_stlzc(&leadingZeros);
        shift = leadingZeros;
        shift = 22 - (shift & ~1);
        distanceSquared = gSqrtLutQ3[(unsigned int)distanceSquared >> shift];
        horizontalDist = ((unsigned int)distanceSquared << 16) >> (19 - (shift >> 1));
    }
    targetAngle = ANGLE_QUARTER - ratan2(horizontalDist, fighter->position.vy - camera->target.vy);
    camera->angles.vx += DivideSignedAngle(targetAngle - camera->angles.vx, 4);
    targetAngle = -ratan2(dx, dz);
    camera->angles.vy += DivideSignedAngle(targetAngle - camera->angles.vy, 4);
    camera->angles.vz = 0;
    camera->distance = 0;
    if (gCurrentStageId == 9) {
        camera->target.vy -= 64;
    }
}

/* 100-man Slash clear cutscene 360-degree sweep; progress is Q12 (0..4096). */
void SetSlashClearCamera(Fighter *fighter, int progress)
{
    int heightOffset;
    int pitch;
    int cosine;
    int cosIndex;

    gOcclusionMarkers[0].vx = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[0].vz = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[1].vx = OCCLUSION_MARKER_NONE;
    gOcclusionMarkers[1].vz = OCCLUSION_MARKER_NONE;
    heightOffset = (ONE - progress) * 1200;
    gOcclusionMarkers[0].vy = 0;
    gOcclusionMarkers[1].vy = 0;
    gCameraState.target.vx = fighter->position.vx;
    gCameraState.target.vy = -200 - heightOffset / ONE;
    pitch = progress - ANGLE_HALF;
    gCameraState.target.vz = fighter->position.vz;
    gCameraState.angles.vx = pitch / 16;
    gCameraState.angles.vy = progress - 512;
    cosIndex = (progress + ANGLE_QUARTER) & ANGLE_MASK;
    gCameraState.angles.vz = 0;
    cosine = gSinLutQ12[cosIndex];
    gCameraState.distance = 4000 - cosine * 3000 / ONE;
}
