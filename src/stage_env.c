/* Stage environment: the stage's lighting, resources, fog CLUTs and billboard effects set up
 * when a stage loads, and the per-frame cull that queues the stage and its fog. */
#include "billboard_driver.h"
#include "globals.h"
#include "model_arena.h"
#include "stage_cull.h"
#include "stage_env.h"
#include "stage_light.h"
#include "subweapon_node.h"

extern void QueueStageFogClutMoves(int stageId, int startDepth);
extern void BuildFogClutTable(); /* the original passes stageId, which the routine does not take */

void InitStageRenderEnvironment(int stageId)
{
    ApplyStageLighting(stageId);
    LoadStageAndLighting(stageId);
    InitBillboardEffects();
    gStageRenderFlags = STAGE_RENDER_MIP_MODE_AREA_LOD | STAGE_RENDER_TRI_MIPMAP;
}

int gFogStartDepth;

void CullStageAndQueueFog(int fogStartDepth)
{
    int stageId;

    CullAndEnqueueStageScene();
    gFogStartDepth = fogStartDepth;
    stageId = GetLoadedStageId();
    QueueStageFogClutMoves(stageId, gFogStartDepth);
}

void LoadStageAndLighting(int stageId)
{
    LoadStageResource(stageId);
    ApplyStageLighting(stageId);
    BuildFogClutTable(stageId);
    InitStage4SubWeaponSlot(stageId);
}
