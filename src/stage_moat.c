/* Dozaemon Moat (stage 19): the fighters' wading billboards and water ripples, the tint of the
 * billboard models, and the stage's piecewise floor height (sunken floor, upper ground). */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include "gpu_code.h"
#include "effect_slot.h"
#include "fighter_skeleton.h"
#include "mesh_stream.h"
#include "scene_node.h"
#include "render_scratch.h"
#include "weather.h"
#include "angle.h"
#include "effect.h"
#include "rot_matrix.h"
#include "scene_math.h"
#include "stage_light.h"
#include "stage_moat.h"
#include "tim.h"
#include "tpage_reloc.h"
#include "globals.h"
#include "model_table.h"
#include "scene.h"

typedef struct SceneNode SceneNode;

#define MOAT_BILLBOARD_NODE_COUNT     4
#define MOAT_SPECIAL_LIGHT_DEPTH      1500
#define MOAT_BILLBOARD_Y              2000
#define MOAT_BILLBOARD_HALF_WIDTH     1400
#define MOAT_BILLBOARD_CORNER_RADIUS  1980 /* ~1400 * sqrt(2) */
#define MOAT_BILLBOARD_TINT_R         0xcf
#define MOAT_BILLBOARD_TINT_G         0xaf
#define MOAT_BILLBOARD_TINT_B         0xbf
#define MOAT_BILLBOARD_TPAGE_ADD      GET_TPAGE(0, 1, 0, 0)
#define MOAT_BILLBOARD_TPAGE_SUB      GET_TPAGE(0, 2, 0, 0)
#define MOAT_BILLBOARD_BACK_OTZ_BIAS  1600
#define MOAT_BILLBOARD_FRONT_OTZ_BIAS 200

#define MOAT_LOWER_RECT_COUNT         3
#define MOAT_UPPER_RECT_COUNT         4

/* An XZ rectangle of Stage 19's floor. */
typedef struct MoatFloorRect {
    int minX;
    int minZ;
    int maxX;
    int maxZ;
} MoatFloorRect;

/* Where the floor is the sunken one (Y 1200), and where it is the upper ground (Y 0). */
MoatFloorRect gMoatLowerRects[MOAT_LOWER_RECT_COUNT] = {
    { -19200, -12800, -12800, 0 },
    { -12800, -12800, 0, -3200 },
    { 0, -12800, 12800, 9600 },
};
MoatFloorRect gMoatUpperRects[MOAT_UPPER_RECT_COUNT] = {
    { -12800, -3200, 0, 3200 },
    { -3200, 3200, 0, 12800 },
    { 0, 9600, 19200, 19200 },
    { 12800, -12800, 19200, 19200 },
};

static SceneNode gMoatBillboardNodes[2][2]; /* per fighter: the billboard and its copy */
static int gMoatBillboardActive[2];
/* 32 bytes in four objects; nothing reads or writes them. */
static int gMoatUnusedA[2];
static int gMoatUnusedB[2];
static int gMoatUnusedC[2];
static int gMoatUnusedD[2];

/* The end of a flat quad record with a draw mode (primitive type 37): a depth bias, then a
 * DR_MODE for each display buffer. */
typedef struct SemiTransQuadTail {
    int otzBias;
    DR_MODE drawModes[2];
} SemiTransQuadTail;

#define SEMI_TRANS_QUAD_TAIL(type) ((SemiTransQuadTail *)(MESH_CURSOR + gPrimRecordHalfwords[type]) - 1)

void InitMoatBillboards(int modelSlot)
{
    int monochrome = gRendererMonochrome;
    CVECTOR color;

    color.r = MOAT_BILLBOARD_TINT_R;
    color.g = MOAT_BILLBOARD_TINT_G;
    color.b = MOAT_BILLBOARD_TINT_B;
    if (monochrome)
        color.r = color.g = color.b =
            ConvertRgbToLuminance(MOAT_BILLBOARD_TINT_R, MOAT_BILLBOARD_TINT_G, MOAT_BILLBOARD_TINT_B);

    TintMoatBillboardModels(modelSlot, &color);
    InitMoatBillboardNodes(modelSlot);
}

void TintMoatBillboardModels(int modelSlot, CVECTOR *color)
{
    int meshIndex;

    for (meshIndex = 0; meshIndex < MOAT_BILLBOARD_NODE_COUNT; meshIndex++) {
        ConfigureSemiTransQuadPrims(meshIndex, modelSlot, color);
    }
}

