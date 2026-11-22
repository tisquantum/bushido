/* CPU fighter AI controller: per-character tendencies, input-macro bytecode interpreter,
 * reactive priority overrides, and the 30-state gAiStateHandlers behavior machine. */

#include <sys/types.h>
#include <libgte.h>
#include <inline_o.h>
#include <rand.h>

#include "pad.h"
#include "collision_scratch.h"
#include "fighter_ai.h"
#include "world_vector.h"
#include "fighter.h"
#include "game_state.h"
#include "motion_file.h"
#include "floor_collision.h"
#include "angle.h"
#include "cloth.h"
#include "combat_hit.h"
#include "scene_math.h"
#include "globals.h"
#include "combat_state.h"
#include "match_setup.h"

void SetAiState(Fighter *fighter, int state, int delay)
{
    fighter->aiScriptPtr = 0;
    fighter->aiState = state;
    fighter->aiStateFrames = 0;
    fighter->aiDelayFrames = delay;
    fighter->aiSubStep = 0;
}

void InitFighterAi(int fighterIndex, int skillRating)
{
    Fighter *fighter;
    int characterClass;
    int classSeed;
    int stageSeed;

    fighter = &gFighters[fighterIndex];
    fighter->aiSkillRating = skillRating;
    fighter->aiAttackTables = 0;
    fighter->aiPrevInput.held = 0;
    fighter->aiPrevInput.pressed = 0;
    fighter->aiPrevInput.released = 0;
    fighter->aiPrevInput.notHeld = (unsigned int)-1;
    fighter->aiDefenseDelay = 0x2000 / fighter->aiSkillRating + 2;
    fighter->aiAttackDelay = 0x2000 / fighter->aiSkillRating + 2;
    SetAiState(fighter, AI_STATE_IDLE, 0);

    characterClass = fighter->characterClass;
    classSeed = characterClass * characterClass * 77;
    stageSeed = gCurrentStageId * 33 + 55;
    fighter->aiForceDefend = 1;
    fighter->aiOscAngle = classSeed + stageSeed;
}

static void LoadAiAttackTables(Fighter *fighter)
{
    int weaponSlot;
    int stance;
    AiAttackChoice **tables;
    int *entry;
    int count;

    weaponSlot = fighter->weaponPairSlot;
    stance = 0;
    tables = MOTION_FILE->aiAttackTables[weaponSlot];
    fighter->aiAttackTables = tables;
    do {
        entry = (int *)fighter->aiAttackTables[stance];
        count = 0;
        if (*entry != 0) {
            do {
                entry += 2;
                ++count;
            } while (*entry != 0);
        }
        fighter->aiAttackCounts[stance] = count;
        ++stance;
    } while (stance < STANCE_COUNT);
    fighter->aiCycleIndex = 0;
}

