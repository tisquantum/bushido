/* The fighter: its run-time record (Fighter), the motion graph's placements and triggers, each animation
 * clip's record (AnimInfo), and the action, forced-action, class and face codes the game tests. */
#ifndef FIGHTER_H
#define FIGHTER_H

#include "world_vector.h"
#include "angle.h"
#include "pose_angles.h"
#include "pad.h"
#include <sys/types.h>
#include <libgte.h>
#include "fighter_hit_sphere.h"
#include "subweapon.h"

/* Euler angles in ANGLE_FULL units per turn. */
typedef struct FighterAngles {
    short pitch, heading, roll, pad;
} FighterAngles;

/* AnimInfo.flags (+0x03) bitmask (low 2 bits encode stance 0..2). */
typedef enum AnimInfoFlag {
    ANIM_FLAG_STANCE_MASK = 0x03,
    ANIM_FLAG_RED_TRAIL = 0x04, /* bright red (255,127,127) weapon trail instead of neutral (128,128,128) */
    ANIM_FLAG_RUN = 0x08,
    ANIM_FLAG_WALK = 0x10,
    ANIM_FLAG_HOMING_JUMP = 0x20,
    ANIM_FLAG_FIXED_SPEED = 0x40,
    ANIM_FLAG_DOWNED = 0x80
} AnimInfoFlag;

/* MotionPlacement.flags (+0x12) bitmask. */
typedef enum MotionPlacementFlag {
    MOTION_FLAG_FORWARD = 0x01,
    MOTION_FLAG_FATAL = 0x02,
    MOTION_FLAG_SURFACE_SUBWEAPON = 0x04,
    MOTION_FLAG_BLEND_ON_EXIT = 0x08
} MotionPlacementFlag;

/* MotionPlacement.facingMode (+0x13) and Fighter.facingState; values >= 3 invert heading on exit. */
typedef enum MotionFacingMode {
    MOTION_FACING_AUTO = 0,
    MOTION_FACING_ON_ENTRY = 1,
    MOTION_FACING_LOCK = 2,
    MOTION_FACING_INVERTED = 3,
    MOTION_FACING_SUPINE_FLIP = 4,
    MOTION_FACING_PRONE_FLIP = 5
} MotionFacingMode;

typedef struct MotionPlacement MotionPlacement;

/* MotionTrigger.mask layout:
 *   bits 0..15:  controller button mask (libetc.h PAD* bits)
 *   bits 16..17: input edge selector (0 = pressed, 1 = released, 2 = held, 3 = notHeld)
 *   bits 18..31: characterClass bitmask (0 = any class) */
#define MOTION_TRIGGER_CLASS_MASK  0xfffc0000
#define MOTION_TRIGGER_CLASS_SHIFT 18
#define MOTION_TRIGGER_EDGE_SHIFT  16
#define MOTION_TRIGGER_EDGE_MASK   3

/* Null-terminated input trigger entry following a MotionPlacement header (+0x14). */
typedef struct MotionTrigger {
    int mask;
    MotionPlacement *next;
} MotionTrigger;

/* Combat animation state node (20-byte header + null-terminated triggers[]). */
struct MotionPlacement {
    MotionPlacement **actionTable;
    MotionPlacement *nextPlacement;
    int animClipId;
    short startFrame;
    short frameLimit;
    unsigned char triggerLimit;
    unsigned char canQueue;
    unsigned char flags;
    unsigned char facingMode;
    MotionTrigger triggers[1];
};

struct AiAttackChoice;

/* Fighter.action categories copied from AnimInfo.type. */
typedef enum FighterAction {
    FIGHTER_ACTION_NEUTRAL = 0,
    FIGHTER_ACTION_ATTACK = 1,
    FIGHTER_ACTION_GUARD = 2,
    FIGHTER_ACTION_RECOIL = 3,
    FIGHTER_ACTION_STAGGER = 4,
    FIGHTER_ACTION_FATAL_KO = 5,
    FIGHTER_ACTION_CROUCH = 6,
    FIGHTER_ACTION_HEAVY_RECOIL = 7,
    FIGHTER_ACTION_CLIMB = 9,
    FIGHTER_ACTION_FALL = 10,
    FIGHTER_ACTION_JUMP = 11,
    FIGHTER_ACTION_DOWNED = 18,
    FIGHTER_ACTION_DEAD = 19,
    FIGHTER_ACTION_SPECIAL = 20,
    FIGHTER_ACTION_DEFEATED = 21,
    FIGHTER_ACTION_PICKUP = 22,
    FIGHTER_ACTION_THROW = 23,
    FIGHTER_ACTION_BLADE_LOCK = 25,
    FIGHTER_ACTION_BLEND = 26,
    FIGHTER_ACTION_RELOAD = 27
} FighterAction;

