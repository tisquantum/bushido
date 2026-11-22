/* Fighter skeleton setup: the 18-joint rest pose scaled to each character's height, the meshes a
 * character's override table hangs on its joints (with clones where two share a joint), its ground
 * shadow parts, and the weapon meshes on its hands. */

#include <sys/types.h>
#include <libgte.h>
#include "model_arena.h"
#include "scene_node.h"
#include "fighter_skeleton.h"

#define SKELETON_PART_SLOT_COUNT 20
#define SKELETON_NO_PARENT       (-1)

#define WEAPON_MESH_PRIMARY      0
#define WEAPON_MESH_SECONDARY    1
#define WEAPON_MESH_SHADOW       2

/* Packed 6-byte 3D translation offset from a joint's parent in the rest pose. */
typedef struct BoneOffset {
    short x, y, z;
} BoneOffset;

static struct SceneNode gSkeletonCloneParts[CHAR_SLOT_COUNT][SKELETON_PART_SLOT_COUNT];
static struct SceneNode *gSkeletonPartByMesh[CHAR_SLOT_COUNT][SKELETON_PART_SLOT_COUNT];

/* 18-joint skeleton order:
 *  0 root, 1 torso, 2 head, 3..6 right shoulder/upper arm/forearm/hand,
 *  7..10 left shoulder/upper arm/forearm/hand, 11 pelvis,
 *  12..14 right thigh/shin/foot, 15..17 left thigh/shin/foot. */
short gShadowJointMeshMap[FIGHTER_JOINT_COUNT] = {
    SCENE_MESH_NONE, 0, 1, SCENE_MESH_NONE, 2, 3, 4, SCENE_MESH_NONE, 5,
    6, 7, 8, 9, 10, 11, 12, 13, 14
};
short gSkeletonParentIndices[FIGHTER_JOINT_COUNT] = {
    SKELETON_NO_PARENT, 0, 1, 1, 3, 4, 5, 1, 7, 8, 9, 0, 11, 12, 13, 11, 15, 16
};
/* Rest-pose bone offsets from each joint's parent, scaled by gCharHeightScales. */
BoneOffset gSkeletonRestOffsets[FIGHTER_JOINT_COUNT] = {
    { 0, 0, 0 },
    { 0, 10, 0 },
    { 468, 0, 0 },
    { 289, 0, -100 },
    { 140, 0, 0 },
    { 262, 0, 0 },
    { 211, 0, -3 },
    { 289, 0, 100 },
    { 140, 0, 0 },
    { 262, 0, 0 },
    { 211, 0, -2 },
    { 0, 10, 0 },
    { 180, 0, 110 },
    { 390, 0, 0 },
    { 441, 0, 0 },
    { 180, 0, -109 },
    { 390, 0, 0 },
    { 444, 0, 0 },
};
PoseAngles gSkeletonRestAngles[FIGHTER_JOINT_COUNT] = {
    { 0, 0, 0 },
    { 1241, -209, 1023 },
    { 966, 210, 846 },
    { 2047, 1023, 1023 },
    { -825, 237, -788 },
    { 0, 0, 694 },
    { 0, -7, 0 },
    { -2046, -1023, -1024 },
    { -1306, -221, 930 },
    { 0, 0, 521 },
    { -809, -791, -442 },
    { 566, 0, -1024 },
    { 45, 115, 276 },
    { 0, 0, -485 },
    { 0, 409, 0 },
    { 575, -333, 186 },
    { 0, 0, -421 },
    { 0, -113, 0 },
};
/* Per-character Q12 skeleton height scales (4096 = 1.0):
 *  0..5   playable (Kannuki, Mikado, Black Lotus, Tatsumi, Red Shadow, Utsusemi)
 *  6..11  playable story alts
 * 12..25  bosses and Slash enemies
 * 26..31  shared-weapon models (M19..M24)
 * 32..34  opening demo models
 * 35..40  playable-scaled alternate slots (followed by trailing 0) */