/* AI input scripts executed by StepAiInputScript (press/release button bit, hold N frames, end). */
unsigned char gAiScriptLeapForward[] = {
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(4),
    AI_SCRIPT_PRESS(AI_BTN_R2), AI_SCRIPT_HOLD(4),
    AI_SCRIPT_PRESS(AI_BTN_R1), AI_SCRIPT_END
};
/* nothing reads it */
unsigned char gAiScriptLeapForwardQuick[] = {
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_PRESS(AI_BTN_R2), AI_SCRIPT_HOLD(4),
    AI_SCRIPT_PRESS(AI_BTN_R1), AI_SCRIPT_END
};
unsigned char gAiScriptBackForwardAttack[] = {
    AI_SCRIPT_PRESS(AI_BTN_BACK), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_RELEASE(AI_BTN_BACK), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_CROSS), AI_SCRIPT_END
};
unsigned char gAiScriptSidestepDown[] = {
    AI_SCRIPT_PRESS(AI_BTN_DOWN), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_RELEASE(AI_BTN_DOWN), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_DOWN), AI_SCRIPT_END
};
unsigned char gAiScriptSidestepUp[] = {
    AI_SCRIPT_PRESS(AI_BTN_UP), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_RELEASE(AI_BTN_UP), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_UP), AI_SCRIPT_END
};
unsigned char gAiScriptBackstep[] = {
    AI_SCRIPT_PRESS(AI_BTN_BACK), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_RELEASE(AI_BTN_BACK), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_BACK), AI_SCRIPT_END
};
unsigned char gAiScriptDashForward[] = {
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_RELEASE(AI_BTN_FORWARD), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_END
};
unsigned char gAiScriptBackflipRetreat[] = {
    AI_SCRIPT_PRESS(AI_BTN_BACK), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_RELEASE(AI_BTN_BACK), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_BACK), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_RELEASE(AI_BTN_BACK), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_BACK), AI_SCRIPT_END
};
unsigned char gAiScriptThrowSubWeapon[] = {
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_R2), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptThrowSurfaceDebris[] = {
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_R2), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_R2), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_R2), AI_SCRIPT_END
};
/* nothing reads it */
unsigned char gAiScriptSingleTriangle[] = {
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_END
};
unsigned char gAiScriptGroundedRetreatAttack[] = {
    AI_SCRIPT_PRESS(AI_BTN_BACK), AI_SCRIPT_HOLD(8),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptCrouchTriangle[] = {
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_END
};
unsigned char gAiScriptCrouchCircle[] = {
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptCrouchCross[] = {
    AI_SCRIPT_PRESS(AI_BTN_CROSS), AI_SCRIPT_END
};
unsigned char gAiScriptCrouchR1Triangle[] = {
    AI_SCRIPT_PRESS(AI_BTN_R1), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_END
};
unsigned char gAiScriptCrouchForwardR2[] = {
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_R2), AI_SCRIPT_END
};
unsigned char gAiScriptCrouchThrowSubWeapon[] = {
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptKatzeTriangle[] = {
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_END
};
unsigned char gAiScriptKatzeCircle[] = {
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptKatzeCross[] = {
    AI_SCRIPT_PRESS(AI_BTN_CROSS), AI_SCRIPT_END
};
unsigned char gAiScriptKatzeDoubleCircle[] = {
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_CIRCLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptKatzeR1Triangle[] = {
    AI_SCRIPT_PRESS(AI_BTN_R1), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_END
};
unsigned char gAiScriptKatzeR1Circle[] = {
    AI_SCRIPT_PRESS(AI_BTN_R1), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptKatzeR2Circle[] = {
    AI_SCRIPT_PRESS(AI_BTN_R2), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptSlashTriangle[] = {
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_END
};
unsigned char gAiScriptSlashCircle[] = {
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptSlashCross[] = {
    AI_SCRIPT_PRESS(AI_BTN_CROSS), AI_SCRIPT_END
};
unsigned char gAiScriptSlashBackForwardTriangle[] = {
    AI_SCRIPT_PRESS(AI_BTN_BACK), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_BACK), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_END
};
unsigned char gAiScriptSlashDashCircle[] = {
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_FORWARD), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptSlashDashTriangle[] = {
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_FORWARD), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_END
};
unsigned char gAiScriptSlashDashCross[] = {
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_FORWARD), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_FORWARD), AI_SCRIPT_HOLD(1),
    AI_SCRIPT_PRESS(AI_BTN_CROSS), AI_SCRIPT_END
};
unsigned char gAiScriptSlashTripleCircle[] = {
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_CIRCLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_CIRCLE), AI_SCRIPT_HOLD(8),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptSlashTriangleDoubleCircle[] = {
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_TRIANGLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_CIRCLE), AI_SCRIPT_HOLD(8),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_END
};
unsigned char gAiScriptSlashTriangleCircleTriangle[] = {
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_TRIANGLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_PRESS(AI_BTN_CIRCLE), AI_SCRIPT_HOLD(2),
    AI_SCRIPT_RELEASE(AI_BTN_CIRCLE), AI_SCRIPT_HOLD(8),
    AI_SCRIPT_PRESS(AI_BTN_TRIANGLE), AI_SCRIPT_END
};
/* Indexed by gSlashLevelIndex 0..8; level 9 scores the stance's list, so the last entry is never read. */
unsigned char *gAiSlashAttackScripts[] = {
    gAiScriptSlashTriangle,
    gAiScriptSlashCircle,
    gAiScriptSlashCross,
    gAiScriptSlashBackForwardTriangle,
    gAiScriptSlashDashCircle,
    gAiScriptSlashDashTriangle,
    gAiScriptSlashDashCross,
    gAiScriptSlashTripleCircle,
    gAiScriptSlashTriangleDoubleCircle,
    gAiScriptSlashTriangleCircleTriangle,
};

/* Per-character AI tendencies as Q12 chances (4096 = always), indexed by Fighter.characterClass. */
AiCharParams gAiCharParamTable[FIGHTER_CLASS_COUNT] = {
    { 1024, 1024, 2304, 2048, 4096, 0, 256 },
    { 3072, 2304, 1792, 2048, 2048, 4096, 1536 },
    { 1792, 1024, 1536, 2304, 1024, 4096, 1280 },
    { 3072, 2048, 4096, 1536, 4096, 512, 2048 },
    { 4096, 4096, 1280, 1792, 4096, 1024, 3072 },
    { 1536, 256, 1024, 2304, 512, 4096, 256 },
    { 2048, 1024, 256, 4096, 0, 0, 4096 },
    { 1024, 1536, 3072, 4096, 4096, 512, 4096 },
    { 2048, 0, 4096, 1792, 0, 0, 0 },
    { 1536, 1024, 80, 4096, 0, 4096, 2048 },
    { 2048, 2048, 1024, 2048, 0, 0, 1536 },
    { 4096, 4096, 1280, 4096, 4096, 0, 4096 },
};

static void StartAiInputScript(Fighter *fighter, unsigned char *script)
{
    fighter->aiScriptPtr = script;
    fighter->aiScriptButtons = 0;
    fighter->aiScriptTicks = 0;
}

static int StepAiInputScript(Fighter *fighter)
{
    short ticks = fighter->aiScriptTicks;
    unsigned char *script = fighter->aiScriptPtr;
    int opcode;
    int result;

    if (ticks != 0) {
        if (fighter->animFrameStalled == 0)
            fighter->aiScriptTicks = ticks - 1;
        return fighter->aiScriptButtons;
    }

    for (;;) {
        opcode = *script++;
        if (opcode == AI_SCRIPT_OP_CHAR_MASK) {
            script += 2;
        } else if (opcode & AI_SCRIPT_OP_HOLD_FLAG) {
            if (opcode == AI_SCRIPT_OP_END) {
                int buttons = fighter->aiScriptButtons;
                fighter->aiScriptPtr = 0;
                return buttons;
            }
            fighter->aiScriptTicks = opcode & AI_SCRIPT_OP_HOLD_MASK;
            break;
        } else if (opcode & AI_SCRIPT_OP_RELEASE) {
            fighter->aiScriptButtons &= ~(1 << (opcode & AI_SCRIPT_OP_BUTTON_MASK));
        } else {
            fighter->aiScriptButtons |= 1 << (opcode & AI_SCRIPT_OP_BUTTON_MASK);
        }
    }

    result = fighter->aiScriptButtons;
    fighter->aiScriptPtr = script;
    return result;
}

static void UpdateAiOscillator(Fighter *fighter)
{
    fighter->aiOscAngle += gSinLutQ12[fighter->angles.heading & ANGLE_MASK] >> 8;
    fighter->aiOscSine = gSinLutQ12[fighter->aiOscAngle & ANGLE_MASK];
    fighter->aiOscCosine = gSinLutQ12[(fighter->aiOscAngle + ANGLE_QUARTER) & ANGLE_MASK];
    fighter->aiRangeScale = (fighter->aiOscSine >> 8) + ONE;
    fighter->aiRangeScale -= AI_SCALE_Q12(fighter->aiRangeScale - ONE, fighter->aiSkillRating);
    fighter->aiBelowOpponent = fighter->opponent->anchor.vy + (FLOOR_BAND_HEIGHT / 2) < fighter->anchor.vy;
}

static void UpdateAiPriorityOverrides(Fighter *fighter)
{
    Fighter *opponent;
    short action;

    if (fighter->opponent->action == FIGHTER_ACTION_DEAD &&
        fighter->aiState != AI_STATE_WAIT_OPPONENT_DOWN) {
        SetAiState(fighter, AI_STATE_WAIT_OPPONENT_DOWN, 0);
    }

    action = fighter->action;
    if (action == FIGHTER_ACTION_STAGGER || action == FIGHTER_ACTION_HEAVY_RECOIL || action == 8 ||
        action == FIGHTER_ACTION_DEAD || action == FIGHTER_ACTION_FATAL_KO ||
        action == FIGHTER_ACTION_DEFEATED || (fighter->placement->flags & MOTION_FLAG_FATAL) != 0) {
        SetAiState(fighter, AI_STATE_IDLE, 0);
    }

    opponent = fighter->opponent;
    if (opponent->action == FIGHTER_ACTION_ATTACK &&
        opponent->animInfo->activeEndFrame >= opponent->animFrame &&
        gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 3000) &&
        fighter->aiState != AI_STATE_DEFEND_OR_EVADE &&
        fighter->aiState != AI_STATE_FLANK_UP &&
        ShouldAiDefendAttack(fighter) != 0) {
        SetAiState(fighter, AI_STATE_DEFEND_OR_EVADE, fighter->aiDefenseDelay);
    }

    if (fighter->action == FIGHTER_ACTION_DOWNED) {
        short state = fighter->aiState;
        if (state != AI_STATE_GROUNDED &&
            state != AI_STATE_ROLL_DOWN && state != AI_STATE_ROLL_UP &&
            state != AI_STATE_GET_UP_FORWARD && state != AI_STATE_GET_UP_BACKWARD &&
            state != AI_STATE_GROUNDED_RETREAT_ATTACK) {
            SetAiState(fighter, AI_STATE_GROUNDED, 0);
        }
    }

    if (gBladeLockState != BLADE_LOCK_NONE && fighter->aiState != AI_STATE_BLADE_LOCK) {
        SetAiState(fighter, AI_STATE_BLADE_LOCK, rand() & 0xf);
        fighter->aiScratch.bladeLock.opponentWinning = 0;
        fighter->aiScratch.bladeLock.holdFrames = 0;
    }

    if (gCurrentStageId == 4 && gStage4LowerLevelActive == 0 &&
        fighter->position.vy > FLOOR_BAND_HEIGHT * 2 &&
        fighter->opponent->position.vy <= FLOOR_BAND_HEIGHT * 2) {
        short state = fighter->aiState;
        if (state == AI_STATE_SPRINT_APPROACH || state == AI_STATE_SPRINT_RETREAT ||
            state == AI_STATE_FLANK_UP || state == AI_STATE_FLANK_DOWN) {
            SetAiState(fighter, AI_STATE_IDLE, 0);
        }
    }

    if (gCurrentStageId != 10 && gCurrentStageId != 4 && gCurrentStageId != 9) {
        return;
    }
    if (ProbeAiTerrain(fighter, 3000, 0) != AI_TERRAIN_DROP_OFF ||
        fighter->groundY + FLOOR_BAND_HEIGHT >= fighter->opponent->groundY) {
        return;
    }
    if (fighter->aiState == AI_STATE_IDLE) {
        SetAiState(fighter, AI_STATE_LEAP_FORWARD, 8);
    } else if (fighter->aiState != AI_STATE_LEAP_FORWARD) {
        SetAiState(fighter, AI_STATE_IDLE, 0);
    }
}

static void UpdateFighterAiInputNoOp(Fighter *fighter)
{
}

void UpdateFighterAiInput(int player, struct PadInputFrame *out)
{
    Fighter *fighter = &gFighters[player];
    PadInputFrame frame;
    unsigned int held;
    unsigned int inverse;

    if (!fighter->aiAttackTables)
        LoadAiAttackTables(fighter);
    UpdateAiOscillator(fighter);
    UpdateAiPriorityOverrides(fighter);
    UpdateFighterAiInputNoOp(fighter);

    if (fighter->aiScriptPtr)
        frame.held = StepAiInputScript(fighter);
    else
        frame.held = DispatchAiState(fighter);
    held = frame.held & 0xffff;
    inverse = ~held;
    frame.held = held;
    frame.pressed = held & ~fighter->aiPrevInput.held;
    frame.notHeld = inverse & 0xffff;
    frame.released = inverse & fighter->aiPrevInput.held;
    frame.type[player] = PAD_TYPE_DIGITAL;

    *out = frame;
    fighter->aiPrevInput = frame;
}

static int RollAiApproachChance(Fighter *fighter, int percent)
{
    return gAiCharParamTable[fighter->characterClass].approach * percent / 100 > (rand() & AI_Q12_MASK);
}

static int RollAiLeapChance(Fighter *fighter, int percent)
{
    return gAiCharParamTable[fighter->characterClass].leap * percent / 100 > (rand() & AI_Q12_MASK);
}

/* Nothing calls it. */
static int RollAiUnusedChance(Fighter *fighter, int percent)
{
    return gAiCharParamTable[fighter->characterClass].unusedChance * percent / 100 > (rand() & AI_Q12_MASK);
}

static int RollAiEvadeChance(Fighter *fighter, int percent)
{
    return gAiCharParamTable[fighter->characterClass].evade * percent / 100 > (rand() & AI_Q12_MASK);
}

static int RollAiDefenseChance(Fighter *fighter, int percent)
{
    int mode = gPlayMode;
    int chance;
    int roll;
    int limit;

    if (mode == PLAY_MODE_SLASH) {
        chance = AI_SCALE_Q12(fighter->aiSkillRating, 1024 + gSlashLevelIndex * 102);
    } else if (mode == PLAY_MODE_VS && gPovModeEnabled == 1) {
        int level = gVsWinCounts[0];

        if (level > AI_POV_VS_WIN_CAP)
            level = AI_POV_VS_WIN_CAP;
        chance = AI_SCALE_Q12(fighter->aiSkillRating, 1024 + (level * 3072) / 10);
    } else {
        chance = AI_SCALE_Q12(fighter->aiSkillRating, gAiCharParamTable[fighter->characterClass].defense);
    }

    roll = rand();
    limit = chance * percent / 100;
    roll &= AI_Q12_MASK;
    return roll < limit;
}

static int RollAiRecoveryChance(Fighter *fighter, int percent)
{
    int chance;
    int roll;
    int threshold;
    int mode = gPlayMode;

    if (mode == PLAY_MODE_SLASH) {
        chance = AI_SCALE_Q12(fighter->aiSkillRating, gSlashLevelIndex * 102 + 1024);
    } else if (mode == PLAY_MODE_VS && gPovModeEnabled == 1) {
        int level = gVsWinCounts[0];

        if (level > AI_POV_VS_WIN_CAP)
            level = AI_POV_VS_WIN_CAP;
        chance = AI_SCALE_Q12(fighter->aiSkillRating, 1024 + (level * 3072) / 10);
    } else {
        chance = AI_SCALE_Q12(fighter->aiSkillRating, gAiCharParamTable[fighter->characterClass].recovery);
    }

    roll = rand();
    threshold = chance * percent / 100;
    roll &= AI_Q12_MASK;
    return roll < threshold;
}

static int RollAiAttackChance(Fighter *fighter, int percent)
{
    int characterClass = fighter->characterClass;
    int skillRating = fighter->aiSkillRating;
    int chance = AI_SCALE_Q12(skillRating, gAiCharParamTable[characterClass].attack);
    int roll = rand();
    return chance * percent / 100 > (roll & AI_Q12_MASK);
}

int ProbeAiTerrain(Fighter *fighter, int distance, int angleOffset)
{
    int probeAngle;
    int probeY;
    int terrainKind;

    probeAngle = fighter->angles.heading + angleOffset;
    probeY = QueryFloorHeight(
        fighter->position.vx + AI_SCALE_Q12(distance, gSinLutQ12[probeAngle & ANGLE_MASK]),
        fighter->position.vy,
        fighter->position.vz + AI_SCALE_Q12(distance, gSinLutQ12[(probeAngle + ANGLE_QUARTER) & ANGLE_MASK])
    );
    if (probeY == FLOOR_QUERY_MISS) {
        terrainKind = AI_TERRAIN_BLOCKED;
    } else if (probeY < fighter->groundY - FLOOR_BAND_HEIGHT) {
        terrainKind = AI_TERRAIN_BLOCKED;
    } else if (fighter->groundY + FLOOR_BAND_HEIGHT < probeY) {
        terrainKind = AI_TERRAIN_DROP_OFF;
    } else {
        terrainKind = (fighter->groundY < probeY) ? AI_TERRAIN_SLOPE_DOWN : AI_TERRAIN_LEVEL;
    }
    return terrainKind;
}

unsigned char *gAiKatzeAttackScripts[] = {
    gAiScriptKatzeTriangle,
    gAiScriptKatzeCircle,
    gAiScriptKatzeCross,
    gAiScriptKatzeDoubleCircle,
    gAiScriptKatzeR1Triangle,
    gAiScriptKatzeR1Circle,
    gAiScriptKatzeR2Circle,
    gAiScriptKatzeDoubleCircle,
};
unsigned char *gAiCrouchedAttackScripts[8] = {
    gAiScriptCrouchTriangle,
    gAiScriptCrouchTriangle,
    gAiScriptCrouchCircle,
    gAiScriptCrouchCircle,
    gAiScriptCrouchCross,
    gAiScriptCrouchCross,
    gAiScriptCrouchR1Triangle,
    gAiScriptCrouchForwardR2,
};
/* Indexed by RunAiDefendOrEvade's choice minus 1; a null entry starts no script. */
unsigned char *gAiEvadeScripts[7] = {
    0,
    gAiScriptSidestepDown,
    gAiScriptSidestepUp,
    gAiScriptBackstep,
    0,
    gAiScriptSidestepDown,
    gAiScriptSidestepUp,
};

int ShouldAiDefendAttack(Fighter *fighter)
{
    int mode = gPlayMode;
    int amount;
    int level;

    if (mode == PLAY_MODE_SLASH) {
        if (gSlashBoutIndex != AI_SLASH_BOSS_BOUT_INDEX)
            return 0;
        return RollAiDefenseChance(fighter, 10);
    }
    if (mode == PLAY_MODE_VS && gPovModeEnabled == 1) {
        level = gVsWinCounts[0];
        if (level > AI_POV_VS_WIN_CAP)
            level = AI_POV_VS_WIN_CAP;
        amount = level * 9 + 10;
        return RollAiDefenseChance(fighter, amount);
    }
    return RollAiDefenseChance(fighter, 100);
}

int gAiLastCycleIndex = -1;
int gAiMoveCycleMode;

static void SelectAiAttackScript(Fighter *fighter)
{
    int stance = fighter->animInfo->flags & ANIM_FLAG_STANCE_MASK;
    AiAttackChoice *base = fighter->aiAttackTables[stance];
    unsigned char *script;
    int slashLevel;
    Fighter *opponent;
    int opponentLegsIntact;
    int opponentDowned;
    int bestScore;
    int bestIndex;
    int count;
    int i;
    int score;
    int distError;
    int jitterA;
    int jitterB;
    AiAttackChoice *entry;

    if (gAiMoveCycleMode != 0) {
        int cycleIndex;
        unsigned int bit;
        unsigned char *cycleScript;

        if (fighter->aiAttackCounts[stance] == 0) {
            SetAiState(fighter, AI_STATE_IDLE, 0);
            return;
        }
        cycleIndex = fighter->aiCycleIndex++ % fighter->aiAttackCounts[stance];
        cycleScript = base[cycleIndex].script;
        if (cycleScript[0] == AI_SCRIPT_OP_CHAR_MASK) {
            distError = (cycleScript[1] << 8) | cycleScript[2];
            bit = 1U << fighter->characterClass;
            if ((distError & bit) == 0) {
                SetAiState(fighter, AI_STATE_IDLE, 0);
                return;
            }
        }
        StartAiInputScript(fighter, base[cycleIndex].script);
        gAiLastCycleIndex = cycleIndex;
        return;
    }

    if (fighter->characterClass == FIGHTER_CLASS_KATZE) {
        if (fighter->opponent->action == FIGHTER_ACTION_DOWNED) {
            StartAiInputScript(fighter, gAiScriptKatzeR1Circle);
            return;
        }
        if (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 3000)) {
            jitterA = rand();
            script = gAiScriptKatzeR1Triangle;
            if (jitterA & 1)
                script = gAiScriptKatzeDoubleCircle;
            StartAiInputScript(fighter, script);
            return;
        }
        StartAiInputScript(fighter, gAiKatzeAttackScripts[rand() & 7]);
        return;
    }

    if (gPlayMode == PLAY_MODE_SLASH && gSlashBoutIndex < AI_SLASH_BOSS_BOUT_INDEX) {
        slashLevel = gSlashLevelIndex;
        if (slashLevel < AI_SLASH_SCRIPT_LEVEL_MAX) {
            StartAiInputScript(fighter, gAiSlashAttackScripts[slashLevel]);
            return;
        }
    }

    if (fighter->characterClass < PLAYABLE_CHARACTER_COUNT && fighter->fighterId < FIGHTER_PLAYABLE_ID_COUNT &&
        RollAiEvadeChance(fighter, 100) == 0 && fighter->leftArmIntact != 0) {
        if (fighter->subWeaponCount > 0) {
            if ((rand() & 7) == 0) {
                StartAiInputScript(fighter, gAiScriptThrowSubWeapon);
                return;
            }
        } else {
            if ((rand() & 15) == 0) {
                StartAiInputScript(fighter, gAiScriptThrowSurfaceDebris);
                return;
            }
        }
    }

    bestIndex = -1;
    opponent = fighter->opponent;
    bestScore = 0x7fffffff;
    opponentDowned = opponent->action == FIGHTER_ACTION_DOWNED;
    count = fighter->aiAttackCounts[stance];
    opponentLegsIntact = opponent->legsIntact;
    for (i = 0; i < count; ++i) {
        unsigned char *scanScript;
        entry = &base[i];
        scanScript = entry->script;
        if (scanScript[0] == AI_SCRIPT_OP_CHAR_MASK) {
            unsigned int bit;
            distError = (scanScript[1] << 8) | scanScript[2];
            bit = 1U << fighter->characterClass;
            if ((distError & bit) == 0)
                continue;
        }

        distError = entry->distance * 20 - gFighterDistanceXZ;
        if (distError < 0)
            distError = -distError;
        jitterA = rand();
        jitterB = rand();
        jitterA &= AI_Q12_MASK;
        jitterA += jitterB & AI_Q12_MASK;
        score = distError + (entry->penalty << 5) +
            AI_SCALE_Q12(jitterA, ONE - fighter->aiSkillRating / 2);
        if (opponentDowned) {
            if (entry->flags & AI_ATTACK_PREFER_DOWNED)
                score /= 2;
        } else if (opponentLegsIntact == 0) {
            if (entry->flags & AI_ATTACK_PREFER_CRIPPLED)
                score /= 2;
        }
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }
    if (bestIndex == -1) {
        EnterAiSprintApproach(fighter);
        return;
    }
    StartAiInputScript(fighter, base[bestIndex].script);
}