/* Actions whose last 11 frames collect pressed buttons in Fighter.bufferedButtons; they are
 * pressed again on the frame the next motion starts. */
#define BUFFERED_INPUT_ACTIONS \
    (1 << FIGHTER_ACTION_RECOIL | 1 << FIGHTER_ACTION_STAGGER | 1 << FIGHTER_ACTION_HEAVY_RECOIL | \
     1 << 8 | 1 << FIGHTER_ACTION_SPECIAL)

#define CLAMP_SEPARATION_ACTIONS \
    (1 << FIGHTER_ACTION_NEUTRAL | 1 << FIGHTER_ACTION_ATTACK | 1 << FIGHTER_ACTION_GUARD | \
     1 << FIGHTER_ACTION_CROUCH | 1 << FIGHTER_ACTION_JUMP | 1 << FIGHTER_ACTION_DOWNED | \
     1 << FIGHTER_ACTION_SPECIAL | 1 << FIGHTER_ACTION_PICKUP | 1 << FIGHTER_ACTION_THROW | \
     1 << FIGHTER_ACTION_BLEND)

/* MotionPlacement.actionTable indices written to Fighter.pendingAction and pendingFallAction outside a blade lock. */
typedef enum ForcedAction {
    FORCED_ACTION_NONE = -1,
    FORCED_ACTION_GUARD_PARRY_BASE = 0, /* +stance (0..2); mid-stance parry (1) is used for sub-weapons */
    FORCED_ACTION_GUARD_PARRY_MID = 1,
    FORCED_ACTION_GUARD_BREAK_BASE = 3, /* +stance (3..5) */
    FORCED_ACTION_STAGGER_FRONT = 6,
    FORCED_ACTION_STAGGER_BACK = 7,
    FORCED_ACTION_LEG_CRIPPLE = 8,
    FORCED_ACTION_RIGHT_ARM_WOUND = 9,
    FORCED_ACTION_LEFT_ARM_CRIPPLE = 10,
    FORCED_ACTION_FATAL_FRONT = 11,
    FORCED_ACTION_FATAL_BACK = 12,
    FORCED_ACTION_BLADE_LOCK_ENTER = 13,
    FORCED_ACTION_CLASH_RECOIL = 14,
    FORCED_ACTION_CLIMB_BASE = 15, /* + gCharClimbActionOffset[characterClass] (15..17) */
    FORCED_ACTION_HEAD_ON_STUMBLE = 18,
    FORCED_ACTION_PRONE_LEG_CRIPPLE = 19,
    FORCED_ACTION_FALL_FRONT = 21,
    FORCED_ACTION_FALL_BACK = 22,
    FORCED_ACTION_THIGH_WOUND = 23
} ForcedAction;

/* Blade-lock MotionPlacement.actionTable indices written to Fighter.pendingAction during blade lock. */
typedef enum BladeLockAction {
    BLADE_LOCK_ACTION_DEFEND = 0,
    BLADE_LOCK_ACTION_PUSH = 1,
    BLADE_LOCK_ACTION_WIN_PUSH = 2,
    BLADE_LOCK_ACTION_LOSE_STAGGER = 3,
    BLADE_LOCK_ACTION_ADVANTAGE_EVADED = 4,
    BLADE_LOCK_ACTION_DISENGAGE = 5,
    BLADE_LOCK_ACTION_MUTUAL_BREAK = 8,
    BLADE_LOCK_ACTION_ADVANTAGE = 9
} BladeLockAction;

#define PLAYABLE_CHARACTER_COUNT     6
#define FIGHTER_PLAYABLE_ID_COUNT    12

