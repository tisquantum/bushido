/* Scene display-list submission: the frame's view matrices and scratchpad parameters for the
 * assembly renderer, and the trap that fills the unused slots of its primitive dispatch tables. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include "display_buffer.h"
#include "fog.h"
#include "scene_node.h"
#include "render_scratch.h"
#include "stage_cull.h"
#include "display_list.h"
#include "main.h"
#include "otz.h"
#include "rot_matrix.h"
#include "shadow_proj.h"
#include "tile_clip.h"
#include "globals.h"
#include "render.h"
#include "model_table.h"

MATRIX gAuxViewMatrix;
struct SceneNode *gCameraNodePtr;

/* Nothing calls it. */
static void CallSubmitSceneDisplayList(void)
{
    SubmitSceneDisplayList();
}

void SubmitSceneDisplayList(void)
{
    BuildRotMatrixZXY(&gAuxViewAngles, &gAuxViewMatrix);
    MulMatrix(&gAuxViewMatrix, &gCameraNodePtr->worldMatrix);
    MulMatrix2(&gSecondaryCameraNodePtr->worldMatrix, &gAuxViewMatrix);
    MulMatrix0(&gSecondaryCameraNodePtr->worldMatrix, &gCameraNodePtr->worldMatrix, &gCombinedViewMatrix);

    if (gFrameCounter & 1)
        RENDER_SCRATCH->doubleBufferMask = -1;
    else
        RENDER_SCRATCH->doubleBufferMask = 0;

    BuildShadowProjectionMatrix();
    SubmitSceneDisplayListNoOp();

    RENDER_SCRATCH->primTable = gPrimRenderDispatch;
    RENDER_SCRATCH->orderingTable = gCurrentOt;
    RENDER_SCRATCH->fogFarOtz = GetFogFarOtzBucket();

    if (!GetVisibilityMaskPreset() && IsStageTileNearClipEnabled())
        RENDER_SCRATCH->stageTileMinOtz = STAGE_TILE_NEAR_CLIP_OTZ;
    else
        RENDER_SCRATCH->stageTileMinOtz = STAGE_TILE_NO_NEAR_CLIP_OTZ;

    RenderSceneDisplayList();
    SetStageTileNearClip(1);
}

void TrapInvalidPrimType(void)
{
    TriggerDebugTrap();
}