void ConfigureSemiTransQuadPrims(int meshIndex, int modelSlot, CVECTOR *color)
{
    int count;
    int type;
    unsigned short tpage;

    MESH_CURSOR = gModelTables[modelSlot][meshIndex];
    /* the original has a loop here that runs once */
    do {
        count = *MESH_CURSOR++;
        count |= *MESH_CURSOR++ << 16;
        MESH_CURSOR += count * 12;

        while ((count = *MESH_CURSOR++) != 0) {
            type = *MESH_CURSOR++;
            if (type == MESH_PRIM_DRAWMODE_F3 || type == MESH_PRIM_DRAWMODE_F4) {
                while (count--) {
                    switch (type & 1) {
                    case 1:
                        ((POLY_F4 *)MESH_CURSOR)->r0 = color->r;
                        ((POLY_F4 *)MESH_CURSOR)->g0 = color->g;
                        ((POLY_F4 *)MESH_CURSOR)->b0 = color->b;
                        (((POLY_F4 *)MESH_CURSOR) + 1)->r0 = color->r;
                        (((POLY_F4 *)MESH_CURSOR) + 1)->g0 = color->g;
                        (((POLY_F4 *)MESH_CURSOR) + 1)->b0 = color->b;
                        if ((meshIndex & 1) == 0) {
                            tpage = MOAT_BILLBOARD_TPAGE_ADD;
                            SEMI_TRANS_QUAD_TAIL(type)->otzBias = MOAT_BILLBOARD_BACK_OTZ_BIAS;
                        } else {
                            tpage = MOAT_BILLBOARD_TPAGE_SUB;
                            SEMI_TRANS_QUAD_TAIL(type)->otzBias = MOAT_BILLBOARD_FRONT_OTZ_BIAS;
                        }
                        SetDrawMode(&SEMI_TRANS_QUAD_TAIL(type)->drawModes[0], 1, 1, tpage, 0);
                        SetDrawMode(&SEMI_TRANS_QUAD_TAIL(type)->drawModes[1], 1, 1, tpage, 0);
                        break;
                    }
                    MESH_CURSOR += gPrimRecordHalfwords[type];
                }
            } else {
                count = (short)(count * gPrimRecordHalfwords[type]);
                MESH_CURSOR += count;
            }
        }
    } while (0);
}

/* returns no value */
int InitMoatBillboardNodes(int modelSlot)
{
    int i;
    SceneNode *node;

    for (i = 0; i < MOAT_BILLBOARD_NODE_COUNT; i++) {
        node = &gMoatBillboardNodes[0][0] + i;
        node->rotOrder = SCENE_ROT_ORDER_FAST;
        node->lightSlot = 1;
        node->meshIndex = i;
        node->modelSlot = modelSlot;
        node->renderType = SCENE_NODE_STANDARD;
        node->parent = 0;
        node->rotation.vx = 0;
        node->rotation.vy = 0;
        node->rotation.vz = 0;
        gRotMatrixBuilders[node->rotOrder](&node->rotation, &node->worldMatrix);
    }
}

void ResetMoatBillboardFlags(void)
{
    gMoatBillboardActive[0] = 0;
    gMoatBillboardActive[1] = 0;
}

