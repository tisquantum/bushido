/* Fighter skeletons: each fighter's 18-joint SceneNode tree posed from decoded joint angles (or a
 * menu, demo or cutscene pose), its skinning and world matrix tables and ground shadow parts, and
 * the CLUT tint of Slash-mode enemies. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "render.h"
#include "stage_id.h"
#include "story_cutscene.h"

#include "light.h"
#include "fighter.h"
#include "model_arena.h"
#include "scene_node.h"
#include "fighter_cloth.h"
#include "fighter_pose.h"
#include "fighter_skeleton.h"
#include "resource_cache.h"
#include "tim.h"
#include "scene_math.h"
#include "fighter_cloth_setup.h"
#include "fighter_texture.h"
#include "opening_demo.h"
#include "point_light.h"
#include "stage_moat.h"
#include "select_scene.h"
#include "subweapon_node.h"
#include "globals.h"

typedef struct SceneNode SceneNode;

#define SLASH_ENEMY_CHAR_FIRST          19   /* Slash enemy character IDs 19..22 */
#define SLASH_ENEMY_CHAR_COUNT          4
#define CLUT_4BIT_COLOR_COUNT           16

typedef struct VramClutCoord {
    short x;
    short y;
} VramClutCoord;

static SceneNode gFighterSkeletons[2][20];
static SceneNode gFighterShadows[2][20];
static MATRIX *gFighterSkinMatrices[2][40];
static MATRIX *gFighterJointWorldMatrices[2][40];

/* Skeleton joint j (1..17) takes pose joint gAnimToSkeletonJointMap[j - 1]. */
int gAnimToSkeletonJointMap[FIGHTER_JOINT_COUNT - 1] = {
    8, 9, 14, 15, 16, 17, 10, 11, 12, 13, 1, 2, 3, 4, 5, 6, 7,
};

/* 68 bytes, joints 0 to 16 in order; nothing reads them. */
int gUnusedJointIdentityMap[FIGHTER_JOINT_COUNT - 1] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};

/* CLUT rows (x within the 192-column VRAM block, y) of each Slash enemy (IDs 19..22), ended by x < 0. */
VramClutCoord gSlashEnemyChar19Cluts[] = {
    { 688, 252 }, { 640, 250 }, { 656, 250 }, { 672, 250 }, { 640, 248 }, { -1, 0 },
};
VramClutCoord gSlashEnemyChar20Cluts[] = {
    { 640, 250 }, { 656, 250 }, { -1, 0 },
};
VramClutCoord gSlashEnemyChar21Cluts[] = {
    { 656, 250 }, { -1, 0 },
};
VramClutCoord gSlashEnemyChar22Cluts[] = {
    { 672, 248 }, { 688, 248 }, { 640, 246 }, { 688, 252 }, { -1, 0 },
};

/* Indexed by characterId - SLASH_ENEMY_CHAR_FIRST (Slash enemies 19..22). */
VramClutCoord *gSlashEnemyClutLists[SLASH_ENEMY_CHAR_COUNT] = {
    gSlashEnemyChar19Cluts, gSlashEnemyChar20Cluts, gSlashEnemyChar21Cluts, gSlashEnemyChar22Cluts,
};