short gCharHeightScales[42] = {
    4218, 3891, 4096, 3932, 4096, 4096,
    4218, 3891, 4096, 3932, 4096, 4096,
    4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
    4218, 3891, 4096, 3932, 4096, 4096,
    4096, 4096, 4096,
    4218, 3891, 4096, 3932, 4096, 4096,
    0,
};

int SetupFighterSkeletonAndModel(int slot, int characterId, struct SceneNode *joints, int modelSlot)
{
    const unsigned short *overrides;

    InitPoseParts(joints, characterId, modelSlot, slot);
    if (slot < CHAR_SLOT_SELECT_AUX) {
        LoadFighterModel(slot, characterId);
    }
    overrides = GetFighterOverrideTable(slot);
    if (overrides != 0) {
        ApplyPosePartOverrides(overrides, slot, joints);
    }
    return 1;
}

void InitPoseParts(struct SceneNode *joints, int characterId, int modelSlot, int lightSlot)
{
    struct SceneNode *base = joints;
    int i;
    int parentIndex;

    for (i = 0; i < FIGHTER_JOINT_COUNT; ++i) {
        joints->renderType = SCENE_NODE_STANDARD;
        joints->rotOrder = SCENE_ROT_ORDER_FAST;
        joints->meshIndex = SCENE_MESH_NONE;
        parentIndex = gSkeletonParentIndices[i];
        if (parentIndex != SKELETON_NO_PARENT) {
            joints->parent = &base[parentIndex];
        } else {
            joints->parent = 0;
        }
        joints->localMatrix.t[0] = gSkeletonRestOffsets[i].x * gCharHeightScales[characterId] >> 12;
        joints->localMatrix.t[1] = gSkeletonRestOffsets[i].y * gCharHeightScales[characterId] >> 12;
        joints->localMatrix.t[2] = gSkeletonRestOffsets[i].z * gCharHeightScales[characterId] >> 12;
        joints->rotation.vx = gSkeletonRestAngles[i].x;
        joints->rotation.vy = -gSkeletonRestAngles[i].y;
        joints->rotation.vz = -gSkeletonRestAngles[i].z;
        joints->matrixState = SCENE_MATRIX_DIRTY;
        joints->lightSlot = lightSlot;
        joints->modelSlot = modelSlot;
        ++joints;
    }
}

void InitFighterShadowParts(struct SceneNode *joints, struct SceneNode *shadows, int modelSlot)
{
    int i;
    short meshIndex;

    for (i = 0; i < FIGHTER_JOINT_COUNT; ++i) {
        meshIndex = gShadowJointMeshMap[i];
        if (meshIndex != SCENE_MESH_NONE) {
            shadows->meshIndex = meshIndex;
            shadows->renderType = SCENE_NODE_SHADOW;
            shadows->rotOrder = SCENE_ROT_ORDER_FAST;
            shadows->parent = &joints[i];
            shadows->matrixState = SCENE_MATRIX_WORLD_VALID;
            shadows->lightSlot = 0;
            shadows->modelSlot = modelSlot;
            shadows->param.shadowFloorY = 0;
            ++shadows;
        }
    }
    shadows->meshIndex = SCENE_MESH_NONE;
}

void AppendWeaponShadowPart(struct SceneNode *joints, struct SceneNode *shadows, int modelSlot)
{
    while (shadows->meshIndex != SCENE_MESH_NONE)
        ++shadows;

    shadows->meshIndex = WEAPON_MESH_SHADOW;
    shadows->renderType = SCENE_NODE_SHADOW;
    shadows->parent = &joints[FIGHTER_JOINT_RIGHT_HAND];
    shadows->matrixState = SCENE_MATRIX_WORLD_VALID;
    shadows->rotOrder = SCENE_ROT_ORDER_FAST;
    shadows->lightSlot = 0;
    shadows->modelSlot = modelSlot;
    shadows->param.shadowFloorY = 0;
    shadows[1].meshIndex = SCENE_MESH_NONE;
}