/* Fighter.characterClass character archetypes (from gFighterIdToClass). */
#define FIGHTER_CLASS_KANNUKI        0
#define FIGHTER_CLASS_MIKADO         1
#define FIGHTER_CLASS_BLACK_LOTUS    2
#define FIGHTER_CLASS_TATSUMI        3
#define FIGHTER_CLASS_RED_SHADOW     4
#define FIGHTER_CLASS_UTSUSEMI       5
#define FIGHTER_CLASS_FINAL_BOSS     6
#define FIGHTER_CLASS_FINAL_BOSS_ALT 7
#define FIGHTER_CLASS_KATZE          8
#define FIGHTER_CLASS_BOUT8_BOSS     9
#define FIGHTER_CLASS_BOUT6_BOSS     10
#define FIGHTER_CLASS_BOUT7_BOSS     11
#define FIGHTER_CLASS_COUNT          12

/* Fighter.faceExpression facial expression IDs (3 eye states x 2 mouth states). */
typedef enum FighterFaceExpression {
    FIGHTER_FACE_LOCKED_NEUTRAL = -1,
    FIGHTER_FACE_NORMAL = 0,
    FIGHTER_FACE_DEAD = 1,
    FIGHTER_FACE_LETHAL = 2,
    FIGHTER_FACE_NORMAL_OPEN_MOUTH = 3,
    FIGHTER_FACE_CLOSED_EYES_OPEN_MOUTH = 4,
    FIGHTER_FACE_WOUND = 5
} FighterFaceExpression;

/* Fighter.weaponGripType weapon handedness. */
typedef enum FighterWeaponGrip {
    FIGHTER_GRIP_ONE_HANDED = 1,
    FIGHTER_GRIP_TWO_HANDED = 2
} FighterWeaponGrip;

/* Fighter.aiScratch: one AI state's working values; each state that reads them sets them first. */
typedef union AiStateScratch {
    struct {
        int lastX;
        int lastZ;
        int retreatFrames;
    } sprint;
    struct {
        int opponentWinning;
        int holdFrames;
    } bladeLock;
    int flankFrames;
    int evadeChoice;
    unsigned char *crouchAttackScript; /* written, never read */
} AiStateScratch;

/* Runtime combatant state (gFighters[2]; 0x328 bytes, 0x318 in the Japanese build). */
typedef struct Fighter {
    short aiOscAngle;
    short aiOscSine;
    short aiOscCosine;
    short aiRangeScale;
    short aiBelowOpponent;
    short aiOscUnused[3]; /* nothing reads or writes them */
    short aiSkillRating;
    short aiState;
    short aiSubStep;
    short aiScriptTicks;
    PadInputFrame aiPrevInput;
    struct AiAttackChoice **aiAttackTables;
    short aiAttackCounts[3];
    short aiCycleIndex;
    int aiStateFrames;
    int aiDelayFrames;
    unsigned char *aiScriptPtr;
    unsigned int aiScriptButtons;
    AiStateScratch aiScratch;
    int aiScratchUnused; /* nothing reads or writes it */
    short aiForceDefend;
    unsigned char aiDefenseDelay;
    unsigned char aiAttackDelay;
    struct Fighter *opponent;
    short side;
    short isCpu;
    short characterClass;
    short characterPairSlot;
    short fighterId;
    short weaponId;
    short weaponPairSlot;
    short weaponGripType;
    short statureScale;
    short baseAnimSpeed;
    short baseClashPower;
    short tractionWeight;
    short clashPower;
    PadInputFrame currentInput;
    unsigned int activeFrames;
    MotionPlacement *placement;
    struct AnimInfo *animInfo;
    short action;
    short animClipId;
    short animBaseFrame;
    short animFrame;
    short animFrac;
    short animSpeed;
    MotionPlacement *queuedPlacement;
    short motionJustChanged;
    short hitActive;
    short lookRoll;
    short lookYaw;
    short lookPitch;
    short isPovMode;
    unsigned short surfaceFlags;
    VECTOR anchor;
    WorldVector rootOffset;
    VECTOR position;
    VECTOR frameDelta;
    VECTOR weaponTipVel;
    WorldVector velocity;
    int velocityUnused; /* nothing reads or writes it */
    int groundY;
    int shadowGroundY;
    short turnRemaining;
    short velocityRetention;
    WorldVector cameraTargetPos;
    WorldVector headPos;
    WorldVector torsoCenter;
    FighterAngles angles;
    FighterAngles savedAngles;
    short angleToOpponent;
    short isVerticalSwing; /* always 0: IsWeaponSwingPlaneVertical is never true */
    short attackActive;
    short pendingAction;
    short lastForcedAction;
    short prevAction;
    WorldVector prevRootOffset;
    WorldVector rootOffsetDelta;
    WorldVector prevWeaponPoints[4];
    short facingState;
    short prevFacingState;
    WorldVector prevRawRoot;
    MotionPlacement *prevPlacement;
    struct AnimInfo *prevAnimInfo;
    short lastSampledFrame;
    short nearWallContact;
    WorldVector wallContactPos;
    SVECTOR wallContactNormal;
    short unreadLimbFlag; /* zeroed on spawn; nothing reads it */
    short leftArmIntact;
    short legsIntact;
    short torsoWoundCount;
    short thighWoundCount;
    short rightArmWoundCount;
    short fallGravityTicks;
    short fallState;
    short pendingFallAction;
    short climbState;
    short jumpState;
    short isHomingJump;
    WorldVector jumpVel;
    int jumpLaunchParams[2];
    short rootFreezeState;
    WorldVector frozenRootOffset;
    short animFrameStalled;
    short onSlopeBand;
    WorldVector leftFootPos;
    WorldVector rightFootPos;
    int bladeLockMeter;
    short bladeLockTimers[4];
    short isSupine;
    short subWeaponCount;
    short subWeaponIds[SUB_WEAPON_MAX];
    FighterPose savedPose;
    short poseBlendWeight;
    short poseBlendStep;
    /* 4-byte aligned: the Japanese build pads 2 bytes here after its shorter subWeaponIds. */
    WorldVector blendSavedRoot;
    short runSpeed;
    short lastTurnStep;
    short lockedMoveHeading;
    unsigned short rumbleIntensity;
    short rumbleTimer;
    short gunAmmo;
    short gunMagazines;
    unsigned short bufferedButtons;
    short isStationary;
    int pitFallTimer;
    WorldVector envPushVel;
    SVECTOR knockbackDir;
    short faceExpression;
    short flipHeadingNextTick;
} Fighter;

