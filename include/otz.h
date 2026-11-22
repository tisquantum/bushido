#ifndef OTZ_H
#define OTZ_H

#include <sys/types.h>
#include <libgte.h>
#include "render_scratch.h"

/* CompressDepthToOtz returns a bucket below this. */
#define OTZ_BUCKET_COUNT 4096

extern PrimRenderFn gPrimRenderDispatch[38];

void DrawSkinnedFighterModel(MATRIX **boneMatrices, void *modelData, short lightSlotIndex);
int GetDepthScaleShift(int distance);
int CompressDepthToOtz(int depth, int shift);

#endif
