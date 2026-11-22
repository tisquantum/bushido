#ifndef COMBAT_HIT_H
#define COMBAT_HIT_H

#include "fighter.h"
#include <sys/types.h>
#include <libgte.h>

/* What delivered a hit to ApplyFighterHitImpact. */
typedef enum HitImpactMode {
    HIT_BY_BLADE = 0,
    HIT_BY_SUB_WEAPON = 1, /* a thrown sub-weapon (ids 1..5) */
    HIT_BY_DEBRIS = 2      /* a thrown dirt, snow or debris burst (ids 8..11) */
} HitImpactMode;

void ApplyFighterHitImpact(int mode, Fighter *fighter, int sphere, int impactAngle, int isVitalHit,
                           int guardDeflects);
int ResolveBladeClashReaction(Fighter *fighter, int impactAngle, VECTOR *contactPoint);
void FireGunshotRayAndCheckHit(Fighter *target, int *hitMask, int *vitalHitMask);
void UpdateBladeLockSeparation(void);
void UpdateBladeLockInput(Fighter *fighter);
void InitFighterBladeLock(Fighter *fighter);
void UpdateBladeLockState(void);
void ResolveFighterBodyCollision(void);

extern short gBladeLockWinner;

#endif
