/* CPU fighter AI constants, state and terrain enums, attack-choice archive entries,
 * and per-character Q12 tendency parameters used by fighter_ai.c. */
#ifndef FIGHTER_AI_H
#define FIGHTER_AI_H

#include "pad.h"
#include "fighter.h"

#define AI_Q12_SHIFT       12
#define AI_Q12_MASK        0xfff
#define AI_SCALE_Q12(s, d) (((s) * (d)) >> AI_Q12_SHIFT)

#define AI_SLASH_BOSS_BOUT_INDEX  9
#define AI_SLASH_SCRIPT_LEVEL_MAX 9
#define AI_POV_VS_WIN_CAP         10

/* Stance values encoded in AnimInfo.flags & ANIM_FLAG_STANCE_MASK. */
#define STANCE_HIGH  0
#define STANCE_MID   1
#define STANCE_LOW   2
#define STANCE_COUNT 3

/* AI input-script bytecode. */
#define AI_SCRIPT_OP_CHAR_MASK   0x40
#define AI_SCRIPT_OP_END         0x80
#define AI_SCRIPT_OP_HOLD_FLAG   0x80
#define AI_SCRIPT_OP_HOLD_MASK   0x7f
#define AI_SCRIPT_OP_RELEASE     0x10
#define AI_SCRIPT_OP_BUTTON_MASK 0x0f

/* Pad button bit numbers: <libetc.h>'s masks are 1 << these. */
#define AI_BTN_L2       0 
#define AI_BTN_R2       1 
#define AI_BTN_L1       2 
#define AI_BTN_R1       3 
#define AI_BTN_TRIANGLE 4
#define AI_BTN_CIRCLE   5
#define AI_BTN_CROSS    6
#define AI_BTN_SQUARE   7
#define AI_BTN_UP       12
#define AI_BTN_FORWARD  13 /* PADLright (toward opponent) */
#define AI_BTN_DOWN     14
#define AI_BTN_BACK     15 /* PADLleft (away from opponent) */

#define AI_SCRIPT_PRESS(btn)   (btn)
#define AI_SCRIPT_RELEASE(btn) (AI_SCRIPT_OP_RELEASE | (btn))
#define AI_SCRIPT_HOLD(frames) (AI_SCRIPT_OP_HOLD_FLAG | (frames))
#define AI_SCRIPT_END          AI_SCRIPT_OP_END

typedef enum AiState {
    AI_STATE_IDLE = 0,
    AI_STATE_DEFEND_OR_EVADE = 1,
    AI_STATE_ATTACK = 2,
    AI_STATE_WAIT_OPPONENT_DOWN = 3,
    AI_STATE_GROUNDED = 4,
    AI_STATE_CROUCHED = 5,
    AI_STATE_ROLL_DOWN = 6,
    AI_STATE_ROLL_UP = 7,
    AI_STATE_GET_UP_FORWARD = 8,
    AI_STATE_GET_UP_BACKWARD = 9,
    AI_STATE_BACK_FORWARD_ATTACK = 10,
    AI_STATE_WALK_FORWARD = 11,
    AI_STATE_WALK_BACKWARD = 12, /* nothing enters it */
    AI_STATE_SPRINT_APPROACH = 13,
    AI_STATE_DASH_FORWARD = 14,
    AI_STATE_CROUCHED_ATTACK = 15,
    AI_STATE_GROUNDED_RETREAT_ATTACK = 16,
    AI_STATE_CRAWL_FORWARD = 17,
    AI_STATE_CRAWL_BACKWARD = 18,
    AI_STATE_CROUCH_SUB_WEAPON_THROW = 19,
    AI_STATE_CROUCH_FORWARD_R2 = 20,
    AI_STATE_SPRINT_RETREAT = 21,
    AI_STATE_BACKFLIP_RETREAT = 22,
    AI_STATE_LEAP_FORWARD = 23,
    AI_STATE_FLANK_UP = 24,
    AI_STATE_FLANK_DOWN = 25,
    AI_STATE_STAND_FROM_CROUCH = 26,
    AI_STATE_BLADE_LOCK = 27,
    AI_STATE_SIDESTEP_UP = 28,
    AI_STATE_SIDESTEP_DOWN = 29 /* nothing enters it */
} AiState;

typedef enum AiTerrainKind {
    AI_TERRAIN_LEVEL = 0,
    AI_TERRAIN_DROP_OFF = 1,
    AI_TERRAIN_SLOPE_DOWN = 2,
    AI_TERRAIN_BLOCKED = 3
} AiTerrainKind;

/* AiAttackChoice.flags (+0x00): halves the distance/penalty score when the condition holds. */
typedef enum AiAttackFlag {
    AI_ATTACK_PREFER_CRIPPLED = 0x01,
    AI_ATTACK_PREFER_DOWNED = 0x02
} AiAttackFlag;

/* 8-byte stance attack scoring entry in the loaded weapon-pair motion archive (MOTION_FILE->aiAttackTables). */
typedef struct AiAttackChoice {
    unsigned char flags;    /* +0x00: AiAttackFlag preference bits */
    unsigned char unused;   /* +0x01: never set; read only within the zero first word ending a list */
    unsigned char distance; /* +0x02: preferred engagement range (units of 20) */
    unsigned char penalty;  /* +0x03: additive score penalty (shifted left by 5) */
    unsigned char *script;  /* +0x04: AI input-script bytecode stream */
} AiAttackChoice;

/* Per-character AI tendencies as Q12 chances (4096 = always), indexed by Fighter.characterClass. */
typedef struct AiCharParams {
    short approach;
    short leap;
    short attack;
    short defense;
    short unusedChance; /* read only by RollAiUnusedChance, which nothing calls */
    short evade;
    short recovery;
} AiCharParams;

extern int gAiMoveCycleMode;

void SetAiState(Fighter *fighter, int state, int delay);
void InitFighterAi(int fighterIndex, int skillRating);
void UpdateFighterAiInput(int player, struct PadInputFrame *out);
void EnterAiSprintApproach(Fighter *fighter);
int RunAiDefendOrEvade(Fighter *fighter);
int RunAiSprintApproach(Fighter *fighter);
int RunAiDashForward(Fighter *fighter);
int RunAiCrouchedAttack(Fighter *fighter);
int RunAiGroundedRetreatAttack(Fighter *fighter);
int RunAiCrawlForward(Fighter *fighter);
int RunAiCrawlBackward(Fighter *fighter);
int RunAiCrouchSubWeaponThrow(Fighter *fighter);
int RunAiCrouchForwardR2(Fighter *fighter);
int RunAiSprintRetreat(Fighter *fighter);
int RunAiBackflipRetreat(Fighter *fighter);
int RunAiSidestepUp(Fighter *fighter);
int RunAiSidestepDown(Fighter *fighter);
int RunAiLeapForward(Fighter *fighter);
int RunAiFlankUp(Fighter *fighter);
int RunAiFlankDown(Fighter *fighter);
int RunAiStandFromCrouch(Fighter *fighter);
int RunAiBladeLock(Fighter *fighter);
int DispatchAiState(Fighter *fighter);
int ProbeAiTerrain(Fighter *fighter, int distance, int angleOffset);
int ShouldAiDefendAttack(Fighter *fighter);

#endif