static void SelectAiCrouchedAttackScript(Fighter *fighter)
{
    unsigned char *script;

    if (fighter->opponent->action == FIGHTER_ACTION_DOWNED) {
        StartAiInputScript(fighter, gAiScriptCrouchCross);
    } else {
        script = gAiCrouchedAttackScripts[rand() & 7];
        fighter->aiScratch.crouchAttackScript = script;
        StartAiInputScript(fighter, script);
    }
}

static int IsOpponentOnLeftSide(Fighter *fighter)
{
    int target = fighter->angles.heading - ANGLE_QUARTER;
    int distance = (fighter->angleToOpponent - target) & ANGLE_MASK;

    if (distance >= ANGLE_HALF) {
        distance = ANGLE_FULL - distance;
    }
    return distance < ANGLE_QUARTER;
}

/* Nothing calls it. */
static void EnterAiRollAway(Fighter *fighter)
{
    int state;

    if (IsOpponentOnLeftSide(fighter) != 0) {
        state = AI_STATE_ROLL_DOWN;
    } else {
        state = AI_STATE_ROLL_UP;
    }
    SetAiState(fighter, state, 0);
}

static void EnterAiGetUpDirection(Fighter *fighter)
{
    int state;

    if (IsOpponentOnLeftSide(fighter) != 0) {
        state = AI_STATE_GET_UP_FORWARD;
    } else {
        state = AI_STATE_GET_UP_BACKWARD;
    }
    SetAiState(fighter, state, 0);
}