void UpdateMoatFighterEffects(int side, SceneNode *joints)
{
    SVECTOR offset;
    VECTOR pos;
    VECTOR headPos;
    SceneNode *leftBillboard;
    SceneNode *rightBillboard;
    int useSpecialLight;
    int pelvisY;
    int headY;
    int angle;
    int angleDiff;
    int rightYaw;
    int forward;
    int lateral;

    pelvisY = joints[FIGHTER_JOINT_PELVIS].worldMatrix.t[1];
    headY = joints[FIGHTER_JOINT_HEAD].worldMatrix.t[1];
    if (pelvisY < 0) {
        SetFighterUseSpecialLight(side, 0);
        return;
    }
    useSpecialLight = (pelvisY > MOAT_SPECIAL_LIGHT_DEPTH && headY > MOAT_SPECIAL_LIGHT_DEPTH);
    offset.vx = 200;
    offset.vy = 0;
    offset.vz = 0;
    ApplyMatrix(&joints[FIGHTER_JOINT_PELVIS].worldMatrix, &offset, &pos);
    pos.vx += joints[FIGHTER_JOINT_PELVIS].worldMatrix.t[0];
    pos.vz += joints[FIGHTER_JOINT_PELVIS].worldMatrix.t[2];
    pos.vy = MOAT_BILLBOARD_Y;
    if (useSpecialLight == 0) {
        SubmitMoatBillboardPair(side, pos.vx, MOAT_BILLBOARD_Y, pos.vz);
        gMoatBillboardActive[side] = 1;
        SetFighterUseSpecialLight(side, 0);
    } else {
        SetFighterUseSpecialLight(side, 1);
    }
    if ((gSceneFrameCounter & 7) == 7) {
        offset.vx = 100;
        offset.vy = 0;
        offset.vz = 0;
        ApplyMatrix(&joints[FIGHTER_JOINT_HEAD].worldMatrix, &offset, &headPos);
        headPos.vy += joints[FIGHTER_JOINT_HEAD].worldMatrix.t[1];
        if (headPos.vy < MOAT_WATER_SURFACE_Y) {
            SpawnEffect(EFFECT_WATER_RIPPLE, pos.vx, 0, pos.vz);
        }
    }
    if (gMoatBillboardActive[0] != 0 && gMoatBillboardActive[1] != 0) {
        leftBillboard = gMoatBillboardNodes[0];
        rightBillboard = gMoatBillboardNodes[1];
        pos.vx = gMoatBillboardNodes[0][0].worldMatrix.t[0] - gCameraNode.localMatrix.t[0];
        pos.vz = gMoatBillboardNodes[0][0].worldMatrix.t[2] - gCameraNode.localMatrix.t[2];
        angle = ratan2(pos.vx, pos.vz);
        pos.vx = gMoatBillboardNodes[1][0].worldMatrix.t[0] - gCameraNode.localMatrix.t[0];
        pos.vz = gMoatBillboardNodes[1][0].worldMatrix.t[2] - gCameraNode.localMatrix.t[2];
        angleDiff = ratan2(pos.vx, pos.vz) - angle;
        if ((angleDiff < 0 ? -angleDiff : angleDiff) > ANGLE_HALF) {
            angleDiff = angleDiff < 0 ? angleDiff + ANGLE_FULL : angleDiff - ANGLE_FULL;
        }
        if (angleDiff < 0) {
            rightBillboard = gMoatBillboardNodes[0];
            leftBillboard = gMoatBillboardNodes[1];
        }
        offset.vx = MOAT_BILLBOARD_HALF_WIDTH;
        offset.vy = 0;
        offset.vz = -MOAT_BILLBOARD_HALF_WIDTH;
        ApplyMatrix(&leftBillboard->worldMatrix, &offset, &pos);
        pos.vx = pos.vx + leftBillboard->worldMatrix.t[0] - gCameraNode.localMatrix.t[0];
        pos.vz = pos.vz + leftBillboard->worldMatrix.t[2] - gCameraNode.localMatrix.t[2];
        angle = ratan2(pos.vx, pos.vz);
        offset.vx = -MOAT_BILLBOARD_HALF_WIDTH;
        offset.vy = 0;
        offset.vz = -MOAT_BILLBOARD_HALF_WIDTH;
        ApplyMatrix(&rightBillboard->worldMatrix, &offset, &pos);
        pos.vx = pos.vx + rightBillboard->worldMatrix.t[0] - gCameraNode.localMatrix.t[0];
        pos.vz = pos.vz + rightBillboard->worldMatrix.t[2] - gCameraNode.localMatrix.t[2];
        angleDiff = ratan2(pos.vx, pos.vz) - angle;
        if ((angleDiff < 0 ? -angleDiff : angleDiff) > ANGLE_HALF) {
            angleDiff = angleDiff < 0 ? angleDiff + ANGLE_FULL : angleDiff - ANGLE_FULL;
        }
        if (angleDiff < 0) {
            angleDiff = (angle + (angleDiff >> 1)) & ANGLE_MASK;

            offset.vx = leftBillboard->worldMatrix.t[0] - gCameraNode.localMatrix.t[0];
            offset.vz = leftBillboard->worldMatrix.t[2] - gCameraNode.localMatrix.t[2];
            forward = (rcos(-angleDiff) * offset.vz - rsin(-angleDiff) * offset.vx) >> 12;
            lateral = (rsin(-angleDiff) * offset.vz + rcos(-angleDiff) * offset.vx) >> 12;
            if (lateral > -MOAT_BILLBOARD_HALF_WIDTH) {
                angle = angleDiff;
                lateral = -MOAT_BILLBOARD_HALF_WIDTH;
            } else if (lateral > -MOAT_BILLBOARD_CORNER_RADIUS) {
                angle = angleDiff + (ComputeAcosQ12(-(lateral << 12) / MOAT_BILLBOARD_CORNER_RADIUS) - ANGLE_EIGHTH);
            } else {
                angle = angleDiff;
            }
            leftBillboard->rotation.vy = angle;
            gRotMatrixBuilders[leftBillboard->rotOrder](&leftBillboard->rotation, &leftBillboard->worldMatrix);
            leftBillboard->worldMatrix.t[2] = (rcos(angleDiff) * forward - rsin(angleDiff) * lateral) >> 12;
            leftBillboard->worldMatrix.t[0] = (rsin(angleDiff) * forward + rcos(angleDiff) * lateral) >> 12;
            leftBillboard->worldMatrix.t[0] += gCameraNode.localMatrix.t[0];
            leftBillboard->worldMatrix.t[2] += gCameraNode.localMatrix.t[2];

            offset.vx = rightBillboard->worldMatrix.t[0] - gCameraNode.localMatrix.t[0];
            offset.vy = 0;
            offset.vz = rightBillboard->worldMatrix.t[2] - gCameraNode.localMatrix.t[2];
            forward = (rcos(-angleDiff) * offset.vz - rsin(-angleDiff) * offset.vx) >> 12;
            lateral = (rsin(-angleDiff) * offset.vz + rcos(-angleDiff) * offset.vx) >> 12;
            if (lateral < MOAT_BILLBOARD_HALF_WIDTH) {
                rightYaw = angleDiff;
                lateral = MOAT_BILLBOARD_HALF_WIDTH;
            } else if (lateral < MOAT_BILLBOARD_CORNER_RADIUS) {
                rightYaw = angleDiff + ANGLE_EIGHTH - ComputeAcosQ12((lateral << 12) / MOAT_BILLBOARD_CORNER_RADIUS);
            } else {
                rightYaw = rightBillboard->rotation.vy;
            }
            rightBillboard->rotation.vy = rightYaw;
            gRotMatrixBuilders[rightBillboard->rotOrder](&rightBillboard->rotation, &rightBillboard->worldMatrix);
            rightBillboard->worldMatrix.t[2] = (rcos(angleDiff) * forward - rsin(angleDiff) * lateral) >> 12;
            rightBillboard->worldMatrix.t[0] = (rsin(angleDiff) * forward + rcos(angleDiff) * lateral) >> 12;
            rightBillboard->worldMatrix.t[0] += gCameraNode.localMatrix.t[0];
            rightBillboard->worldMatrix.t[2] += gCameraNode.localMatrix.t[2];

            gMoatBillboardNodes[0][1].worldMatrix = gMoatBillboardNodes[0][0].worldMatrix;
            gMoatBillboardNodes[1][1].worldMatrix = gMoatBillboardNodes[1][0].worldMatrix;
        }
    }
}

