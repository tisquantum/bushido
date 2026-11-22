#ifndef LIGHT_INIT_H
#define LIGHT_INIT_H

#include <sys/types.h>
#include <libgte.h>
#include "light.h"

void InitSceneCameras(void);
void InitLightSlots(void);
void RebuildLightDirectionMatrix(LightSlot *slot);
void ApplyActiveLightColorToGte(void);
void InitPropIdentityMatrix3x3(MATRIX *matrix);

#endif
