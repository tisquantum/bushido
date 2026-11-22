#ifndef SUBWEAPON_H
#define SUBWEAPON_H

#include "world_vector.h"
#include "combat_event.h"

/* Sub-weapons a fighter can carry (Fighter.subWeaponIds), and slots in gSubWeaponSlots. */
#ifdef VERSION_JP
#define SUB_WEAPON_MAX 11
#else
#define SUB_WEAPON_MAX 16
#endif

/* SubWeaponSlot.id: 0 = empty, 1..5 = thrown character sub-weapons,
 * 8..11 = thrown surface debris bursts (surfaceType + 7; id - 8 indexes SpawnParticleBurst). */
#define SUB_WEAPON_NONE         0
#define SUB_WEAPON_SURFACE_BASE 8

/* Launch and motion-trail descriptor in gSubWeaponParams[12]. */
typedef struct SubWeaponParams {
    short speed;         /* horizontal launch speed (Q12-scaled by sin/cos) */
    short verticalSpeed; /* initial Y velocity */
    short spin;          /* initial yaw angular velocity (spinY) */
    short trailWidth;    /* half-width of rendered motion trail */
    int trailRgb;        /* 24-bit RGB trail color (0 disables trail) */
} SubWeaponParams;

/* SubWeaponSlot.hitState live-combat values (replay stores SubWeaponHistoryMode). */
typedef enum SubWeaponHitState {
    SUB_WEAPON_HIT_INACTIVE = 0,
    SUB_WEAPON_HIT_ACTIVE = 1
} SubWeaponHitState;

/* Kinematic and orientation state of a SubWeaponSlot. */
typedef struct SubWeaponMotion {
    WorldVector position;
    WorldVector prevPosition;    /* last frame's, for the hit sweep and the trail */
    int velocityX;               /* with velocityY and velocityZ, passed to the GTE as one vector */
    int velocityY;
    int velocityZ;
    int isMoving;                /* 0 once it rests on the ground and can be picked up */
    short angleX;                /* zeroed on spawn; nothing reads it */
    short angleY;
    short angleZ;                /* zeroed on spawn; nothing reads it */
    short stageId;               /* the stage the item lies in */
    short spinX;                 /* zeroed on spawn; nothing reads it */
    short spinY;                 /* scaled by 7/8 each frame while it moves */
    short spinZ;                 /* zeroed on spawn; nothing reads it */
    short pad;                   /* 2 bytes; nothing reads or writes them */
} SubWeaponMotion;

/* Thrown sub-weapon / surface debris slot in gSubWeaponSlots. */
typedef struct SubWeaponSlot {
    short id;                    /* SUB_WEAPON_NONE, 1..5 sub-weapon, or 8..11 debris burst */
    short hitState;              /* SubWeaponHitState in live combat; SubWeaponHistoryMode in replay */
    int owner;                   /* throwing fighter side (0 = P1, 1 = P2) */
    SubWeaponMotion motion;
} SubWeaponSlot;

/* SubWeaponModel.kind entry state in gSubWeaponModels[3]. */
typedef enum SubWeaponModelKind {
    SUB_WEAPON_MODEL_NONE = -1,
    SUB_WEAPON_MODEL_FIGHTER = 0, /* P1/P2 fighter character-archive sub-weapon mesh */
    SUB_WEAPON_MODEL_STAGE4 = 1   /* Stage 4 slot 2 marker (never drawn by SubmitSubWeaponNode) */
} SubWeaponModelKind;

#define SUB_WEAPON_MODEL_SLOT_STAGE4 2
#define SUB_WEAPON_MODEL_COUNT       3

/* Sub-weapon model descriptor in gSubWeaponModels[3]:
 * [0..1] P1/P2 fighter sub-weapons, [2] Stage 4 slot. */
typedef struct SubWeaponModel {
    int kind;      /* SubWeaponModelKind */
    int meshIndex; /* sub-model index within modelSlot (-1 when unset) */
    int modelSlot; /* gModelTables slot from GetFighterModelTableSlot (-1 when unset) */
} SubWeaponModel;

/* SubWeaponHistoryEntry ring dimensions per slot. */
#define SUB_WEAPON_HISTORY_COUNT 16
#define SUB_WEAPON_HISTORY_MASK  (SUB_WEAPON_HISTORY_COUNT - 1)

/* SubWeaponHistoryEntry.frame sentinel for an unused ring slot. */
#define SUB_WEAPON_HISTORY_EMPTY (-1)

/* SubWeaponHistoryEntry.flagsAndAngle bit masks. */
#define SUB_WEAPON_HISTORY_ANGLE_MASK 0x0FFF
#define SUB_WEAPON_HISTORY_MODE_MASK  0x3000
#define SUB_WEAPON_HISTORY_MOVING     0x8000

/* SubWeaponHistoryEntry.flagsAndAngle bits 13..12 replay event modes. */
typedef enum SubWeaponHistoryMode {
    SUB_WEAPON_HISTORY_CLEARED = 0x0000,
    SUB_WEAPON_HISTORY_THROWN = 0x1000,
    SUB_WEAPON_HISTORY_DEFLECTED = 0x2000,
    SUB_WEAPON_HISTORY_SETTLED = 0x3000
} SubWeaponHistoryMode;

/* Circular replay snapshot in gSubWeaponHistory[SUB_WEAPON_MAX][SUB_WEAPON_HISTORY_COUNT]. */
typedef struct SubWeaponHistoryEntry {
    int frame;                    /* gCombatFrameCounter, or SUB_WEAPON_HISTORY_EMPTY */
    int x;
    int z;
    short y;                      /* world Y truncated to 16 bits */
    short velocity[3];            /* (vx, vy, vz) truncated to 16 bits */
    unsigned short flagsAndAngle; /* bit 15 moving, bits 13..12 mode, bits 11..0 angleY */
    short spinSpeed;              /* yaw angular velocity */
    short id;                     /* 0 empty, 1..5 thrown sub-weapon, 8..11 debris burst */
} SubWeaponHistoryEntry;

extern unsigned char gSubWeaponModelOwner[16];  /* by sub-weapon ID: the fighter side whose model draws it */

struct Fighter;

void UpdateReplaySubWeapons(void);
void RewindSubWeaponsForReplay(void);
void ClearSubWeaponSlots(void);
void ResetSubWeaponHistory(void);
void InitFighterSubWeapons(struct Fighter *fighter);
void UpdateAndDrawSubWeapons(void);
void CheckSubWeaponFighterHits(void);
int ThrowFighterSubWeapon(struct Fighter *fighter, int surfaceType);
int PickupNearbySubWeapons(struct Fighter *fighter);

#endif
