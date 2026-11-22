#ifndef TPAGE_RELOC_H
#define TPAGE_RELOC_H

#include <libgpu.h>
#include "scene_node.h"

extern short gPrimRecordHalfwords[44];

void OffsetNodeMeshTPageClut(struct SceneNode *node, short pageDx, short pageDy, short clutDx, short clutDy);
void OffsetMeshStreamTPageClut(short pageDx, short pageDy, short clutDx, short clutDy);
void OffsetFlatTexturedTriTPageClut(POLY_FT3 *poly, int pageDx, int pageDy, int clutDx, int clutDy);
void OffsetFlatTexturedQuadTPageClut(POLY_FT4 *poly, int pageDx, int pageDy, int clutDx, int clutDy);
void OffsetGouraudTexturedTriTPageClut(POLY_GT3 *poly, int pageDx, int pageDy, int clutDx, int clutDy);
void OffsetGouraudTexturedQuadTPageClut(POLY_GT4 *poly, int pageDx, int pageDy, int clutDx, int clutDy);
short OffsetPackedTPage(short tpage, int pageDx, int pageDy);
short OffsetPackedClut(short clut, int clutDx, int clutDy);
void OffsetModelSlotTPageClut(int modelSlot, int pageDx, int pageDy, int clutDx, int clutDy);

#endif
