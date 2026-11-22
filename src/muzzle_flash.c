/* Katze's gun muzzle flash: an attached scene node on his right hand that steps through four
 * flash meshes (16..19), one a frame, paired with a billboard flash effect at the barrel tip. */

#include <sys/types.h>
#include <libgte.h>

#include "attached_part.h"
#include "effect.h"
#include "effect_slot.h"
#include "fighter_pose.h"
#include "fighter_skeleton.h"
#include "model_arena.h"
#include "muzzle_flash.h"
#include "scene_node.h"
#include "rot_matrix.h"
#include "scene_math.h"

#define MUZZLE_FLASH_FIRST_MESH 16
#define MUZZLE_FLASH_MESH_COUNT 4

void SpawnGunMuzzleFlash(int player)
{
    struct SceneNode *node;
    struct SceneNode *fighterNodes;
    SVECTOR muzzleOffset;
    VECTOR muzzlePos;

    node = AllocAttachedPartSlot(ATTACHED_PART_MUZZLE_FLASH);
    if (node != 0) {
        fighterNodes = GetFighterRootNode(player);
        node->renderType = SCENE_NODE_STANDARD;
        node->meshIndex = MUZZLE_FLASH_FIRST_MESH - 1;
        node->modelSlot = GetFighterModelTableSlot(player);
        node->lightSlot = player;
        node->rotOrder = SCENE_ROT_ORDER_FAST;
        node->parent = &fighterNodes[1 + FIGHTER_JOINT_RIGHT_HAND];
        node->rotation.vx = 0;
        node->rotation.vy = 0;
        node->rotation.vz = 0;
        gRotMatrixBuilders[SCENE_ROT_ORDER_FAST](&node->rotation, &node->worldMatrix);
        node->localMatrix.t[0] = 0;
        node->localMatrix.t[1] = 0;
        node->localMatrix.t[2] = 0;
        muzzleOffset.vx = -38;
        muzzleOffset.vy = 138;
        muzzleOffset.vz = 91;
        ApplyMatrix(&node->parent->worldMatrix, &muzzleOffset, &muzzlePos);
        muzzlePos.vx += node->parent->worldMatrix.t[0];
        muzzlePos.vy += node->parent->worldMatrix.t[1];
        muzzlePos.vz += node->parent->worldMatrix.t[2];
        if (player == CHAR_SLOT_P1) {
            SpawnEffect(EFFECT_GUN_MUZZLE_FLASH_P1, muzzlePos.vx, muzzlePos.vy, muzzlePos.vz);
        } else {
            SpawnEffect(EFFECT_GUN_MUZZLE_FLASH_P2, muzzlePos.vx, muzzlePos.vy, muzzlePos.vz);
        }
    }
}

void UpdateGunMuzzleFlash(struct SceneNode *node)
{
    if (GetFighterCharacterId(CHAR_SLOT_P1) != CHAR_ARCHIVE_KATZE &&
        GetFighterCharacterId(CHAR_SLOT_P2) != CHAR_ARCHIVE_KATZE) {
        FreeAttachedPartSlot(node);
        return;
    }
    switch (++node->meshIndex) {
    case MUZZLE_FLASH_FIRST_MESH:
        node->matrixState = SCENE_MATRIX_DIRTY;
        UpdateNodeWorldMatrix(node);
        *gRenderQueueCursor++ = node;
        return;
    case MUZZLE_FLASH_FIRST_MESH + MUZZLE_FLASH_MESH_COUNT:
        FreeAttachedPartSlot(node);
        return;
    default:
        *gRenderQueueCursor++ = node;
        return;
    }
}
