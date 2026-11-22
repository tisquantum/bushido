/* Combat collision: weapon tips against walls and floors, swept weapon quads against each other
 * (blade clashes) and against the opponent's hit spheres, run each frame on the scratchpad. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "fighter.h"
#include "collision_scratch.h"
#include "combat_event.h"
#include "combat_hit.h"
#include "game_state.h"
#include "weapon_trail.h"
#include "rumble_pulse.h"
#include "floor_collision.h"
#include "stage_id.h"
#include "weapon_id.h"
#include "collision.h"
#include "geometry.h"
#include "stage_prop_hit.h"
#include "subweapon.h"
#include "globals.h"
#include "fighter_hit_sphere.h"

#define CLASH_MIN_ACTIVE_FRAMES        4
#define BLADE_LOCK_MAX_GROUND_DIFF     1000
#define COLLISION_DIST_MAX             0x7fffffff
#define HIT_PART_NONE                  (-1)
#define WEAPON_SWEEP_SUBDIVIDE_DIST_SQ (2500 * 2500 - 1)

#define COMPUTE_TRIANGLE_AABB(verts, outMin, outMax) \
    do { \
        int _vi; \
        int _c; \
        (outMax) = *(verts)[0]; \
        (outMin) = (outMax); \
        _vi = 1; \
        do { \
            _c = (verts)[_vi]->x; \
            if (_c < (outMin).x) { \
                (outMin).x = _c; \
            } else if ((outMax).x < _c) { \
                (outMax).x = _c; \
            } \
            _c = (verts)[_vi]->y; \
            if (_c < (outMin).y) { \
                (outMin).y = _c; \
            } else if ((outMax).y < _c) { \
                (outMax).y = _c; \
            } \
            _c = (verts)[_vi]->z; \
            if (_c < (outMin).z) { \
                (outMin).z = _c; \
            } else if ((outMax).z < _c) { \
                (outMax).z = _c; \
            } \
            ++_vi; \
        } while (_vi < 3); \
    } while (0)

#define TEST_AABB_OVERLAP(aMin, aMax, bMin, bMax, outOverlap) \
    { \
        int _aMinX = (aMin).x; \
        int _bMaxX = (bMax).x; \
        (outOverlap) = 0; \
        if (_bMaxX >= _aMinX && \
            (aMax).x >= (bMin).x && \
            (aMin).y <= (bMax).y && \
            (aMax).y >= (bMin).y && \
            (aMin).z <= (bMax).z) { \
            (outOverlap) = ((aMax).z >= (bMin).z); \
        } \
    }

static int CheckWeaponWallStrike(int side)
{
    WorldVector (*points)[WEAPON_CONTROL_POINT_COUNT] = COLLISION_SCRATCH->work.workPoints;
    int height;
    SVECTOR *normal;

    if ((gFighters[side].animInfo->flags & ANIM_FLAG_HOMING_JUMP) ||
        gPlayMode == PLAY_MODE_SLASH || gCurrentStageId == 11) {
        return -1;
    }

    height = QueryFloorHeight(points[side][1].x, points[side][1].y, points[side][1].z);
    if (height == FLOOR_QUERY_MISS) {
        height = FLOOR_HEIGHT_NONE;
    } else {
        height &= ~7;
    }
    normal = GetLastFloorNormal();
    if (height < gFighters[side].groundY) {
        WorldVector *row = points[side];
        if (height < row[1].y && !(GetLastFloorAttr() & FLOOR_ATTR_NO_IMPACT)) {
            if (gCurrentStageId != STAGE_ID_EXECUTIONERS_COVE || normal->vy >= FLOOR_WALKABLE_NORMAL_Y) {
                return 1;
            }
        }
    }

    {
        WorldVector *row = points[side];
        height = QueryFloorHeight(row[0].x, row[0].y, row[0].z);
    }
    if (height == FLOOR_QUERY_MISS) {
        height = FLOOR_HEIGHT_NONE;
    } else {
        height &= ~7;
    }
    if (height < gFighters[side].groundY) {
        WorldVector *row = points[side];
        if (height < row[0].y && !(GetLastFloorAttr() & FLOOR_ATTR_NO_IMPACT)) {
            if (gCurrentStageId != STAGE_ID_EXECUTIONERS_COVE || normal->vy >= FLOOR_WALKABLE_NORMAL_Y) {
                return 0;
            }
        }
    }
    return -1;
}

static int CheckWeaponClashAndEnvHit(void)
{
    CollisionWork *work;
    int weaponId;
    const WorldVector *currentPoints;
    WorldVector *workPoints;
    int side, bounceAction;
    Fighter *fighter;
    WorldVector *envPoints;
    WorldVector (*scalePoints)[WEAPON_CONTROL_POINT_COUNT];
    int action, frame;
    const AnimInfo *animInfo;
    int hitIndex;
    int p1Weapon, p2Weapon;
    int degenerateMask, primaryTri;
    WorldVector *p2Point1, *p2Point2;
    WorldVector *p1Point1, *p1Point2;

    work = &COLLISION_SCRATCH->work;
    if ((int)gFighters[0].activeFrames < CLASH_MIN_ACTIVE_FRAMES) {
        return 0;
    }
    for (side = 0; side < 2; ++side) {
        weaponId = gFighters[side].weaponId;
        currentPoints = &COLLISION_SCRATCH->weaponPoints[side][1];
        workPoints = work->workPoints[side];
        if (weaponId == WEAPON_ID_NAGINATA || weaponId == 9) {
            workPoints[0] = currentPoints[1];
            workPoints[1] = currentPoints[0];
            workPoints[2] = gFighters[side].prevWeaponPoints[2];
            workPoints[3] = gFighters[side].prevWeaponPoints[1];
        } else if (weaponId == WEAPON_ID_SLEDGEHAMMER) {
            workPoints[0] = currentPoints[1];
            workPoints[1] = currentPoints[2];
            workPoints[2] = gFighters[side].prevWeaponPoints[2];
            workPoints[3] = gFighters[side].prevWeaponPoints[3];
        } else if (weaponId != WEAPON_ID_GUN) {
            workPoints[0] = currentPoints[-1];
            workPoints[1] = currentPoints[0];
            workPoints[2] = gFighters[side].prevWeaponPoints[0];
            workPoints[3] = gFighters[side].prevWeaponPoints[1];
        }
    }

    side = 0;
    bounceAction = FORCED_ACTION_CLASH_RECOIL;
    envPoints = work->workPoints[0];
    do {
        fighter = &gFighters[side];
        weaponId = fighter->weaponId;
        if (weaponId != WEAPON_ID_GUN) {
            action = fighter->action;
            if (action == FIGHTER_ACTION_ATTACK) {
                animInfo = fighter->animInfo;
                frame = fighter->animFrame;
                if (frame >= animInfo->activeStartFrame &&
                    animInfo->activeEndFrame >= frame) {
                    if (CheckWeaponStagePropHit(side) != 0) {
                        TriggerFighterRumble(fighter, RUMBLE_WEAPON_ENV_HIT);
                        EmitCombatEvent(side, COMBAT_EVENT_WEAPON_ENV_HIT, &work->localPoint);
                        fighter->pendingAction = bounceAction;
                        fighter->hitActive = 0;
                    } else {
                        hitIndex = CheckWeaponWallStrike(side);
                        if (hitIndex >= 0) {
                            TriggerFighterRumble(fighter, RUMBLE_WEAPON_ENV_HIT);
                            EmitCombatEvent(side, COMBAT_EVENT_WEAPON_ENV_HIT, (VECTOR *)&envPoints[hitIndex]);
                            fighter->pendingAction = bounceAction;
                            fighter->hitActive = 0;
                        }
                    }
                }
            }
        }
        envPoints += WEAPON_CONTROL_POINT_COUNT;
        ++side;
    } while (side < 2);

    p1Weapon = gFighters[0].weaponId;
    if (p1Weapon == WEAPON_ID_GUN) {
        return 0;
    }
    p2Weapon = gFighters[1].weaponId;
    if (p2Weapon == WEAPON_ID_GUN) {
        return 0;
    }
    scalePoints = work->workPoints;
    for (side = 0; side < 2; ++side) {
        scalePoints[side][0].x >>= 1;
        scalePoints[side][0].y >>= 1;
        scalePoints[side][0].z >>= 1;
        scalePoints[side][1].x >>= 1;
        scalePoints[side][1].y >>= 1;
        scalePoints[side][1].z >>= 1;
        scalePoints[side][2].x >>= 1;
        scalePoints[side][2].y >>= 1;
        scalePoints[side][2].z >>= 1;
        scalePoints[side][3].x >>= 1;
        scalePoints[side][3].y >>= 1;
        scalePoints[side][3].z >>= 1;
    }

    degenerateMask = 0;
    primaryTri = 0;
    p2Point1 = &work->workPoints[1][1];
    p2Point2 = &work->workPoints[1][2];
    do {
        if (primaryTri != 0) {
            work->primaryVerts[1] = &work->workPoints[0][1];
            work->primaryVerts[0] = &work->workPoints[0][0];
            work->primaryVerts[2] = &work->workPoints[0][2];
        } else {
            work->primaryVerts[0] = &work->workPoints[0][2];
            work->primaryVerts[1] = &work->workPoints[0][3];
            work->primaryVerts[2] = &work->workPoints[0][1];
        }
        if (BuildNonDegenerateTriangleFrame(work) == 0) {
            degenerateMask |= 1 << primaryTri;
        } else {
            COMPUTE_TRIANGLE_AABB(work->primaryVerts, work->primaryMin, work->primaryMax);

            hitIndex = 0;
            do {
                if (hitIndex != 0) {
                    work->secondaryVerts[0] = &work->workPoints[1][0];
                    work->secondaryVerts[1] = p2Point1;
                    work->secondaryVerts[2] = p2Point2;
                } else {
                    work->secondaryVerts[0] = p2Point2;
                    work->secondaryVerts[1] = &work->workPoints[1][3];
                    work->secondaryVerts[2] = p2Point1;
                }
                COMPUTE_TRIANGLE_AABB(work->secondaryVerts, work->secondaryMin, work->secondaryMax);
                {
                    int overlap;
                    TEST_AABB_OVERLAP(work->primaryMin, work->primaryMax,
                                      work->secondaryMin, work->secondaryMax, overlap);
                    if (overlap != 0 &&
                        TestTriangleIntersectsAlignedTriangle(work, work->secondaryVerts[0],
                                                              work->secondaryVerts[1],
                                                              work->secondaryVerts[2]) != 0) {
                        return 1;
                    }
                }
                ++hitIndex;
            } while (hitIndex < 2);
        }
        ++primaryTri;
    } while (primaryTri < 2);

    primaryTri = 0;
    if (degenerateMask == 0) {
        return 0;
    }
    p1Point1 = &work->workPoints[0][1];
    do {
        if (primaryTri != 0) {
            work->primaryVerts[0] = &work->workPoints[1][0];
            work->primaryVerts[1] = &work->workPoints[1][1];
            work->primaryVerts[2] = &work->workPoints[1][2];
        } else {
            work->primaryVerts[0] = &work->workPoints[1][2];
            work->primaryVerts[1] = &work->workPoints[1][3];
            work->primaryVerts[2] = &work->workPoints[1][1];
        }
        if (BuildNonDegenerateTriangleFrame(work) != 0) {
            COMPUTE_TRIANGLE_AABB(work->primaryVerts, work->primaryMin, work->primaryMax);
            hitIndex = 0;
            do {
                int triBit = 1 << hitIndex;
                if ((degenerateMask & triBit) != 0) {
                    p1Point2 = &work->workPoints[0][2];
                    if (hitIndex != 0) {
                        work->secondaryVerts[0] = &work->workPoints[0][0];
                        work->secondaryVerts[1] = p1Point1;
                        work->secondaryVerts[2] = p1Point2;
                    } else {
                        work->secondaryVerts[0] = p1Point2;
                        work->secondaryVerts[1] = &work->workPoints[0][3];
                        work->secondaryVerts[2] = p1Point1;
                    }
                    COMPUTE_TRIANGLE_AABB(work->secondaryVerts, work->secondaryMin, work->secondaryMax);
                    {
                        int overlap;
                        TEST_AABB_OVERLAP(work->primaryMin, work->primaryMax,
                                          work->secondaryMin, work->secondaryMax, overlap);
                        if (overlap != 0 &&
                            TestTriangleIntersectsAlignedTriangle(work, work->secondaryVerts[0],
                                                                  work->secondaryVerts[1],
                                                                  work->secondaryVerts[2]) != 0) {
                            return 1;
                        }
                    }
                }
                ++hitIndex;
            } while (hitIndex < 2);
        }
        ++primaryTri;
    } while (primaryTri < 2);

    return 0;
}

static void ResolveWeaponVsBodyHits(void)
{
    CollisionWork *work;
    int hitMasks[2];
    int side, anyClash, emitClashEvent;
    int pointA, segmentCount, gunHits;
    WorldVector *prevBasePoint, *currentTipPoint;
    Fighter *defender, *attacker;
    int weaponId, segment;
    int pointB, checkVital, triangleCount, hitPart;
    WorldVector *midBasePoint, *midTipPoint;
    int dx, dy, dz;
    int isNonDegenerate;
    const FighterHitSphere *sphere;
    int bestDistance, distance, part, partMask, bitOne;
    int baseX, baseZ, tipX, tipZ;
    int angle, awayHeading, tipPushX, tipPushZ;
    int bladePushX, bladePushZ;
    int frame;
    const AnimInfo *animInfo;

    work = &COLLISION_SCRATCH->work;
    emitClashEvent = 1;
    prevBasePoint = &work->workPoints[0][2];
    anyClash = 0;
    side = 0;
    currentTipPoint = &work->workPoints[0][1];
    do {
        defender = &gFighters[side];
        attacker = gFighters;
        attacker += side == 0 ? 1 : 0;
        hitMasks[0] = 0;
        hitMasks[1] = 0;
        weaponId = attacker->weaponId;
        if (weaponId == WEAPON_ID_GUN) {
            segmentCount = 1;
            FireGunshotRayAndCheckHit(defender, &hitMasks[0], &hitMasks[1]);
            gunHits = hitMasks[0];
        } else if (weaponId == WEAPON_ID_NAGINATA || weaponId == WEAPON_ID_SLEDGEHAMMER) {
            segmentCount = 2;
        } else {
            segmentCount = 1;
        }
        for (segment = 0; segment < segmentCount; ++segment) {
            checkVital = 1;
            if (segment == 0) {
                if (attacker->weaponId == 9) {
                    pointA = 2;
                    pointB = 1;
                } else {
                    pointA = 0;
                    pointB = 1;
                }
            } else {
                weaponId = attacker->weaponId;
                if (weaponId == WEAPON_ID_NAGINATA) {
                    pointA = 2;
                    pointB = 0;
                    checkVital = 0;
                } else if (weaponId == WEAPON_ID_SLEDGEHAMMER) {
                    pointA = 2;
                    pointB = 3;
                }
            }
            work->workPoints[0][0] = COLLISION_SCRATCH->weaponPoints[side == 0 ? 1 : 0][pointA];
            work->workPoints[0][1] = COLLISION_SCRATCH->weaponPoints[side == 0 ? 1 : 0][pointB];
            work->workPoints[0][2] = attacker->prevWeaponPoints[pointA];
            work->workPoints[0][3] = attacker->prevWeaponPoints[pointB];
            work->workPoints[1][0].x = (work->workPoints[0][0].x + work->workPoints[0][2].x) / 2;
            work->workPoints[1][0].y = (work->workPoints[0][0].y + work->workPoints[0][2].y) / 2;
            work->workPoints[1][0].z = (work->workPoints[0][0].z + work->workPoints[0][2].z) / 2;
            work->workPoints[1][1].x = (work->workPoints[0][1].x + work->workPoints[0][3].x) / 2;
            work->workPoints[1][1].y = (work->workPoints[0][1].y + work->workPoints[0][3].y) / 2;
            work->workPoints[1][1].z = (work->workPoints[0][1].z + work->workPoints[0][3].z) / 2;
            dx = work->workPoints[0][1].x - work->workPoints[0][3].x;
            dy = work->workPoints[0][1].y - work->workPoints[0][3].y;
            dz = work->workPoints[0][1].z - work->workPoints[0][3].z;
            distance = dx * dx + dy * dy + dz * dz;
            triangleCount = 2;
            if (distance > WEAPON_SWEEP_SUBDIVIDE_DIST_SQ) {
                triangleCount = 4;
            }
            for (hitPart = 0; hitPart < triangleCount; ++hitPart) {
                midTipPoint = &work->workPoints[1][1];
                midBasePoint = &work->workPoints[1][0];
                if (triangleCount == 2) {
                    if (hitPart == 0) {
                        work->primaryVerts[0] = prevBasePoint;
                        work->primaryVerts[1] = &work->workPoints[0][3];
                        work->primaryVerts[2] = currentTipPoint;
                    } else {
                        work->primaryVerts[0] = &work->workPoints[0][0];
                        work->primaryVerts[1] = currentTipPoint;
                        work->primaryVerts[2] = prevBasePoint;
                    }
                } else {
                    switch (hitPart) {
                    case 0:
                        work->primaryVerts[0] = &work->workPoints[0][3];
                        work->primaryVerts[1] = prevBasePoint;
                        work->primaryVerts[2] = midTipPoint;
                        break;
                    case 1:
                        work->primaryVerts[0] = midTipPoint;
                        work->primaryVerts[1] = midBasePoint;
                        work->primaryVerts[2] = prevBasePoint;
                        break;
                    case 2:
                        work->primaryVerts[0] = midTipPoint;
                        work->primaryVerts[1] = midBasePoint;
                        work->primaryVerts[2] = currentTipPoint;
                        break;
                    case 3:
                        work->primaryVerts[0] = currentTipPoint;
                        work->primaryVerts[1] = &work->workPoints[0][0];
                        work->primaryVerts[2] = midBasePoint;
                        break;
                    }
                }
                COMPUTE_TRIANGLE_AABB(work->primaryVerts, work->primaryMin, work->primaryMax);
                work->hits = hitMasks[0];
                work->vitalHits = hitMasks[1];
                isNonDegenerate = BuildTriangleFrame(work);
                TestTriangleVsBodySpheres(defender, isNonDegenerate, checkVital);
                hitMasks[0] |= work->hits;
                hitMasks[1] |= work->vitalHits;
            }
        }

        if (hitMasks[0] != 0 || gWeaponClashOccurred != 0) {
            int hadClash;
            hadClash = gWeaponClashOccurred;
            work->normalOrTarget.x = attacker->prevWeaponPoints[1].x;
            work->normalOrTarget.y = attacker->prevWeaponPoints[1].y;
            sphere = gFighterHitSpheres[side];
            work->normalOrTarget.z = attacker->prevWeaponPoints[1].z;
            if (hadClash != 0) {
                dx = work->clashContactPoint.x - work->normalOrTarget.x;
                dy = work->clashContactPoint.y - work->normalOrTarget.y;
                dz = work->clashContactPoint.z - work->normalOrTarget.z;
                hitPart = HIT_PART_NONE;
                bestDistance = dx * dx + dy * dy + dz * dz;
            } else {
                bestDistance = COLLISION_DIST_MAX;
            }
            part = 0;
            bitOne = 1;
            partMask = hitMasks[0];
            do {
                if ((partMask & (bitOne << part)) != 0) {
                    dx = COLLISION_SCRATCH->bodyPartPos[side][part].x - work->normalOrTarget.x;
                    dy = COLLISION_SCRATCH->bodyPartPos[side][part].y - work->normalOrTarget.y;
                    dz = COLLISION_SCRATCH->bodyPartPos[side][part].z - work->normalOrTarget.z;
                    distance = dx * dx + dy * dy + dz * dz - sphere->outerRadiusSq;
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        hitPart = part;
                    }
                }
                ++part;
                ++sphere;
            } while (part < FIGHTER_HIT_SPHERE_COUNT);
            if (hitPart != HIT_PART_NONE) {
                if (attacker->hitActive == 0 || attacker->action != FIGHTER_ACTION_ATTACK ||
                    (animInfo = attacker->animInfo,
                     frame = attacker->animFrame,
                     frame + 2 < animInfo->activeStartFrame || animInfo->activeEndFrame < frame)) {
                    baseX = COLLISION_SCRATCH->weaponPoints[side == 0 ? 1 : 0][0].x;
                    baseZ = COLLISION_SCRATCH->weaponPoints[side == 0 ? 1 : 0][0].z;
                    tipX = COLLISION_SCRATCH->weaponPoints[side == 0 ? 1 : 0][1].x;
                    tipZ = COLLISION_SCRATCH->weaponPoints[side == 0 ? 1 : 0][1].z;
                    dx = baseX - attacker->position.vx;
                    dz = baseZ - attacker->position.vz;
                    pointA = dx * dx + dz * dz; /* reused, as in the original */
                    dx = tipX - attacker->position.vx;
                    dz = tipZ - attacker->position.vz;
                    pointB = dx * dx + dz * dz; /* reused, as in the original */
                    dx = tipX - baseX;
                    dz = tipZ - baseZ;
                    if (pointB < pointA) {
                        dx = -dx;
                        dz = -dz;
                    }
                    distance = ratan2(attacker->weaponTipVel.vx,
                                      attacker->weaponTipVel.vz); /* reused, as in the original */
                    awayHeading = defender->angleToOpponent - ANGLE_HALF;
                    distance = (distance - awayHeading) & ANGLE_MASK;
                    if (distance >= ANGLE_HALF) {
                        distance = ANGLE_FULL - distance;
                    }
                    distance = ANGLE_QUARTER - distance;
                    if (distance < 0) {
                        distance = 0;
                    }
                    bladePushX = dx / 4;
                    tipPushX = attacker->weaponTipVel.vx * distance;
                    defender->velocity.x += bladePushX + ((tipPushX / 2) >> 11);
                    bladePushZ = dz / 4;
                    tipPushZ = attacker->weaponTipVel.vz * distance;
                    defender->velocity.z += bladePushZ + ((tipPushZ / 2) >> 11);
                }
            }
            if (attacker->attackActive != 0 && attacker->hitActive != 0 &&
                (attacker->weaponId != WEAPON_ID_GUN || gunHits != 0)) {
                angle = defender->angles.heading - attacker->angles.heading;
                if (defender->climbState != 0) {
                    angle += ANGLE_HALF;
                }
                angle &= ANGLE_MASK;
                if (angle >= ANGLE_HALF) {
                    angle -= ANGLE_FULL;
                }
                if (angle < 0) {
                    angle = -angle;
                }
                if (hitPart != HIT_PART_NONE) {
                    ApplyFighterHitImpact(HIT_BY_BLADE, defender, hitPart, angle,
                                          defender->action == FIGHTER_ACTION_DOWNED ? 1 : hitMasks[1] & (1 << hitPart),
                                          0);
                } else {
                    anyClash = 1;
                    emitClashEvent &= ResolveBladeClashReaction(defender, angle, (VECTOR *)&work->clashContactPoint);
                }
                attacker->hitActive = 0;
            }
        }
        ++side;
    } while (side < 2);
    if (anyClash != 0 && emitClashEvent != 0) {
        if (gFighters[0].action == FIGHTER_ACTION_ATTACK) {
            EmitCombatEvent(0, COMBAT_EVENT_WEAPON_CLASH, (VECTOR *)&work->clashContactPoint);
        }
        if (gFighters[1].action == FIGHTER_ACTION_ATTACK) {
            EmitCombatEvent(1, COMBAT_EVENT_WEAPON_CLASH, (VECTOR *)&work->clashContactPoint);
        }
    }
}