/* Nothing calls it. */
static void EnterAiWalkOrDash(Fighter *fighter)
{
    int state;

    if ((rand() & 1) != 0) {
        state = AI_STATE_WALK_FORWARD;
    } else {
        state = AI_STATE_DASH_FORWARD;
    }
    SetAiState(fighter, state, 0);
}

void EnterAiSprintApproach(Fighter *fighter)
{
    SetAiState(fighter, AI_STATE_SPRINT_APPROACH, 0);
}

static void EnterAiRetreatOrLeap(Fighter *fighter)
{
    int state;

    if (ProbeAiTerrain(fighter, 3000, ANGLE_HALF) == AI_TERRAIN_DROP_OFF) {
        SetAiState(fighter, AI_STATE_LEAP_FORWARD, 0);
        return;
    }
    state = (rand() & 0x3f) == 0 ? AI_STATE_SPRINT_RETREAT : AI_STATE_BACKFLIP_RETREAT;
    SetAiState(fighter, state, 0);
}

static void EnterAiFlankDirection(Fighter *fighter)
{
    Fighter *opponent;
    int dx, dz;
    unsigned int squared;
    int distance;
    int leading;
    int sideX, sideZ;
    VECTOR left, right;
    int state;

    opponent = fighter->opponent;
    dx = opponent->position.vx - fighter->position.vx;
    dz = opponent->position.vz - fighter->position.vz;
    squared = dx * dx + dz * dz;
    if (squared < 1024) {
        distance = gSqrtLutQ3[squared] >> 3;
    } else {
        unsigned int shift;
        gte_ldlzc(squared);
        gte_nop();
        gte_nop();
        gte_stlzc(&leading);
        shift = leading;
        shift = 22 - (shift & ~1);
        squared = gSqrtLutQ3[squared >> shift];
        distance = (squared << 16) >> (19 - (shift >> 1));
    }
    if (distance > 6000) {
        distance = 6000;
    } else if (distance < 1000) {
        distance = 1000;
    }
    dx = dx * 3000 / distance;
    dz = dz * 3000 / distance;
    sideX = (dx * gSinLutQ12[ANGLE_QUARTER + ANGLE_EIGHTH] + dz * gSinLutQ12[ANGLE_EIGHTH]) >> AI_Q12_SHIFT;
    sideZ = (-dx * gSinLutQ12[ANGLE_EIGHTH] + dz * gSinLutQ12[ANGLE_QUARTER + ANGLE_EIGHTH]) >> AI_Q12_SHIFT;
    /* left and right are one point: both add sideX and sideZ */
    left.vx = fighter->position.vx + sideX;
    left.vz = fighter->position.vz + sideZ;
    right.vx = fighter->position.vx + sideX;
    right.vz = fighter->position.vz + sideZ;
    left.vy = QueryFloorHeight(left.vx, fighter->position.vy, left.vz);
    if (left.vy == FLOOR_QUERY_MISS) {
        left.vy = FLOOR_HEIGHT_NONE;
    }
    right.vy = QueryFloorHeight(right.vx, fighter->position.vy, right.vz);
    if (right.vy == FLOOR_QUERY_MISS) {
        right.vy = FLOOR_HEIGHT_NONE;
    }
    if (left.vy < fighter->groundY - FLOOR_BAND_HEIGHT) {
        state = AI_STATE_FLANK_UP;
    } else if (right.vy < fighter->groundY - FLOOR_BAND_HEIGHT) {
        state = AI_STATE_FLANK_DOWN;
    } else if (rand() & 1) {
        state = AI_STATE_FLANK_UP;
    } else {
        state = AI_STATE_FLANK_DOWN;
    }
    fighter->aiScratch.flankFrames = 0;
    SetAiState(fighter, state, 0);
}