void UpdateAndEnqueueFighterPose(int side, int mode, long *position,
                                 PoseAngles *rotation, PoseAngles *poseAngles, int floorY)
{
    int stageId = GetLoadedStageId();
    SceneNode *rootNode;
    SceneNode *joints;
    MATRIX **skinMatrices;
    PoseAngles *angles;
    int *jointMap;
    int i;
    SceneNode *node;
    int heightScale;

    if (side < 2) {
        if (mode == FIGHTER_POSE_MODE_ANGLES) {
            angles = poseAngles;
        }
        rootNode = &gFighterSkeletons[side][0];
        joints = &gFighterSkeletons[side][1];
        skinMatrices = gFighterSkinMatrices[side];
    } else {
        rootNode = GetSelectAuxModelParts();
        joints = rootNode + 1;
        skinMatrices = GetSelectAuxMatrixTable();
    }

    rootNode->rotation.vx = rotation->x;
    rootNode->rotation.vy = rotation->y;
    rootNode->rotation.vz = rotation->z;
    rootNode->localMatrix.t[0] = position[0];
    rootNode->localMatrix.t[1] = position[1];
    rootNode->localMatrix.t[2] = position[2];

    switch (mode) {
    case FIGHTER_POSE_MODE_ANGLES:
        i = 1;
        jointMap = gAnimToSkeletonJointMap;
        joints->localMatrix.t[0] = 0;
        joints->localMatrix.t[1] = 0;
        joints->localMatrix.t[2] = 0;
        joints->rotation.vx = 0;
        joints->rotation.vy = 0;
        joints->rotation.vz = 0;
        do {
            joints[i].rotation.vx = angles[*jointMap].x;
            joints[i].rotation.vy = -angles[*jointMap].y;
            joints[i].rotation.vz = -angles[*jointMap].z;
            ++jointMap;
            ++i;
        } while (i < FIGHTER_JOINT_COUNT);
        for (i = FIGHTER_JOINT_COUNT - 1; i >= 0; --i) {
            joints[i].matrixState = SCENE_MATRIX_DIRTY;
        }
        break;
    case FIGHTER_POSE_MODE_PRECOMPUTED:
        joints->rotation.vx = 0;
        joints->rotation.vy = 0;
        joints->rotation.vz = 0;
        joints->matrixState = SCENE_MATRIX_DIRTY;
        joints->localMatrix.t[0] = 0;
        joints->localMatrix.t[2] = 0;
        break;
    case FIGHTER_POSE_MODE_OPENING_DEMO:
        ApplyOpeningDemoPose(rootNode);
        break;
    case FIGHTER_POSE_MODE_MENU_SHOWCASE:
        ApplyMenuShowcasePose(side, rootNode);
        break;
    case FIGHTER_POSE_MODE_CUTSCENE:
        ApplyCutsceneFighterPose(side, rootNode);
        break;
    }

    heightScale = GetFighterHeightScale(side);
    i = 0;
    joints->localMatrix.t[1] = (joints->localMatrix.t[1] * heightScale) >> 12;
    rootNode->matrixState = SCENE_MATRIX_DIRTY;
    do {
        UpdateNodeWorldMatrix(&joints[i]);
        ++i;
    } while (i < FIGHTER_JOINT_COUNT);

    i = 0;
    skinMatrices[0] = &joints[5].worldMatrix;
    skinMatrices[1] = &joints[4].worldMatrix;
    skinMatrices[2] = &joints[3].worldMatrix;
    skinMatrices[3] = &joints[9].worldMatrix;
    skinMatrices[4] = &joints[8].worldMatrix;
    skinMatrices[5] = &joints[7].worldMatrix;
    skinMatrices[6] = &joints[2].worldMatrix;
    skinMatrices[7] = &joints[1].worldMatrix;
    skinMatrices[8] = &joints[16].worldMatrix;
    skinMatrices[9] = &joints[15].worldMatrix;
    skinMatrices[10] = &joints[13].worldMatrix;
    skinMatrices[11] = &joints[12].worldMatrix;
    skinMatrices[12] = &joints[11].worldMatrix;
    skinMatrices[13] = &joints[0].worldMatrix;
    skinMatrices[14] = &joints[6].worldMatrix;
    skinMatrices[15] = &joints[10].worldMatrix;
    skinMatrices[16] = &joints[17].worldMatrix;
    skinMatrices[17] = &joints[14].worldMatrix;
    /* the original writes this field through a pointer */
    *(MATRIX ***)&joints->param = skinMatrices;
    joints->meshIndex = 0;
    joints->renderType = SCENE_NODE_SKINNED;
    *gRenderQueueCursor++ = joints;
    /* side 2, the select screen's host, writes these past the array, onto gMoatBillboardNodes[0][0] */
    do {
        gFighterJointWorldMatrices[side][i] = &joints[i].worldMatrix;
        ++i;
    } while (i < FIGHTER_JOINT_COUNT);

    i = 1;
    do {
        if (joints[i].meshIndex >= 0) {
            *gRenderQueueCursor++ = &joints[i];
        }
        ++i;
    } while (i < FIGHTER_JOINT_COUNT);

    node = GetSkeletonCloneParts(side);
    while (1) {
        rootNode = node->param.cloneSource;
        if (rootNode == 0) {
            break;
        }
        node->worldMatrix = rootNode->worldMatrix;
        *gRenderQueueCursor++ = node;
        ++node;
    }

    if (side < 2) {
        if (stageId != STAGE_ID_DOZAEMON_MOAT) {
            SetSlotClothFloorY(side, floorY);
            SetShadowPartsFloorY(gFighterShadows[side], floorY);
            node = gFighterShadows[side];
            while (node->meshIndex != SCENE_MESH_NONE) {
                *gRenderQueueCursor++ = node;
                ++node;
            }
        } else if (GetMoatFloorHeight(joints[FIGHTER_JOINT_PELVIS].worldMatrix.t[0],
                                         joints[FIGHTER_JOINT_PELVIS].worldMatrix.t[2]) == 0) {
            SetSlotClothFloorY(side, floorY);
            SetShadowPartsFloorY(gFighterShadows[side], floorY);
            node = gFighterShadows[side];
            while (node->meshIndex != SCENE_MESH_NONE) {
                *gRenderQueueCursor++ = node;
                ++node;
            }
        } else {
            SetSlotClothFloorY(side, MOAT_LOWER_FLOOR_Y);
        }
        RegisterPointLightTarget(side, (WorldVector *)&joints->worldMatrix.t[0]);
        if (stageId == STAGE_ID_DOZAEMON_MOAT) {
            UpdateMoatFighterEffects(side, joints);
        }
    }
}

