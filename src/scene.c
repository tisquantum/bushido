/*
 * The 3D engine's frame: Init3dEngine at boot, BeginSceneFrame placing the camera and opening the
 * frame's GPU packet buffer, EndSceneFrame running the frame's effects and submitting its display list.
 */
#include <sys/types.h>
#include <libgte.h>

#include "attached_part.h"
#include "billboard_driver.h"
#include "display_list.h"
#include "effect.h"
#include "fighter_cloth.h"
#include "fighter_cloth_setup.h"
#include "fighter_pose.h"
#include "globals.h"
#include "light.h"
#include "light_init.h"
#include "main.h"
#include "model_arena.h"
#include "model_table.h"
#include "packet_build.h"
#include "point_light.h"
#include "scene_node.h"
#include "render.h"
#include "rot_matrix.h"
#include "scene.h"
#include "scene_math.h"
#include "sightline.h"
#include "stage_callbacks.h"
#include "stage_cull.h"
#include "stage_env.h"
#include "tim.h"

static unsigned int gPacketBufferUsed;     /* GPU packet bytes the last frame used */
static unsigned int gPacketBufferPeak;     /* most GPU packet bytes any frame used */
static int gCameraOrbitDistance; /* camera distance from its target this frame */
struct SceneNode *gRenderQueue[512]; /* per-frame display list of SceneNode/SceneTile pointers */

void Init3dEngine(void)
{
    Init3dEngineNoOp();
    CallResetModelArena();
    InitRotMatrixDispatchTable();
    InitGpuPrimCodeCache();
    ClearModelSlotCounts();
    InitSceneCameras();
    InitLightSlots();
    gSceneUnreadFlag = 0;
    gCameraNode.rotation.vx = 0;
    gCameraNode.rotation.vy = 0;
    gCameraNode.rotation.vz = 0;
    gCameraNode.localMatrix.t[0] = 0;
    gCameraNode.localMatrix.t[1] = 0;
    gCameraNode.localMatrix.t[2] = 0;
    InitClothSystem();
    ClearAttachedPartPool();
    gPacketBufferPeak = 0;
    gRendererMonochrome = 0;
}

/* Nothing calls it. */
static void ResetDefaultFighters(void)
{
    LoadDefaultFighters();
    gSceneUnreadFlag = 0;
}

void BeginSceneFrame(VECTOR *target, SVECTOR *rotation, int distance)
{
    VECTOR offset;
    VECTOR displacement;
    SVECTOR inverseRotation;
    MATRIX rotationMatrix;

    gCameraNode.rotation.vx = -rotation->vx;
    gCameraNode.rotation.vy = -rotation->vy;
    gCameraNode.rotation.vz = -rotation->vz;
    displacement.vx = 0;
    displacement.vy = 0;
    displacement.vz = -distance;
    gRenderQueueCursor = gRenderQueue;
    inverseRotation.vx = -rotation->vx;
    ++gSceneFrameCounter;
    inverseRotation.vy = -rotation->vy;
    inverseRotation.vz = -rotation->vz;
    gRotMatrixBuilders[SCENE_ROT_ORDER_FAST](&inverseRotation, &rotationMatrix);
    ApplyMatrixLV(&rotationMatrix, &displacement, &offset);
    gCameraNode.localMatrix.t[0] = offset.vx + target->vx;
    gCameraNode.localMatrix.t[1] = offset.vy + target->vy;
    gCameraNode.localMatrix.t[2] = offset.vz + target->vz;
    BuildNegatedNodeRotation(&gCameraNode);
    UpdateAspectCorrectedCameraMatrix();
    StampPackedCellGrid();
    ClearSightlineGrid();
    gCameraOrbitDistance = distance;
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P1]);
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P2]);
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_STAGE]);
    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);
    RunSceneCallback();
    ResetPointLightTargets();
    ResetClothWindWithJitter();
}

void EndSceneFrame(void)
{
    int bytesUsed;

    EndSceneFrameNoOp();
    CullStageAndQueueFog(gCameraOrbitDistance);
    RebuildFighterClothIfDirty(CHAR_SLOT_P1);
    RebuildFighterClothIfDirty(CHAR_SLOT_P2);
    UpdateAllFighterCloth();
    UpdateAndDrawBillboardEffects();
    UpdateAndDrawEffects();
    UpdateAttachedPartPool();
    ApplyPointLightToTargets();
    SubmitSceneDisplayList();
    bytesUsed = (char *)gPacketCursor - (char *)GetGpuPacketBuffer(gFrameCounter & 1);
    gPacketBufferUsed = bytesUsed;
    if (gPacketBufferPeak < bytesUsed) {
        gPacketBufferPeak = bytesUsed;
    }
    if (GetPacketBufferSize() < gPacketBufferUsed) {
        TriggerDebugTrap();
    }
}