void SubmitMoatBillboardPair(int side, int x, int y, int z)
{
    SceneNode *primary = &gMoatBillboardNodes[side][0];
    SceneNode *secondary = &gMoatBillboardNodes[side][1];
    int yaw;

    yaw = ratan2(x - gCameraNode.localMatrix.t[0], z - gCameraNode.localMatrix.t[2]);
    primary->rotation.vy = yaw;
    gRotMatrixBuilders[primary->rotOrder](&primary->rotation, &primary->worldMatrix);
    primary->worldMatrix.t[0] = x;
    primary->worldMatrix.t[1] = y;
    primary->worldMatrix.t[2] = z;
    secondary->worldMatrix = primary->worldMatrix;
    *gRenderQueueCursor++ = primary;
    *gRenderQueueCursor++ = secondary;
}

int GetMoatFloorHeight(int x, int z)
{
    int i;

    for (i = 0; i < MOAT_LOWER_RECT_COUNT; i++) {
        if (x >= gMoatLowerRects[i].minX && x <= gMoatLowerRects[i].maxX &&
            z >= gMoatLowerRects[i].minZ && z <= gMoatLowerRects[i].maxZ) {
            return MOAT_LOWER_FLOOR_Y;
        }
    }

    for (i = 0; i < MOAT_UPPER_RECT_COUNT; i++) {
        if (x >= gMoatUpperRects[i].minX && x <= gMoatUpperRects[i].maxX &&
            z >= gMoatUpperRects[i].minZ && z <= gMoatUpperRects[i].maxZ) {
            return MOAT_UPPER_FLOOR_Y;
        }
    }

    return WEATHER_NO_FLOOR;
}
