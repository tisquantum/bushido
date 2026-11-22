/* Instant replay: 256-frame per-fighter snapshot ring buffer, 64-slot combat-event effect
 * marks, post-match/replay state routing, and 195-frame replay setup and playback
 * (gGameState 17-18). */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include "pad.h"
#include "billboard.h"
#include "camera.h"
#include "cd.h"
#include "collision_scratch.h"
#include "combat_event.h"
#include "combat_state.h"
#include "decal.h"
#include "effect.h"
#include "fighter.h"
#include "fighter_cloth.h"
#include "fighter_face.h"
#include "fighter_hit.h"
#include "fighter_pose.h"
#include "floor_collision.h"
#include "font_hud.h"
#include "game_state.h"
#include "globals.h"
#include "link.h"
#include "main.h"
#include "model_arena.h"
#include "motion_file.h"
#include "point_light.h"
#include "pose_stream.h"
#include "replay.h"
#include "scene.h"
#include "sightline.h"
#include "sound.h"
#include "stage_callbacks.h"
#include "stage_cull.h"
#include "stage_env.h"
#include "stage_prop_hit.h"
#include "story_state.h"
#include "subweapon.h"
#include "weapon_trail.h"
#include "match_setup.h"

#define REPLAY_RING_FRAMES           256
#define REPLAY_RING_MASK             0xff
#define REPLAY_MARK_COUNT            64
#define REPLAY_MARK_MASK             0x3f

/* ReplayFrame.packedHeading bit layout: [11:0] heading, [13:12] surfaceFlags & 3, [15] leftArmIntact. */
#define REPLAY_HEADING_MASK          0xfff
#define REPLAY_SURFACE_SHIFT         12
#define REPLAY_SURFACE_RECORD_MASK   3
#define REPLAY_SURFACE_PLAYBACK_MASK 7
#define REPLAY_LEFT_ARM_INTACT_SHIFT 15
#define REPLAY_LEFT_ARM_INTACT_BIT   0x8000

/* ReplayFrame.blendAndFlipFlags bit layout: [11:0] Q12 weight/frac, [12] blending, [15:13] pose flips. */
#define REPLAY_BLEND_WEIGHT_MASK     0xfff
#define REPLAY_BLEND_ACTIVE_BIT      0x1000
#define REPLAY_FLIP_CLIMB_BIT        0x2000
#define REPLAY_FLIP_SUPINE_BIT       0x4000
#define REPLAY_FLIP_PRONE_BIT        0x8000

/* ReplayFrame.lookAnglesAndFace[0] bit layout: [11:0] lookRoll, [14:12] faceExpression. */
#define REPLAY_LOOK_ROLL_MASK        0xfff
#define REPLAY_FACE_SHIFT            12
#define REPLAY_FACE_MASK             7

#define REPLAY_BLOOD_DIR_SHIFT       5
#define REPLAY_CAMERA_YAW_OFFSET     0x200
#define REPLAY_CAMERA_DISTANCE       10000
#define REPLAY_FALLBACK_FLOOR_OFFSET 900

#define FIGHTER_CLIMB_ENTER          1

/* 32-byte per-fighter snapshot in the 256 x 2 ring held in gSharedWorkBuffer. */
typedef struct ReplayFrame {
    int x;
    int z;
    short packedHeading;
    short y;
    MotionPlacement *placement;
    short shadowGroundY;
    short combatEvent;
    unsigned char markIndex;
    unsigned char animFrame;
    short blendTargetFrame;
    short blendAndFlipFlags;
    unsigned short lookAnglesAndFace[3];
} ReplayFrame;

/* 10-byte combat-event offset and quantized blood-spray direction in gReplayEffectMarks[64]. */
typedef struct ReplayEffectMark {
    short x;
    short y;
    short z;
    unsigned char inUse;
    signed char dirX;
    signed char dirY;
    signed char dirZ;
} ReplayEffectMark;

int gReplayPrevPoseFrame[2];
short gReplayPlayFrame;
int gReplayStartFrame;

VECTOR gReplayFighterPos[2];
SVECTOR gReplayFighterAngles[2];
ReplayEffectMark gReplayEffectMarks[REPLAY_MARK_COUNT]; /* the image gives it 736 bytes: nothing reads the 96
                                                           after the 64 marks */

