/* First-person (POV) overlay: the 15-part mannequin of the viewed fighter, posed from the fighter's
 * joint matrices and drawn into its own ordering table chained ahead of the frame's. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "effect_slot.h"
#include "scene_node.h"
#include "fighter_pose.h"
#include "display_buffer.h"
#include "angle.h"
#include "model_table.h"
#include "asset_load.h"
#include "main.h"
#include "pov_overlay.h"
#include "scene_math.h"
#include "globals.h"
#include "render.h"
#include "tim.h"

#define POV_OVERLAY_PART_COUNT   15
#define POV_OVERLAY_MODEL_SLOT   15
#define POV_OVERLAY_CAMERA_DIST  (-6000)
#define POV_OVERLAY_OT_LEN       256
#define POV_OVERLAY_OT_ADDR_MASK 0xffffff

/* Disc file indexes of the first-person overlay models: the Japanese disc numbers them one lower. */
#ifdef VERSION_JP
#define CD_FILE_POV_OVERLAY 125
#else
#define CD_FILE_POV_OVERLAY 126
#endif
#define CD_FILE_POV_OVERLAY_ALT (CD_FILE_POV_OVERLAY + 1)

typedef struct PovOverlayArchiveHeader {
    int sectionCount;
    int modelOffset;
    int modelSize;
} PovOverlayArchiveHeader;

u_long gOverlayOtBuffers[2][POV_OVERLAY_OT_LEN];
MATRIX gPovOverlayCameraMatrix;

static int gPovOverlayFighterSlot;               /* 0 = P1, 1 = P2 */
static struct SceneNode *gPovOverlayModelParts; /* the 15-part POV mannequin */
u_long *gOverlayOt;                              /* active 256-entry overlay OT;
                                                    also read by pov_tex.s and pov_flat.s */
short gOverlayModelBoneMap[16] = { 1, 2, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, -1 };

struct SceneNode *LoadPovOverlayModel(int fighterSlot, char *resource)
{
    int modelOffset;
    int modelSize;
    int i;

    gPovOverlayFighterSlot = fighterSlot;
    if (gRendererMonochrome)
        ReadCdFileSync(CD_FILE_POV_OVERLAY_ALT, resource);
    else
        ReadCdFileSync(CD_FILE_POV_OVERLAY, resource);
    modelOffset = ((PovOverlayArchiveHeader *)resource)->modelOffset;
    modelSize = ((PovOverlayArchiveHeader *)resource)->modelSize;
    resource += modelOffset;
    RegisterModelTable((int *)resource, POV_OVERLAY_MODEL_SLOT);
    resource += modelSize;
    gPovOverlayModelParts = (struct SceneNode *)resource;
    for (i = 0; i < POV_OVERLAY_PART_COUNT; ++i) {
        gPovOverlayModelParts[i].meshIndex = i;
        gPovOverlayModelParts[i].modelSlot = POV_OVERLAY_MODEL_SLOT;
        gPovOverlayModelParts[i].renderType = SCENE_NODE_POV_OVERLAY;
        gPovOverlayModelParts[i].lightSlot = fighterSlot;
    }
    return (struct SceneNode *)resource + POV_OVERLAY_PART_COUNT;
}

void UpdatePovOverlayModel(int orbitAngle)
{
    MATRIX cameraRotation;
    SVECTOR cameraVector;
    struct SceneNode *part;
    MATRIX **fighterMatrices;
    short *boneMap;
    int boneIndex;
    u_long *overlayOt;
    u_long mainOtHead;

    boneMap = gOverlayModelBoneMap;
    fighterMatrices = GetFighterWorldMatrixList(gPovOverlayFighterSlot);
    cameraVector.vx = POV_OVERLAY_CAMERA_DIST;
    cameraVector.vy = 0;
    cameraVector.vz = 0;
    cameraVector.vz = (gSinLutQ12[orbitAngle & ANGLE_MASK] * cameraVector.vx) >> 12;
    cameraVector.vx = (gSinLutQ12[(orbitAngle + ANGLE_QUARTER) & ANGLE_MASK] * cameraVector.vx) >> 12;
    part = gPovOverlayModelParts;
    ApplyMatrix(fighterMatrices[0], &cameraVector, (VECTOR *)gPovOverlayCameraMatrix.t);
    /* x from joint 0's world position, y from joint 1's and z from joint 2's. */
    gPovOverlayCameraMatrix.t[0] += fighterMatrices[0]->t[0];
    gPovOverlayCameraMatrix.t[1] += fighterMatrices[1]->t[1];
    gPovOverlayCameraMatrix.t[2] += fighterMatrices[2]->t[2];
    cameraVector.vx = 0;
    cameraVector.vy = ANGLE_QUARTER - orbitAngle;
    cameraVector.vz = 0;
    BuildRotationMatrixFast(&cameraVector, &cameraRotation);
    MulMatrix3x3Fast(fighterMatrices[0], &cameraRotation, &cameraRotation);
    gPovOverlayCameraMatrix.m[0][0] = cameraRotation.m[0][0];
    gPovOverlayCameraMatrix.m[0][1] = cameraRotation.m[1][0];
    gPovOverlayCameraMatrix.m[0][2] = cameraRotation.m[2][0];
    gPovOverlayCameraMatrix.m[1][0] = cameraRotation.m[0][1];
    gPovOverlayCameraMatrix.m[1][1] = cameraRotation.m[1][1];
    gPovOverlayCameraMatrix.m[1][2] = cameraRotation.m[2][1];
    gPovOverlayCameraMatrix.m[2][0] = cameraRotation.m[0][2];
    gPovOverlayCameraMatrix.m[2][1] = cameraRotation.m[1][2];
    gPovOverlayCameraMatrix.m[2][2] = cameraRotation.m[2][2];
    while ((boneIndex = *boneMap++) >= 0) {
        part->worldMatrix = *fighterMatrices[boneIndex];
        *gRenderQueueCursor++ = part++;
    }
    overlayOt = gOverlayOtBuffers[gFrameCounter & 1];
    gOverlayOt = overlayOt;
    ClearOTagR(overlayOt, POV_OVERLAY_OT_LEN);
    mainOtHead = *gCurrentOt;
    *gCurrentOt = (u_long)(gOverlayOt + (POV_OVERLAY_OT_LEN - 1)) & POV_OVERLAY_OT_ADDR_MASK;
    *gOverlayOt = mainOtHead;
}