static int RunAiIdleDecision(Fighter *fighter)
{
    int action;
    Fighter *opponent;
    int ownStance, otherStance;
    int localStance;
    short ammo;
    int terrain;
    int range;
    int angularDifference;
    int random;
    unsigned char *script;

    if (fighter->action == FIGHTER_ACTION_NEUTRAL && fighter->animInfo->activeStartFrame == 1)
        return 0;
    if (fighter->aiForceDefend != 0) {
        SetAiState(fighter, AI_STATE_DEFEND_OR_EVADE, 0);
        RunAiDefendOrEvade(fighter);
        return 0;
    }
    if (gAiMoveCycleMode != 0) {
        otherStance = fighter->opponent->animInfo->flags & ANIM_FLAG_STANCE_MASK;
        if (fighter->action != FIGHTER_ACTION_NEUTRAL)
            return 0;
        localStance = fighter->animInfo->flags & ANIM_FLAG_STANCE_MASK;
        if (localStance == STANCE_HIGH) {
            if (otherStance != STANCE_HIGH)
                return PADR2;
        } else if (localStance == STANCE_LOW) {
            if (otherStance != STANCE_LOW)
                return PADR1;
        } else {
            if (otherStance == STANCE_HIGH)
                return PADR1;
            if (otherStance == STANCE_LOW)
                return PADR2;
        }
        SetAiState(fighter, AI_STATE_ATTACK, fighter->aiAttackDelay);
        return 0;
    }
    if (fighter->characterClass == FIGHTER_CLASS_KATZE) {
        ammo = fighter->gunAmmo;
        if (ammo == 0) {
            if (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 3000) && (rand() & 1) == 0) {
                if ((rand() & 0xf) != 0)
                    SetAiState(fighter, AI_STATE_SPRINT_RETREAT, 0);
                else
                    SetAiState(fighter, AI_STATE_BACKFLIP_RETREAT, 0);
            } else
                return (rand() & 1) ? PADselect : 0;
        } else if ((rand() & 3) + 1 >= ammo && (rand() & 0xf) == 0) {
            if (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 3000))
                SetAiState(fighter, AI_STATE_BACKFLIP_RETREAT, 0);
            else
                return PADselect;
        } else if (RollAiAttackChance(fighter, 60) != 0)
            SetAiState(fighter, AI_STATE_ATTACK, 0);
        else if ((rand() & 1) == 0)
            SetAiState(fighter, AI_STATE_BACKFLIP_RETREAT, 0);
        else if ((rand() & 1) == 0)
            SetAiState(fighter, AI_STATE_SIDESTEP_UP, 0);
        else if ((rand() & 1) == 0)
            SetAiState(fighter, AI_STATE_SIDESTEP_UP, 0); /* the same state again in the original */
        else if ((rand() & 1) == 0)
            SetAiState(fighter, AI_STATE_SPRINT_RETREAT, 0);
        return 0;
    }
    action = fighter->action;
    if (action == FIGHTER_ACTION_NEUTRAL) {
        if (fighter->animInfo->flags & ANIM_FLAG_FIXED_SPEED)
            return 0;
        terrain = ProbeAiTerrain(fighter, 3000, ANGLE_HALF);
        if (terrain == AI_TERRAIN_DROP_OFF) {
            terrain = ProbeAiTerrain(fighter, 3000, 0);
            if (terrain != AI_TERRAIN_DROP_OFF) {
                SetAiState(fighter, AI_STATE_LEAP_FORWARD, 0);
                return 0;
            }
        }
        opponent = fighter->opponent;
        if (opponent->action == FIGHTER_ACTION_DOWNED) {
            if (fighter->aiBelowOpponent != 0 || gFighterDistanceXZ >= AI_SCALE_Q12(fighter->aiRangeScale, 5000)) {
                SetAiState(fighter, AI_STATE_SPRINT_APPROACH, 0);
                return 0;
            }
            if (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 3500)) {
                if (RollAiAttackChance(fighter, 100) == 0) {
                    SetAiState(fighter, AI_STATE_ATTACK, fighter->aiAttackDelay);
                    return 0;
                }
                SetAiState(fighter, AI_STATE_BACK_FORWARD_ATTACK, fighter->aiAttackDelay);
                return 0;
            }
            SetAiState(fighter, AI_STATE_DASH_FORWARD, 0);
            return 0;
        }
        if (opponent->legsIntact == 0) {
            if (fighter->aiBelowOpponent != 0 || gFighterDistanceXZ >= AI_SCALE_Q12(fighter->aiRangeScale, 5000)) {
                SetAiState(fighter, AI_STATE_SPRINT_APPROACH, 0);
                return 0;
            }
            if (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 3500)) {
                SetAiState(fighter, AI_STATE_ATTACK, fighter->aiAttackDelay);
                return 0;
            }
            SetAiState(fighter, AI_STATE_DASH_FORWARD, 0);
            return 0;
        }
        if ((rand() & 0x1ff) == 0) {
            if (RollAiEvadeChance(fighter, 25) != 0) {
                EnterAiRetreatOrLeap(fighter);
                return 0;
            }
            if (RollAiLeapChance(fighter, 33) != 0) {
                SetAiState(fighter, AI_STATE_LEAP_FORWARD, 0);
                return 0;
            }
            if (RollAiApproachChance(fighter, 50) != 0)
                EnterAiFlankDirection(fighter);
            return 0;
        }
        if (gPlayMode == PLAY_MODE_SLASH && gSlashBoutIndex < AI_SLASH_BOSS_BOUT_INDEX) {
            ownStance = fighter->animInfo->flags & ANIM_FLAG_STANCE_MASK;
            if (ownStance != STANCE_MID) {
                if (ownStance == STANCE_HIGH)
                    return PADR2;
                return PADR1;
            }
        }
        if ((rand() & 0xff) == 0) {
            ownStance = fighter->animInfo->flags & ANIM_FLAG_STANCE_MASK;
            if (ownStance == STANCE_HIGH)
                return PADR2;
            if (ownStance == STANCE_LOW)
                return PADR1;
            return (rand() & 1) != 0 ? PADR1 : PADR2;
        }
        range = fighter->aiRangeScale;
        if (gFighterDistanceXZ >= AI_SCALE_Q12(range, 10000)) {
            if (RollAiApproachChance(fighter, 100) != 0)
                EnterAiSprintApproach(fighter);
            return 0;
        }
        if (gFighterDistanceXZ >= AI_SCALE_Q12(range, 5000)) {
            if (RollAiApproachChance(fighter, 10) != 0)
                EnterAiSprintApproach(fighter);
            return 0;
        }
        if (fighter->aiBelowOpponent != 0) {
            if (RollAiApproachChance(fighter, 5) != 0)
                EnterAiFlankDirection(fighter);
            return 0;
        }
        angularDifference = fighter->angles.heading - fighter->opponent->angles.heading;
        if (fighter->climbState != 0)
            angularDifference += ANGLE_HALF;
        angularDifference &= ANGLE_MASK;
        if (angularDifference >= ANGLE_HALF)
            angularDifference = ANGLE_FULL - angularDifference;
        if (angularDifference < ANGLE_QUARTER && RollAiEvadeChance(fighter, 100) != 0) {
            if (RollAiLeapChance(fighter, 50) != 0) {
                SetAiState(fighter, AI_STATE_LEAP_FORWARD, 0);
                return 0;
            }
            random = rand();
            script = gAiScriptSidestepUp;
            if (random & 1)
                script = gAiScriptSidestepDown;
            StartAiInputScript(fighter, script);
            return 0;
        }
        if (RollAiAttackChance(fighter, 100) == 0)
            return 0;
        SetAiState(fighter, AI_STATE_ATTACK, fighter->aiAttackDelay);
        return 0;
    }
    if (action == FIGHTER_ACTION_CROUCH) {
        SetAiState(fighter, AI_STATE_CROUCHED, 0);
        return 0;
    }
    if ((action == FIGHTER_ACTION_HEAVY_RECOIL || action == FIGHTER_ACTION_STAGGER) &&
        RollAiRecoveryChance(fighter, 33) != 0)
        return PADR2;
    return 0;
}

