#ifndef DECAL_H
#define DECAL_H

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include "billboard.h"

/* UpdateAndBuildGroundDecals's working record in the scratchpad (render_scratch.h): the half-height
 * camera matrix in, the packet cursor in and out (the deferred packet list and depths follow it). */
typedef struct GroundDecalScratch {
    MATRIX matrix;
    long translation[3];
    int unused0; /* 4 bytes nothing reads or writes */
    POLY_FT4 *packets;
    int unused1; /* 4 bytes nothing reads or writes */
    SVECTOR corners[4];
    int unused2[3]; /* 12 bytes nothing reads or writes */
    long opz;
} GroundDecalScratch;

extern EffectTextureEntry gGroundDecalTexture;

void ClearGroundDecals(void);
void InitGroundDecalAtlas(EffectTextureEntry *texture);
void SpawnGroundDecal(int uvIndex, int *position, int angle);
int UpdateAndBuildGroundDecals(GroundDecalScratch *scratch);

#endif