void ApplyPosePartOverrides(const unsigned short *overrides, int slot, struct SceneNode *joints)
{
    int boneAssigned[FIGHTER_JOINT_COUNT];
    int index;
    struct SceneNode *clone;

    for (index = 0; index < FIGHTER_JOINT_COUNT; ++index) {
        boneAssigned[index] = 0;
    }

    for (index = 0; index < SKELETON_PART_SLOT_COUNT; ++index) {
        gSkeletonPartByMesh[slot][index] = 0;
    }

    /* Stream of (meshIndex, boneIndex) pairs terminated by FIGHTER_OVERRIDE_END. */
    clone = gSkeletonCloneParts[slot];
    while (*overrides != FIGHTER_OVERRIDE_END) {
        int meshIndex = *overrides++;
        int boneIndex = *overrides++;

        if (!boneAssigned[boneIndex]) {
            joints[boneIndex].meshIndex = meshIndex;
            boneAssigned[boneIndex] = 1;
            gSkeletonPartByMesh[slot][meshIndex] = &joints[boneIndex];
        } else {
            *clone = joints[boneIndex];
            clone->meshIndex = meshIndex;
            clone->param.cloneSource = &joints[boneIndex];
            gSkeletonPartByMesh[slot][meshIndex] = clone;
            ++clone;
        }
    }
    clone->param.cloneSource = 0;
}

void SelectFighterCharAndWeapon(int slot, int characterId, int weaponId)
{
    SetFighterCharacterId(slot, characterId);
    if (characterId < STANDARD_WEAPON_CHAR_COUNT ||
        (characterId >= SHARED_WEAPON_CHAR_FIRST && characterId < SHARED_WEAPON_CHAR_END)) {
        LoadFighterWeaponModel(slot, weaponId);
    }
}

void ReloadFighterWeaponModel(int slot, struct SceneNode *joints, int modelSlot, int weaponId)
{
    int characterId;

    characterId = GetFighterCharacterId(slot);
    if (characterId < STANDARD_WEAPON_CHAR_COUNT ||
        (characterId >= SHARED_WEAPON_CHAR_FIRST && characterId < SHARED_WEAPON_CHAR_END)) {
        LoadFighterWeaponModel(slot, weaponId);
        if (characterId < STANDARD_WEAPON_CHAR_COUNT) {
            joints[FIGHTER_JOINT_RIGHT_HAND].meshIndex = WEAPON_MESH_PRIMARY;
            joints[FIGHTER_JOINT_LEFT_HAND].meshIndex = WEAPON_MESH_SECONDARY;
            joints[FIGHTER_JOINT_RIGHT_HAND].modelSlot = modelSlot;
            joints[FIGHTER_JOINT_LEFT_HAND].modelSlot = modelSlot;
        } else {
            AttachSingleWeaponClone(slot, joints, modelSlot);
        }
    }
}

void AttachSingleWeaponClone(int slot, struct SceneNode *joints, int modelSlot)
{
    struct SceneNode *clone;

    clone = gSkeletonCloneParts[slot];
    *clone = joints[FIGHTER_JOINT_RIGHT_HAND];
    clone->param.cloneSource = &joints[FIGHTER_JOINT_RIGHT_HAND];
    clone->meshIndex = WEAPON_MESH_PRIMARY;
    clone->modelSlot = modelSlot;
    clone++;
    clone->param.cloneSource = 0;
}

struct SceneNode *GetSkeletonCloneParts(int slot)
{
    return gSkeletonCloneParts[slot];
}

int GetFighterHeightScale(int slot)
{
    return gCharHeightScales[GetFighterCharacterId(slot)];
}

struct SceneNode *GetSkeletonPartByMeshIndex(int slot, int meshIndex)
{
    return gSkeletonPartByMesh[slot][meshIndex];
}

void SetShadowPartsFloorY(struct SceneNode *shadows, int floorY)
{
    while (shadows->meshIndex != SCENE_MESH_NONE) {
        shadows->param.shadowFloorY = floorY;
        shadows++;
    }
}