void BlendPoseJointRotations(int side, PoseAngles *fromPose, PoseAngles *toPose,
                             int weight, MATRIX *scratch)
{
    int i = 1;
    SceneNode *joints = &gFighterSkeletons[side][1];
    MATRIX *toMatrix = scratch + 1;
    SVECTOR rotation;

    do {
        int sourceJoint = gAnimToSkeletonJointMap[i - 1];

        rotation.vx = fromPose[sourceJoint].x;
        rotation.vy = -fromPose[sourceJoint].y;
        rotation.vz = -fromPose[sourceJoint].z;
        BuildRotationMatrixFast(&rotation, scratch);

        rotation.vx = toPose[sourceJoint].x;
        rotation.vy = -toPose[sourceJoint].y;
        rotation.vz = -toPose[sourceJoint].z;
        BuildRotationMatrixFast(&rotation, toMatrix);

        InterpolateOrthonormalMatrix(scratch, toMatrix, weight, &joints[i].localMatrix);
        joints[i].matrixState = SCENE_MATRIX_LOCAL_PRECOMPUTED;
        ++i;
    } while (i < FIGHTER_JOINT_COUNT);
}

void LoadDefaultFighters(void)
{
    LoadMatchFighters(0, 0, 0, 0);
}

void LoadMatchFighters(int p1ModelId, int p2ModelId, int p1WeaponId, int p2WeaponId)
{
    SelectFighterCharAndWeapon(0, p1ModelId, p1WeaponId);
    SelectFighterCharAndWeapon(1, p2ModelId, p2WeaponId);

    SetupFighterSkeletonAndModel(0, p1ModelId, &gFighterSkeletons[0][1], MODEL_SLOT_P1_FIGHTER);
    if (DidLastArenaLoadReload()) {
        LoadFighterBaseTextures(0, GetFighterInjuryOverlayMask(0));
    }
    SetupFighterSkeletonAndModel(1, p2ModelId, &gFighterSkeletons[1][1], MODEL_SLOT_P2_FIGHTER);
    if (DidLastArenaLoadReload()) {
        LoadFighterBaseTextures(1, GetFighterInjuryOverlayMask(1));
    }

    gFighterSkeletons[0][1].parent = gFighterSkeletons[0];
    gFighterSkeletons[1][1].parent = gFighterSkeletons[1];
    gFighterSkeletons[0][0].rotOrder = SCENE_ROT_ORDER_FAST;
    gFighterSkeletons[1][0].rotOrder = SCENE_ROT_ORDER_FAST;
    gFighterSkeletons[0][0].parent = 0;
    gFighterSkeletons[1][0].parent = 0;

    if (p1ModelId < STANDARD_WEAPON_CHAR_COUNT) {
        gFighterSkeletons[0][FIGHTER_NODE_LEFT_HAND].meshIndex = 1;
        gFighterSkeletons[0][FIGHTER_NODE_RIGHT_HAND].meshIndex = 0;
        gFighterSkeletons[0][FIGHTER_NODE_RIGHT_HAND].modelSlot = MODEL_SLOT_P1_WEAPON;
        gFighterSkeletons[0][FIGHTER_NODE_LEFT_HAND].modelSlot = MODEL_SLOT_P1_WEAPON;
    } else if ((unsigned int)p1ModelId - SHARED_WEAPON_CHAR_FIRST < PLAYABLE_CHARACTER_COUNT) {
        AttachSingleWeaponClone(0, &gFighterSkeletons[0][1], MODEL_SLOT_P1_WEAPON);
    }
    if (p2ModelId < STANDARD_WEAPON_CHAR_COUNT) {
        gFighterSkeletons[1][FIGHTER_NODE_LEFT_HAND].meshIndex = 1;
        gFighterSkeletons[1][FIGHTER_NODE_RIGHT_HAND].meshIndex = 0;
        gFighterSkeletons[1][FIGHTER_NODE_RIGHT_HAND].modelSlot = MODEL_SLOT_P2_WEAPON;
        gFighterSkeletons[1][FIGHTER_NODE_LEFT_HAND].modelSlot = MODEL_SLOT_P2_WEAPON;
    } else if ((unsigned int)p2ModelId - SHARED_WEAPON_CHAR_FIRST < PLAYABLE_CHARACTER_COUNT) {
        AttachSingleWeaponClone(1, &gFighterSkeletons[1][1], MODEL_SLOT_P2_WEAPON);
    }

    InitFighterShadowParts(&gFighterSkeletons[0][1], gFighterShadows[0], MODEL_SLOT_P1_SHADOW);
    InitFighterShadowParts(&gFighterSkeletons[1][1], gFighterShadows[1], MODEL_SLOT_P2_SHADOW);
    if (p1ModelId < STANDARD_WEAPON_CHAR_COUNT) {
        AppendWeaponShadowPart(&gFighterSkeletons[0][1], gFighterShadows[0], MODEL_SLOT_P1_WEAPON);
    }
    if (p2ModelId < STANDARD_WEAPON_CHAR_COUNT) {
        AppendWeaponShadowPart(&gFighterSkeletons[1][1], gFighterShadows[1], MODEL_SLOT_P2_WEAPON);
    }
    SetFighterClothSkeletonBase(0, &gFighterSkeletons[0][1]);
    SetFighterClothSkeletonBase(1, &gFighterSkeletons[1][1]);
    SetFighterClothDirty(0, 1);
    SetFighterClothDirty(1, 1);
    InitSubWeaponModels();
}