int RunAiDefendOrEvade(Fighter *fighter)
{
    int result = 0;
    int choice;
    int action;
    Fighter *opponent;
    int step;

    if (fighter->aiSubStep == 0) {
        if (fighter->aiForceDefend != 0) {
            fighter->aiForceDefend = 0;
            fighter->aiScratch.evadeChoice = 0;
        } else if (fighter->opponent->isVerticalSwing == 0) { /* always taken: isVerticalSwing is always 0 */
            fighter->aiScratch.evadeChoice = 0;
        } else {
            action = fighter->action;
            if (action == FIGHTER_ACTION_NEUTRAL) {
                choice = rand() & 3;
                fighter->aiScratch.evadeChoice = choice;
            } else if (action == FIGHTER_ACTION_DOWNED) {
                choice = (rand() & 1) + 5;
                fighter->aiScratch.evadeChoice = choice;
            } else if (action == FIGHTER_ACTION_CROUCH) {
                fighter->aiScratch.evadeChoice = 0;
            } else {
                choice = -1;
                fighter->aiScratch.evadeChoice = choice;
            }
        }
        ++fighter->aiSubStep;
    }

    switch (fighter->aiScratch.evadeChoice) {
    case -1:
        fighter->aiSubStep = 0;
        break;
    case 0:
        result = PADRleft;
        if (fighter->aiStateFrames >= 2 && fighter->action != FIGHTER_ACTION_GUARD) {
            SetAiState(fighter, AI_STATE_IDLE, 0);
        } else {
            opponent = fighter->opponent;
            if (opponent->action != FIGHTER_ACTION_ATTACK || opponent->animInfo->activeEndFrame < opponent->animFrame)
                SetAiState(fighter, AI_STATE_IDLE, 0);
        }
        break;
    case 1:
    case 2:
    case 3:
    case 5:
    case 6:
        step = fighter->aiSubStep;
        switch (step) {
        case 1:
            StartAiInputScript(fighter, gAiEvadeScripts[fighter->aiScratch.evadeChoice - 1]);
            ++fighter->aiSubStep;
            break;
        case 2:
            SetAiState(fighter, AI_STATE_IDLE, 0);
            break;
        }
        break;
    case 4:
        break;
    }
    return result;
}

static int RunAiExecuteAttack(Fighter *fighter)
{
    short step;

    step = fighter->aiSubStep;
    switch (step) {
    case 0:
        fighter->aiSubStep += 1;
        SelectAiAttackScript(fighter);
        break;
    case 1:
        if (fighter->action == FIGHTER_ACTION_NEUTRAL) {
            SetAiState(fighter, AI_STATE_IDLE, 8);
        }
        break;
    }
    return 0;
}

static int RunAiWaitOpponentDown(Fighter *fighter)
{
    return 0;
}

int gAiGroundedAttackButtons[4] = { PADRup, PADRright, PADRdown, PADRright };

static int RunAiGroundedAction(Fighter *fighter)
{
    int buttons = 0;
    int probe;

    if (fighter->legsIntact == 0 &&
        gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 3500)) {
        if (fighter->isSupine == 0) {
            int rollButtons = IsOpponentOnLeftSide(fighter) == 0 ? PADLdown : PADLup;
            return rollButtons;
        }
        SetAiState(fighter, AI_STATE_GROUNDED_RETREAT_ATTACK, 0);
        return 0;
    }

    if (fighter->action != FIGHTER_ACTION_DOWNED ||
        fighter->placement->triggers[0].mask == 0)
        return buttons;

    probe = IsOpponentOnLeftSide(fighter);
    if (fighter->isSupine == 0)
        probe = !probe;
    probe = probe ? ANGLE_QUARTER : -ANGLE_QUARTER;
    probe = ProbeAiTerrain(fighter, 500, probe);
    if (probe != AI_TERRAIN_DROP_OFF && probe != AI_TERRAIN_BLOCKED &&
        gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 4500)) {
        if (RollAiDefenseChance(fighter, 100)) {
            probe = IsOpponentOnLeftSide(fighter);
            if (fighter->isSupine == 0)
                probe = !probe;
            buttons = probe ? PADLdown : PADLup;
        } else if (RollAiAttackChance(fighter, 100)) {
            buttons = gAiGroundedAttackButtons[rand() & 3];
        } else {
            EnterAiGetUpDirection(fighter);
        }
    } else {
        EnterAiGetUpDirection(fighter);
    }
    return buttons;
}

static int RunAiCrouchedDecision(Fighter *fighter)
{
    if (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 2500)) {
        SetAiState(fighter, AI_STATE_CROUCHED_ATTACK, fighter->aiAttackDelay);
        return 0;
    }
    switch (rand() & 7) {
    case 0:
    case 1:
        return 0;
    case 2:
    case 3:
        SetAiState(fighter, AI_STATE_CRAWL_FORWARD, 0);
        return 0;
    case 4:
        SetAiState(fighter, AI_STATE_CRAWL_BACKWARD, 0);
        return 0;
    case 5:
        SetAiState(fighter, AI_STATE_STAND_FROM_CROUCH, 0);
        return PADR2;
    case 6:
        SetAiState(fighter, AI_STATE_CROUCH_SUB_WEAPON_THROW, 0);
        return 0;
    case 7:
        SetAiState(fighter, AI_STATE_CROUCH_FORWARD_R2, 0);
        return 0;
    default:
        return 0;
    }
}

static int RunAiRollDown(Fighter *fighter)
{
    SetAiState(fighter, AI_STATE_GROUNDED, 0);
    return PADLdown;
}

static int RunAiRollUp(Fighter *fighter)
{
    SetAiState(fighter, AI_STATE_GROUNDED, 0);
    return PADLup;
}

static int RunAiGetUpForward(Fighter *fighter)
{
    int action;

    action = fighter->action;
    if (action == FIGHTER_ACTION_NEUTRAL || action == FIGHTER_ACTION_CROUCH) {
        SetAiState(fighter, AI_STATE_IDLE, 0);
    }
    return PADLright;
}

static int RunAiGetUpBackward(Fighter *fighter)
{
    int action;

    action = fighter->action;
    if (action == FIGHTER_ACTION_NEUTRAL || action == FIGHTER_ACTION_CROUCH) {
        SetAiState(fighter, AI_STATE_IDLE, 0);
    }
    return PADLleft;
}

static int RunAiBackForwardAttack(Fighter *fighter)
{
    switch (fighter->aiSubStep) {
    case 0:
        StartAiInputScript(fighter, gAiScriptBackForwardAttack);
        ++fighter->aiSubStep;
        break;
    case 1:
        if (fighter->action == FIGHTER_ACTION_NEUTRAL)
            SetAiState(fighter, AI_STATE_IDLE, 0);
        break;
    }
    return 0;
}

static int RunAiWalkForward(Fighter *fighter)
{
    int buttons = PADLright;
    int range, distance;
    int stepX, stepZ, probeX;

    if (fighter->action == FIGHTER_ACTION_NEUTRAL) {
        range = fighter->aiRangeScale;
        distance = gFighterDistanceXZ;

        if (distance < AI_SCALE_Q12(range, 2400)) {
            if (fighter->opponent->anchor.vy + (FLOOR_BAND_HEIGHT / 2) < fighter->anchor.vy) {
                if (RollAiApproachChance(fighter, 5))
                    EnterAiFlankDirection(fighter);
            } else {
                SetAiState(fighter, AI_STATE_IDLE, 8);
            }
        } else if (distance < AI_SCALE_Q12(range, 5000)) {
            if (fighter->nearWallContact != 0) {
                stepX = fighter->wallContactNormal.vx / 4;
                probeX = fighter->position.vx - stepX;
                stepZ = fighter->wallContactNormal.vz / 4;
                if ((QueryFloorHeight(probeX, fighter->position.vy,
                                  fighter->position.vz - stepZ) & ~7) ==
                    fighter->groundY - FLOOR_BAND_HEIGHT) {
                    buttons |= PADR1;
                } else {
                    EnterAiFlankDirection(fighter);
                }
            }
        } else if (RollAiApproachChance(fighter, 10)) {
            EnterAiSprintApproach(fighter);
        }
    }

    return buttons;
}

static int RunAiWalkBackward(Fighter *fighter)
{
    short rangeScale;
    int distance;

    rangeScale = fighter->aiRangeScale;
    distance = gFighterDistanceXZ;
    if (distance < AI_SCALE_Q12(rangeScale, 2300))
        SetAiState(fighter, AI_STATE_IDLE, 8);
    else if (distance >= AI_SCALE_Q12(rangeScale, 5000))
        EnterAiSprintApproach(fighter);
    return PADLleft;
}

