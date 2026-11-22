/* Fighter state machine and movement: scene spawn and stage-portal relocation, motion graph
 * transitions, floor/wall/ledge/jump/fall kinematics, head-tracking look angles, controller
 * rumble triggers, and pose-flip helpers. */

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <inline_o.h>
#include <rand.h>

#include "pad.h"
#include "fighter.h"
#include "fighter_anim_speed.h"
#include "geometry.h"
#include "fighter_hit_sphere.h"
#include "game_state.h"
#include "stage_tables.h"
#include "motion_file.h"
#include "collision_scratch.h"
#include "weapon_id.h"
#include "combat_event.h"
#include "combat_hit.h"
#include "fighter_pose.h"
#include "floor_collision.h"
#include "pose_stream.h"
#include "rumble_pulse.h"
#include "stage_id.h"
#include "cloth.h"
#include "subtitle.h"
#include "fighter_cloth.h"
#include "fighter_cloth_setup.h"
#include "fighter_face.h"
#include "billboard.h"
#include "combat_state.h"
#include "decal.h"
#include "effect.h"
#include "fighter_ai.h"
#include "fighter_hit.h"
#include "fighter_skeleton.h"
#include "fighter_turn.h"
#include "main.h"
#include "point_light.h"
#include "replay.h"
#include "scene_math.h"
#include "story_state.h"
#include "subweapon.h"
#include "weapon_trail.h"
#include "globals.h"
#include "match_setup.h"

#define MOTION_SCRATCH ((FighterMotionScratch *)&COLLISION_SCRATCH->work)

/* Floor heights come in 2000-unit bands; a height between two band edges lies on a slope. */
#define FLOOR_BAND_BELOW(h) \
    (((h) < 0 ? ((h) - (FLOOR_BAND_HEIGHT - 1)) / FLOOR_BAND_HEIGHT : (h) / FLOOR_BAND_HEIGHT) * FLOOR_BAND_HEIGHT)
#define FLOOR_BAND_ABOVE(h) \
    (((h) < 0 ? (h) / FLOOR_BAND_HEIGHT : ((h) + (FLOOR_BAND_HEIGHT - 1)) / FLOOR_BAND_HEIGHT) * FLOOR_BAND_HEIGHT)

#define QUERY_FLOOR_BAND(x, y, z, outHeight, outLower, outUpper)                         \
    (outHeight) = QueryFloorHeight((x), (y), (z));                                       \
    if ((outHeight) == FLOOR_QUERY_MISS) {                                               \
        (outUpper) = FLOOR_HEIGHT_NONE;                                                  \
        (outLower) = FLOOR_HEIGHT_NONE;                                                  \
        (outHeight) = FLOOR_HEIGHT_NONE;                                                 \
    } else {                                                                             \
        (outHeight) &= -8;                                                               \
        (outLower) = FLOOR_BAND_BELOW(outHeight);                                        \
        (outUpper) = FLOOR_BAND_ABOVE(outHeight);                                        \
    }

#define ROTATE_DPAD_POV(buttons)                                                         \
    (((buttons) & 0xfff) | ((unsigned int)((buttons) & PADLleft) >> 3) |                 \
     (((buttons) & (PADLup | PADLright | PADLdown)) << 1))