SceneNode *LoadSingleFighterSlot(int side, int modelId, int weaponId,
                                 SceneNode *skeletonParts, SceneNode *shadowParts,
                                 int reloadTextures)
{
    int injuryMask;
    int weaponSlot;

    if (skeletonParts == 0) {
        skeletonParts = gFighterSkeletons[side];
    }
    if ((unsigned int)side < 2) {
        shadowParts = gFighterShadows[side];
    }
    if (weaponId >= 0) {
        SelectFighterCharAndWeapon(side, modelId, weaponId);
    }
    SetupFighterSkeletonAndModel(side, modelId, &skeletonParts[1], side + MODEL_SLOT_P1_FIGHTER);
    if (reloadTextures != 0 && DidLastArenaLoadReload() != 0) {
        injuryMask = GetFighterInjuryOverlayMask(side);
        LoadFighterBaseTextures(side, injuryMask);
    }
    skeletonParts[1].parent = skeletonParts;
    skeletonParts[0].rotOrder = SCENE_ROT_ORDER_FAST;
    if (weaponId >= 0 && modelId < STANDARD_WEAPON_CHAR_COUNT) {
        skeletonParts[FIGHTER_NODE_LEFT_HAND].meshIndex = 1;
        weaponSlot = side + MODEL_SLOT_P1_WEAPON;
        skeletonParts[FIGHTER_NODE_RIGHT_HAND].meshIndex = 0;
        skeletonParts[FIGHTER_NODE_RIGHT_HAND].modelSlot = weaponSlot;
        skeletonParts[FIGHTER_NODE_LEFT_HAND].modelSlot = weaponSlot;
    }
    if (shadowParts != 0) {
        InitFighterShadowParts(&skeletonParts[1], shadowParts, side + MODEL_SLOT_P1_SHADOW);
        if (weaponId >= 0 && modelId < STANDARD_WEAPON_CHAR_COUNT) {
            /* Always appends to P1's shadow array regardless of side. */
            AppendWeaponShadowPart(&gFighterSkeletons[0][1], gFighterShadows[0], MODEL_SLOT_P1_WEAPON);
        }
    }
    SetFighterClothSkeletonBase(side, &skeletonParts[1]);
    SetFighterClothDirty(side, 1);
    return skeletonParts;
}

