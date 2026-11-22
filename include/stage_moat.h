#ifndef STAGE_MOAT_H
#define STAGE_MOAT_H

#include <sys/types.h>
#include <libgte.h>

#define MOAT_WATER_SURFACE_Y 1200
/* GetMoatFloorHeight's two values: over gMoatLowerRects, and over the upper ground. */
#define MOAT_LOWER_FLOOR_Y   1200
#define MOAT_UPPER_FLOOR_Y   0

void InitMoatBillboards(int modelSlot);
void TintMoatBillboardModels(int modelSlot, CVECTOR *color);
void ConfigureSemiTransQuadPrims(int meshIndex, int modelSlot, CVECTOR *color);
int InitMoatBillboardNodes(int modelSlot);
void ResetMoatBillboardFlags(void);
void SubmitMoatBillboardPair(int side, int x, int y, int z);
int GetMoatFloorHeight(int x, int z);
void UpdateMoatFighterEffects(int side, struct SceneNode *joints);

#endif