static int FindPracticeRespawnPos(Fighter *fighter, WorldVector *outPos)
{
    int floorY;
    int probeX, probeZ;
    int probeAngle;
    int step;
    int angleOffset;
    short *sinLut;
    int sinVal, cosVal;
    int anchorY;

    floorY = QueryFloorHeight(fighter->anchor.vx, fighter->anchor.vy, fighter->anchor.vz);
    if ((unsigned int)(floorY + 30000) > 40000) {
        return 0;
    }
    /* A floor: its Q12 normal's Y at or below -0.75. */
    if (GetLastFloorNormal()->vy >= -3071) {
        return 0;
    }
    anchorY = fighter->anchor.vy;
    if (floorY > anchorY - 300) {
        if (floorY >= anchorY + 300) {
            return 0;
        }
        sinLut = gSinLutQ12;
        for (step = 0, angleOffset = 0; step < 16; ++step, angleOffset += 1792) {
            probeAngle = fighter->angles.heading + angleOffset;
            anchorY = fighter->anchor.vy;
            sinVal = sinLut[probeAngle & ANGLE_MASK];
            cosVal = sinLut[(probeAngle + ANGLE_QUARTER) & ANGLE_MASK];
            probeX = fighter->anchor.vx + (sinVal * 625 >> 9);
            probeZ = fighter->anchor.vz + (cosVal * 625 >> 9);
            floorY = QueryFloorHeight(probeX, anchorY, probeZ);
            if ((unsigned int)(floorY + 30000) <= 40000 &&
                GetLastFloorNormal()->vy < -3071) {
                anchorY = fighter->anchor.vy;
                if (floorY > anchorY - 300 && floorY < anchorY + 300) {
                    outPos->x = probeX;
                    outPos->y = floorY;
                    outPos->z = probeZ;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* Per-class Q12 combat stat tables: playable classes 0..5 by weapon 0..7, followed by boss classes 6..11. */
typedef struct FighterStatTable {
    short byWeapon[6][8];
    short boss[6];
} FighterStatTable;

FighterStatTable gFighterClashPower = {
    {
        { 4341, 4218, 4833, 4096, 4587, 3522, 4587, 4915 },
        { 4014, 3932, 4096, 3768, 4587, 3276, 4341, 4751 },
        { 4341, 4218, 4710, 3850, 4096, 3522, 4587, 4751 },
        { 3932, 3686, 4218, 3522, 4014, 3276, 4218, 4505 },
        { 4014, 4218, 4218, 4096, 4014, 3276, 4341, 4587 },
        { 4341, 4096, 4710, 3768, 4505, 3276, 4587, 4751 },
    },
    { 5120, 4915, 3891, 4874, 4751, 4505 },
};

FighterStatTable gFighterAnimSpeed = {
    {
        { 3891, 3686, 4096, 3686, 3481, 3481, 4096, 4096 },
        { 3686, 3891, 3481, 4096, 4096, 3686, 4096, 3891 },
        { 4096, 4096, 4096, 3481, 3891, 4096, 3891, 3686 },
        { 3481, 3481, 3686, 4096, 3481, 3686, 3686, 3686 },
        { 3686, 4096, 3481, 4096, 3686, 3891, 3686, 3481 },
        { 4096, 3686, 3891, 3481, 3686, 3481, 4096, 3686 },
    },
    { 4096, 4096, 4096, 4096, 4096, 4096 },
};
/* Q12 animation speed multipliers (0x1000 = 1.0): rows are indexed by WoundSpeedCategory
 * (columns hold wound counts 1..4; the two-handed left-arm row uses only column 0). */
short gWoundSpeedScales[4][4] = {
    { 3891, 3686, 3481, 3276 },
    { 3686, 3276, 2867, 2457 },
    { 3891, 3686, 3276, 2867 },
    { 3481,    0,    0,    0 },
};
FighterHitSphere gBaseHitSpheres[22] = {
    { 1, 2, { 56, 0, -57 }, 0, 228, 51984, 228, 51984 },
    { 1, 1, { 94, 0, 0 }, 0, 210, 44100, 210, 44100 },
    { 1, 1, { 279, 0, -120 }, 0, 210, 44100, 210, 44100 },
    { 1, 1, { 279, 0, 120 }, 0, 205, 42025, 205, 42025 },
    { 1, 11, { 133, 0, -82 }, 0, 174, 30276, 164, 26896 },
    { 1, 11, { 133, 0, 82 }, 0, 174, 30276, 164, 26896 },
    { 0, 8, { 41, 0, 0 }, 0, 90, 8100, 0, 0 },
    { 0, 8, { 180, 0, 0 }, 0, 90, 8100, 0, 0 },
    { 0, 9, { 45, 0, 0 }, 0, 65, 4225, 0, 0 },
    { 0, 9, { 144, 0, 0 }, 0, 65, 4225, 0, 0 },
    { 0, 4, { 41, 0, 0 }, 0, 90, 8100, 0, 0 },
    { 0, 4, { 180, 0, 0 }, 0, 90, 8100, 0, 0 },
    { 0, 5, { 45, 0, 0 }, 0, 65, 4225, 0, 0 },
    { 0, 5, { 144, 0, 0 }, 0, 65, 4225, 0, 0 },
    { 0, 12, { 120, 0, 0 }, 0, 130, 16900, 0, 0 },
    { 0, 12, { 291, 0, 0 }, 0, 130, 16900, 0, 0 },
    { 0, 13, { 103, 0, 0 }, 0, 130, 16900, 0, 0 },
    { 0, 13, { 287, 0, 0 }, 0, 130, 16900, 0, 0 },
    { 0, 15, { 120, 0, 0 }, 0, 130, 16900, 0, 0 },
    { 0, 15, { 291, 0, 0 }, 0, 130, 16900, 0, 0 },
    { 0, 16, { 103, 0, 0 }, 0, 130, 16900, 0, 0 },
    { 0, 16, { 287, 0, 0 }, 0, 130, 16900, 0, 0 },
};

SVECTOR gKatanaEdgeOffsets[2] = {
    { -24, 105, 72, 0 }, { -24, 14, 996, 0 },
};
SVECTOR gLongSwordEdgeOffsets[2] = {
    { -17, 72, 89, 0 }, { -17, 72, 800, 0 },
};
SVECTOR gFinalBossAltWeaponEdgeOffsets[2] = {
    { -27, 85, 72, 0 }, { -24, 14, 996, 0 },
};
SVECTOR gBroadswordEdgeOffsets[2] = {
    { -13, 85, 325, 0 }, { -13, 85, 1051, 0 },
};
SVECTOR gSaberEdgeOffsets[2] = {
    { -17, 98, 100, 0 }, { -17, 48, 755, 0 },
};
/* Three points where gWeaponEdgePointCounts gives the naginata four: the fourth is read from gRapierEdgeOffsets[0]. */
SVECTOR gNaginataEdgeOffsets[3] = {
    { -20, 65, 737, 0 }, { -21, -1, 1319, 0 }, { -21, 66, -1201, 0 },
};
SVECTOR gRapierEdgeOffsets[2] = {
    { -18, 74, 123, 0 }, { -18, 74, 848, 0 },
};
SVECTOR gNodachiEdgeOffsets[3] = {
    { -24, 88, 72, 0 }, { -24, -32, 1253, 0 }, { -24, 52, 945, 0 },
};
SVECTOR gSledgehammerEdgeOffsets[4] = {
    { -15, -62, 652, 0 }, { -15, 174, 652, 0 }, { -15, 55, -621, 0 }, { -15, 55, 662, 0 },
};
SVECTOR gGunEdgeOffsets[2] = {
    { -34, 59, 105, 0 }, { -34, 508, 100, 0 },
};
SVECTOR gWeapon9EdgeOffsets[4] = {
    { -34, 63, 591, 0 }, { -34, -54, 1247, 0 }, { -34, 194, -1247, 0 }, { -34, 71, -674, 0 },
};

unsigned char gWeaponEdgePointCounts[12] = { 2, 2, 2, 2, 4, 2, 3, 4, 2, 4, 0, 0 };

/* Active trail strips per weaponId (0..9): 2 for two-segment weapons 6/9, 0 for Katze's gun (8). */
unsigned char gWeaponTrailCount[12] = { 1, 1, 1, 1, 1, 1, 2, 1, 0, 2, 0, 0 };

/* By weaponId and trail. */
WeaponTrailSpan gWeaponTrailEndpoints[10][2] = {
    { { 0, 1 }, { 0, 0 } },
    { { 0, 1 }, { 0, 0 } },
    { { 0, 1 }, { 0, 0 } },
    { { 0, 1 }, { 0, 0 } },
    { { 0, 1 }, { 0, 0 } },
    { { 0, 1 }, { 0, 0 } },
    { { 0, 2 }, { 2, 1 } },
    { { 0, 1 }, { 0, 0 } },
    { { 0, 1 }, { 0, 0 } },
    { { 0, 1 }, { 3, 2 } },
};

SVECTOR *gWeaponEdgeOffsets[11] = {
    gKatanaEdgeOffsets, gLongSwordEdgeOffsets, gBroadswordEdgeOffsets, gSaberEdgeOffsets,
    gNaginataEdgeOffsets, gRapierEdgeOffsets, gNodachiEdgeOffsets, gSledgehammerEdgeOffsets,
    gGunEdgeOffsets, gWeapon9EdgeOffsets, gFinalBossAltWeaponEdgeOffsets,
};

unsigned char gWeaponGripTypes[12] = {
    FIGHTER_GRIP_TWO_HANDED, FIGHTER_GRIP_ONE_HANDED, FIGHTER_GRIP_TWO_HANDED,
    FIGHTER_GRIP_ONE_HANDED, FIGHTER_GRIP_TWO_HANDED, FIGHTER_GRIP_ONE_HANDED,
    FIGHTER_GRIP_TWO_HANDED, FIGHTER_GRIP_TWO_HANDED, FIGHTER_GRIP_ONE_HANDED,
    FIGHTER_GRIP_TWO_HANDED, 0, 0,
};
unsigned short gWeaponTractionWeights[10] = { 15, 14, 41, 12, 22, 9, 19, 45, 7, 22 };
unsigned char gCharClimbActionOffset[FIGHTER_CLASS_COUNT] = { 2, 2, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0 };

unsigned char gFighterIdToClass[24] = {
    0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 1, 4, 5, 0, 1, 0,
};
short gCharJumpHorizontalSpeed[FIGHTER_CLASS_COUNT] = {
    3840, 4080, 4080, 3968, 3840, 4080, 4096, 4096, 4096, 4096, 4096, 4096,
};
short gCharJumpVerticalSpeed[FIGHTER_CLASS_COUNT] = {
    -510, -520, -520, -520, -530, -520, -540, -540, -540, -540, -540, -540,
};
unsigned short gCharTractionWeights[FIGHTER_CLASS_COUNT] = {
    900, 590, 810, 650, 690, 850, 900, 860, 730, 840, 900, 610,
};
static short gPracticeRespawnFound;
static int gVsSpawnPairIndex;
short gFighterClothInitialized;
static WorldVector gPracticeRespawnPos;

void InitFighterForScene(int stageId, int prevStageId, int side, int isCpu,
                         int fighterId, int weaponId, int resetMatchState)
{
    Fighter *opponent;
    Fighter *fighter;
    int adjustZ, playMode, slashBout;
    Fighter *peer;
    int spawnStep, spawnZ;
    int p1X, p1Y, p1Z;
    WorldVector *respawnPos;
    int respawnFound;
    WorldVector *spawnPair, *lookTarget;
    Fighter *targetFighter;
    int gameState, variantMode, storyMode, currentStage;
    int belowThreshold, spawnOffset;
    int anchorX, offsetX, anchorY, offsetZ, offsetY, pointIndex, anchorZ;
    int index;
    int heading;
    int charClass;
    int storedFighterId;
    int speedScale, clashScale;
    int jumpSpeedY, statureScale, cpuFlag;
    int hardDifficulty, aiSkill;
    StageTransition *transition;
    int deltaX, oldY, deltaY, oldZ, deltaZ, movedX, movedZ;
    int destX, spanX, adjustX, destZ, spanZ, baseDeltaX, baseDeltaZ;
    int adjustedDeltaX, anchorYBefore, anchorDeltaY, anchorZBefore, anchorDeltaZ;
    WorldVector lookAt, delta;

    fighter = &gFighters[side];
    opponent = &gFighters[0];
    if (side == 0) {
        opponent = &gFighters[1];
    }
    fighter->opponent = opponent;
    fighter->side = side;
    fighter->isCpu = isCpu;
    fighter->rumbleTimer = 0;
    if (side == 0) {
        gFighterClothInitialized = 0;
        gPostKoTimer = 0;
        gWeaponClashOccurred = 0;
        gBladeLockState = BLADE_LOCK_NONE;
        gMatchWinnerSide = -1;
        ResetReplayBuffer();
        ResetSubWeaponHistory();
        ClearBillboardEffects();
        ClearGroundDecals();
        StopPointLightFlash();
        ClearEffectPool();
    } else if (resetMatchState != 0) {
        gPostKoTimer = 0;
        gWeaponClashOccurred = 0;
        gBladeLockState = BLADE_LOCK_NONE;
        gMatchWinnerSide = -1;
        ResetReplayBuffer();
        ResetSubWeaponHistory();
    }
    if (stageId == prevStageId) {
        playMode = gPlayMode;
        fighter->activeFrames = 0;
        if (playMode == PLAY_MODE_SLASH) {
            if (side == 0) {
                fighter->anchor.vx = 0;
                fighter->anchor.vy = 0;
                fighter->anchor.vz = 0;
                lookAt.x = 0;
                lookAt.y = 0;
                lookAt.z = 5000;
            } else {
                int zDir;

                slashBout = gSlashBoutIndex;
                zDir = 1;
                if (slashBout != 0) {
                    zDir = ((rand() & 1) << 1) - 1;
                }
                peer = fighter->opponent;
                spawnStep = zDir * 625;
                spawnZ = peer->anchor.vz + (spawnStep << 3);
                if ((unsigned int)(spawnZ + 0xa800) > 0x15000) {
                    spawnZ -= spawnStep << 4;
                }
                fighter->anchor.vx = 0;
                fighter->anchor.vy = 0;
                fighter->anchor.vz = spawnZ;
                p1X = gFighters[0].anchor.vx;
                p1Y = gFighters[0].anchor.vy;
                p1Z = gFighters[0].anchor.vz;
                lookAt.x = p1X;
                lookAt.y = p1Y;
                lookAt.z = p1Z;
            }
        } else if (playMode == PLAY_MODE_PRACTICE && gPracticeFirstBoutFlag == 0) {
            if (side == 0) {
                respawnPos = &gPracticeRespawnPos;
                gPracticeRespawnFound = FindPracticeRespawnPos(fighter, respawnPos);
                lookAt = *respawnPos;
            }
            respawnFound = gPracticeRespawnFound;
            if (respawnFound == 0) {
                WorldVector (*positions)[8];

                positions = (WorldVector (*)[8])GetStageSpawnTable();
                spawnPair = positions[stageId];
                *(WorldVector *)&fighter->anchor = positions[stageId][side];
                lookAt = *(side ? spawnPair : spawnPair + 1);
            } else if (side == 1) {
                *(WorldVector *)&fighter->anchor = gPracticeRespawnPos;
                targetFighter = fighter->opponent;
                lookAt = *(WorldVector *)&targetFighter->anchor;
            }
        } else {
            gameState = gGameState;
            if (gameState == GAME_STATE_INIT_APPROACH_CUTSCENE) {
                GetApproachCutscenePositions(side, (WorldVector *)&fighter->anchor, &lookAt);
            } else if (gameState == GAME_STATE_INIT_SLASH_CLEAR_CUTSCENE) {
                GetSlashClearCutscenePositions(side, (WorldVector *)&fighter->anchor, &lookAt);
            } else {
                WorldVector *positions;

                variantMode = gPlayMode;
                if (variantMode == PLAY_MODE_VS) {
                    if (side == 0) {
                        index = rand() & 3;
                        gVsSpawnPairIndex = index;
                    } else {
                        index = gVsSpawnPairIndex;
                    }
                } else {
                    index = 0;
                }
                storyMode = gPlayMode;
                if (storyMode == PLAY_MODE_STORY) {
                    currentStage = gCurrentStageId;
                    if (currentStage == 10) {
                        belowThreshold = gStoryBoutStartPos.y < -4000;
                        index = belowThreshold << 1;
                    } else if (currentStage == 4) {
                        belowThreshold = gStoryBoutStartPos.y < 4001;
                        index = belowThreshold << 1;
                    }
                }
                positions = GetStageSpawnTable();
                spawnOffset = stageId * 8 + index * 2;
                spawnPair = &positions[spawnOffset];
                *(WorldVector *)&fighter->anchor = spawnPair[side];
                lookTarget = side ? spawnPair : spawnPair + 1;
                lookAt = *lookTarget;
            }
        }
        anchorX = fighter->anchor.vx;
        fighter->rootOffset.x = 0;
        fighter->rootOffset.y = -900;
        anchorZ = fighter->anchor.vz;
        fighter->rootOffset.z = 0;
        pointIndex = 0;
        fighter->frameDelta.vx = 0;
        fighter->frameDelta.vy = 0;
        fighter->frameDelta.vz = 0;
        offsetX = fighter->rootOffset.x;
        offsetY = fighter->rootOffset.y;
        offsetZ = fighter->rootOffset.z;
        fighter->position.vx = anchorX + offsetX;
        anchorY = fighter->anchor.vy;
        fighter->position.vy = anchorY + offsetY;
        fighter->position.vz = anchorZ + offsetZ;
        fighter->angles.pitch = 0;
        heading = ratan2(lookAt.x - fighter->position.vx,
                         lookAt.z - fighter->position.vz);
        fighter->angleToOpponent = heading;
        fighter->angles.heading = heading;
        fighter->angles.roll = 0;
        fighter->savedAngles = fighter->angles;
        fighter->weaponTipVel.vx = 0;
        fighter->weaponTipVel.vy = 0;
        fighter->weaponTipVel.vz = 0;
        fighter->velocity.x = 0;
        fighter->velocity.y = 0;
        fighter->velocity.z = 0;
        fighter->prevRootOffset = fighter->rootOffset;
        fighter->rootOffsetDelta.x = 0;
        fighter->rootOffsetDelta.y = 0;
        fighter->rootOffsetDelta.z = 0;
        fighter->lookRoll = 0;
        fighter->lookYaw = 0;
        fighter->lookPitch = 0;
        fighter->turnRemaining = 0;
        fighter->lastTurnStep = 0;
        fighter->velocityRetention = 0;
        do {
            fighter->prevWeaponPoints[pointIndex] = *(WorldVector *)&fighter->position;
            ++pointIndex;
        } while (pointIndex < 4);
        fighter->pendingAction = FORCED_ACTION_NONE;
        fighter->animSpeed = ONE;
        fighter->facingState = MOTION_FACING_AUTO;
        fighter->nearWallContact = 0;
        fighter->action = FIGHTER_ACTION_NEUTRAL;
        fighter->bufferedButtons = 0;
        fighter->isStationary = 1;
        if (side == 0) {
            ClearSubWeaponSlots();
        }
        fighter->fighterId = fighterId;
        charClass = gFighterIdToClass[fighterId];
        fighter->characterClass = charClass;
        fighter->weaponId = weaponId;
        index = fighter->characterClass;
        storedFighterId = fighter->fighterId;
        fighter->weaponGripType = gWeaponGripTypes[weaponId];
        fighter->statureScale = gCharHeightScales[storedFighterId];
        if (index >= PLAYABLE_CHARACTER_COUNT) {
            fighter->baseAnimSpeed = gFighterAnimSpeed.boss[index - PLAYABLE_CHARACTER_COUNT];
        } else {
            fighter->baseAnimSpeed = gFighterAnimSpeed.byWeapon[index][weaponId];
        }
        if (index >= PLAYABLE_CHARACTER_COUNT) {
            fighter->baseClashPower = gFighterClashPower.boss[index - PLAYABLE_CHARACTER_COUNT];
        } else {
            fighter->baseClashPower = gFighterClashPower.byWeapon[index][weaponId];
        }
        if (gPlayMode == PLAY_MODE_SLASH && fighter->isCpu != 0) {
            fighter->baseAnimSpeed = (short)CalcSlashAiAnimSpeed();
            fighter->clashPower = (short)CalcSlashAiClashPower();
        }
        if (gPlayMode == PLAY_MODE_VS && gPovModeEnabled == 1 &&
            fighter->isCpu != 0) {
            speedScale = CalcPovAiAnimSpeedScale();
            fighter->baseAnimSpeed = (short)(fighter->baseAnimSpeed * speedScale / 4096);
            clashScale = CalcPovAiClashPowerScale();
            fighter->clashPower = (short)(fighter->clashPower * clashScale / 4096);
        }
        fighter->tractionWeight = gCharTractionWeights[fighter->characterClass]
                                + gWeaponTractionWeights[fighter->weaponId];
        fighter->jumpLaunchParams[1] = gCharJumpHorizontalSpeed[fighter->characterClass];
        jumpSpeedY = gCharJumpVerticalSpeed[fighter->characterClass];
        fighter->unreadLimbFlag = 0;
        fighter->leftArmIntact = 1;
        fighter->legsIntact = 1;
        fighter->torsoWoundCount = 0;
        fighter->thighWoundCount = 0;
        fighter->rightArmWoundCount = 0;
        fighter->jumpLaunchParams[0] = jumpSpeedY;
        InitFighterSubWeapons(fighter);
        statureScale = fighter->statureScale;
        ScaleFighterHitSpheres(side, statureScale);
        cpuFlag = fighter->isCpu;
        fighter->fallState = 0;
        fighter->pendingFallAction = FORCED_ACTION_NONE;
        fighter->climbState = 0;
        fighter->flipHeadingNextTick = 0;
        fighter->jumpState = 0;
        fighter->rootFreezeState = 0;
        fighter->animFrameStalled = 0;
        fighter->onSlopeBand = 0;
        fighter->poseBlendWeight = 0;
        fighter->runSpeed = 0;
        fighter->prevFacingState = 0;
        fighter->faceExpression = FIGHTER_FACE_NORMAL;
        if (cpuFlag != 0) {
            hardDifficulty = gOptionDifficulty;
            aiSkill = hardDifficulty != 0 ? 0x1000 : 0x400;
            InitFighterAi(side, aiSkill);
        }
        InitFighterWeaponTrails(side);
        fighter->gunAmmo = 10;
        fighter->gunMagazines = 9;
    } else {
        adjustZ = 0;
        transition = gPendingTransition;
        index = transition->stageA != stageId;
        deltaX = transition->anchors[index].x - transition->anchors[!index].x;
        delta.x = deltaX;
        delta.y = transition->anchors[index].y - transition->anchors[!index].y;
        delta.z = transition->anchors[index].z - transition->anchors[!index].z;
        fighter->position.vx += deltaX;
        oldY = fighter->position.vy;
        deltaY = delta.y;
        oldZ = fighter->position.vz;
        fighter->position.vy = oldY + deltaY;
        deltaZ = delta.z;
        movedX = fighter->position.vx;
        movedZ = oldZ + deltaZ;
        fighter->position.vz = movedZ;
        destX = transition->anchors[index].x;
        spanX = transition->width;
        adjustX = 0;
        if (movedX < destX + 100) {
            adjustX = 100;
        }
        if (destX + spanX - 100 < movedX) {
            adjustX -= 100;
        }
        destZ = transition->anchors[index].z;
        if (movedZ < destZ + 100) {
            adjustZ = 100;
        }
        spanZ = transition->depth;
        if (destZ + spanZ - 100 < movedZ) {
            adjustZ -= 100;
        }
        fighter->position.vx = movedX + adjustX;
        fighter->position.vz += adjustZ;
        baseDeltaX = delta.x;
        baseDeltaZ = delta.z;
        adjustedDeltaX = baseDeltaX + adjustX;
        delta.x += adjustX;
        delta.z = baseDeltaZ + adjustZ;
        fighter->anchor.vx += adjustedDeltaX;
        anchorYBefore = fighter->anchor.vy;
        anchorDeltaY = delta.y;
        fighter->anchor.vy = anchorYBefore + anchorDeltaY;
        anchorZBefore = fighter->anchor.vz;
        anchorDeltaZ = delta.z;
        fighter->anchor.vz = anchorZBefore + anchorDeltaZ;
        for (pointIndex = 0; pointIndex < 4; ++pointIndex) {
            fighter->prevWeaponPoints[pointIndex].x += delta.x;
            fighter->prevWeaponPoints[pointIndex].y += delta.y;
            fighter->prevWeaponPoints[pointIndex].z += delta.z;
        }
        ResetFighterWeaponTrails(side);
    }
    fighter->cameraTargetPos = *(WorldVector *)&fighter->position;
    fighter->torsoCenter = *(WorldVector *)&fighter->position;
    fighter->headPos.x = fighter->position.vx;
    fighter->headPos.y = fighter->position.vy - 500;
    fighter->headPos.z = fighter->position.vz;
    fighter->groundY = fighter->anchor.vy;
    fighter->shadowGroundY = fighter->anchor.vy;
    fighter->pitFallTimer = 0;
}

void SetFighterMotionPlacement(Fighter *fighter, MotionPlacement *placement)
{
    unsigned short facing, heading;
    MotionPlacement *prevPlacement, *next;
    int clipId;
    unsigned short startFrame;
    AnimInfo *prevInfo;
    int infoOffset;
    unsigned char type;

    if (fighter->facingState >= MOTION_FACING_INVERTED) {
        facing = fighter->savedAngles.heading;
        heading = fighter->angles.heading;
        fighter->savedAngles.heading = facing + ANGLE_HALF;
        fighter->angles.heading = heading + ANGLE_HALF;
    }
    prevPlacement = fighter->placement;
    fighter->placement = placement;
    fighter->prevPlacement = prevPlacement;
    if (placement->frameLimit == 0) {
        do {
            next = fighter->placement->nextPlacement;
            fighter->placement = next;
        } while (next->frameLimit == 0);
    }
    clipId = fighter->placement->animClipId;
    fighter->animClipId = clipId;
    fighter->animBaseFrame = MOTION_FILE->animBaseFrames[(short)clipId - 1];
    startFrame = fighter->placement->startFrame;
    prevInfo = fighter->animInfo;
    infoOffset = fighter->animClipId * sizeof(AnimInfo);
    fighter->animFrac = 0;
    fighter->prevAnimInfo = prevInfo;
    fighter->animFrame = startFrame;
    fighter->animInfo = (AnimInfo *)((unsigned char *)MOTION_FILE->animInfo + (infoOffset - sizeof(AnimInfo)));
    fighter->prevAction = fighter->action;
    type = fighter->animInfo->type;
    fighter->queuedPlacement = 0;
    fighter->motionJustChanged = 1;
    fighter->action = type;
    if (fighter->prevAction != FIGHTER_ACTION_ATTACK || !(fighter->prevAnimInfo->flags & ANIM_FLAG_HOMING_JUMP)) {
        fighter->hitActive = 1;
    }
    fighter->animFrameStalled = 0;
    fighter->facingState = fighter->placement->facingMode;
    if (fighter->placement->facingMode == MOTION_FACING_LOCK) {
        fighter->turnRemaining = 0;
    }
}

void StartWinnerVictoryMotion(void)
{
    Fighter *fighter = &gFighters[gMatchWinnerSide];
    MotionPlacement **placements = MOTION_FILE->victoryPlacements;
    unsigned short facing;

    if (fighter->legsIntact == 0)
        placements = MOTION_FILE->legsLostVictoryPlacements;
    SetFighterMotionPlacement(fighter, placements[gFighters[gMatchWinnerSide].weaponPairSlot]);
    facing = fighter->savedAngles.heading;
    fighter->isHomingJump = 0;
    fighter->lockedMoveHeading = facing;
}

void StartFighterFallFromLedge(Fighter *fighter)
{
    int angle;
    int diff;
    int isBackHit;

    isBackHit = 0;
    fighter->fallGravityTicks = 0;
    if (fighter->climbState != 0) {
        angle = fighter->savedAngles.heading + ANGLE_HALF;
        fighter->knockbackDir.vx = -gSinLutQ12[angle & ANGLE_MASK];
        fighter->knockbackDir.vz = -gSinLutQ12[(angle + ANGLE_QUARTER) & ANGLE_MASK];
    } else {
        angle = ratan2(MOTION_SCRATCH->normal->vx,
                       MOTION_SCRATCH->normal->vz);
        fighter->knockbackDir = *MOTION_SCRATCH->normal;
    }

    diff = (angle - fighter->savedAngles.heading) & ANGLE_MASK;
    if (diff >= ANGLE_HALF) {
        diff -= ANGLE_FULL;
    }
    if (diff > -ANGLE_QUARTER && diff < ANGLE_QUARTER) {
        if (fighter->placement->facingMode == MOTION_FACING_INVERTED) {
            angle += ANGLE_HALF;
        }
    } else {
        if (fighter->placement->facingMode != MOTION_FACING_INVERTED) {
            isBackHit = 1;
        }
        angle += ANGLE_HALF;
    }

    if (fighter->action == FIGHTER_ACTION_DOWNED ||
        (fighter->action == FIGHTER_ACTION_ATTACK && (fighter->animInfo->flags & ANIM_FLAG_DOWNED))) {
        fighter->fallState = 2;
    } else if (fighter->climbState != 0 && fighter->action != FIGHTER_ACTION_CLIMB) {
        fighter->fallState = 1;
    } else {
        fighter->fallState = isBackHit + 1;
    }

    if (gBladeLockState != BLADE_LOCK_NONE) {
        fighter->pendingAction = isBackHit + FORCED_ACTION_STAGGER_FRONT;
        fighter->rootFreezeState = 1;
        fighter->frozenRootOffset.x = 0;
        fighter->frozenRootOffset.y = fighter->prevRootOffset.y;
        fighter->frozenRootOffset.z = 0;
        gBladeLockState = BLADE_LOCK_BREAK;
    } else {
        fighter->pendingFallAction = isBackHit + FORCED_ACTION_FALL_FRONT;
    }

    fighter->savedAngles.heading = angle;
    AdjustFighterTurnTarget(fighter, angle);
}

short gDpadAngleTable[3][3] = {
    { -512, 0, 512 },
    { -1024, 0, 1024 },
    { -1536, 2048, 1536 },
};

static void UpdateFighterRunSteering(Fighter *fighter)
{
    int padType;
    int analog;
    unsigned char rawX;
    unsigned char rawY;
    int axisX;
    int delta;
    unsigned int buttons;
    int signedSpeed;
    int maxSpeed;

    int axisY;

    if ((fighter->animInfo->flags & ANIM_FLAG_RUN) != 0) {
        if ((fighter->currentInput.held & PAD_DPAD_MASK) != 0) {
            padType = fighter->currentInput.type[fighter->side];
            analog = 0;
            if (padType == PAD_TYPE_ANALOG_JOYSTICK || padType == PAD_TYPE_ANALOG_CONTROLLER) {
                analog = 1;
            }
            if (analog) {
                rawX = fighter->currentInput.analog[fighter->side][PAD_ANALOG_LEFT_X];
                rawY = fighter->currentInput.analog[fighter->side][PAD_ANALOG_LEFT_Y];
                axisX = rawX - 128;
                axisY = rawY - 128;
                if ((unsigned int)(rawX - 32) <= 192 &&
                    (unsigned int)(rawY - 32) <= 192) {
                    analog = 0;
                }
            }
            if (analog) {
                delta = (ANGLE_QUARTER - ratan2(axisX, axisY)) & ANGLE_MASK;
                if (delta >= ANGLE_HALF) {
                    delta -= ANGLE_FULL;
                }
                if (fighter->isPovMode && fighter->isCpu == 0) {
                    delta = (delta + ANGLE_QUARTER) & ANGLE_MASK;
                    if (delta >= ANGLE_HALF) {
                        delta -= ANGLE_FULL;
                    }
                    SetFighterTurnTarget(fighter, fighter->angles.heading + delta / 4);
                } else {
                    SetFighterTurnTarget(fighter, fighter->angleToOpponent + delta);
                }
            } else {
                buttons = fighter->currentInput.held;
                axisX = (buttons >> 14) & 1;
                if (!(buttons & PADLup)) {
                    ++axisX;
                }
                axisY = (buttons >> 15) & 1;
                if (!(buttons & PADLright)) {
                    ++axisY;
                }
                delta = gDpadAngleTable[axisY][axisX];
                if (fighter->isPovMode && fighter->isCpu == 0) {
                    if (delta == ANGLE_HALF) {
                        delta = 0;
                    }
                    SetFighterTurnTarget(fighter, fighter->angles.heading + delta / 4);
                } else {
                    SetFighterTurnTarget(fighter, fighter->angleToOpponent + delta);
                }
            }
        } else if (fighter->isPovMode != 0 && fighter->isCpu == 0) {
            fighter->turnRemaining = 0;
        }
        delta = fighter->lastTurnStep;
        if (delta < 0) {
            delta = 0 - delta;
        }
        if (delta > ANGLE_QUARTER) {
            delta = ANGLE_QUARTER;
        }
        fighter->runSpeed -= delta;
        fighter->runSpeed += fighter->statureScale / 16;
        maxSpeed = fighter->statureScale * 25 >> 5;
        signedSpeed = fighter->runSpeed;
        if (maxSpeed < signedSpeed) {
            fighter->runSpeed = maxSpeed;
        } else if (signedSpeed < 0) {
            fighter->runSpeed = 0;
        }
        delta = fighter->runSpeed * fighter->animSpeed >> 12;
        fighter->anchor.vx += gSinLutQ12[fighter->angles.heading & ANGLE_MASK] * delta >> 16;
        fighter->anchor.vz += gSinLutQ12[(fighter->angles.heading + ANGLE_QUARTER) & ANGLE_MASK] * delta >> 16;
    } else if (fighter->runSpeed > 0) {
        if (fighter->climbState == 0) {
            fighter->turnRemaining = 0;
        }
        fighter->runSpeed = 0;
    }
}

void TryStartWallBumpStumble(Fighter *fighter, int x, int z)
{
    int floorHeight;
    int lowerFloorBand, upperFloorBand;

    if (gPlayMode == PLAY_MODE_LINK)
        return;
    if (gOptionWallBump == 0)
        return;
    if (!(fighter->animInfo->flags & ANIM_FLAG_RUN))
        return;
    if (fighter->runSpeed <= 3000)
        return;
    if (fighter->frameDelta.vx * fighter->frameDelta.vx + fighter->frameDelta.vz * fighter->frameDelta.vz <= 25600)
        return;

    QUERY_FLOOR_BAND(x, fighter->position.vy - 1000, z, floorHeight, lowerFloorBand, upperFloorBand);

    if (!(GetLastFloorAttr() & FLOOR_ATTR_NO_IMPACT)) {
        fighter->pendingAction = FORCED_ACTION_HEAD_ON_STUMBLE;
        TriggerFighterRumble(fighter, RUMBLE_BODY_BUMP);
    }
}

void RecordWallContactPoint(Fighter *fighter, FighterMotionScratch *contact)
{
    fighter->nearWallContact = 1;
    fighter->wallContactPos.x = fighter->position.vx;
    fighter->wallContactPos.z = fighter->position.vz;
    fighter->wallContactNormal.vx = contact->normal->vx;
    fighter->wallContactNormal.vz = contact->normal->vz;
}

int IsNearRecordedWallContact(Fighter *fighter)
{
    int dx = fighter->wallContactPos.x - fighter->position.vx;
    int dz = fighter->wallContactPos.z - fighter->position.vz;

    return dx * dx + dz * dz <= 10000;
}

void ClampFighterMaxSeparation(Fighter *fighter, FighterMotionScratch *proposal)
{
    Fighter *opponent;
    int opponentX, opponentZ;
    int dx, dz, distance, x;

    if (fighter->isPovMode != 0 || gGameState == GAME_STATE_UPDATE_APPROACH_CUTSCENE)
        return;
    if (!((CLAMP_SEPARATION_ACTIONS >> fighter->action) & 1))
        return;

    opponent = fighter->opponent;
    opponentX = opponent->position.vx;
    dx = proposal->dest.x - opponentX;
    opponentZ = opponent->position.vz;
    dz = proposal->dest.z - opponentZ;
    distance = dx * dx + dz * dz;
    if (gMaxFighterDistSq < distance) {
        x = fighter->position.vx;
        dx = x - opponentX;
        dz = fighter->position.vz - opponentZ;
        if (dx * dx + dz * dz < distance) {
            proposal->dest.x = x;
            proposal->dest.z = fighter->position.vz;
            fighter->anchor.vx = proposal->dest.x - fighter->rootOffset.x;
            fighter->anchor.vz = proposal->dest.z - fighter->rootOffset.z;
        }
    }
}

static int gWallSlideAxisFallback;

/* returns no value when mode is not 0, 1 or 2 */
int SlideFighterAlongObstacle(Fighter *fighter, FighterMotionScratch *contact, int floorHeight, int mode)
{
    int dx;
    int dz;
    int surfaceAngle;
    int normalAngle;
    int moveAngle;
    int angleDiff;
    int sinVal;
    int cosVal;

    if (mode == 0) {
        dx = contact->dest.x - fighter->position.vx;
        dz = contact->dest.z - fighter->position.vz;
        fighter->anchor.vx -= dx;
        contact->dest.x -= dx;
        fighter->anchor.vz -= dz;
        contact->dest.z -= dz;
        contact->slideVx = dx * 3 / 4;
        contact->slideVz = dz * 3 / 4;
        if ((gWallSlideAxisFallback = floorHeight == FLOOR_HEIGHT_NONE) != 0) {
            fighter->anchor.vx += contact->slideVx;
            contact->dest.x += contact->slideVx;
        } else {
            surfaceAngle = ratan2(contact->normal->vx, contact->normal->vz);
            normalAngle = surfaceAngle + ANGLE_QUARTER;
            moveAngle = ratan2(contact->slideVx, contact->slideVz) + ANGLE_QUARTER;
            angleDiff = (normalAngle - moveAngle) & ANGLE_MASK;
            if (angleDiff >= ANGLE_HALF)
                angleDiff = ANGLE_FULL - angleDiff;
            if (angleDiff < ANGLE_QUARTER) {
                fighter->anchor.vx += contact->slideVx;
                contact->dest.x += contact->slideVx;
                gWallSlideAxisFallback = 1;
            } else {
                sinVal = gSinLutQ12[(surfaceAngle + ANGLE_HALF) & ANGLE_MASK];
                cosVal = gSinLutQ12[normalAngle & ANGLE_MASK];
                dx = (sinVal * contact->slideVx - cosVal * contact->slideVz) >> 12;
                dz = (cosVal * contact->slideVx + sinVal * contact->slideVz) >> 12;
                contact->slideVx = (-sinVal * dx / 32 + cosVal * dz) >> 12;
                contact->slideVz = (cosVal * dx / 32 + sinVal * dz) >> 12;
                fighter->anchor.vx += contact->slideVx;
                contact->dest.x += contact->slideVx;
                fighter->anchor.vz += contact->slideVz;
                contact->dest.z += contact->slideVz;
            }
        }
        return 1;
    }
    if (mode == 1) {
        if (gWallSlideAxisFallback != 0) {
            fighter->anchor.vx -= contact->slideVx;
            contact->dest.x -= contact->slideVx;
            fighter->anchor.vz += contact->slideVz;
            contact->dest.z += contact->slideVz;
            return 2;
        }
        fighter->anchor.vx -= contact->slideVx;
        contact->dest.x -= contact->slideVx;
        fighter->anchor.vz -= contact->slideVz;
        contact->dest.z -= contact->slideVz;
        return 3;
    }
    if (mode == 2) {
        fighter->anchor.vz -= contact->slideVz;
        contact->dest.z -= contact->slideVz;
        return 3;
    }
}

static void ResolveFighterFloorMovement(Fighter *fighter, FighterMotionScratch *contact,
                                        int height, int lower, int upper)
{
    int lzBefore, lzAfter;
    int oldX, oldZ, distance, wantedX, wantedZ;
    int moved;
    int dx, dz;
    unsigned int squared;
    int slideStage, heightDiff;

    wantedX = contact->dest.x;
    oldX = fighter->position.vx;
    dx = wantedX - oldX;
    wantedZ = contact->dest.z;
    oldZ = fighter->position.vz;
    dz = wantedZ - oldZ;
    squared = dx * dx + dz * dz;
    if ((unsigned int)squared < 1024) {
        distance = (unsigned int)gSqrtLutQ3[squared] >> 3;
    } else {
        unsigned int shift;
        gte_ldlzc(squared);
        gte_nop();
        gte_nop();
        gte_stlzc(&lzBefore);
        shift = lzBefore;
        shift = 22 - (shift & ~1);
        squared = gSqrtLutQ3[(unsigned int)squared >> shift];
        distance = ((unsigned int)squared << 16) >> (19 - (shift >> 1));
    }

    slideStage = 0;
    for (;;) {
        if (fighter->onSlopeBand == 0 && (height == FLOOR_HEIGHT_NONE ||
                (contact->normal->vy > FLOOR_WALKABLE_NORMAL_Y && contact->normal->vy < -FLOOR_WALKABLE_NORMAL_Y))) {
            if (lower < fighter->groundY) {
                if (height != FLOOR_HEIGHT_NONE) {
                    RecordWallContactPoint(fighter, contact);
                }
                fighter->position.vy = fighter->groundY + fighter->rootOffset.y;
                fighter->anchor.vy = fighter->groundY;
                slideStage = SlideFighterAlongObstacle(fighter, contact, height, slideStage);
                if (slideStage == 3) {
                    break;
                }
            } else if (fighter->groundY < upper) {
                fighter->nearWallContact = 0;
                fighter->position.vx = contact->dest.x;
                fighter->position.vy = fighter->groundY + fighter->rootOffset.y;
                fighter->position.vz = contact->dest.z;
                fighter->anchor.vy = fighter->groundY;
                fighter->shadowGroundY = fighter->groundY;
                StartFighterFallFromLedge(fighter);
                break;
            } else {
                fighter->nearWallContact = IsNearRecordedWallContact(fighter);
                *(WorldVector *)&fighter->position = contact->dest;
                fighter->position.vy = height + fighter->rootOffset.y;
                fighter->anchor.vy = height;
                fighter->groundY = height;
                fighter->shadowGroundY = fighter->position.vy - fighter->rootOffset.y;
                fighter->surfaceFlags = GetLastFloorAttr();
                break;
            }
        } else if (height == FLOOR_HEIGHT_NONE) {
            fighter->position.vy = fighter->groundY + fighter->rootOffset.y;
            fighter->anchor.vy = fighter->groundY;
            slideStage = SlideFighterAlongObstacle(fighter, contact, height, slideStage);
            if (slideStage == 3) {
                break;
            }
        } else {
            fighter->nearWallContact = IsNearRecordedWallContact(fighter);
            heightDiff = height - fighter->groundY;
            if (heightDiff > 10) {
                if (contact->normal->vy < FLOOR_WALKABLE_NORMAL_Y) {
                    int offset;
                    fighter->position.vx = contact->dest.x;
                    offset = fighter->rootOffset.y;
                    fighter->position.vz = contact->dest.z;
                    fighter->anchor.vy = height;
                    fighter->shadowGroundY = height;
                    fighter->groundY = height;
                    fighter->position.vy = height + offset;
                    fighter->onSlopeBand = upper != lower;
                    fighter->surfaceFlags = GetLastFloorAttr();
                } else {
                    fighter->position.vx = contact->dest.x;
                    fighter->position.vz = contact->dest.z;
                    fighter->position.vy = fighter->groundY + fighter->rootOffset.y;
                    fighter->anchor.vy = fighter->groundY;
                    fighter->shadowGroundY = fighter->groundY;
                    StartFighterFallFromLedge(fighter);
                }
                break;
            }
            if (heightDiff >= -99 && contact->normal->vy < FLOOR_WALKABLE_NORMAL_Y) {
                int offset;
                fighter->position.vx = contact->dest.x;
                offset = fighter->rootOffset.y;
                fighter->position.vz = contact->dest.z;
                fighter->anchor.vy = height;
                fighter->shadowGroundY = height;
                fighter->groundY = height;
                fighter->position.vy = height + offset;
                fighter->onSlopeBand = upper != lower;
                fighter->surfaceFlags = GetLastFloorAttr();
                break;
            }
            fighter->position.vy = fighter->groundY + fighter->rootOffset.y;
            fighter->anchor.vy = fighter->groundY;
            slideStage = SlideFighterAlongObstacle(fighter, contact, height, slideStage);
            if (slideStage == 3) {
                break;
            }
        }

        ClampFighterMaxSeparation(fighter, contact);
        QUERY_FLOOR_BAND(contact->dest.x, contact->dest.y - 1000,
                         contact->dest.z, height, lower, upper);
        contact->normal = (SVECTOR *)GetLastFloorNormal();
    }

    if (slideStage > 0) {
        dx = fighter->position.vx - oldX;
        dz = fighter->position.vz - oldZ;
        fighter->shadowGroundY = fighter->groundY;
        squared = dx * dx + dz * dz;
        if ((unsigned int)squared < 1024) {
            moved = (unsigned int)gSqrtLutQ3[squared] >> 3;
        } else {
            unsigned int shift;
            gte_ldlzc(squared);
            gte_nop();
            gte_nop();
            gte_stlzc(&lzAfter);
            shift = lzAfter;
            shift = 22 - (shift & ~1);
            squared = gSqrtLutQ3[(unsigned int)squared >> shift];
            moved = ((unsigned int)squared << 16) >> (19 - (shift >> 1));
        }
        if (moved < distance / 2) {
            TryStartWallBumpStumble(fighter, wantedX, wantedZ);
        }
    }
}

static int CanClimbRecordedLedge(Fighter *fighter)
{
    unsigned char unused[16]; /* not used */
    int dx, dz, x, z;
    int ground;

    dx = -fighter->wallContactNormal.vx / 4;
    dz = -fighter->wallContactNormal.vz / 4;
    x = fighter->position.vx + dx;
    z = fighter->position.vz + dz;
    ground = QueryFloorHeight(x, fighter->position.vy, z);
    ground &= -8;
    if (ground != fighter->groundY - FLOOR_BAND_HEIGHT) {
        return 0;
    }
    dx /= 3;
    dz /= 3;
    ground = QueryFloorHeight(x + dz, fighter->position.vy, z - dx);
    ground &= -8;
    if (ground == fighter->groundY - FLOOR_BAND_HEIGHT) {
        ground = QueryFloorHeight(x - dz, fighter->position.vy, z + dx);
        ground &= -8;
        return ground == fighter->groundY - FLOOR_BAND_HEIGHT;
    }
    return 0;
}

static void SnapFighterToNearestFloor(Fighter *fighter)
{
    int radius, step, angle;
    int sinVal, cosVal, x, z, height;

    for (radius = 100; ; radius += 100) {
        for (step = 0, angle = ANGLE_QUARTER; step < 16; ++step, angle += 0x100) {
            sinVal = gSinLutQ12[(step << 8) & ANGLE_MASK];
            cosVal = gSinLutQ12[angle & ANGLE_MASK];
            x = fighter->anchor.vx + fighter->rootOffset.x + ((sinVal * radius) >> 12);
            z = fighter->anchor.vz + fighter->rootOffset.z + ((cosVal * radius) >> 12);
            height = QueryFloorHeight(x, fighter->anchor.vy, z);

            if (height != FLOOR_QUERY_MISS &&
                GetLastFloorNormal()->vy < -3071) {
                fighter->position.vx = x;
                fighter->position.vz = z;
                fighter->anchor.vy = height;
                fighter->anchor.vx = x - fighter->rootOffset.x;
                fighter->anchor.vz = z - fighter->rootOffset.z;
                fighter->position.vy = height + fighter->rootOffset.y;
                return;
            }
        }
    }
}

#ifndef VERSION_JP
unsigned char gCharTauntSubtitleOffset[] = { 0, 2, 3, 4, 5, 6 };
#endif
short gMoatWaterDrag = 2048;

void UpdateFighterFrame(int player, PadInputFrame *input)
{
    MotionTrigger *trigger;

    Fighter *fighter;
    FighterMotionScratch *scratch;
    MotionPlacement *currentPlacement;
    int bufferedActions;
    int actionBit;
    MotionPlacement *triggerPlacement;
    int triggerMask;
    int classBit;
    int value;
    MotionPlacement *activePlacement;
#ifndef VERSION_JP
    int tauntClass;
#endif
    int prevActionBit;
    int climbHeading;
    int driftX;
    int driftZ;
    MotionPlacement *followingPlacement;
    int facingMode;
    MotionPlacement *blendPlacement;
    int sampleFrame;
    MotionPlacement *samplePlacement;
    int sampleFrameLimit;
    unsigned char sampleFlags;
    int lastFrame;
    int frame;
    int nextFrame;
    MotionPlacement *forwardPlacement;
    int forwardClipStart;
    MotionPlacement *backwardPlacement;
    int backwardClipStart;
    int stature;
    int blendedSavedY;
    int freezeState;
    int stageId;
    int dampedX;
    int dampedY;
    int dampedZ;
    unsigned short heldButtons;
    int turnAdjusted;
    int ceiling;
    int floor;
    int work;
    int homing;
    int airClearance;
    int obstructedAirborne;
    SVECTOR *airNormal;
    int airNormalY;
    int airNormalX;
    int airNormalZ;
    int airRunSq;
    SVECTOR *airSlideNormal;
    int landZ;
    int ledgeTop;
    SVECTOR *ledgeNormal;
    int ledgeRunSq;
    int normalRun;
    SVECTOR *ledgePushNormal;
    short fallState;
    int fallStature;
    int fallClearance;
    int obstructedStep;
    SVECTOR *fallNormal;
    int fallNormalY;
    int fallNormalX;
    int fallNormalZ;
    int fallRunSq;
    SVECTOR *fallSlideNormal;
    int fallLandNormalY;
    unsigned short wounds;
    int fallLandZ;
    AnimInfo *throwInfo;
    int waterDrag;
    int attackWindow;
    int hitFrame;
    unsigned short clashPower;
    int moveX;
    int moveY;
    int moveZ;
    int moveSq;
    int stationary;
    FighterPose pose[2];
    VECTOR prevPos;
    VECTOR lzc;
    VECTOR cutscenePos;
    int moveLzc;
    Fighter *opponent;

    fighter = &gFighters[player];
    opponent = fighter->opponent;
    fighter->currentInput = *input;
    scratch = MOTION_SCRATCH;
    if (fighter->activeFrames == 0) {
        fighter->currentInput.released = 0;
        fighter->currentInput.pressed = 0;
        fighter->currentInput.held = 0;
        fighter->currentInput.notHeld = 0xffff;
        QUERY_FLOOR_BAND(fighter->position.vx, fighter->position.vy - 1000, fighter->position.vz,
                         work, floor, ceiling);
        fighter->groundY = work;
        fighter->onSlopeBand = ceiling != floor;
        fighter->surfaceFlags = GetLastFloorAttr();
    }
    if (gPovModeEnabled == 1 && (player == 0 || (player == 1 && fighter->isCpu == 0))) {
        fighter->currentInput.held = ROTATE_DPAD_POV(fighter->currentInput.held);
        fighter->currentInput.pressed = ROTATE_DPAD_POV(fighter->currentInput.pressed);
        fighter->currentInput.released = ROTATE_DPAD_POV(fighter->currentInput.released);
        fighter->currentInput.notHeld = ROTATE_DPAD_POV(fighter->currentInput.notHeld);
    }
    fighter->isPovMode = gPovModeEnabled == 1;
    fighter->angleToOpponent = ratan2(opponent->position.vx - fighter->position.vx,
                                      opponent->position.vz - fighter->position.vz);
    fighter->activeFrames++;
    if (fighter->action == FIGHTER_ACTION_DEAD ||
        fighter->action == FIGHTER_ACTION_FATAL_KO ||
        fighter->action == FIGHTER_ACTION_DEFEATED ||
        (fighter->placement->flags & MOTION_FLAG_FATAL) != 0) {
        if (gMatchWinnerSide == -1) {
            gMatchWinnerSide = player == 0;
        }
    }
    if (gHumanPlayerCount == 1) {
        if (fighter->action == FIGHTER_ACTION_DEFEATED) {
            gMatchWinnerSide = player == 0;
        }
    }
    fighter->motionJustChanged = 0;
    UpdateFighterAnimSpeed(fighter);
    if (fighter->action != FIGHTER_ACTION_DEAD) {
        if (fighter->fallState != 0) {
            fighter->animFrac += ONE;
        } else if (fighter->climbState != 0) {
            fighter->animFrac += fighter->animSpeed < 0x880 ? 0x880 : fighter->animSpeed;
        } else if (fighter->poseBlendWeight > 0) {
            fighter->animFrac += ONE;
        } else {
            fighter->animFrac += fighter->animSpeed;
        }
    }
    work = fighter->animFrac >> 12;
    fighter->animFrame += work;
    fighter->animFrac -= work << 12;
    fighter->animFrameStalled = work < 1;
    if (fighter->pendingAction != FORCED_ACTION_NONE) {
        if (fighter->isCpu != 0 && gStoryHonorWindowTimer > 0) {
            gBushidoHonorIntact = 0;
            gStoryHonorWindowActive = 0;
#ifndef VERSION_JP
            gSubtitleActive = 0;
#endif
        }
        if (fighter->isCpu != 0 && fighter->climbState != 0) {
            if (fighter->pendingAction == FORCED_ACTION_STAGGER_FRONT ||
                fighter->pendingAction == FORCED_ACTION_STAGGER_BACK ||
                fighter->pendingAction == FORCED_ACTION_LEG_CRIPPLE ||
                fighter->pendingAction == FORCED_ACTION_RIGHT_ARM_WOUND ||
                fighter->pendingAction == FORCED_ACTION_LEFT_ARM_CRIPPLE ||
                fighter->pendingAction == FORCED_ACTION_FATAL_FRONT ||
                fighter->pendingAction == FORCED_ACTION_FATAL_BACK ||
                fighter->pendingAction == FORCED_ACTION_CLASH_RECOIL ||
                fighter->pendingAction == FORCED_ACTION_THIGH_WOUND) {
                gBushidoHonorIntact = 0;
                gStoryHonorWindowActive = 0;
#ifndef VERSION_JP
                gSubtitleActive = 0;
#endif
            }
        }
        fighter->prevFacingState = fighter->facingState;
        SetFighterMotionPlacement(fighter, fighter->placement->actionTable[fighter->pendingAction]);
        fighter->lastForcedAction = fighter->pendingAction;
        fighter->pendingAction = FORCED_ACTION_NONE;
        if (fighter->climbState == 0 && fighter->fallState == 0) {
            if (fighter->prevFacingState == MOTION_FACING_SUPINE_FLIP) {
                StartFighterPoseBlend(fighter, 0x400);
                FlipPoseFromSupineState(fighter->savedPose.joint);
            } else if (fighter->prevFacingState == MOTION_FACING_INVERTED) {
                if (fighter->prevAction == FIGHTER_ACTION_DOWNED) {
                    fighter->prevFacingState = MOTION_FACING_PRONE_FLIP;
                    StartFighterPoseBlend(fighter, 0x400);
                    FlipPoseFromProneState(fighter->savedPose.joint);
                }
            } else if (fighter->prevAction != FIGHTER_ACTION_GUARD || fighter->action != FIGHTER_ACTION_GUARD) {
                StartFighterPoseBlend(fighter, 0x400);
            }
        }
    } else {
        currentPlacement = fighter->placement;
        if (currentPlacement->canQueue != 0) {
            if (fighter->animFrame == currentPlacement->frameLimit) {
                if (fighter->queuedPlacement != 0) {
                    SetFighterMotionPlacement(fighter, fighter->queuedPlacement);
                } else {
                    SetFighterMotionPlacement(fighter, currentPlacement->nextPlacement);
                }
            } else if (fighter->animFrame >= currentPlacement->triggerLimit && fighter->queuedPlacement != 0) {
                SetFighterMotionPlacement(fighter, fighter->queuedPlacement);
            }
        } else if (fighter->animFrame == currentPlacement->frameLimit) {
            SetFighterMotionPlacement(fighter, currentPlacement->nextPlacement);
        }
    }
    bufferedActions = BUFFERED_INPUT_ACTIONS;
    if (fighter->motionJustChanged != 0) {
        prevActionBit = 1 << fighter->prevAction;
        if ((prevActionBit & bufferedActions) != 0) {
            fighter->currentInput.pressed |= fighter->bufferedButtons;
        }
    }
    if (fighter->queuedPlacement == 0) {
        triggerPlacement = fighter->placement;
        if (fighter->animFrame < triggerPlacement->triggerLimit) {
            for (trigger = triggerPlacement->triggers; trigger->mask != 0; trigger++) {
                triggerMask = trigger->mask;
                if ((triggerMask & MOTION_TRIGGER_CLASS_MASK) != 0) {
                    classBit = 1 << fighter->characterClass + MOTION_TRIGGER_CLASS_SHIFT;
                    if ((triggerMask & classBit) == 0) {
                        continue;
                    }
                }
                switch (triggerMask >> MOTION_TRIGGER_EDGE_SHIFT & MOTION_TRIGGER_EDGE_MASK) {
                case 0:
                    value = fighter->currentInput.pressed;
                    break;
                case 1:
                    value = fighter->currentInput.released;
                    break;
                case 2:
                    value = fighter->currentInput.held;
                    break;
                case 3:
                    value = fighter->currentInput.notHeld;
                    break;
                }
                if ((value & trigger->mask) != 0) {
                    activePlacement = fighter->placement;
                    if (activePlacement->canQueue != 0) {
                        fighter->queuedPlacement = trigger->next;
                    } else {
                        if (fighter->action == FIGHTER_ACTION_GUARD &&
                            activePlacement->animClipId == trigger->next->animClipId) {
                            work = fighter->animFrame;
                            SetFighterMotionPlacement(fighter, trigger->next);
                            fighter->animFrame = fighter->placement->frameLimit - 1 - work;
                        } else {
                            SetFighterMotionPlacement(fighter, trigger->next);
                        }
#ifndef VERSION_JP
                        if (value == PADselect) {
                            tauntClass = fighter->characterClass;
                            if (tauntClass < PLAYABLE_CHARACTER_COUNT) {
                                StartSubtitleById(gCharTauntSubtitleOffset[tauntClass] + 0x54);
                                gSubtitleActive = 1;
                            }
                        }
#endif
                    }
                    break;
                }
            }
        }
    }
    actionBit = 1 << fighter->action;
    if ((bufferedActions & actionBit) != 0) {
        if (fighter->placement->frameLimit - 12 < fighter->animFrame) {
            fighter->bufferedButtons |= fighter->currentInput.pressed;
        } else {
            fighter->bufferedButtons = 0;
        }
    }
    if (fighter->pendingFallAction != FORCED_ACTION_NONE) {
        if (fighter->climbState == 0) {
            fighter->frozenRootOffset.x = 0;
            fighter->frozenRootOffset.y = fighter->prevRootOffset.y;
            fighter->frozenRootOffset.z = 0;
            fighter->rootFreezeState = 1;
        }
        if (fighter->isCpu != 0) {
            if (gStoryHonorWindowTimer > 0) {
                gBushidoHonorIntact = 0;
                gStoryHonorWindowActive = 0;
#ifndef VERSION_JP
                gSubtitleActive = 0;
#endif
            }
        }
        SetFighterMotionPlacement(fighter, fighter->placement->actionTable[fighter->pendingFallAction]);
        fighter->lastForcedAction = fighter->pendingFallAction;
        fighter->pendingFallAction = FORCED_ACTION_NONE;
        if (fighter->facingState != MOTION_FACING_INVERTED) {
            StartFighterPoseBlend(fighter, 0x400);
            if (fighter->climbState != 0) {
                FlipPoseForClimbTransition(fighter->savedPose.joint);
                fighter->climbState = 0;
                fighter->angles.heading += ANGLE_HALF;
                fighter->savedAngles.heading = fighter->angles.heading;
            }
        }
    }
    if (fighter->climbState == 0
#ifndef VERSION_JP
        && gBladeLockState == BLADE_LOCK_NONE
#endif
    ) {
        if (fighter->legsIntact != 0) {
            if (fighter->nearWallContact != 0) {
                if ((fighter->currentInput.held & PAD_DPAD_MASK) != 0 && (fighter->currentInput.held & PADR1) != 0) {
                    if (fighter->action == FIGHTER_ACTION_NEUTRAL ||
                        fighter->lastForcedAction == FORCED_ACTION_HEAD_ON_STUMBLE && fighter->motionJustChanged != 0) {
                        if (CanClimbRecordedLedge(fighter) != 0) {
                            fighter->climbState = 1;
                            climbHeading = ratan2(fighter->wallContactNormal.vx,
                                                  fighter->wallContactNormal.vz) + ANGLE_HALF;
                            fighter->savedAngles.heading = climbHeading;
                            SetFighterTurnTarget(fighter, climbHeading);
                            fighter->angles.heading += ANGLE_HALF;
                            SetFighterMotionPlacement(
                                fighter,
                                fighter->placement->actionTable[gCharClimbActionOffset[fighter->characterClass] +
                                                                FORCED_ACTION_CLIMB_BASE]);
                            StartFighterPoseBlend(fighter, 0x300);
                            FlipPoseForClimbTransition(fighter->savedPose.joint);
                            fighter->frozenRootOffset.x = 0;
                            fighter->frozenRootOffset.y = fighter->prevRootOffset.y;
                            fighter->frozenRootOffset.z = 0;
                            fighter->rootFreezeState = 1;
                        }
                    }
                }
            }
        }
    }
    if (fighter->weaponId == WEAPON_ID_GUN &&
        fighter->animFrameStalled == 0 &&
        fighter->action == FIGHTER_ACTION_RELOAD &&
        fighter->animInfo->activeStartFrame == fighter->animFrame &&
        fighter->gunMagazines > 0) {
        fighter->gunAmmo = 10;
        fighter->gunMagazines--;
    }
    if (fighter->motionJustChanged != 0) {
        if (fighter->action == FIGHTER_ACTION_NEUTRAL) {
            if (fighter->prevAction == FIGHTER_ACTION_BLEND) {
                StartFighterPoseBlend(fighter, 0x400);
            }
        }
        if (fighter->action == FIGHTER_ACTION_BLEND) {
            StartFighterPoseBlend(fighter, 0x400);
        }
        if (fighter->prevAction == FIGHTER_ACTION_ATTACK &&
            (fighter->prevPlacement->flags & MOTION_FLAG_BLEND_ON_EXIT) != 0) {
            if (fighter->action == FIGHTER_ACTION_NEUTRAL || fighter->action == FIGHTER_ACTION_CROUCH) {
                StartFighterPoseBlend(fighter, 0x400);
            }
        }
        if (fighter->prevAction == FIGHTER_ACTION_THROW) {
            StartFighterPoseBlend(fighter, 0x400);
        }
        if (fighter->prevAction == FIGHTER_ACTION_NEUTRAL && (fighter->prevAnimInfo->flags & ANIM_FLAG_WALK) != 0) {
            if (fighter->action != FIGHTER_ACTION_NEUTRAL ||
                ((fighter->animInfo->flags & ANIM_FLAG_WALK) == 0 &&
                 (fighter->animInfo->flags & ANIM_FLAG_DOWNED) == 0)) {
                if (fighter->climbState == 0) {
                    StartFighterPoseBlend(fighter, 0x400);
                }
            }
        }
        if (fighter->prevAction == FIGHTER_ACTION_CROUCH && (fighter->prevAnimInfo->flags & ANIM_FLAG_WALK) != 0) {
            if (fighter->action != fighter->prevAction || (fighter->animInfo->flags & ANIM_FLAG_WALK) == 0) {
                if (fighter->climbState == 0) {
                    StartFighterPoseBlend(fighter, 0x400);
                }
            }
        }
        if (fighter->prevAction == FIGHTER_ACTION_GUARD) {
            if (fighter->action == FIGHTER_ACTION_ATTACK) {
                StartFighterPoseBlend(fighter, 0x400);
            }
        }
        if (fighter->prevAction == FIGHTER_ACTION_NEUTRAL && (fighter->prevAnimInfo->flags & ANIM_FLAG_RUN) != 0) {
            if (fighter->action != FIGHTER_ACTION_NEUTRAL || (fighter->animInfo->flags & ANIM_FLAG_RUN) == 0) {
                StartFighterPoseBlend(fighter, 0x400);
                work = fighter->runSpeed * fighter->animSpeed >> 12;
                driftX = gSinLutQ12[(unsigned short)fighter->angles.heading & ANGLE_MASK] * work >> 16;
                driftZ = gSinLutQ12[fighter->angles.heading + ANGLE_QUARTER & ANGLE_MASK] * work >> 16;
                driftX = driftX * fighter->poseBlendWeight >> 12;
                driftZ = driftZ * fighter->poseBlendWeight >> 12;
                work = ONE - fighter->velocityRetention;
                fighter->velocity.x += driftX;
                fighter->velocity.z += driftZ;
                fighter->anchor.vx += driftX * work >> 12;
                fighter->anchor.vz += driftZ * work >> 12;
            }
        }
        if (fighter->prevAction == FIGHTER_ACTION_NEUTRAL && (fighter->prevAnimInfo->flags & ANIM_FLAG_RUN) != 0) {
            if (fighter->action == FIGHTER_ACTION_NEUTRAL && (fighter->animInfo->flags & ANIM_FLAG_RUN) == 0) {
                StartFighterPoseBlend(fighter, 0x400);
            }
        }
        if (fighter->prevAction == FIGHTER_ACTION_DOWNED && (fighter->prevPlacement->flags & MOTION_FLAG_FATAL) != 0) {
            if (fighter->action == FIGHTER_ACTION_DEAD) {
                StartFighterPoseBlend(fighter, 0x80);
            }
        }
        if (fighter->action == FIGHTER_ACTION_NEUTRAL && (fighter->animInfo->flags & ANIM_FLAG_FIXED_SPEED) != 0) {
            if (fighter->flipHeadingNextTick == 0) {
                StartFighterPoseBlend(fighter, 0x400);
            }
        }
    }
    if (fighter->action == FIGHTER_ACTION_PICKUP) {
        if (fighter->animFrame + 1 == fighter->placement->frameLimit) {
            followingPlacement = fighter->placement->nextPlacement;
            if (MOTION_FILE->animInfo[followingPlacement->animClipId - 1].type == FIGHTER_ACTION_NEUTRAL) {
                SetFighterMotionPlacement(fighter, followingPlacement);
                StartFighterPoseBlend(fighter, 0x400);
            }
        }
    }
    if (fighter->flipHeadingNextTick != 0) {
        fighter->flipHeadingNextTick = 0;
        fighter->angles.heading += ANGLE_HALF;
    }
    if (fighter->isCpu != 0 ||
        fighter->isPovMode == 0 ||
        fighter->opponent->isStationary == 0 ||
        gFighterDistanceXZ < 8000) {
        if (fighter->placement->facingMode == MOTION_FACING_AUTO) {
            AdjustFighterTurnTarget(fighter, fighter->angleToOpponent);
        }
        if (fighter->motionJustChanged != 0 &&
            (facingMode = fighter->placement->facingMode) != MOTION_FACING_INVERTED) {
            if (fighter->action == FIGHTER_ACTION_GUARD && fighter->prevAction != FIGHTER_ACTION_GUARD ||
                fighter->action == FIGHTER_ACTION_ATTACK && facingMode == MOTION_FACING_ON_ENTRY) {
                SetFighterTurnTarget(fighter, fighter->angleToOpponent);
            }
        } else if (fighter->animFrameStalled == 0 &&
                   fighter->action == FIGHTER_ACTION_SPECIAL &&
                   fighter->animInfo->activeStartFrame == fighter->animFrame) {
            SetFighterTurnTarget(fighter, fighter->angleToOpponent);
        }
        if (fighter->motionJustChanged != 0) {
            if (fighter->action == FIGHTER_ACTION_SPECIAL || fighter->action == FIGHTER_ACTION_ATTACK) {
                fighter->lockedMoveHeading = fighter->savedAngles.heading;
            }
            if (fighter->action == FIGHTER_ACTION_NEUTRAL &&
                (fighter->animInfo->flags & (ANIM_FLAG_WALK | ANIM_FLAG_DOWNED)) != 0) {
                fighter->lockedMoveHeading = fighter->angleToOpponent;
            }
            if (fighter->climbState == 0) {
                if (fighter->prevAction == FIGHTER_ACTION_NEUTRAL &&
                    (fighter->prevAnimInfo->flags & ANIM_FLAG_DOWNED) != 0) {
                    SetFighterTurnTarget(fighter, fighter->angleToOpponent);
                }
            }
        }
    } else {
        fighter->lockedMoveHeading = fighter->savedAngles.heading;
    }
    work = fighter->turnRemaining * fighter->animSpeed / 0x6000;
    fighter->angles.heading += work;
    fighter->lastTurnStep = work;
    fighter->turnRemaining -= work;
    if (fighter->climbState == 0) {
        if (fighter->fallState == 0) {
            fighter->savedAngles.heading = fighter->angles.heading;
        }
    }
    if (fighter->poseBlendWeight > 0) {
        blendPlacement = fighter->placement;
        if ((blendPlacement->flags & MOTION_FLAG_FORWARD) != 0) {
            frame = fighter->animBaseFrame + fighter->animFrame;
        } else {
            frame = fighter->animBaseFrame + (blendPlacement->frameLimit - 1) -
                (fighter->animFrame - blendPlacement->startFrame);
        }
        DecodePoseFramePair(frame, frame, pose[0].joint, pose[0].joint, (PoseWork *)&COLLISION_SCRATCH->work);
        if (fighter->leftArmIntact == 0) {
            ApplyDisabledLeftArmPose(pose[0].joint);
        }
        UpdateFighterLookAngles(fighter);
        pose[0].joint[POSE_JOINT_HEAD].x += fighter->lookRoll;
        pose[0].joint[POSE_JOINT_HEAD].y += fighter->lookYaw;
        pose[0].joint[POSE_JOINT_HEAD].z += fighter->lookPitch;
    } else {
        sampleFrame = fighter->animFrame;
        samplePlacement = fighter->placement;
        sampleFrameLimit = samplePlacement->frameLimit;
        sampleFlags = samplePlacement->flags;
        lastFrame = sampleFrame + 1 == sampleFrameLimit;
        if ((sampleFlags & MOTION_FLAG_FORWARD) != 0) {
            frame = fighter->animBaseFrame + sampleFrame;
            if (lastFrame != 0) {
                if (fighter->action == FIGHTER_ACTION_THROW || samplePlacement->facingMode == MOTION_FACING_INVERTED) {
                    nextFrame = frame;
                } else if (fighter->action == FIGHTER_ACTION_DOWNED && (sampleFlags & MOTION_FLAG_FATAL) != 0) {
                    nextFrame = frame;
                } else if (fighter->action == FIGHTER_ACTION_ATTACK &&
                           (fighter->placement->flags & MOTION_FLAG_BLEND_ON_EXIT) != 0) {
                    nextFrame = frame;
                } else if (fighter->action == FIGHTER_ACTION_BLEND || fighter->climbState == 3) {
                    nextFrame = frame;
                } else {
                    forwardPlacement = fighter->placement->nextPlacement;
                    forwardClipStart = MOTION_FILE->animBaseFrames[forwardPlacement->animClipId - 1];
                    if ((forwardPlacement->flags & MOTION_FLAG_FORWARD) != 0) {
                        nextFrame = forwardClipStart + forwardPlacement->startFrame;
                    } else {
                        nextFrame = forwardClipStart + (forwardPlacement->frameLimit - 1);
                    }
                }
            } else {
                nextFrame = frame + 1;
            }
            DecodePoseFramePair(frame, nextFrame, pose[0].joint, pose[1].joint, (PoseWork *)&COLLISION_SCRATCH->work);
        } else {
            frame = fighter->animBaseFrame + (sampleFrameLimit - 1) - (sampleFrame - samplePlacement->startFrame);
            if (lastFrame != 0) {
                backwardPlacement = samplePlacement->nextPlacement;
                backwardClipStart = MOTION_FILE->animBaseFrames[backwardPlacement->animClipId - 1];
                if ((backwardPlacement->flags & MOTION_FLAG_FORWARD) != 0) {
                    nextFrame = backwardClipStart + backwardPlacement->startFrame;
                } else {
                    nextFrame = backwardClipStart + (backwardPlacement->frameLimit - 1);
                }
            } else {
                nextFrame = frame - 1;
            }
            DecodePoseFramePair(nextFrame, frame, pose[1].joint, pose[0].joint, (PoseWork *)&COLLISION_SCRATCH->work);
        }
        if (fighter->leftArmIntact == 0) {
            ApplyDisabledLeftArmPose(pose[0].joint);
            ApplyDisabledLeftArmPose(pose[1].joint);
        }
        UpdateFighterLookAngles(fighter);
        pose[0].joint[POSE_JOINT_HEAD].x += fighter->lookRoll;
        pose[1].joint[POSE_JOINT_HEAD].x += fighter->lookRoll;
        pose[0].joint[POSE_JOINT_HEAD].y += fighter->lookYaw;
        pose[1].joint[POSE_JOINT_HEAD].y += fighter->lookYaw;
        pose[0].joint[POSE_JOINT_HEAD].z += fighter->lookPitch;
        pose[1].joint[POSE_JOINT_HEAD].z += fighter->lookPitch;
        fighter->savedPose = pose[0];
        fighter->lastSampledFrame = frame;
    }
    stature = fighter->statureScale;
    if (fighter->action == FIGHTER_ACTION_SPECIAL ||
        fighter->action == FIGHTER_ACTION_NEUTRAL &&
        (fighter->animInfo->flags & (ANIM_FLAG_WALK | ANIM_FLAG_DOWNED)) != 0 ||
        fighter->action == FIGHTER_ACTION_ATTACK) {
        work = fighter->lockedMoveHeading;
    } else {
        work = fighter->savedAngles.heading;
    }
    scratch->currentRoot.x = (unsigned short)pose[0].joint[POSE_JOINT_ROOT].z *
        gSinLutQ12[(unsigned short)pose[0].joint[POSE_JOINT_ROOT].y - work & ANGLE_MASK] >> 12;
    scratch->currentRoot.y = -pose[0].joint[POSE_JOINT_ROOT].x;
    scratch->currentRoot.z = (unsigned short)pose[0].joint[POSE_JOINT_ROOT].z *
        gSinLutQ12[(unsigned short)pose[0].joint[POSE_JOINT_ROOT].y - ANGLE_QUARTER - work & ANGLE_MASK] >> 12;
    if (fighter->poseBlendWeight > 0) {
        fighter->rootOffset = scratch->currentRoot;
        blendedSavedY = fighter->blendSavedRoot.y * fighter->poseBlendWeight / stature;
        work = ONE - fighter->poseBlendWeight;
        fighter->rootOffset.y = (scratch->currentRoot.y * work >> 12) + blendedSavedY;
        if (fighter->prevAction == FIGHTER_ACTION_NEUTRAL && (fighter->prevAnimInfo->flags & ANIM_FLAG_RUN) != 0) {
            if (fighter->action == FIGHTER_ACTION_NEUTRAL && (fighter->animInfo->flags & ANIM_FLAG_RUN) == 0) {
                fighter->rootOffset.x = (scratch->currentRoot.x * work >> 12) +
                    fighter->blendSavedRoot.x * fighter->poseBlendWeight / stature;
                fighter->rootOffset.z = (scratch->currentRoot.z * work >> 12) +
                    fighter->blendSavedRoot.z * fighter->poseBlendWeight / stature;
            }
        }
    } else {
        if (fighter->action == FIGHTER_ACTION_SPECIAL ||
            fighter->action == FIGHTER_ACTION_NEUTRAL &&
            (fighter->animInfo->flags & (ANIM_FLAG_WALK | ANIM_FLAG_DOWNED)) != 0 ||
            fighter->action == FIGHTER_ACTION_ATTACK) {
            work = fighter->lockedMoveHeading;
        } else {
            work = fighter->savedAngles.heading;
        }
        scratch->nextRoot.x = (unsigned short)pose[1].joint[POSE_JOINT_ROOT].z *
            gSinLutQ12[(unsigned short)pose[1].joint[POSE_JOINT_ROOT].y - work & ANGLE_MASK] >> 12;
        scratch->nextRoot.y = -pose[1].joint[POSE_JOINT_ROOT].x;
        scratch->nextRoot.z = (unsigned short)pose[1].joint[POSE_JOINT_ROOT].z *
            gSinLutQ12[(unsigned short)pose[1].joint[POSE_JOINT_ROOT].y - ANGLE_QUARTER - work & ANGLE_MASK] >> 12;
        if (lastFrame != 0) {
            fighter->rootOffset.x = scratch->currentRoot.x +
                ((scratch->currentRoot.x - fighter->prevRawRoot.x) * fighter->animFrac >> 12);
            fighter->rootOffset.y = scratch->currentRoot.y +
                ((scratch->currentRoot.y - fighter->prevRawRoot.y) * fighter->animFrac >> 12);
            fighter->rootOffset.z = scratch->currentRoot.z +
                ((scratch->currentRoot.z - fighter->prevRawRoot.z) * fighter->animFrac >> 12);
        } else {
            work = ONE - fighter->animFrac;
            fighter->rootOffset.x = scratch->currentRoot.x * work + scratch->nextRoot.x * fighter->animFrac >> 12;
            fighter->rootOffset.y = scratch->currentRoot.y * work + scratch->nextRoot.y * fighter->animFrac >> 12;
            fighter->rootOffset.z = scratch->currentRoot.z * work + scratch->nextRoot.z * fighter->animFrac >> 12;
        }
    }
    fighter->rootOffset.x = fighter->rootOffset.x * stature >> 12;
    fighter->rootOffset.y = fighter->rootOffset.y * stature >> 12;
    fighter->rootOffset.z = fighter->rootOffset.z * stature >> 12;
    if (lastFrame == 0) {
        fighter->prevRawRoot = scratch->currentRoot;
    }
    if (fighter->motionJustChanged != 0) {
        freezeState = fighter->rootFreezeState;
        if (freezeState == 1) {
            fighter->rootFreezeState = 2;
        } else if (freezeState == 2) {
            fighter->rootFreezeState = 0;
            fighter->prevRootOffset = fighter->frozenRootOffset;
        }
        fighter->anchor.vx += fighter->prevRootOffset.x - fighter->rootOffset.x + fighter->rootOffsetDelta.x;
        fighter->anchor.vy += fighter->prevRootOffset.y - fighter->rootOffset.y + fighter->rootOffsetDelta.y;
        fighter->anchor.vz += fighter->prevRootOffset.z - fighter->rootOffset.z + fighter->rootOffsetDelta.z;
        fighter->rootOffsetDelta.x = 0;
        fighter->rootOffsetDelta.y = 0;
        fighter->rootOffsetDelta.z = 0;
    } else {
        fighter->rootOffsetDelta.x = fighter->rootOffset.x - fighter->prevRootOffset.x;
        fighter->rootOffsetDelta.y = fighter->rootOffset.y - fighter->prevRootOffset.y;
        fighter->rootOffsetDelta.z = fighter->rootOffset.z - fighter->prevRootOffset.z;
    }
    fighter->prevRootOffset = fighter->rootOffset;
    if (fighter->rootFreezeState != 0) {
        fighter->anchor.vx += fighter->rootOffsetDelta.x;
        fighter->anchor.vy += fighter->rootOffsetDelta.y;
        fighter->anchor.vz += fighter->rootOffsetDelta.z;
        fighter->rootOffset = fighter->frozenRootOffset;
    }
    stageId = gCurrentStageId;
    if ((stageId == 8 || stageId == STAGE_ID_YAGURA_POINT) && fighter->groundY == 0) {
        fighter->envPushVel.x = 32;
        fighter->envPushVel.y = 0;
        fighter->envPushVel.z = 32;
    } else {
        fighter->envPushVel.x = 0;
        fighter->envPushVel.y = 0;
        fighter->envPushVel.z = 0;
    }
    if (fighter->velocityRetention >= 0x801) {
        fighter->velocity.x += fighter->envPushVel.x;
        fighter->velocity.z += fighter->envPushVel.z;
    }
    dampedX = fighter->velocity.x * fighter->velocityRetention >> 12;
    fighter->velocity.x = dampedX;
    if ((unsigned int)(dampedX + 3) < 7) {
        fighter->velocity.x = 0;
    }
    dampedY = fighter->velocity.y * fighter->velocityRetention >> 12;
    fighter->velocity.y = dampedY;
    if ((unsigned int)(dampedY + 3) < 7) {
        fighter->velocity.y = 0;
    }
    dampedZ = fighter->velocity.z * fighter->velocityRetention >> 12;
    fighter->velocity.z = dampedZ;
    if ((unsigned int)(dampedZ + 3) < 7) {
        fighter->velocity.z = 0;
    }
    fighter->anchor.vx += fighter->velocity.x;
    fighter->anchor.vy += fighter->velocity.y;
    fighter->anchor.vz += fighter->velocity.z;
    if (fighter->jumpState != 0) {
        fighter->anchor.vx += fighter->jumpVel.x;
        fighter->anchor.vy += fighter->jumpVel.y;
        fighter->anchor.vz += fighter->jumpVel.z;
    }
    UpdateFighterRunSteering(fighter);
    if (fighter->isPovMode != 0) {
        if (fighter->isCpu == 0) {
            if (fighter->action == FIGHTER_ACTION_NEUTRAL) {
                heldButtons = (unsigned short)fighter->currentInput.held;
                if (heldButtons == PADLup) {
                    turnAdjusted = (unsigned short)fighter->turnRemaining - 32;
                    fighter->turnRemaining = turnAdjusted;
                } else if (heldButtons == PADLdown) {
                    turnAdjusted = (unsigned short)fighter->turnRemaining + 32;
                    fighter->turnRemaining = turnAdjusted;
                }
            }
        }
    }
    scratch->dest.x = fighter->anchor.vx + fighter->rootOffset.x;
    scratch->dest.y = fighter->anchor.vy + fighter->rootOffset.y;
    scratch->dest.z = fighter->anchor.vz + fighter->rootOffset.z;
    prevPos = fighter->position;
    if (fighter->isPovMode != 0 &&
        fighter->isCpu == 0 &&
        fighter->opponent->isStationary != 0 &&
        gFighterDistanceXZ >= 8000) {
        if (fighter->action == FIGHTER_ACTION_NEUTRAL && (fighter->animInfo->flags & ANIM_FLAG_WALK) != 0) {
            work = 0 - ((input->held & PADLleft) != 0);
            if ((input->held & PADLright) != 0) {
                ++work;
            }
            if (work != 0) {
                SetFighterTurnTarget(fighter, fighter->angles.heading + (work << 8));
                scratch->dest.x = fighter->position.vx;
                scratch->dest.z = fighter->position.vz;
                fighter->anchor.vx = scratch->dest.x - fighter->rootOffset.x;
                fighter->anchor.vz = scratch->dest.z - fighter->rootOffset.z;
            } else {
                fighter->turnRemaining = 0;
            }
        } else if (fighter->action == FIGHTER_ACTION_NEUTRAL && (fighter->animInfo->flags & ANIM_FLAG_RUN) == 0) {
            fighter->turnRemaining = 0;
        } else if (fighter->action == FIGHTER_ACTION_CROUCH && (fighter->animInfo->flags & ANIM_FLAG_WALK) != 0) {
            work = 0 - ((input->held & PADLleft) != 0);
            if ((input->held & PADLright) != 0) {
                ++work;
            }
            if (work != 0) {
                SetFighterTurnTarget(fighter, fighter->angles.heading + (work << 7));
                scratch->dest.x = fighter->position.vx;
                scratch->dest.z = fighter->position.vz;
                fighter->anchor.vx = scratch->dest.x - fighter->rootOffset.x;
                fighter->anchor.vz = scratch->dest.z - fighter->rootOffset.z;
            } else {
                fighter->turnRemaining = 0;
            }
        }
    }
    ClampFighterMaxSeparation(fighter, scratch);
    QUERY_FLOOR_BAND(scratch->dest.x, scratch->dest.y - 1000, scratch->dest.z,
                     work, floor, ceiling);
    scratch->normal = (SVECTOR *)GetLastFloorNormal();
    if (fighter->jumpState == 0) {
        homing = 0;
        if (fighter->action == FIGHTER_ACTION_ATTACK) {
            homing = fighter->animInfo->flags >> 5 & 1;
        }
        fighter->isHomingJump = homing;
        if (fighter->isHomingJump != 0 || fighter->action == FIGHTER_ACTION_JUMP) {
            fighter->jumpState = 1;
        }
    }
    if (gBladeLockState != BLADE_LOCK_NONE) {
        UpdateBladeLockInput(fighter);
    }
    if (fighter->jumpState == 1) {
        if (floor < fighter->groundY) {
            fighter->position.vy = fighter->groundY + fighter->rootOffset.y;
            fighter->anchor.vx -= scratch->dest.x - fighter->position.vx;
            fighter->anchor.vz -= scratch->dest.z - fighter->position.vz;
        } else {
            *(WorldVector *)&fighter->position = scratch->dest;
            fighter->groundY = work;
        }
        fighter->jumpState = 2;
        if (fighter->isHomingJump != 0) {
            fighter->jumpVel.y = fighter->jumpLaunchParams[0] * 70 / 100;
            SolveBallisticJumpVelocity((VECTOR *)&fighter->torsoCenter, (VECTOR *)&opponent->torsoCenter,
                                       fighter->jumpVel.y, 42, (VECTOR *)&fighter->jumpVel);
            /* the original writes these back unchanged */
            fighter->jumpVel.x = fighter->jumpVel.x - fighter->position.vx + fighter->position.vx;
            fighter->jumpVel.z = fighter->jumpVel.z - fighter->position.vz + fighter->position.vz;
            fighter->anchor.vy += fighter->jumpVel.y;
        } else {
            fighter->jumpVel.x = gSinLutQ12[(unsigned short)fighter->savedAngles.heading & ANGLE_MASK] *
                fighter->jumpLaunchParams[1] >> 16;
            fighter->jumpVel.z = gSinLutQ12[fighter->savedAngles.heading + ANGLE_QUARTER & ANGLE_MASK] *
                fighter->jumpLaunchParams[1] >> 16;
            fighter->jumpVel.y = fighter->jumpLaunchParams[0];
        }
        fighter->shadowGroundY = work;
    } else if (fighter->jumpState == 2) {
        fighter->jumpVel.y += 62;
        airClearance = fighter->statureScale * 225 >> 10;
        if (scratch->dest.y + airClearance >= work) {
            fighter->jumpVel.x = fighter->jumpVel.x * 7 / 8;
            fighter->jumpVel.y = fighter->jumpVel.y * 7 / 8;
            fighter->jumpVel.z = fighter->jumpVel.z * 7 / 8;
            obstructedAirborne = 0;
            if (work == FLOOR_HEIGHT_NONE || work == -30000) {
                fighter->anchor.vx -= scratch->dest.x - fighter->position.vx;
                fighter->anchor.vz -= scratch->dest.z - fighter->position.vz;

                work = QueryFloorHeight(fighter->position.vx, fighter->position.vy - 1000, fighter->position.vz);
                if (work == FLOOR_QUERY_MISS) {
                    work = FLOOR_HEIGHT_NONE;
                } else {
                    work &= -8;
                }
                scratch->dest.x = fighter->position.vx;
                scratch->dest.z = fighter->position.vz;
                if (scratch->dest.y + airClearance < work) {
                    obstructedAirborne = 1;
                }
            }
            if (obstructedAirborne == 0) {
                airNormal = scratch->normal;
                airNormalY = airNormal->vy;
                if (airNormalY >= -4095) {
                    if (airNormalY >= FLOOR_WALKABLE_NORMAL_Y) {
                        airNormalX = airNormal->vx;
                        airNormalZ = airNormal->vz;
                        airRunSq = airNormalX * airNormalX + airNormalZ * airNormalZ;
                        if ((unsigned int)airRunSq < 1024) {
                            normalRun = (unsigned int)gSqrtLutQ3[airRunSq] >> 3;
                        } else {
                            unsigned int shift;
                            gte_ldlzc(airRunSq);
                            gte_nop();
                            gte_nop();
                            gte_stlzc(&lzc.vx);
                            shift = lzc.vx;
                            shift = 22 - (shift & ~1);
                            airRunSq = gSqrtLutQ3[(unsigned int)airRunSq >> shift];
                            normalRun = ((unsigned int)airRunSq << 16) >> (19 - (shift >> 1));
                        }
                        if (normalRun > 0) {
                            airSlideNormal = scratch->normal;
                            work = (work - (scratch->dest.y + airClearance)) * airSlideNormal->vy / normalRun;
                            fighter->anchor.vx += airSlideNormal->vx * work / normalRun;
                            fighter->anchor.vz += scratch->normal->vz * work / normalRun;
                        }
                    } else {
                        obstructedAirborne = 1;
                    }
                    scratch->dest.x = fighter->anchor.vx + fighter->rootOffset.x;
                    scratch->dest.z = fighter->anchor.vz + fighter->rootOffset.z;
                }
            }
            QUERY_FLOOR_BAND(scratch->dest.x, scratch->dest.y - 1000, scratch->dest.z,
                             work, floor, ceiling);
            if (work == FLOOR_HEIGHT_NONE) {
                SnapFighterToNearestFloor(fighter);
            } else if (fighter->jumpVel.y > 0 &&
                       scratch->normal->vy < FLOOR_WALKABLE_NORMAL_Y &&
                       scratch->dest.y + airClearance >= work) {
                TriggerFighterRumble(fighter, RUMBLE_LANDING);
                fighter->position.vx = scratch->dest.x;
                landZ = scratch->dest.z;
                fighter->position.vy = work - airClearance;
                fighter->onSlopeBand = ceiling != floor;
                fighter->position.vz = landZ;
                fighter->jumpState = 0;
                fighter->anchor.vy = work;
                fighter->groundY = work;
                if (fighter->action == FIGHTER_ACTION_JUMP || fighter->isHomingJump != 0) {
                    fighter->animFrame = fighter->placement->frameLimit - 1;
                    fighter->animFrac = ONE - 1;
                }
            } else {
                fighter->position.vx = scratch->dest.x;
                fighter->position.vz = scratch->dest.z;
                if (obstructedAirborne == 0) {
                    fighter->position.vy = work + fighter->rootOffset.y;
                } else {
                    fighter->position.vy = scratch->dest.y;
                }
            }
        } else {
            *(WorldVector *)&fighter->position = scratch->dest;
        }
        if (fighter->animFrame == fighter->placement->frameLimit - 1) {
            if (fighter->jumpState != 0) {
                fighter->animFrame--;
            }
        }
        fighter->shadowGroundY = fighter->groundY;
    } else {
        if (fighter->climbState != 0) {
            fighter->surfaceFlags = GetLastFloorAttr();
            fighter->position.vy = scratch->dest.y;
            if (work < scratch->dest.y + fighter->statureScale * 500 / ONE) {
                fighter->anchor.vx -= scratch->dest.x - fighter->position.vx;
                fighter->anchor.vz -= scratch->dest.z - fighter->position.vz;
            } else {
                fighter->position.vx = scratch->dest.x;
                fighter->position.vz = scratch->dest.z;
            }
            if (fighter->action != FIGHTER_ACTION_CLIMB) {
                if (work == fighter->groundY - FLOOR_BAND_HEIGHT) {
                    fighter->groundY = work;
                    fighter->climbState = 0;
                    fighter->flipHeadingNextTick = 1;
                } else {
                    fighter->climbState = 0;
                    fighter->flipHeadingNextTick = 1;
                    fighter->fallGravityTicks = 0;
                    fighter->fallState = 1;
                }
            } else if (fighter->climbState >= 2 && work == fighter->groundY) {
                StartFighterFallFromLedge(fighter);
            } else if (fighter->animFrame == fighter->placement->frameLimit - 1) {
                fighter->animFrac = ONE - 1;
                if (++fighter->climbState == 4) {
                    QUERY_FLOOR_BAND(fighter->position.vx, fighter->position.vy - 1000, fighter->position.vz,
                                     work, floor, ceiling);
                    ledgeTop = fighter->groundY - FLOOR_BAND_HEIGHT;
                    if (work == ledgeTop) {
                        fighter->climbState = 0;
                        fighter->flipHeadingNextTick = 1;
                        fighter->groundY = floor;
                    } else if (ledgeTop < work) {
                        fighter->groundY = ledgeTop;
                        StartFighterFallFromLedge(fighter);
                    } else if (work == FLOOR_HEIGHT_NONE) {
                        fighter->climbState = 0;
                        fighter->flipHeadingNextTick = 1;

                        QUERY_FLOOR_BAND(fighter->position.vx, fighter->position.vy - 1000, fighter->position.vz,
                                         work, floor, ceiling);
                        fighter->groundY = floor;
                    } else {
                        do {
                                ledgeNormal = scratch->normal;
                                if (ledgeNormal->vy >= -4095) {
                                    ledgeRunSq = ledgeNormal->vx * ledgeNormal->vx + ledgeNormal->vz * ledgeNormal->vz;
                                    if ((unsigned int)ledgeRunSq < 1024) {
                                        normalRun = (unsigned int)gSqrtLutQ3[ledgeRunSq] >> 3;
                                    } else {
                                        unsigned int shift;
                                        gte_ldlzc(ledgeRunSq);
                                        gte_nop();
                                        gte_nop();
                                        gte_stlzc(&lzc.vy);
                                        shift = lzc.vy;
                                        shift = 22 - (shift & ~1);
                                        ledgeRunSq = gSqrtLutQ3[(unsigned int)ledgeRunSq >> shift];
                                        normalRun = ((unsigned int)ledgeRunSq << 16) >> (19 - (shift >> 1));
                                    }
                                    if (normalRun > 0) {
                                        ledgePushNormal = scratch->normal;
                                        work = (work - (fighter->groundY - FLOOR_BAND_HEIGHT)) * ledgePushNormal->vy /
                                            normalRun;
                                        fighter->position.vx += ledgePushNormal->vx * work / normalRun;
                                        fighter->position.vz += scratch->normal->vz * work / normalRun;
                                    }
                                }
                            QUERY_FLOOR_BAND(fighter->position.vx, fighter->position.vy - 1000, fighter->position.vz,
                                             work, floor, ceiling);
                            if (work == FLOOR_HEIGHT_NONE) {
                                SnapFighterToNearestFloor(fighter);
                                work = fighter->anchor.vy;
                                break;
                            }
                        } while (work < fighter->groundY - FLOOR_BAND_HEIGHT);
                        if (work == fighter->groundY - FLOOR_BAND_HEIGHT) {
                            fighter->flipHeadingNextTick = 1;
                            fighter->climbState = 0;
                            fighter->groundY = floor;
                            fighter->anchor.vx = fighter->position.vx - fighter->rootOffset.x;
                            fighter->anchor.vz = fighter->position.vz - fighter->rootOffset.z;
                        } else {
                            StartFighterFallFromLedge(fighter);
                        }
                    }
                }
            }
            fighter->shadowGroundY = ceiling;
            fighter->savedAngles.heading = fighter->angles.heading;
        } else {
            fallState = fighter->fallState;
            if (fallState != 0) {
                fallStature = fighter->statureScale;
                if (fallState == 1) {
                    fallClearance = fallStature * 175 >> 10;
                } else {
                    fallClearance = fallStature * 25 >> 9;
                }
                if (scratch->dest.y + fallClearance >= work) {
                    obstructedStep = 0;
                    if (work == FLOOR_HEIGHT_NONE || work == -30000) {
                        fighter->anchor.vx -= scratch->dest.x - fighter->position.vx;
                        fighter->anchor.vz -= scratch->dest.z - fighter->position.vz;

                        work = QueryFloorHeight(fighter->position.vx, fighter->position.vy - 1000,
                                                fighter->position.vz);
                        if (work == FLOOR_QUERY_MISS) {
                            work = FLOOR_HEIGHT_NONE;
                        } else {
                            work &= -8;
                        }
                        scratch->dest.x = fighter->position.vx;
                        scratch->dest.z = fighter->position.vz;
                        if (scratch->dest.y + fallClearance < work) {
                            obstructedStep = 1;
                        }
                    }
                    if (obstructedStep == 0) {
                        fallNormal = scratch->normal;
                        fallNormalY = fallNormal->vy;
                        if (fallNormalY >= -4095) {
                            if (fallNormalY >= FLOOR_WALKABLE_NORMAL_Y) {
                                fallNormalX = fallNormal->vx;
                                fallNormalZ = fallNormal->vz;
                                fallRunSq = fallNormalX * fallNormalX + fallNormalZ * fallNormalZ;
                                if ((unsigned int)fallRunSq < 1024) {
                                    normalRun = (unsigned int)gSqrtLutQ3[fallRunSq] >> 3;
                                } else {
                                    unsigned int shift;
                                    gte_ldlzc(fallRunSq);
                                    gte_nop();
                                    gte_nop();
                                    gte_stlzc(&lzc.vz);
                                    shift = lzc.vz;
                                    shift = 22 - (shift & ~1);
                                    fallRunSq = gSqrtLutQ3[(unsigned int)fallRunSq >> shift];
                                    normalRun = ((unsigned int)fallRunSq << 16) >> (19 - (shift >> 1));
                                }
                                if (normalRun > 0) {
                                    fallSlideNormal = scratch->normal;
                                    work = (work - (scratch->dest.y + fallClearance)) * fallSlideNormal->vy / normalRun;
                                    fighter->anchor.vx += fallSlideNormal->vx * work / normalRun;
                                    fighter->anchor.vz += scratch->normal->vz * work / normalRun;
                                }
                            } else {
                                obstructedStep = 1;
                            }
                            scratch->dest.x = fighter->anchor.vx + fighter->rootOffset.x;
                            scratch->dest.z = fighter->anchor.vz + fighter->rootOffset.z;
                        }
                    }
                    QUERY_FLOOR_BAND(scratch->dest.x, scratch->dest.y - 1000, scratch->dest.z,
                                     work, floor, ceiling);
                    if (work == FLOOR_HEIGHT_NONE) {
                        SnapFighterToNearestFloor(fighter);
                    } else {
                        fallLandNormalY = scratch->normal->vy;
                        if (fallLandNormalY < FLOOR_WALKABLE_NORMAL_Y && scratch->dest.y + fallClearance >= work) {
                            TriggerFighterRumble(fighter, RUMBLE_LANDING);
                            if (fighter->fallState == 2) {
                                wounds = (unsigned short)fighter->torsoWoundCount;
                                fighter->torsoWoundCount = wounds + 1;
                                if (fighter->groundY + FLOOR_BAND_HEIGHT < work) {
                                    fighter->torsoWoundCount = wounds + 2;
                                }
                            }
                            fighter->fallState = 0;
                            fighter->position.vx = scratch->dest.x;
                            fallLandZ = scratch->dest.z;
                            fighter->position.vy = work - fallClearance;
                            fighter->onSlopeBand = ceiling != floor;
                            fighter->position.vz = fallLandZ;
                            fighter->groundY = work;
                            fighter->anchor.vy = work;
                            if (fighter->action == FIGHTER_ACTION_FALL) {
                                fighter->animFrame = fighter->placement->frameLimit - 1;
                                fighter->animFrac = ONE - 1;
                            }
                        } else {
                            fighter->position.vx = scratch->dest.x;
                            fighter->position.vz = scratch->dest.z;
                            if (obstructedStep == 0) {
                                fighter->groundY = work;
                                fighter->position.vy = work + fighter->rootOffset.y;
                            } else {
                                fighter->position.vy = scratch->dest.y;
                            }
                        }
                    }
                } else {
                    *(WorldVector *)&fighter->position = scratch->dest;
                    fighter->groundY = work;
                }
                if (fighter->fallState != 0) {
                    if (fighter->animFrame == fighter->placement->frameLimit - 1) {
                        fighter->animFrame--;
                        fighter->velocity.y += 500;
                    } else if (fighter->action != FIGHTER_ACTION_FALL) {
                        fighter->fallGravityTicks++;
                        fighter->velocity.y += fighter->fallGravityTicks * 52;
                    }
                }
                fighter->shadowGroundY = work;
            } else {
                ResolveFighterFloorMovement(fighter, scratch, work, floor, ceiling);
            }
        }
    }
    fighter->cameraTargetPos.x = fighter->position.vx;
    fighter->cameraTargetPos.z = fighter->position.vz;
    value = -fighter->statureScale * 1152 / ONE;
    if (fighter->position.vy - value < fighter->groundY) {
        fighter->cameraTargetPos.y += (fighter->position.vy - fighter->cameraTargetPos.y) / 2;
    } else {
        work = fighter->groundY + value;
        fighter->cameraTargetPos.y += (work - fighter->cameraTargetPos.y) / 8;
    }
    if (fighter->poseBlendWeight > 0) {
        BlendPoseJointRotations(player, pose[0].joint, fighter->savedPose.joint, fighter->poseBlendWeight,
                                (MATRIX *)&COLLISION_SCRATCH->work);
    } else {
        BlendPoseJointRotations(player, pose[0].joint, pose[1].joint, fighter->animFrac,
                                (MATRIX *)&COLLISION_SCRATCH->work);
    }
    if (gGameState == GAME_STATE_UPDATE_APPROACH_CUTSCENE && player == 1) {
        cutscenePos.vx = fighter->position.vx;
        cutscenePos.vy = fighter->position.vy + 4000;
        cutscenePos.vz = fighter->position.vz;
        UpdateAndEnqueueFighterPose(player, FIGHTER_POSE_MODE_PRECOMPUTED, &cutscenePos.vx,
                                    (PoseAngles *)&fighter->angles, 0, fighter->shadowGroundY + FLOOR_BAND_HEIGHT);
    } else {
        UpdateAndEnqueueFighterPose(player, FIGHTER_POSE_MODE_PRECOMPUTED, &fighter->position.vx,
                                    (PoseAngles *)&fighter->angles, 0, fighter->shadowGroundY);
    }
    if (gFighterClothInitialized == 0) {
        gFighterClothInitialized = 1;
        FreeAllFighterCloth();
        SetFighterClothDirty(0, 1);
        SetFighterClothDirty(1, 1);
    }
    UpdateFighterHitGeometry(fighter, COLLISION_SCRATCH->bodyPartPos[player], COLLISION_SCRATCH->weaponPoints[player]);
    fighter->headPos = COLLISION_SCRATCH->bodyPartPos[player][0];
    if (fighter->animFrameStalled == 0 && fighter->action == FIGHTER_ACTION_THROW) {
        throwInfo = fighter->animInfo;
        if (fighter->animFrame == throwInfo->activeStartFrame) {
            if (fighter->leftArmIntact != 0) {
                if (fighter->fighterId < FIGHTER_PLAYABLE_ID_COUNT) {
                    if ((fighter->placement->flags & MOTION_FLAG_SURFACE_SUBWEAPON) != 0) {
                        work = GetStageSurfaceParticleType(fighter->surfaceFlags & FLOOR_ATTR_SURFACE_MASK);
                        if (work != 0) {
                            ThrowFighterSubWeapon(fighter, work);
                        }
                    } else {
                        ThrowFighterSubWeapon(fighter, 0);
                    }
                }
            }
        }
    }
    if (fighter->animFrameStalled == 0) {
        if (fighter->action == FIGHTER_ACTION_PICKUP && fighter->animFrame == fighter->animInfo->activeStartFrame ||
            fighter->action == FIGHTER_ACTION_DOWNED ||
            fighter->action == FIGHTER_ACTION_CROUCH) {
            PickupNearbySubWeapons(fighter);
        }
    }
    if (fighter->action == FIGHTER_ACTION_DEAD || fighter->action == FIGHTER_ACTION_DEFEATED) {
        fighter->velocityRetention = 0;
    } else if (fighter->action == FIGHTER_ACTION_CLIMB) {
        fighter->velocityRetention = 0x800;
    } else {
        if (fighter->groundY < fighter->rightFootPos.y + 100) {
            work = 0x600;
        } else {
            work = 0x800;
        }
        if (fighter->groundY < fighter->leftFootPos.y + 100) {
            work -= 0x200;
        }
        work = work * (1400 - fighter->tractionWeight) * 3 / 1400;
        if (work < 0x600) {
            work = 0x600;
        } else if (work > 0xc00) {
            work = 0xc00;
        }
        fighter->velocityRetention = work;
    }
    /* nextFrame is unset on the blend path, where RecordFighterReplayFrame records lastSampledFrame instead. */
    RecordFighterReplayFrame(player, nextFrame);
    if (gCurrentStageId == STAGE_ID_DOZAEMON_MOAT) {
        if (prevPos.vy < 700 && fighter->position.vy >= 700) {
            EmitCombatEvent(player, COMBAT_EVENT_WATER_ENTRY, &fighter->position);
        }
        if (fighter->position.vy >= 700) {
            waterDrag = gMoatWaterDrag;
            fighter->velocity.x -= (fighter->position.vx - prevPos.vx) * waterDrag >> 12;
            fighter->velocity.z -= (fighter->position.vz - prevPos.vz) * waterDrag >> 12;
        }
    }
    if (fighter->poseBlendWeight > 0) {
        fighter->poseBlendWeight -= fighter->poseBlendStep;
        if (fighter->poseBlendWeight < 0) {
            fighter->poseBlendWeight = 0;
        }
    }
    attackWindow = 0;
    if (fighter->hitActive != 0 && fighter->action == FIGHTER_ACTION_ATTACK) {
        hitFrame = fighter->animFrame;
        if (hitFrame >= fighter->animInfo->activeStartFrame) {
            attackWindow = hitFrame <= fighter->animInfo->activeEndFrame;
        }
    }
    fighter->attackActive = attackWindow;
    clashPower = fighter->baseClashPower;
    fighter->clashPower = clashPower;
    if (fighter->action == FIGHTER_ACTION_ATTACK) {
        if (fighter->prevAction == FIGHTER_ACTION_NEUTRAL &&
            (fighter->prevAnimInfo->flags & ANIM_FLAG_STANCE_MASK) == 0) {
            fighter->clashPower = clashPower + 0x51e;
        }
    }
    if (fighter->weaponId == WEAPON_ID_GUN) {
        fighter->weaponTipVel.vx = COLLISION_SCRATCH->weaponPoints[player][1].x -
            COLLISION_SCRATCH->weaponPoints[player][0].x;
        fighter->weaponTipVel.vy = COLLISION_SCRATCH->weaponPoints[player][1].y -
            COLLISION_SCRATCH->weaponPoints[player][0].y;
        fighter->weaponTipVel.vz = COLLISION_SCRATCH->weaponPoints[player][1].z -
            COLLISION_SCRATCH->weaponPoints[player][0].z;
    } else {
        fighter->weaponTipVel.vx = COLLISION_SCRATCH->weaponPoints[player][1].x - fighter->prevWeaponPoints[1].x;
        fighter->weaponTipVel.vy = COLLISION_SCRATCH->weaponPoints[player][1].y - fighter->prevWeaponPoints[1].y;
        fighter->weaponTipVel.vz = COLLISION_SCRATCH->weaponPoints[player][1].z - fighter->prevWeaponPoints[1].z;
    }
    fighter->isVerticalSwing = IsWeaponSwingPlaneVertical(fighter);
    fighter->isSupine = IsFighterSupine(fighter);
    if (fighter->pitFallTimer != 0) {
        fighter->pitFallTimer++;
    } else if (gCurrentStageId == 4) {
        if (fighter->position.vy >= 4000) {
            fighter->pitFallTimer = 1;
        }
    } else if (gCurrentStageId == 9) {
        if (fighter->position.vy >= 3000) {
            fighter->pitFallTimer = 1;
        }
    } else if (gCurrentStageId == 10) {
        if (fighter->position.vy >= -2000) {
            fighter->pitFallTimer = 1;
        }
    }
    fighter->frameDelta.vx = fighter->position.vx - prevPos.vx;
    fighter->frameDelta.vy = fighter->position.vy - prevPos.vy;
    fighter->frameDelta.vz = fighter->position.vz - prevPos.vz;
    if (fighter->animFrameStalled == 0) {
        ProcessAnimFrameEvents(player, fighter->placement, fighter->animFrame, &fighter->position,
                               fighter->surfaceFlags & FLOOR_ATTR_SURFACE_MASK, fighter->groundY);
    }
    if (gGameState == GAME_STATE_UPDATE_COMBAT) {
        if (fighter->rumbleTimer > 0) {
            fighter->rumbleTimer--;
        }
        gRumblePulses[player].dutyStep = (fighter->isCpu == 0 && fighter->rumbleTimer != 0)
            ? fighter->rumbleIntensity : 0;
    } else {
        gRumblePulses[player].dutyStep = 0;
    }
    moveX = fighter->frameDelta.vx;
    moveY = fighter->frameDelta.vy;
    moveZ = fighter->frameDelta.vz;
    moveSq = moveX * moveX + moveY * moveY + moveZ * moveZ;
    if ((unsigned int)moveSq < 1024) {
        work = (unsigned int)gSqrtLutQ3[moveSq] >> 3;
    } else {
        unsigned int shift;
        gte_ldlzc(moveSq);
        gte_nop();
        gte_nop();
        gte_stlzc(&moveLzc);
        shift = moveLzc;
        shift = 22 - (shift & ~1);
        moveSq = gSqrtLutQ3[(unsigned int)moveSq >> shift];
        work = ((unsigned int)moveSq << 16) >> (19 - (shift >> 1));
    }
    stationary = 0;
    if (work < 100) {
        if (fighter->action == FIGHTER_ACTION_NEUTRAL ||
            fighter->action == FIGHTER_ACTION_CROUCH ||
            fighter->action == FIGHTER_ACTION_DOWNED) {
            stationary = 1;
        }
    }
    fighter->isStationary = stationary;
    if (fighter->faceExpression == FIGHTER_FACE_LOCKED_NEUTRAL) {
        SetFighterFacialExpression(player, fighter->fighterId, FIGHTER_FACE_NORMAL);
    } else {
        if (fighter->action == FIGHTER_ACTION_DEAD) {
            fighter->faceExpression = FIGHTER_FACE_DEAD;
        } else if (fighter->action == FIGHTER_ACTION_NEUTRAL ||
                   fighter->action == FIGHTER_ACTION_CROUCH ||
                   fighter->action == FIGHTER_ACTION_DOWNED ||
                   fighter->action == FIGHTER_ACTION_BLEND) {
            fighter->faceExpression = FIGHTER_FACE_NORMAL;
        }
        SetFighterFacialExpression(player, fighter->fighterId, fighter->faceExpression);
    }
}

/* Rumble presets (duration in frames, Q12 PWM duty step); indices 1..5 also drive gOpponentRumbleTable. */
RumblePreset gPrimaryRumbleTable[12] = {
    { 0,  0x0000 }, /* none */
    { 10, 0x1000 }, /* RUMBLE_FATAL_HIT */
    { 6,  0x0c00 }, /* RUMBLE_WOUND */
    { 3,  0x0800 }, /* RUMBLE_BLADE_CLASH */
    { 4,  0x0400 }, /* RUMBLE_GUARD_PARRY */
    { 6,  0x0a00 }, /* RUMBLE_GUARD_BREAK */
    { 4,  0x0400 }, /* RUMBLE_LANDING */
    { 4,  0x0600 }, /* RUMBLE_BODY_BUMP */
    { 6,  0x0200 }, /* RUMBLE_DEBRIS_HIT */
    { 2,  0x1000 }, /* RUMBLE_GUNSHOT */
    { 10, 0x0800 }, /* RUMBLE_PROP_CUT */
    { 3,  0x0800 }, /* RUMBLE_WEAPON_ENV_HIT */
};
RumblePreset gOpponentRumbleTable[RUMBLE_OPPONENT_PATTERN_COUNT] = {
    { 0, 0x0000 }, /* none */
    { 8, 0x0c00 }, /* RUMBLE_FATAL_HIT */
    { 4, 0x0800 }, /* RUMBLE_WOUND */
    { 4, 0x0200 }, /* RUMBLE_BLADE_CLASH */
    { 4, 0x0400 }, /* RUMBLE_GUARD_PARRY */
    { 4, 0x0600 }, /* RUMBLE_GUARD_BREAK */
};

void TriggerFighterRumble(Fighter *fighter, int index)
{
    RumblePreset *entry;

    entry = &gPrimaryRumbleTable[index];
    fighter->rumbleTimer = entry->duration;
    fighter->rumbleIntensity = entry->intensity;

    if (index < RUMBLE_OPPONENT_PATTERN_COUNT && fighter->opponent->weaponId != WEAPON_ID_GUN) {
        entry = &gOpponentRumbleTable[index];
        fighter->opponent->rumbleTimer = entry->duration;
        fighter->opponent->rumbleIntensity = entry->intensity;
    }
}

void UpdateFighterLookAngles(Fighter *fighter)
{
    Fighter *opponent;
    int angle;
    int baseYaw;
    int dx, dz;
    unsigned int squared;
    int distance;
    int leading;

    opponent = fighter->opponent;
    if (fighter->weaponId != WEAPON_ID_GUN &&
        (fighter->action == FIGHTER_ACTION_NEUTRAL || fighter->action == FIGHTER_ACTION_CROUCH)) {
        angle = (fighter->angles.heading - fighter->angleToOpponent) & ANGLE_MASK;
        if (angle >= ANGLE_HALF) {
            angle -= ANGLE_FULL;
        }
        angle /= 2;
        if (angle > 512) {
            angle = 512;
        } else if (angle < -512) {
            angle = -512;
        }
        dx = opponent->headPos.x - fighter->headPos.x;
        dz = opponent->headPos.z - fighter->headPos.z;
        baseYaw = angle;
        if ((unsigned int)(dx + 16384) > 32768 || (unsigned int)(dz + 16384) > 32768) {
            angle = 0;
        } else {
            squared = dx * dx + dz * dz;
            if ((unsigned int)squared < 1024) {
                distance = (unsigned int)gSqrtLutQ3[squared] >> 3;
            } else {
                unsigned int shift;
                gte_ldlzc(squared);
                gte_nop();
                gte_nop();
                gte_stlzc(&leading);
                shift = leading;
                shift = 22 - (shift & ~1);
                squared = gSqrtLutQ3[(unsigned int)squared >> shift];
                distance = ((unsigned int)squared << 16) >> (19 - (shift >> 1));
            }
            angle = -ratan2(opponent->headPos.y - fighter->headPos.y, distance) & ANGLE_MASK;
        }
        if (angle >= ANGLE_HALF) {
            angle -= ANGLE_FULL;
        }
        dx = (fighter->angles.heading - fighter->angleToOpponent) & ANGLE_MASK;
        if (dx >= ANGLE_HALF) {
            dx = ANGLE_FULL - dx;
        }
        angle = angle * (ANGLE_HALF - dx) >> 11;
        if (angle > 640) {
            angle = 640;
        } else if (angle < -64) {
            angle = -64;
        }
        dx = angle;
        if (fighter->weaponId == WEAPON_ID_RAPIER) {
            if (fighter->action == FIGHTER_ACTION_NEUTRAL && (fighter->animInfo->flags & ANIM_FLAG_RUN) == 0 &&
                (fighter->animInfo->flags & ANIM_FLAG_STANCE_MASK) == 2) {
                int targetRoll;
                int targetYaw;
                int targetPitch;

                targetRoll = 512 - angle / 2;
                targetYaw = baseYaw + angle;
                targetPitch = angle - 512;
                fighter->lookRoll += (targetRoll - fighter->lookRoll) / 4;
                fighter->lookYaw += (targetYaw - fighter->lookYaw) / 4;
                fighter->lookPitch += (targetPitch - fighter->lookPitch) / 4;
            } else {
                fighter->lookRoll = 0;
                fighter->lookYaw += (baseYaw + dx - fighter->lookYaw) / 4;
                fighter->lookPitch += (dx - fighter->lookPitch) / 4;
            }
        } else {
            fighter->lookRoll = 0;
            fighter->lookYaw += (baseYaw - fighter->lookYaw) / 4;
            fighter->lookPitch += (angle - fighter->lookPitch) / 4;
        }
    } else {
        fighter->lookPitch = 0;
        fighter->lookYaw = 0;
        fighter->lookRoll = 0;
    }
}

void FlipPoseForClimbTransition(PoseAngles *joints)
{
    joints[POSE_JOINT_PELVIS].x += ANGLE_HALF;
    joints[POSE_JOINT_TORSO].x += ANGLE_HALF;
    joints[POSE_JOINT_PELVIS].y = -joints[POSE_JOINT_PELVIS].y;
    joints[POSE_JOINT_TORSO].y = -joints[POSE_JOINT_TORSO].y;
}

void FlipPoseFromSupineState(PoseAngles *joints)
{
    joints[POSE_JOINT_PELVIS].y = -joints[POSE_JOINT_PELVIS].y;
    joints[POSE_JOINT_TORSO].y = -joints[POSE_JOINT_TORSO].y;
    joints[POSE_JOINT_TORSO].x += ANGLE_HALF;
    joints[POSE_JOINT_PELVIS].z = -joints[POSE_JOINT_PELVIS].z;
}

void FlipPoseFromProneState(PoseAngles *joints)
{
    joints[POSE_JOINT_PELVIS].y += ANGLE_HALF;
    joints[POSE_JOINT_TORSO].y += ANGLE_HALF;
    joints[POSE_JOINT_PELVIS].z = -joints[POSE_JOINT_PELVIS].z;
    joints[POSE_JOINT_TORSO].z = -joints[POSE_JOINT_TORSO].z;
}

void ApplyDisabledLeftArmPose(PoseAngles *joints)
{
    joints[POSE_JOINT_LEFT_ARM].x = 0x3f5;
    joints[POSE_JOINT_LEFT_ARM].y = 0x2b6;
    joints[POSE_JOINT_LEFT_ARM].z = 0x77a;
    joints[POSE_JOINT_LEFT_ARM + 1].x = 0xbee;
    joints[POSE_JOINT_LEFT_ARM + 1].y = 0x8c;
    joints[POSE_JOINT_LEFT_ARM + 1].z = 0x227;
    joints[POSE_JOINT_LEFT_ARM + 3].x = 0x8a0;
    joints[POSE_JOINT_LEFT_ARM + 3].y = 0xf0e;
    joints[POSE_JOINT_LEFT_ARM + 2].x = 0;
    joints[POSE_JOINT_LEFT_ARM + 2].y = 0;
    joints[POSE_JOINT_LEFT_ARM + 2].z = 0;
    joints[POSE_JOINT_LEFT_ARM + 3].z = 0xbcd;
}
