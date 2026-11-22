#ifndef FIGHTER_HIT_SPHERE_H
#define FIGHTER_HIT_SPHERE_H

#include <sys/types.h>

#define FIGHTER_HIT_SPHERE_COUNT 22
#define FIGHTER_VITAL_SPHERE_COUNT 6
#define FIGHTER_SPHERE_TORSO       1
#define FIGHTER_SPHERE_RIGHT_THIGH 15
#define FIGHTER_SPHERE_LEFT_THIGH  19

/* gWeaponEdgeOffsets entries: weapons 0..9 plus the final-boss alt override at index 10. */
#define WEAPON_EDGE_OFFSET_COUNT          11
#define WEAPON_EDGE_OFFSET_FINAL_BOSS_ALT 10

/*
 * 20-byte (0x14) body hit-sphere descriptor: canonical unscaled table in gBaseHitSpheres[22],
 * scaled per-fighter copies in gFighterHitSpheres[2][22] via ScaleFighterHitSpheres.
 * Indices: 0 head, 1..3 torso, 4..5 pelvis (isVital = 1); 6..9 left arm, 10..13 right arm,
 * 14..17 right leg, 18..21 left leg (isVital = 0).
 */
typedef struct FighterHitSphere {
    short isVital;         /* +0x00: 1 = vital zone (0..5, has inner vitalRadius), 0 = limb (6..21) */
    short boneIndex;       /* +0x02: fighter skeleton bone matrix index (GetFighterWorldMatrixList) */
    short localOffset[3];  /* +0x04: bone-local (x, y, z) center, Q12 stature-scaled; loaded via gte_ldv0 */
    short localOffsetPad;  /* +0x0A: SVECTOR.pad completing the 8-byte GTE vector read by gte_ldv0 (always 0) */
    u_short outerRadius;   /* +0x0C: outer hit-sphere radius (Q12 stature-scaled) */
    u_short outerRadiusSq; /* +0x0E: outerRadius squared in the base table; ScaleFighterHitSpheres scales it linearly */
    u_short vitalRadius;   /* +0x10: inner lethal hit-sphere radius (0 for limbs) */
    u_short vitalRadiusSq; /* +0x12: vitalRadius squared in the base table, scaled likewise (0 for limbs) */
} FighterHitSphere;

extern FighterHitSphere gFighterHitSpheres[2][FIGHTER_HIT_SPHERE_COUNT];

#endif