static void TransitionAfterMatchOrReplay(void)
{
    if (gFighters[0].action == FIGHTER_ACTION_DEAD || gFighters[0].action == FIGHTER_ACTION_FATAL_KO ||
        gFighters[0].action == FIGHTER_ACTION_DEFEATED || (gFighters[0].placement->flags & MOTION_FLAG_FATAL)) {
        if (gFighters[1].action == FIGHTER_ACTION_DEAD || gFighters[1].action == FIGHTER_ACTION_FATAL_KO ||
            gFighters[1].action == FIGHTER_ACTION_DEFEATED || (gFighters[1].placement->flags & MOTION_FLAG_FATAL)) {
            if (gPlayMode == PLAY_MODE_STORY) {
                if (gStoryChallengerActive) {
                    gGameState = GAME_STATE_INIT_VICTORY_SCENE;
                } else {
                    gGameState = gOptionContinueEnabled ? GAME_STATE_INIT_CONTINUE_SCREEN : GAME_STATE_INIT_MAIN_MENU;
                }
            } else if (gPlayMode == PLAY_MODE_SLASH || gPlayMode == PLAY_MODE_VS) {
                gGameState = gOptionContinueEnabled ? GAME_STATE_INIT_CONTINUE_SCREEN : GAME_STATE_INIT_MAIN_MENU;
            } else if (gPlayMode == PLAY_MODE_LINK) {
                gGameState = GAME_STATE_INIT_CONTINUE_SCREEN;
            }
            return;
        }
    }

    if (gPlayMode == PLAY_MODE_SLASH) {
        if (gMatchWinnerSide == 0) {
            gGameState = GAME_STATE_INIT_VICTORY_SCENE;
        } else {
            gGameState = gOptionContinueEnabled ? GAME_STATE_INIT_CONTINUE_SCREEN : GAME_STATE_INIT_MAIN_MENU;
        }
    } else if (gPlayMode == PLAY_MODE_STORY) {
        if (gMatchWinnerSide == 0) {
            gStoryDefeatedMask |= 1 << gFighters[1].characterClass;
            if (gStoryChallengerActive) {
                gGameState = GAME_STATE_INIT_VICTORY_SCENE;
            } else if (gStoryBoutIndex == STORY_REGULAR_BOUT_COUNT) {
                if (gBushidoHonorIntact == 0) {
                    gGameState = GAME_STATE_INIT_BAD_ENDING_SLIDE;
                } else if (IsTrueEndingEligible()) {
                    gGameState = GAME_STATE_INIT_STORY_CUTSCENE;
                } else {
                    gGameState = GAME_STATE_PLAY_STORY_ENDING_MOVIE;
                }
            } else if (gStoryBoutIndex == STORY_TRUE_ENDING_WON) {
                if (gBushidoHonorIntact != 0) {
                    gGameState = GAME_STATE_PLAY_STORY_ENDING_MOVIE;
                } else {
                    gGameState = GAME_STATE_INIT_BAD_ENDING_SLIDE;
                }
            } else {
                gGameState = GAME_STATE_INIT_STORY_CUTSCENE;
            }
        } else {
            if (gStoryChallengerActive) {
                gGameState = GAME_STATE_INIT_VICTORY_SCENE;
            } else {
                gGameState = gOptionContinueEnabled ? GAME_STATE_INIT_CONTINUE_SCREEN : GAME_STATE_INIT_MAIN_MENU;
            }
        }
    } else if (gPlayMode == PLAY_MODE_VS || gPlayMode == PLAY_MODE_LINK) {
        gGameState = GAME_STATE_INIT_VICTORY_SCENE;
    }
}

void ResetReplayBuffer(void)
{
    ReplayFrame (*pairs)[2];
    int frameIndex;
    int markIndex;

    gCombatFrameCounter = 0;
    pairs = (ReplayFrame (*)[2])gSharedWorkBuffer;
    for (frameIndex = 0; frameIndex < REPLAY_RING_FRAMES; ++frameIndex) {
        pairs[frameIndex][0].combatEvent = 0;
        pairs[frameIndex][1].combatEvent = 0;
    }

    gReplayMarkCursor = 0;
    for (markIndex = 0; markIndex < REPLAY_MARK_COUNT; ++markIndex) {
        gReplayEffectMarks[markIndex].inUse = 0;
    }
}

