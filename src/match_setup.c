/* Match setup and per-frame match bookkeeping: weapon and character pair slots, the weapon-pair
 * motion file, the combat stage load, fighter distance, the Story honor window and stage events. */

#include <sys/types.h>
#include <libgte.h>
#include "cd.h"
#include "combat_event.h"
#include "world_vector.h"
#include "fighter.h"
#include "floor_collision.h"
#include "game_state.h"
#include "motion_file.h"
#include "stage_prop_hit.h"
#include "angle.h"
#include "stage_id.h"
#include "weapon_id.h"
#include "fighter_texture.h"
#include "match_setup.h"
#include "model_arena.h"
#include "stage_cull.h"
#include "stage_env.h"
#include "globals.h"

#define WEAPON_PAIR_MOTION_NONE 255

#define MAX_FIGHTER_DISTANCE    18316


/* gFighterAltTextureLoaded[side] state values. */
enum FighterAltTextureState {
    ALT_TEXTURE_NONE = 0,
    ALT_TEXTURE_LOADED = 1,
    ALT_TEXTURE_RELOAD = 2
};

void AssignFighterPairSlots(void)
{
    int first = gFighters[0].weaponId;
    int second = gFighters[1].weaponId;

    if (first == 9) {
        first = WEAPON_ID_NAGINATA;
    }
    if (second == 9) {
        second = WEAPON_ID_NAGINATA;
    }
    if (first <= second) {
        gFighters[0].weaponPairSlot = 0;
        gFighters[1].weaponPairSlot = 1;
    } else {
        gFighters[0].weaponPairSlot = 1;
        gFighters[1].weaponPairSlot = 0;
    }

    first = gFighters[0].characterClass;
    second = gFighters[1].characterClass;
    if (first <= second) {
        gFighters[0].characterPairSlot = 0;
        gFighters[1].characterPairSlot = 1;
    } else {
        gFighters[0].characterPairSlot = 1;
        gFighters[1].characterPairSlot = 0;
    }
}

/* Symmetric lookup table of CD_CATEGORY_MOTION file indices by [P2 weapon][P1 weapon].
 * Weapon 9 takes the naginata's row and column; the gun against the gun has none (255). */
unsigned char gWeaponPairMotionFiles[WEAPON_ID_COUNT][WEAPON_ID_COUNT] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 36, 4 },
    { 1, 8, 9, 10, 11, 12, 13, 14, 37, 11 },
    { 2, 9, 15, 16, 17, 18, 19, 20, 38, 17 },
    { 3, 10, 16, 21, 22, 23, 24, 25, 39, 22 },
    { 4, 11, 17, 22, 26, 27, 28, 29, 40, 26 },
    { 5, 12, 18, 23, 27, 30, 31, 32, 41, 27 },
    { 6, 13, 19, 24, 28, 31, 33, 34, 42, 28 },
    { 7, 14, 20, 25, 29, 32, 34, 35, 43, 29 },
    { 36, 37, 38, 39, 40, 41, 42, 43, WEAPON_PAIR_MOTION_NONE, 40 },
    { 4, 11, 17, 22, 26, 27, 28, 29, 40, 26 },
};

void LoadWeaponPairMotions(void)
{
    int column = gFighters[0].weaponId;
    int row = gFighters[1].weaponId;
    int handle;

    if (column == 9)
        column = WEAPON_ID_NAGINATA;
    if (row == 9)
        row = WEAPON_ID_NAGINATA;

    handle = ResolveCdFileHandle(CD_CATEGORY_MOTION, gWeaponPairMotionFiles[row][column]);
    StartCdFileRead(handle, MOTION_FILE);
    WaitCdIdle();
}

int PollCdReady(void)
{
    UpdateCdSubsystem();
    return IsCdIdle();
}

void LoadCombatStage(int stageId)
{
    StagePropMarker *propMarkers;

    /* On a Continue, stages 3 and 14 (cuttable props) are loaded afresh. */
    if (gIsContinueRestart != 0 && (stageId == 3 || stageId == STAGE_ID_BAMBOO_THICKET)) {
        InvalidateStageArenaSlot();
        StopAndWaitCdIdle();
    }
    LoadStageAndLighting(stageId);
    gStageCameraWalls = GetStageCameraWalls();
    propMarkers = GetStagePropMarkers();
    InitStagePropMarkers(propMarkers);
    gMaxFighterDistSq = MAX_FIGHTER_DISTANCE * MAX_FIGHTER_DISTANCE;
    gStage4LowerLevelActive = 0;
    gStage10LowerLevelActive = 0;
}

