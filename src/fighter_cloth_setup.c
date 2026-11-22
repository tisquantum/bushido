/* Fighter garment cloth setup: per-character limb-collider capsules attached to cloth meshes,
 * per-slot skeleton bases, and the dirty flags that schedule cloth rebuilds on the next frame. */

#include <sys/types.h>
#include <libgte.h>
#include "scene_node.h"
#include "fighter_cloth.h"
#include "fighter_skeleton.h"
#include "fighter_cloth_setup.h"
#include "model_arena.h"

struct SceneNode *gFighterClothSkeletonBases[2] = { 0, 0 };
/* 8 bytes; nothing reads or writes them. */
int gFighterClothSetupUnusedWords[2] = { 0, 1 };
int gFighterClothDirty[3] = { 1, 1, 1 };

static void SetupFighterClothAndColliders(int slot, int characterId, struct SceneNode *skeleton)
{
    struct SceneNode *part;

    gFighterClothSkeletonBases[slot] = skeleton;
    BuildFighterClothParts(slot);
    switch (characterId) {
    case CHAR_ARCHIVE_MIKADO:
        part = GetSkeletonPartByMeshIndex(slot, 7);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_RIGHT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        part = GetSkeletonPartByMeshIndex(slot, 8);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_RIGHT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        break;
    case CHAR_ARCHIVE_UTSUSEMI:
        part = GetSkeletonPartByMeshIndex(slot, 4);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        break;
    case CHAR_ARCHIVE_MIKADO_ALT:
        part = GetSkeletonPartByMeshIndex(slot, 8);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_RIGHT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        part = GetSkeletonPartByMeshIndex(slot, 7);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_RIGHT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        break;
    case CHAR_ARCHIVE_BLACK_LOTUS_ALT:
        part = GetSkeletonPartByMeshIndex(slot, 9);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_RIGHT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        break;
    case CHAR_ARCHIVE_RED_SHADOW_ALT:
        part = GetSkeletonPartByMeshIndex(slot, 3);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_HEAD], 50, 0, 50, 60, 0, -100, 300);
        break;
    case CHAR_ARCHIVE_UTSUSEMI_ALT:
        part = GetSkeletonPartByMeshIndex(slot, 4);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_RIGHT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        break;
    case CHAR_ARCHIVE_FINAL_BOSS_ALT:
        part = GetSkeletonPartByMeshIndex(slot, 10);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        break;
    case CHAR_ARCHIVE_BOUT6_BOSS:
        part = GetSkeletonPartByMeshIndex(slot, 7);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_RIGHT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        break;
    case CHAR_ARCHIVE_BOUT7_BOSS:
        part = GetSkeletonPartByMeshIndex(slot, 3);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_RIGHT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        break;
    case CHAR_ARCHIVE_KANNUKI:
    case CHAR_ARCHIVE_RED_SHADOW:
    case CHAR_ARCHIVE_DEMO_KANNUKI:
    case CHAR_ARCHIVE_DEMO_RED_SHADOW:
        part = GetSkeletonPartByMeshIndex(slot, 6);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_RIGHT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        part = GetSkeletonPartByMeshIndex(slot, 5);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_RIGHT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_LEFT_THIGH], 50, 0, 0, 340, 0, 0, 500);
        AddClothLimbCollider(part, &skeleton[FIGHTER_JOINT_PELVIS], -200, 0, 0, 650, 0, 0, 1000);
        break;
    }
}

/* Nothing calls it. */
static int IsFighterClothDirty(int slot)
{
    return gFighterClothDirty[slot];
}

void SetFighterClothDirty(int slot, int dirty)
{
    if (dirty != 0) {
        gFighterClothDirty[slot] = 1;
    } else {
        gFighterClothDirty[slot] = 0;
    }
}

/* Nothing calls it. */
static void MarkFighterClothDirty(int slot)
{
    gFighterClothDirty[slot] = 1;
}

void RebuildFighterClothIfDirty(int slot)
{
    int characterId;

    if (gFighterClothDirty[slot] == 0)
        return;
    if (gFighterClothSkeletonBases[slot] == 0)
        return;

    characterId = GetFighterCharacterId(slot);
    SetupFighterClothAndColliders(slot, characterId, gFighterClothSkeletonBases[slot]);
    gFighterClothDirty[slot] = 0;
}

void SetFighterClothSkeletonBase(int slot, struct SceneNode *skeleton)
{
    gFighterClothSkeletonBases[slot] = skeleton;
}
