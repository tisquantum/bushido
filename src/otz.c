/* The renderer's primitive dispatch tables, the skinned fighter draw that swaps in the skinned
 * table, and the compression of a depth into an ordering-table bucket. */
#include <sys/types.h>
#include <libgte.h>

#include "cuttable_prop.h"
#include "display_list.h"
#include "globals.h"
#include "light.h"
#include "otz.h"
#include "pov_overlay.h"
#include "render.h"
#include "render_scratch.h"
#include "model_table.h"

static MATRIX gSkinnedViewMatrix;

/* Indexed by primitive type: four per family (F3, FT3, F4, FT4, G3, GT3, G4, GT4), the low two bits
 * the lighting (0 lit, 1 unlit, 2 lit on both sides). */
PrimRenderFn gPrimRenderDispatch[38] = {
    /*  0: F3  */ DrawPolyF3Lit,  TrapInvalidPrimType, TrapInvalidPrimType,  TrapInvalidPrimType,
    /*  4: FT3 */ DrawPolyFt3Lit, DrawPolyFt3Unlit,    DrawPolyFt3DoubleLit, TrapInvalidPrimType,
    /*  8: F4  */ DrawPolyF4Lit,  TrapInvalidPrimType, TrapInvalidPrimType,  TrapInvalidPrimType,
    /* 12: FT4 */ DrawPolyFt4Lit, DrawPolyFt4Unlit,    DrawPolyFt4DoubleLit, TrapInvalidPrimType,
    /* 16: G3  */ DrawPolyG3Lit,  TrapInvalidPrimType, TrapInvalidPrimType,  TrapInvalidPrimType,
    /* 20: GT3 */ DrawPolyGt3Lit, TrapInvalidPrimType, DrawPolyGt3DoubleLit, TrapInvalidPrimType,
    /* 24: G4  */ DrawPolyG4Lit,  TrapInvalidPrimType, TrapInvalidPrimType,  TrapInvalidPrimType,
    /* 28: GT4 */ DrawPolyGt4Lit, TrapInvalidPrimType, DrawPolyGt4DoubleLit, TrapInvalidPrimType,
    /* 32..35  */ TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 36..37  */ TrapInvalidPrimType, DrawPolyF4WithDrawMode,
};

PrimRenderFn gSkinnedPrimRenderDispatch[42] = {
    /*  0: F3  */ IgnoreSkinnedPolyF3, TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /*  4: FT3 */ TrapInvalidPrimType,     TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /*  8: F4  */ IgnoreSkinnedPolyF4, TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 12: FT4 */ TrapInvalidPrimType,     TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 16: G3  */ DrawPolyG3Lit,           TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 20: GT3 */ TrapInvalidPrimType,     TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 24: G4  */ DrawPolyG4Lit,           TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 28: GT4 */ TrapInvalidPrimType,     TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 32..35  */ TrapInvalidPrimType,     TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 36..37  */ TrapInvalidPrimType,     TrapInvalidPrimType,
    /* 38..41  */ DrawSkinnedPolyFt3Prelit, DrawSkinnedPolyFt4Prelit,
                  DrawSkinnedPolyGt3Prelit, DrawSkinnedPolyGt4Prelit,
};