void RecordFighterReplayFrame(int side, int blendTargetFrame)
{
    Fighter *fighter;
    ReplayFrame *frame;

    fighter = &gFighters[side];
    frame = &((ReplayFrame *)gSharedWorkBuffer)[((unsigned char)gCombatFrameCounter << 1) + side];
    frame->x = fighter->position.vx;
    frame->y = fighter->position.vy;
    frame->z = fighter->position.vz;
    frame->packedHeading = (fighter->angles.heading & REPLAY_HEADING_MASK) |
        (fighter->leftArmIntact << REPLAY_LEFT_ARM_INTACT_SHIFT) |
        ((fighter->surfaceFlags & REPLAY_SURFACE_RECORD_MASK) << REPLAY_SURFACE_SHIFT);
    frame->placement = fighter->placement;
    frame->animFrame = fighter->animFrame;
    if (fighter->poseBlendWeight > 0) {
        frame->blendTargetFrame = fighter->lastSampledFrame;
        frame->blendAndFlipFlags = fighter->poseBlendWeight + REPLAY_BLEND_ACTIVE_BIT;
        if (fighter->climbState == FIGHTER_CLIMB_ENTER) {
            frame->blendAndFlipFlags |= REPLAY_FLIP_CLIMB_BIT;
        }
        if (fighter->prevFacingState == MOTION_FACING_SUPINE_FLIP) {
            frame->blendAndFlipFlags |= REPLAY_FLIP_SUPINE_BIT;
        }
        if (fighter->prevFacingState == MOTION_FACING_PRONE_FLIP) {
            frame->blendAndFlipFlags |= REPLAY_FLIP_PRONE_BIT;
        }
    } else {
        frame->blendTargetFrame = blendTargetFrame;
        frame->blendAndFlipFlags = fighter->animFrac;
    }
    frame->shadowGroundY = fighter->shadowGroundY;
    if (frame->combatEvent != 0) {
        gReplayEffectMarks[frame->markIndex].inUse = 0;
        frame->combatEvent = 0;
    }
    frame->lookAnglesAndFace[0] = (fighter->lookRoll & REPLAY_LOOK_ROLL_MASK) |
        (fighter->faceExpression << REPLAY_FACE_SHIFT);
    frame->lookAnglesAndFace[1] = fighter->lookYaw;
    frame->lookAnglesAndFace[2] = fighter->lookPitch;
}

void RecordReplayCombatEvent(int side, int event, VECTOR *position)
{
    ReplayFrame *frame;
    ReplayEffectMark *mark;

    frame = &((ReplayFrame *)gSharedWorkBuffer)[((unsigned char)gCombatFrameCounter << 1) + side];
    if (frame->combatEvent == 0) {
        mark = &gReplayEffectMarks[gReplayMarkCursor & REPLAY_MARK_MASK];
        if (mark->inUse == 0) {
            mark->inUse = 1;
            mark->x = position->vx - frame->x;
            mark->y = position->vy - frame->y;
            mark->z = position->vz - frame->z;
            if (event == COMBAT_EVENT_FATAL_HIT) {
                mark->dirX = gBloodSprayDir.vx >> REPLAY_BLOOD_DIR_SHIFT;
                mark->dirY = gBloodSprayDir.vy >> REPLAY_BLOOD_DIR_SHIFT;
                mark->dirZ = gBloodSprayDir.vz >> REPLAY_BLOOD_DIR_SHIFT;
            }
            frame->markIndex = gReplayMarkCursor & REPLAY_MARK_MASK;
            frame->combatEvent = event;
            gReplayMarkCursor++;
        }
    }
}