int (*gAiStateHandlers[])(Fighter *) = {
    RunAiIdleDecision, RunAiDefendOrEvade, RunAiExecuteAttack,
    RunAiWaitOpponentDown, RunAiGroundedAction, RunAiCrouchedDecision,
    RunAiRollDown, RunAiRollUp, RunAiGetUpForward,
    RunAiGetUpBackward, RunAiBackForwardAttack, RunAiWalkForward,
    RunAiWalkBackward, RunAiSprintApproach, RunAiDashForward,
    RunAiCrouchedAttack, RunAiGroundedRetreatAttack, RunAiCrawlForward,
    RunAiCrawlBackward, RunAiCrouchSubWeaponThrow, RunAiCrouchForwardR2,
    RunAiSprintRetreat, RunAiBackflipRetreat, RunAiLeapForward,
    RunAiFlankUp, RunAiFlankDown, RunAiStandFromCrouch,
    RunAiBladeLock, RunAiSidestepUp, RunAiSidestepDown,
};

int RunAiSprintApproach(Fighter *fighter)
{
    int buttons = PADLright | PADL1;
    int distanceDelta;
    int opponentAction;

    if (fighter->action == FIGHTER_ACTION_NEUTRAL &&
        (fighter->animInfo->flags & ANIM_FLAG_RUN) != 0)
        buttons = PADLright | PADR1 | PADL1;
    if (ProbeAiTerrain(fighter, 1000, 0) == AI_TERRAIN_BLOCKED) {
        EnterAiFlankDirection(fighter);
        return buttons;
    } else if (fighter->aiSubStep > 0 &&
               fighter->aiScratch.sprint.lastX == fighter->position.vx &&
               fighter->aiScratch.sprint.lastZ == fighter->position.vz) {
        EnterAiFlankDirection(fighter);
        return buttons;
    } else {
        fighter->aiScratch.sprint.retreatFrames = 0;
        fighter->aiSubStep++;
        fighter->aiScratch.sprint.lastX = fighter->position.vx;
        fighter->aiScratch.sprint.lastZ = fighter->position.vz;
        if (ProbeAiTerrain(fighter, 3000, 0) == AI_TERRAIN_DROP_OFF &&
            fighter->groundY + FLOOR_BAND_HEIGHT >= fighter->opponent->groundY) {
            SetAiState(fighter, AI_STATE_IDLE, 0);
        } else {
            distanceDelta = gFighterDistanceDelta;
            if (distanceDelta < 0)
                distanceDelta = -distanceDelta;
            if (distanceDelta > 100 && gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 3500)) {
                opponentAction = fighter->opponent->action;
                if (opponentAction == FIGHTER_ACTION_NEUTRAL || opponentAction == FIGHTER_ACTION_CROUCH) {
                    if ((rand() & 1) != 0)
                        buttons |= PADRright;
                    else
                        buttons |= PADRdown;
                    SetAiState(fighter, AI_STATE_IDLE, 0);
                } else {
                    SetAiState(fighter, AI_STATE_IDLE, 0);
                }
            } else {
                distanceDelta = fighter->side != 0 ? -gFighterDistanceDelta : gFighterDistanceDelta;
                if (distanceDelta > 200 && (rand() & 0xff) == 0) {
                    SetAiState(fighter, AI_STATE_SPRINT_RETREAT, 0);
                } else if (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 1200)) {
                    buttons |= PADRup;
                }
            }
        }
    }
    return buttons;
}

int RunAiDashForward(Fighter *fighter)
{
    switch (fighter->aiSubStep) {
    case 0:
        StartAiInputScript(fighter, gAiScriptDashForward);
        ++fighter->aiSubStep;
        break;
    case 1:
        SetAiState(fighter, AI_STATE_IDLE, 0);
        break;
    }
    return 0;
}

int RunAiCrouchedAttack(Fighter *fighter)
{
    int step;
    int action;

    step = fighter->aiSubStep;
    switch (step) {
    case 0:
        SelectAiCrouchedAttackScript(fighter);
        ++fighter->aiSubStep;
        break;
    case 1:
        if (fighter->aiStateFrames >= 5) {
            action = fighter->action;
            if (action == FIGHTER_ACTION_CROUCH || action == FIGHTER_ACTION_DOWNED)
                SetAiState(fighter, AI_STATE_IDLE, 0);
        }
        break;
    }
    return 0;
}

int RunAiGroundedRetreatAttack(Fighter *fighter)
{
    int action;

    switch (fighter->aiSubStep) {
    case 0:
        StartAiInputScript(fighter, gAiScriptGroundedRetreatAttack);
        ++fighter->aiSubStep;
        break;
    case 1:
        action = fighter->action;
        if (action == FIGHTER_ACTION_CROUCH || action == FIGHTER_ACTION_DOWNED)
            SetAiState(fighter, AI_STATE_IDLE, 0);
        break;
    }
    return 0;
}

int RunAiCrawlForward(Fighter *fighter)
{
    if (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 2200)) {
        SetAiState(fighter, AI_STATE_CROUCHED_ATTACK, fighter->aiAttackDelay);
    } else if ((rand() & 0xff) == 0) {
        SetAiState(fighter, AI_STATE_CROUCHED_ATTACK, fighter->aiAttackDelay);
    }
    return PADLright;
}

int RunAiCrawlBackward(Fighter *fighter)
{
    if (gFighterDistanceXZ >= AI_SCALE_Q12(fighter->aiRangeScale, 5000)) {
        SetAiState(fighter, AI_STATE_IDLE, 0);
    }
    return PADLleft;
}

int RunAiCrouchSubWeaponThrow(Fighter *fighter)
{
    switch (fighter->aiSubStep) {
    case 0:
        if (fighter->subWeaponCount > 0)
            StartAiInputScript(fighter, gAiScriptCrouchThrowSubWeapon);
        else
            StartAiInputScript(fighter, gAiScriptCrouchForwardR2);
        ++fighter->aiSubStep;
        break;
    case 1:
        if (fighter->action == FIGHTER_ACTION_CROUCH)
            SetAiState(fighter, AI_STATE_IDLE, 0);
        break;
    }
    return 0;
}

int RunAiCrouchForwardR2(Fighter *fighter)
{
    switch (fighter->aiSubStep) {
    case 0:
        StartAiInputScript(fighter, gAiScriptCrouchForwardR2);
        ++fighter->aiSubStep;
        break;
    case 1:
        if (fighter->action == FIGHTER_ACTION_CROUCH)
            SetAiState(fighter, AI_STATE_IDLE, 0);
        break;
    }
    return 0;
}

int RunAiSprintRetreat(Fighter *fighter)
{
    int buttons;
    int terrainCheck;

    buttons = PADLleft | PADL1;
    if (fighter->characterClass == FIGHTER_CLASS_KATZE) {
        if (gFighterDistanceXZ >= AI_SCALE_Q12(fighter->aiRangeScale, 10000) || (rand() & 0x1f) == 0) {
            SetAiState(fighter, AI_STATE_IDLE, 0);
        }
    } else {
        if (fighter->action == FIGHTER_ACTION_NEUTRAL && (fighter->animInfo->flags & ANIM_FLAG_RUN) != 0) {
            buttons = PADLleft | PADR1 | PADL1;
        }
        if (fighter->aiSubStep == 0) {
            fighter->aiScratch.sprint.retreatFrames = 0;
        } else {
            fighter->aiScratch.sprint.retreatFrames += 1;
        }
        terrainCheck = ProbeAiTerrain(fighter, 3000, 0);
        if (terrainCheck == AI_TERRAIN_DROP_OFF && fighter->groundY + FLOOR_BAND_HEIGHT >= fighter->opponent->groundY) {
            SetAiState(fighter, AI_STATE_IDLE, 0);
        } else if (terrainCheck == AI_TERRAIN_BLOCKED &&
                   (rand() & 0xf) + 15 < fighter->aiScratch.sprint.retreatFrames) {
            SetAiState(fighter, AI_STATE_IDLE, 0);
        } else if (fighter->aiSubStep <= 0 ||
                   fighter->aiScratch.sprint.lastX != fighter->position.vx ||
                   fighter->aiScratch.sprint.lastZ != fighter->position.vz) {
            fighter->aiSubStep++;
            fighter->aiScratch.sprint.lastX = fighter->position.vx;
            fighter->aiScratch.sprint.lastZ = fighter->position.vz;
            if (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 10000)) {
                if ((rand() & 0x3f) == 0) {
                    SetAiState(fighter, AI_STATE_SPRINT_APPROACH, 0);
                }
            } else if ((rand() & 1) != 0) {
                SetAiState(fighter, AI_STATE_IDLE, 0);
            } else {
                SetAiState(fighter, AI_STATE_SPRINT_APPROACH, 0);
            }
        } else {
            SetAiState(fighter, AI_STATE_SPRINT_APPROACH, 0);
        }
    }
    return buttons;
}