static int CalcFighterDistanceXZ(void)
{
    int dx = gFighters[1].position.vx - gFighters[0].position.vx;
    int dz = gFighters[1].position.vz - gFighters[0].position.vz;
    int scale = 0;

    /* Keeps dx * dx + dz * dz within an int. */
    while (dx < -0x4000 || dx > 0x4000 ||
           dz < -0x4000 || dz > 0x4000) {
        dx /= 2;
        dz /= 2;
        ++scale;
    }
    return SquareRoot0(dx * dx + dz * dz) << scale;
}

void UpdateStoryHonorWindow(void)
{
    int facingDifference;
    int bearingDifference;

    if (gPlayMode != PLAY_MODE_STORY)
        return;
    if (gFighters[1].action == FIGHTER_ACTION_NEUTRAL && gFighters[1].animInfo->activeStartFrame == 1)
        return;
    if (gFighters[0].action == FIGHTER_ACTION_ATTACK || gStoryHonorWindowActive == 0 ||
        gFighters[1].isCpu == 0 || gStoryHonorWindowTimer <= 0)
        return;

    if (gFighters[1].action == FIGHTER_ACTION_ATTACK)
        gStoryHonorWindowTimer = 1;

    /* Keep the window open while P1 faces the same way as the CPU from behind. */
    facingDifference = (gFighters[1].angles.heading - gFighters[0].angles.heading) & ANGLE_MASK;
    if (facingDifference >= ANGLE_HALF)
        facingDifference = ANGLE_FULL - facingDifference;
    if (facingDifference < ANGLE_QUARTER) {
        bearingDifference = (ratan2(gFighters[0].position.vx - gFighters[1].position.vx,
                                   gFighters[0].position.vz - gFighters[1].position.vz) -
                             gFighters[1].angles.heading) & ANGLE_MASK;
        if (bearingDifference >= ANGLE_HALF)
            bearingDifference = ANGLE_FULL - bearingDifference;
        if (bearingDifference > ANGLE_QUARTER)
            ++gStoryHonorWindowTimer;
    }

    if (--gStoryHonorWindowTimer == 0)
        EmitCombatEvent(1, COMBAT_EVENT_STORY_READY_VOICE, &gFighters[1].position);
}

void UpdateStageEvents(void)
{
    int player;

    if (gFighterAltTextureLoaded[0] == ALT_TEXTURE_RELOAD) {
        LoadFighterAltTextures(0);
        gFighterAltTextureLoaded[0] = ALT_TEXTURE_LOADED;
    }
    if (gCurrentStageId == 4) {
        if (gStage4LowerLevelActive == 0 &&
            gFighters[0].groundY >= 3 * FLOOR_BAND_HEIGHT &&
            gFighters[1].groundY >= 3 * FLOOR_BAND_HEIGHT) {
            gStage4LowerLevelActive = 1;
            HideStageTilesBeyondZ();
            InvalidateStageArenaSlot();
        }
        for (player = 0; player < 2; player++) {
            if (gFighterAltTextureLoaded[player] == ALT_TEXTURE_NONE &&
                (gFighters[player].surfaceFlags & FLOOR_ATTR_SURFACE_MASK) == FLOOR_SURFACE_SNOW &&
                gFighters[player].fallState == 0 &&
                gFighters[player].jumpState == 0 &&
                gFighters[player].climbState == 0) {
                LoadFighterAltTextures(player);
                gFighterAltTextureLoaded[player] = ALT_TEXTURE_LOADED;
            }
        }
    }
    if (gCurrentStageId == 10 && gStage10LowerLevelActive == 0 &&
        gFighters[0].groundY >= FLOOR_BAND_HEIGHT &&
        gFighters[1].groundY >= FLOOR_BAND_HEIGHT) {
        gStage10LowerLevelActive = 1;
    }
}

void UpdateCombatDistanceAndKoTimer(void)
{
    int prevDistance = gFighterDistanceXZ;
    int distance = CalcFighterDistanceXZ();

    gFighterDistanceXZ = distance;
    gFighterDistanceDelta = distance - prevDistance;

    if (gFighters[0].action != FIGHTER_ACTION_DEAD &&
        gFighters[0].action != FIGHTER_ACTION_FATAL_KO &&
        gFighters[0].action != FIGHTER_ACTION_DEFEATED &&
        !(gFighters[0].placement->flags & MOTION_FLAG_FATAL) &&
        gFighters[1].action != FIGHTER_ACTION_DEAD &&
        gFighters[1].action != FIGHTER_ACTION_FATAL_KO &&
        gFighters[1].action != FIGHTER_ACTION_DEFEATED &&
        !(gFighters[1].placement->flags & MOTION_FLAG_FATAL)) {
        return;
    }

    /* While exactly one fighter is defeated, the timer advances every other frame. */
    if (((gFighters[0].action == FIGHTER_ACTION_DEFEATED) ^
         (gFighters[1].action == FIGHTER_ACTION_DEFEATED)) &&
        !(gFighters[0].activeFrames & 1)) {
        return;
    }
    ++gPostKoTimer;
}