void InitBattleReplay(void)
{
    BeginLinkSyncPause();
    WaitGpuIdle();
    if (gPovModeEnabled == 1) {
        StopAndWaitCdIdle();
    }
    SetGeomScreen(CalcGeomScreenFromFov(DEFAULT_SCENE_FOV));
    LoadVisibilityMaskPreset(0);
    gCameraState.target.vx = (gFighters[0].position.vx + gFighters[1].position.vx) / 2;
    gCameraState.target.vy = (gFighters[0].position.vy + gFighters[1].position.vy) / 2;
    gCameraState.target.vz = (gFighters[0].position.vz + gFighters[1].position.vz) / 2;
    gCameraState.angles.vx = 0;
    gCameraState.angles.vy = REPLAY_CAMERA_YAW_OFFSET - ratan2(gFighters[1].position.vx - gFighters[0].position.vx,
                                          gFighters[1].position.vz - gFighters[0].position.vz);
    gCameraState.angles.vz = 0;
    gCameraState.distance = REPLAY_CAMERA_DISTANCE;
    gCameraOffset.target.vx = 0;
    gCameraOffset.target.vy = 0;
    gCameraOffset.target.vz = 0;
    gCameraOffset.angles.vx = 0;
    gCameraOffset.angles.vy = 0;
    gCameraOffset.angles.vz = 0;
    gCameraOffset.distance = 0;
    if (gPovModeEnabled == 1) {
        LoadMatchFighters(gFighters[0].fighterId, gFighters[1].fighterId, gFighters[0].weaponId, gFighters[1].weaponId);
        LoadStageAndLighting(gCurrentStageId);
        gStageCameraWalls = GetStageCameraWalls();
        SetStagePropMarkers(GetStagePropMarkers());
    }
    ResetFighterWeaponTrails(0);
    ResetFighterWeaponTrails(1);
    ClearBillboardEffects();
    ClearGroundDecals();
    StopPointLightFlash();
    ClearEffectPool();
    InitStageSnowIfEnabled();
    gReplayPlayFrame = 0;
    gReplayStartFrame = gCombatFrameCounter - REPLAY_DURATION_FRAMES;
    if (gReplayStartFrame < 0) {
        gReplayStartFrame = 0;
    }
    gReplayPrevPoseFrame[0] = -1;
    gReplayPrevPoseFrame[1] = -1;
    RewindSubWeaponsForReplay();
    StartStageAmbientSounds();
    RequestClothSnapReset();
    gDisplayPresentEnabled = 1;
    gGameState = GAME_STATE_UPDATE_BATTLE_REPLAY;
    WaitLinkSyncBarrier();
}