MATRIX **GetFighterWorldMatrixList(int side)
{
    return gFighterJointWorldMatrices[side];
}

/* Nothing calls it. */
long *GetFighterRootJointLocalPos(int side)
{
    return gFighterSkeletons[side][1].localMatrix.t;
}

SceneNode *GetFighterRootNode(int side)
{
    return gFighterSkeletons[side];
}

void TintSlashEnemyCluts(int red, int green, int blue)
{
    int characterId;
    const CharacterArchiveHeader *archive;
    const VramClutCoord *clutCoord;
    int r, g, b;

    characterId = GetFighterCharacterId(1);
    if ((unsigned int)(characterId - SLASH_ENEMY_CHAR_FIRST) < SLASH_ENEMY_CHAR_COUNT) {
        archive = GetCachedResourceBase(characterId);
        if (archive == 0)
            TriggerDebugTrap();
        LoadTimGroupWithOffset((const unsigned int *)archive + archive->timGroupOffset / 4,
                               0, TPAGE_P2_X_OFFSET, 0);
        DrawSync(0);
        r = (red << 12) / 255;
        g = (green << 12) / 255;
        b = (blue << 12) / 255;
        clutCoord = gSlashEnemyClutLists[characterId - SLASH_ENEMY_CHAR_FIRST];
        if (gRendererMonochrome)
            r = g = b = ConvertRgbToLuminance(r, g, b);
        while (clutCoord->x >= 0) {
            ScaleVramClutInPlace(clutCoord->x + TPAGE_P2_X_OFFSET, clutCoord->y,
                                 CLUT_4BIT_COLOR_COUNT, r, g, b);
            ++clutCoord;
        }
    }
}

void RemoveWeaponShadowPart(int side)
{
    SceneNode *node = gFighterShadows[side];

    while (node->meshIndex != SCENE_MESH_NONE) {
        if (node->modelSlot == side + MODEL_SLOT_P1_WEAPON) {
            node->meshIndex = SCENE_MESH_NONE;
            break;
        }
        ++node;
    }
}
