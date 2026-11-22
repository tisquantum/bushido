#ifndef EFFECT_H
#define EFFECT_H

#include <sys/types.h>
#include <libgte.h>
#include "effect_slot.h"

void ClearEffectPool(void);
void UpdateAndDrawEffects(void);

EffectSlot *AllocEffectSlot(void);
int *DrawEffectMesh(int *meshData);
int SetupEffectTransform(int mode, int x, int y, int z);
void SpawnEffect(int type, long x, long y, long z);
void UpdateSpriteEffect(EffectSlot *effect);
void SpawnStage4LightGlows(void);
void UpdateStage4LightGlow(EffectSlot *effect);
void SpawnWaterSplashEffect(int x, int y, int z);
void UpdateWaterRingEffect(EffectSlot *effect);
void UpdateWaterSplashBillboard(EffectSlot *effect);
void SpawnWaterRippleEffect(int x, int y, int z);
void UpdateWaterRippleEffect(EffectSlot *effect);
void UpdateUvAnimEffect(EffectSlot *effect);
void SpawnBladeLockFlashEffect(int x, int y, int z);
void UpdateBladeLockFlashEffect(EffectSlot *effect);
void UpdateBallisticParticle(EffectSlot *effect);
void SpawnParticleBurst(int kind, VECTOR *position, int *direction);
void UpdateBloodDropParticle(EffectSlot *effect);
void SpawnBloodSpray(VECTOR *position, VECTOR *direction);
void UpdateSprayParticle(EffectSlot *effect);
void SpawnDirectionalSpray(int kind, VECTOR *position, VECTOR *direction, int scale);
void SpawnDirectionalSprayUnit(int kind, VECTOR *position, VECTOR *direction);

#endif
