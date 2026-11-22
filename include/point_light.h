#ifndef POINT_LIGHT_H
#define POINT_LIGHT_H

#include "world_vector.h"

void StopPointLightFlash(void);

int TriggerPointLightFlash(int presetIndex, int x, int y, int z);
int TriggerAltPointLightFlash(int presetIndex, int x, int y, int z);
void ResetPointLightTargets(void);
void RegisterPointLightTarget(int side, WorldVector *position);
void ApplyPointLightToTargets(void);

#endif