/* For mesh batch kind 1 (6-byte vertices). */
PrimRenderFn gDynamicPrimRenderDispatch[44] = {
    /*  0: F3  */ TrapInvalidPrimType,     TrapInvalidPrimType,     TrapInvalidPrimType,           TrapInvalidPrimType,
    /*  4: FT3 */ DrawPolyFt3LitDynamic,   TrapInvalidPrimType,     DrawPolyFt3DoubleLitDynamic,   TrapInvalidPrimType,
    /*  8: F4  */ TrapInvalidPrimType,     TrapInvalidPrimType,     TrapInvalidPrimType,           TrapInvalidPrimType,
    /* 12: FT4 */ DrawPolyFt4LitDynamic,   TrapInvalidPrimType,     DrawPolyFt4DoubleLitDynamic,   TrapInvalidPrimType,
    /* 16: G3  */ TrapInvalidPrimType,     TrapInvalidPrimType,     TrapInvalidPrimType,           TrapInvalidPrimType,
    /* 20: GT3 */ DrawPolyGt3LitDynamic,   TrapInvalidPrimType,     TrapInvalidPrimType,           TrapInvalidPrimType,
    /* 24: G4  */ TrapInvalidPrimType,     TrapInvalidPrimType,     TrapInvalidPrimType,           TrapInvalidPrimType,
    /* 28: GT4 */ DrawPolyGt4LitDynamic,   TrapInvalidPrimType,     TrapInvalidPrimType,           TrapInvalidPrimType,
    /* 32..35  */ DrawShadowPolyF3Dynamic, DrawShadowPolyF4Dynamic, TrapInvalidPrimType,           TrapInvalidPrimType,
    /* 36..39  */ TrapInvalidPrimType,     TrapInvalidPrimType,     TrapInvalidPrimType,           TrapInvalidPrimType,
    /* 40..43  */ TrapInvalidPrimType,     TrapInvalidPrimType,     DrawFadingPolyFt3,           DrawFadingPolyFt4,
};

/* For mesh batch kind 2, the first-person mannequin. */
PrimRenderFn gPovPrimRenderDispatch[32] = {
    /*  0: F3  */ DrawOverlayPolyF3,  TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /*  4: FT3 */ DrawOverlayPolyFt3, TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /*  8: F4  */ DrawOverlayPolyF4,  TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 12: FT4 */ DrawOverlayPolyFt4, TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 16: G3  */ TrapInvalidPrimType,  TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 20: GT3 */ TrapInvalidPrimType,  TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 24: G4  */ TrapInvalidPrimType,  TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
    /* 28: GT4 */ TrapInvalidPrimType,  TrapInvalidPrimType, TrapInvalidPrimType, TrapInvalidPrimType,
};

/* The skinned fighter's light slot, read by PrelightSkinnedMeshNormals in render.s. */
LightSlot *gSkinnedLightSlot;

void DrawSkinnedFighterModel(MATRIX **boneMatrices, void *modelData, short lightSlotIndex)
{
    int *scaleShiftPtr = &RENDER_SCRATCH->scaleShift;
    int scaleShift;

    gSkinnedViewMatrix = gCombinedViewMatrix;
    gSkinnedLightSlot = &gLightSlots[lightSlotIndex];
    RENDER_SCRATCH->primTable = gSkinnedPrimRenderDispatch;

    scaleShift = *scaleShiftPtr;
    *scaleShiftPtr = 3 - scaleShift;
    gSkinnedViewMatrix.m[1][0] >>= 1;
    gSkinnedViewMatrix.m[1][1] >>= 1;
    gSkinnedViewMatrix.m[1][2] >>= 1;
    gSkinnedViewMatrix.t[1] >>= 1;

    RenderSkinnedSkeletalMesh(boneMatrices, modelData, &gSkinnedViewMatrix, scaleShift);
    RENDER_SCRATCH->primTable = gPrimRenderDispatch;
}

int GetDepthScaleShift(int distance)
{
    if (distance > 0x16a09)
        return 3;
    if (distance > 0xb500)
        return 2;
    if (distance > 0x5a00)
        return 1;
    return 0;
}

/* An old-style definition: callers pass shift as an int, and the routine narrows it to short. */
int CompressDepthToOtz(depth, shift)
    int depth;
    short shift;
{
    int scaled = depth >> (3 - shift);
    int exponent = scaled >> 11;

    return (((scaled & 0x7ff) | ~0xfff) >> exponent) & 0xfff;
}

#ifdef VERSION_JP
#define DEPTH_MANTISSA_BITS 12
#else
#define DEPTH_MANTISSA_BITS 11
#endif

/* Nothing calls it. */
static int CompressDepthToOtzScratch(int depth)
{
    int scaled = depth >> RENDER_SCRATCH->scaleShift;
    int exponent = scaled >> DEPTH_MANTISSA_BITS;

    return (((scaled & ((1 << DEPTH_MANTISSA_BITS) - 1)) | ~((2 << DEPTH_MANTISSA_BITS) - 1)) >> exponent) & 0xfff;
}