void UpdateBattleReplay(void)
{
    PoseAngles pose[2][FIGHTER_JOINT_COUNT];
    VECTOR effectPos;
    ReplayFrame *frame;
    ReplayEffectMark *mark;
    int side;
    int poseFrame;
    int samePoseFrame;
    int floorY;

    if (gReplayPlayFrame == 0) {
        for (side = 0; side < 2; side++) {
            frame = &((ReplayFrame *)gSharedWorkBuffer)[(unsigned char)gReplayStartFrame * 2 + side];
            gReplayFighterPos[side].vx = frame->x;
            gReplayFighterPos[side].vy = frame->y;
            gReplayFighterPos[side].vz = frame->z;
            gReplayFighterAngles[side].vx = 0;
            gReplayFighterAngles[side].vy = frame->packedHeading;
            gReplayFighterAngles[side].vz = 0;
        }
    }
    UpdateBattleCamera(BATTLE_CAMERA_REPLAY, &gReplayFighterPos[gMatchWinnerSide],
                       gMatchWinnerSide ? &gReplayFighterPos[0] : &gReplayFighterPos[1],
                       gReplayPlayFrame, 0, 0);
    UpdateSceneCameraAndWind();
    gReplayPlaybackFrame = gReplayStartFrame + gReplayPlayFrame;
    for (side = 0; side < 2; side++) {
        frame = &((ReplayFrame *)gSharedWorkBuffer)[((gReplayStartFrame + gReplayPlayFrame) &
                                                     REPLAY_RING_MASK) * 2 + side];
        if (frame->placement->flags & MOTION_FLAG_FORWARD) {
            poseFrame = MOTION_FILE->animBaseFrames[frame->placement->animClipId - 1] + frame->animFrame;
        } else {
            poseFrame = MOTION_FILE->animBaseFrames[frame->placement->animClipId - 1] +
                (frame->placement->frameLimit - 1) - (frame->animFrame - frame->placement->startFrame);
        }
        DecodePoseFramePair(poseFrame, frame->blendTargetFrame, pose[0], pose[1], (PoseWork *)&COLLISION_SCRATCH->work);
        samePoseFrame = gReplayPrevPoseFrame[side] == poseFrame;
        gReplayPrevPoseFrame[side] = poseFrame;
        if (!(frame->packedHeading & REPLAY_LEFT_ARM_INTACT_BIT)) {
            ApplyDisabledLeftArmPose(pose[0]);
            ApplyDisabledLeftArmPose(pose[1]);
        }
        pose[0][POSE_JOINT_HEAD].x += frame->lookAnglesAndFace[0];
        pose[1][POSE_JOINT_HEAD].x += frame->lookAnglesAndFace[0];
        pose[0][POSE_JOINT_HEAD].y += frame->lookAnglesAndFace[1];
        pose[1][POSE_JOINT_HEAD].y += frame->lookAnglesAndFace[1];
        pose[0][POSE_JOINT_HEAD].z += frame->lookAnglesAndFace[2];
        pose[1][POSE_JOINT_HEAD].z += frame->lookAnglesAndFace[2];
        if (frame->blendAndFlipFlags & REPLAY_FLIP_CLIMB_BIT) {
            FlipPoseForClimbTransition(pose[1]);
        }
        if (frame->blendAndFlipFlags & REPLAY_FLIP_SUPINE_BIT) {
            FlipPoseFromSupineState(pose[1]);
        }
        if (frame->blendAndFlipFlags & REPLAY_FLIP_PRONE_BIT) {
            FlipPoseFromProneState(pose[1]);
        }
        gReplayFighterPos[side].vx = frame->x;
        gReplayFighterPos[side].vy = frame->y;
        gReplayFighterPos[side].vz = frame->z;
        gReplayFighterAngles[side].vx = 0;
        gReplayFighterAngles[side].vy = frame->packedHeading;
        gReplayFighterAngles[side].vz = 0;
        BlendPoseJointRotations(side, pose[0], pose[1], frame->blendAndFlipFlags & REPLAY_BLEND_WEIGHT_MASK,
                                (MATRIX *)&COLLISION_SCRATCH->work);
        UpdateAndEnqueueFighterPose(side, FIGHTER_POSE_MODE_PRECOMPUTED, &gReplayFighterPos[side].vx,
                                    (PoseAngles *)&gReplayFighterAngles[side], 0, frame->shadowGroundY);
        UpdateFighterHitGeometry(&gFighters[side], COLLISION_SCRATCH->bodyPartPos[side],
                                 COLLISION_SCRATCH->weaponPoints[side]);
        if (frame->combatEvent != 0) {
            mark = &gReplayEffectMarks[frame->markIndex];
            effectPos.vx = frame->x + mark->x;
            effectPos.vy = frame->y + mark->y;
            effectPos.vz = frame->z + mark->z;
            if (frame->combatEvent == COMBAT_EVENT_FATAL_HIT) {
                gBloodSprayDir.vx = mark->dirX << REPLAY_BLOOD_DIR_SHIFT;
                gBloodSprayDir.vy = mark->dirY << REPLAY_BLOOD_DIR_SHIFT;
                gBloodSprayDir.vz = mark->dirZ << REPLAY_BLOOD_DIR_SHIFT;
            }
            DispatchCombatEventEffect(side, frame->combatEvent, &effectPos);
        }
        if (!samePoseFrame) {
            floorY = QueryFloorHeight(gReplayFighterPos[side].vx, gReplayFighterPos[side].vy,
                                      gReplayFighterPos[side].vz);
            if (floorY == FLOOR_QUERY_MISS) {
                floorY = gReplayFighterPos[side].vy + REPLAY_FALLBACK_FLOOR_OFFSET;
            }
            ProcessAnimFrameEvents(side, frame->placement, frame->animFrame, &gReplayFighterPos[side],
                                   (frame->packedHeading >> REPLAY_SURFACE_SHIFT) & REPLAY_SURFACE_PLAYBACK_MASK,
                                   floorY);
        }
        TraceCameraSightlineTo(gReplayFighterPos[side].vx, gReplayFighterPos[side].vz);
        SetFighterFacialExpression(side, gFighters[side].fighterId,
                                   (frame->lookAnglesAndFace[0] >> REPLAY_FACE_SHIFT) & REPLAY_FACE_MASK);
    }
    DrawAllWeaponTrails();
    UpdateReplaySubWeapons();
    EndSceneFrame();
    UpdateAmbientSounds();
    FlushQueuedSounds();
#ifndef VERSION_JP
    DrawReplayOverlayText();
#endif
    ++gReplayPlayFrame;
    if ((gCombatFrameCounter >= 0 &&
         gCombatFrameCounter < REPLAY_DURATION_FRAMES &&
         gReplayPlayFrame == gCombatFrameCounter) ||
        gReplayPlayFrame == REPLAY_DURATION_FRAMES) {
        TransitionAfterMatchOrReplay();
    }
    if ((gHumanPlayerCount == 1 && (gPadState.pressed & MENU_CANCEL)) ||
        (gPadState.pressed & (MENU_CANCEL << (gMatchWinnerSide << 4)))) {
        TransitionAfterMatchOrReplay();
    }
}