void UpdateCombatCollisions(void)
{
    Fighter *p1 = gFighters;
    Fighter *p2 = p1 + 1;
    int p1AnimFlags, p2AnimFlags;
    int groundDeltaY;
    int i;

    if (gBladeLockState != BLADE_LOCK_NONE) {
        UpdateBladeLockState();
        UpdateBladeLockSeparation();
        gWeaponClashOccurred = 0;
    } else {
        ResolveFighterBodyCollision();
        if ((gWeaponClashOccurred = CheckWeaponClashAndEnvHit()) != 0) {
            COLLISION_SCRATCH->work.clashContactPoint.x =
                ((COLLISION_SCRATCH->weaponPoints[0][0].x + COLLISION_SCRATCH->weaponPoints[0][1].x +
                  COLLISION_SCRATCH->weaponPoints[1][0].x + COLLISION_SCRATCH->weaponPoints[1][1].x) * 3 +
                 (gFighters[0].prevWeaponPoints[0].x + gFighters[0].prevWeaponPoints[1].x +
                  gFighters[1].prevWeaponPoints[0].x + gFighters[1].prevWeaponPoints[1].x)) / 16;
            COLLISION_SCRATCH->work.clashContactPoint.y =
                ((COLLISION_SCRATCH->weaponPoints[0][0].y + COLLISION_SCRATCH->weaponPoints[0][1].y +
                  COLLISION_SCRATCH->weaponPoints[1][0].y + COLLISION_SCRATCH->weaponPoints[1][1].y) * 3 +
                 (gFighters[0].prevWeaponPoints[0].y + gFighters[0].prevWeaponPoints[1].y +
                  gFighters[1].prevWeaponPoints[0].y + gFighters[1].prevWeaponPoints[1].y)) / 16;
            COLLISION_SCRATCH->work.clashContactPoint.z =
                ((COLLISION_SCRATCH->weaponPoints[0][0].z + COLLISION_SCRATCH->weaponPoints[0][1].z +
                  COLLISION_SCRATCH->weaponPoints[1][0].z + COLLISION_SCRATCH->weaponPoints[1][1].z) * 3 +
                 (gFighters[0].prevWeaponPoints[0].z + gFighters[0].prevWeaponPoints[1].z +
                  gFighters[1].prevWeaponPoints[0].z + gFighters[1].prevWeaponPoints[1].z)) / 16;
            if (gFighters[0].attackActive != 0 && gFighters[1].attackActive != 0 &&
                ((p1AnimFlags = gFighters[0].animInfo->flags) &
                 (ANIM_FLAG_DOWNED | ANIM_FLAG_HOMING_JUMP | ANIM_FLAG_RUN)) == 0 &&
                ((p2AnimFlags = gFighters[1].animInfo->flags) &
                 (ANIM_FLAG_DOWNED | ANIM_FLAG_HOMING_JUMP | ANIM_FLAG_RUN)) == 0 &&
                gFighters[0].action != FIGHTER_ACTION_JUMP && gFighters[1].action != FIGHTER_ACTION_JUMP &&
                gFighters[0].pendingAction == -1 && gFighters[1].pendingAction == -1 &&
                gFighters[0].legsIntact != 0 && gFighters[1].legsIntact != 0 &&
                (p1AnimFlags & ANIM_FLAG_STANCE_MASK) == (p2AnimFlags & ANIM_FLAG_STANCE_MASK) &&
                gFighters[0].pendingFallAction == -1 && gFighters[1].pendingFallAction == -1) {
                groundDeltaY = gFighters[0].groundY - gFighters[1].groundY;
                if (groundDeltaY < 0) {
                    groundDeltaY = -groundDeltaY;
                }
                if (groundDeltaY < BLADE_LOCK_MAX_GROUND_DIFF) {
                    gBladeLockState = BLADE_LOCK_ACTIVE;
                    gBladeLockTimer = 0;
                    InitFighterBladeLock(p1);
                    InitFighterBladeLock(p2);
                    EmitCombatEvent(0, COMBAT_EVENT_WEAPON_CLASH, (VECTOR *)&COLLISION_SCRATCH->work.clashContactPoint);
                    EmitCombatEvent(1, COMBAT_EVENT_WEAPON_CLASH, (VECTOR *)&COLLISION_SCRATCH->work.clashContactPoint);
                }
            }
        }
        if (gBladeLockState == BLADE_LOCK_NONE) {
            ResolveWeaponVsBodyHits();
        }
    }

    for (i = 0; i < WEAPON_CONTROL_POINT_COUNT; ++i) {
        gFighters[0].prevWeaponPoints[i] = COLLISION_SCRATCH->weaponPoints[0][i];
        gFighters[1].prevWeaponPoints[i] = COLLISION_SCRATCH->weaponPoints[1][i];
    }

    for (i = 0; i < 2; ++i) {
        WorldVector *spheres = COLLISION_SCRATCH->bodyPartPos[i];
        gFighters[i].torsoCenter.x = (spheres[1].x + spheres[2].x + spheres[3].x) / 3;
        gFighters[i].torsoCenter.y = (spheres[1].y + spheres[2].y + spheres[3].y) / 3;
        gFighters[i].torsoCenter.z = (spheres[1].z + spheres[2].z + spheres[3].z) / 3;
    }
    CheckSubWeaponFighterHits();
    DrawAllWeaponTrails();
    if (gBladeLockState == BLADE_LOCK_ENDING) {
        gBladeLockState = BLADE_LOCK_NONE;
    }
}

