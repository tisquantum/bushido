/* Sub-weapon models: each fighter's sub-weapon mesh descriptor (its character archive's last mesh
 * when present beyond its override entries), the Stage 4 slot, and scene-node submission. */

#include <sys/types.h>
#include <libgte.h>
#include "scene_node.h"
#include "light.h"
#include "subweapon.h"
#include "packet_build.h"
#include "stage_id.h"
#include "model_arena.h"
#include "rot_matrix.h"
#include "stage_light.h"
#include "subweapon_node.h"
#include "stage_moat.h"
#include "globals.h"

#define FIGHTER_SIDE_COUNT      2

/* One per fighter, then the Stage 4 slot. */
static SubWeaponModel gSubWeaponModels[SUB_WEAPON_MODEL_COUNT];
/* 24 bytes; nothing reads or writes them. */
static unsigned char gSubWeaponUnused[24];

void InitSubWeaponModels(void)
{
    int side;
    int modelCount;

    for (side = 0; side < FIGHTER_SIDE_COUNT; side++) {
        gSubWeaponModels[side].kind = SUB_WEAPON_MODEL_NONE;
        gSubWeaponModels[side].meshIndex = SCENE_MESH_NONE;
        gSubWeaponModels[side].modelSlot = -1;

        modelCount = GetFighterModelCount(side);
        if (modelCount != 0 && CountFighterOverrideEntries(side) + 1 < modelCount) {
            gSubWeaponModels[side].kind = SUB_WEAPON_MODEL_FIGHTER;
            gSubWeaponModels[side].meshIndex = modelCount - 1;
            gSubWeaponModels[side].modelSlot = GetFighterModelTableSlot(side);
        }
    }
}

void InitStage4SubWeaponSlot(int stageId)
{
    gSubWeaponModels[SUB_WEAPON_MODEL_SLOT_STAGE4].kind = SUB_WEAPON_MODEL_NONE;
    if (stageId == 4) {
        gSubWeaponModels[SUB_WEAPON_MODEL_SLOT_STAGE4].kind = SUB_WEAPON_MODEL_STAGE4;
        gSubWeaponModels[SUB_WEAPON_MODEL_SLOT_STAGE4].meshIndex = 0;
    }
}

void SubmitSubWeaponNode(int modelOwner, WorldVector *position, int yaw)
{
    struct SceneNode *node;

    if (gSubWeaponModels[modelOwner].kind == SUB_WEAPON_MODEL_FIGHTER &&
        gSubWeaponModels[modelOwner].meshIndex >= 0) {
        node = gPacketCursor;
        node->renderType = SCENE_NODE_STANDARD;
        node->meshIndex = gSubWeaponModels[modelOwner].meshIndex;
        node->modelSlot = gSubWeaponModels[modelOwner].modelSlot;
        node->rotOrder = SCENE_ROT_ORDER_FAST;
        node->parent = 0;

        if (GetLoadedStageId() == STAGE_ID_DOZAEMON_MOAT) {
            if (position->y >= MOAT_WATER_SURFACE_Y) {
                SetNodeUseSpecialLight(node, 1);
            } else {
                SetNodeUseSpecialLight(node, 0);
            }
        } else {
            node->lightSlot = LIGHT_SLOT_STAGE;
        }

        node->rotation.vx = 0;
        node->rotation.vy = yaw;
        node->rotation.vz = 0;
        gRotMatrixBuilders[SCENE_ROT_ORDER_FAST](&node->rotation, &node->worldMatrix);

        node->worldMatrix.t[0] = position->x;
        node->worldMatrix.t[1] = position->y;
        node->worldMatrix.t[2] = position->z;
        *gRenderQueueCursor++ = node;
        node++;
        gPacketCursor = node;
    }
}