int RunAiBackflipRetreat(Fighter *fighter)
{
    switch (fighter->aiSubStep) {
    case 0:
        StartAiInputScript(fighter, gAiScriptBackflipRetreat);
        ++fighter->aiSubStep;
        break;
    case 1:
        SetAiState(fighter, AI_STATE_IDLE, 0);
        break;
    }
    return 0;
}

int RunAiSidestepUp(Fighter *fighter)
{
    switch (fighter->aiSubStep) {
    case 0:
        StartAiInputScript(fighter, gAiScriptSidestepUp);
        ++fighter->aiSubStep;
        break;
    case 1:
        SetAiState(fighter, AI_STATE_IDLE, 0);
        break;
    }
    return 0;
}

int RunAiSidestepDown(Fighter *fighter)
{
    switch (fighter->aiSubStep) {
    case 0:
        StartAiInputScript(fighter, gAiScriptSidestepDown);
        ++fighter->aiSubStep;
        break;
    case 1:
        SetAiState(fighter, AI_STATE_IDLE, 0);
        break;
    }
    return 0;
}

int RunAiLeapForward(Fighter *fighter)
{
    switch (fighter->aiSubStep) {
    case 0:
        StartAiInputScript(fighter, gAiScriptLeapForward);
        ++fighter->aiSubStep;
        break;
    case 1:
        SetAiState(fighter, AI_STATE_IDLE, 0);
        break;
    }
    return 0;
}

int RunAiFlankUp(Fighter *fighter)
{
    Fighter *opponent;
    int buttons;

    buttons = PADL1;
    if (fighter->action == FIGHTER_ACTION_NEUTRAL && (fighter->animInfo->flags & ANIM_FLAG_RUN) != 0) {
        buttons = PADR1 | PADL1;
    }
    if (ProbeAiTerrain(fighter, 3000, 0) == AI_TERRAIN_DROP_OFF &&
        fighter->groundY + FLOOR_BAND_HEIGHT >= fighter->opponent->groundY) {
        SetAiState(fighter, AI_STATE_IDLE, 0);
        return buttons;
    }
    if (ProbeAiTerrain(fighter, 500, 0) == AI_TERRAIN_BLOCKED) {
        if ((rand() & 0x1f) == 0) {
            SetAiState(fighter, AI_STATE_SPRINT_RETREAT, 0);
            return buttons;
        }
    }
    switch (fighter->aiSubStep) {
    case 0:
        buttons |= PADLleft | PADLup;
        if (++fighter->aiScratch.flankFrames > 10) {
            fighter->aiSubStep++;
        }
        return buttons;
    case 1:
        buttons |= PADLright | PADLup;
        if (++fighter->aiScratch.flankFrames > 90 || RollAiApproachChance(fighter, 5) == 0) {
            fighter->aiSubStep++;
        }
        break;
    case 2:
        buttons |= PADLright | PADLup;
        if ((rand() & 0x3f) == 0) {
            SetAiState(fighter, AI_STATE_SPRINT_APPROACH, 0);
        } else if ((rand() & 0x3f) == 0) {
            SetAiState(fighter, AI_STATE_SPRINT_RETREAT, 0);
        }
        break;
    default:
        return buttons;
    }

    opponent = fighter->opponent;
    if ((opponent->action == FIGHTER_ACTION_ATTACK && opponent->animInfo->activeEndFrame >= opponent->animFrame &&
         gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 3000)) ||
        (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 2000) && (rand() & 0xf) == 0)) {
        buttons |= PADRup;
    }
    return buttons;
}

int RunAiFlankDown(Fighter *fighter)
{
    Fighter *opponent;
    int buttons;

    buttons = PADL1;
    if (fighter->action == FIGHTER_ACTION_NEUTRAL && (fighter->animInfo->flags & ANIM_FLAG_RUN) != 0) {
        buttons = PADR1 | PADL1;
    }
    if (ProbeAiTerrain(fighter, 3000, 0) == AI_TERRAIN_DROP_OFF &&
        fighter->opponent->groundY < fighter->groundY + FLOOR_BAND_HEIGHT) {
        SetAiState(fighter, AI_STATE_IDLE, 0);
        return buttons;
    }
    if (ProbeAiTerrain(fighter, 500, 0) == AI_TERRAIN_BLOCKED) {
        if ((rand() & 0x1f) == 0) {
            SetAiState(fighter, AI_STATE_SPRINT_RETREAT, 0);
            return buttons;
        }
    }
    switch (fighter->aiSubStep) {
    case 0:
        buttons |= PADLleft | PADLdown;
        if (++fighter->aiScratch.flankFrames > 10) {
            fighter->aiSubStep++;
        }
        return buttons;
    case 1:
        buttons |= PADLright | PADLdown;
        if (++fighter->aiScratch.flankFrames > 90 || RollAiApproachChance(fighter, 5) == 0) {
            fighter->aiSubStep++;
        }
        break;
    case 2:
        buttons |= PADLright | PADLdown;
        if ((rand() & 0x3f) == 0) {
            SetAiState(fighter, AI_STATE_SPRINT_APPROACH, 0);
        } else if ((rand() & 0x3f) == 0) {
            SetAiState(fighter, AI_STATE_SPRINT_RETREAT, 0);
        }
        break;
    default:
        return buttons;
    }

    opponent = fighter->opponent;
    if ((opponent->action == FIGHTER_ACTION_ATTACK && opponent->animInfo->activeEndFrame >= opponent->animFrame &&
         gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 3000)) ||
        (gFighterDistanceXZ < AI_SCALE_Q12(fighter->aiRangeScale, 2000) && (rand() & 0xf) == 0)) {
        buttons |= PADRup;
    }
    return buttons;
}

int RunAiStandFromCrouch(Fighter *fighter)
{
    return (fighter->action == FIGHTER_ACTION_CROUCH) ? PADR2 : 0;
}

int RunAiBladeLock(Fighter *fighter)
{
    int winnerSide;
    int fighterSide;
    Fighter *opponent;
    int delay;

    if (gBladeLockState == BLADE_LOCK_NONE) {
        SetAiState(fighter, AI_STATE_IDLE, 0);
        return 0;
    }
    if (fighter->aiScratch.bladeLock.opponentWinning)
        return RollAiDefenseChance(fighter, 3) ? PADLleft : 0;
    winnerSide = gBladeLockWinner;
    fighterSide = fighter->side;
    if (winnerSide == fighterSide) {
        SetAiState(fighter, AI_STATE_IDLE, 0);
        return 0;
    }
    if ((fighterSide == 0 && winnerSide == 1) || (fighterSide != 0 && winnerSide == 0)) {
        fighter->aiScratch.bladeLock.opponentWinning = 1;
        return 0;
    }
    if (gBladeLockState != BLADE_LOCK_BREAK && gBladeLockState != BLADE_LOCK_ADVANTAGE &&
        fighter->action != FIGHTER_ACTION_BLADE_LOCK) {
        opponent = fighter->opponent;
        if (opponent->bladeLockMeter < fighter->bladeLockMeter &&
            (opponent->currentInput.pressed & (PADLleft | PADLdown | PADLup)) != 0) {
            delay = 8;
            fighter->aiScratch.bladeLock.holdFrames = delay;
            return 0;
        }
    }
    delay = fighter->aiScratch.bladeLock.holdFrames;
    if (delay != 0) {
        fighter->aiScratch.bladeLock.holdFrames = delay - 1;
        return 0;
    }
    return RollAiAttackChance(fighter, 50) ? PADRright : 0;
}

int DispatchAiState(Fighter *fighter)
{
    int delay;
    int buttons;
    int stateFrames;
    int aiState;

    delay = fighter->aiDelayFrames;
    if (delay != 0) {
        if (fighter->animFrameStalled == 0) {
            fighter->aiDelayFrames = delay - 1;
        }
        buttons = 0;
    } else {
        stateFrames = fighter->aiStateFrames;
        aiState = fighter->aiState;
        fighter->aiStateFrames = stateFrames + 1;
        buttons = gAiStateHandlers[aiState](fighter);
    }
    return buttons;
}