void TestTriangleVsBodySpheres(Fighter *fighter, int isNonDegenerate, int checkVital)
{
    CollisionWork *work;
    unsigned int hits, vitalHits;
    FighterHitSphere *sphere;
    int side;
    int part;
    int radius;
    int result, vitalHit;

    work = &COLLISION_SCRATCH->work;
    part = 0;
    hits = COLLISION_SCRATCH->work.hits;
    side = fighter->side;
    vitalHits = COLLISION_SCRATCH->work.vitalHits;
    sphere = gFighterHitSpheres[side];
    do {
        if ((hits & (1 << part)) == 0 &&
            (fighter->leftArmIntact != 0 ||
             (unsigned int)(part - CRIPPLED_LEFT_ARM_SKIP_FIRST) >= CRIPPLED_LEFT_ARM_SKIP_COUNT)) {
            radius = sphere->outerRadius;
            result = 0;
            if (work->primaryMax.x < COLLISION_SCRATCH->bodyPartPos[side][part].x - radius ||
                COLLISION_SCRATCH->bodyPartPos[side][part].x + radius < work->primaryMin.x) {
                result = 1;
            } else if (work->primaryMax.y < COLLISION_SCRATCH->bodyPartPos[side][part].y - radius ||
                       COLLISION_SCRATCH->bodyPartPos[side][part].y + radius < work->primaryMin.y) {
                result = 1;
            } else if (work->primaryMax.z < COLLISION_SCRATCH->bodyPartPos[side][part].z - radius ||
                       COLLISION_SCRATCH->bodyPartPos[side][part].z + radius < work->primaryMin.z) {
                result = 1;
            }
            if (result == 0) {
                if (isNonDegenerate != 0) {
                    result = TestSphereVsAlignedTriangle(0, work, &COLLISION_SCRATCH->bodyPartPos[side][part],
                                                         radius, sphere->outerRadiusSq);
                    if (result != 0 && sphere->isVital != 0 && checkVital != 0) {
                        vitalHit = TestSphereVsAlignedTriangle(1, work, 0, sphere->vitalRadius, sphere->vitalRadiusSq);
                        if (vitalHit != 0) {
                            vitalHits |= 1 << part;
                        }
                    }
                } else {
                    result = TestSphereVsAlignedSegment(0, work, &COLLISION_SCRATCH->bodyPartPos[side][part],
                                                        radius, sphere->outerRadiusSq);
                    if (result != 0 && sphere->isVital != 0 && checkVital != 0) {
                        vitalHit = TestSphereVsAlignedSegment(1, work, 0, sphere->vitalRadius, sphere->vitalRadiusSq);
                        if (vitalHit != 0) {
                            vitalHits |= 1 << part;
                        }
                    }
                }
                if (result != 0) {
                    hits |= 1 << part;
                }
            }
        }
        ++part;
        ++sphere;
    } while (part < FIGHTER_HIT_SPHERE_COUNT);
    work->hits = hits;
    work->vitalHits = vitalHits;
}