/* One animation clip's record in the motion file's animInfo table, 1-based by MotionPlacement.animClipId. */
typedef struct AnimInfo {
    unsigned char type;
    unsigned char activeStartFrame;
    unsigned char activeEndFrame;
    unsigned char flags;
    unsigned char *frameEvents;
} AnimInfo;

/* Scratchpad motion work area at 0x1F8002A0 used by UpdateFighterFrame and floor movement helpers. */
typedef struct FighterMotionScratch {
    WorldVector currentRoot;
    WorldVector nextRoot;
    WorldVector dest;
    SVECTOR *normal;
    int slideVx;
    int slideVz;
} FighterMotionScratch;

extern short gWoundSpeedScales[4][4];
extern FighterHitSphere gBaseHitSpheres[22];
extern unsigned char gWeaponEdgePointCounts[12];
extern unsigned char gWeaponTrailCount[12];
/* One slash trail's two control points, indices into CollisionScratch.weaponPoints; equal where the
 * weapon has no such trail. */
typedef struct WeaponTrailSpan {
    unsigned char first;
    unsigned char last;
} WeaponTrailSpan;

extern WeaponTrailSpan gWeaponTrailEndpoints[10][2];
extern SVECTOR *gWeaponEdgeOffsets[11];
extern unsigned char gCharClimbActionOffset[FIGHTER_CLASS_COUNT];

void InitFighterForScene(int stageId, int prevStageId, int side, int isCpu,
                         int fighterId, int weaponId, int resetMatchState);
void SetFighterMotionPlacement(Fighter *fighter, MotionPlacement *placement);
void StartWinnerVictoryMotion(void);
void StartFighterFallFromLedge(Fighter *fighter);
void TryStartWallBumpStumble(Fighter *fighter, int x, int z);
void RecordWallContactPoint(Fighter *fighter, FighterMotionScratch *contact);
int IsNearRecordedWallContact(Fighter *fighter);
void ClampFighterMaxSeparation(Fighter *fighter, FighterMotionScratch *proposal);
int SlideFighterAlongObstacle(Fighter *fighter, FighterMotionScratch *contact, int floorHeight, int mode);
void TriggerFighterRumble(Fighter *fighter, int index);
void UpdateFighterLookAngles(Fighter *fighter);
void FlipPoseForClimbTransition(PoseAngles *joints);
void FlipPoseFromSupineState(PoseAngles *joints);
void FlipPoseFromProneState(PoseAngles *joints);
void ApplyDisabledLeftArmPose(PoseAngles *joints);
void UpdateFighterFrame(int player, PadInputFrame *input);

extern Fighter gFighters[2];

#endif
