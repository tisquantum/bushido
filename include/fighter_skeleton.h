#ifndef FIGHTER_SKELETON_H
#define FIGHTER_SKELETON_H

#include "scene_node.h"

/* Fighter skeleton setup (src/fighter_skeleton.c): what other units call. */

/* Indices into a fighter's 18-joint SceneNode array (nodes 1..18 of gFighterSkeletons). */
enum FighterJointIndex {
    FIGHTER_JOINT_HEAD = 2,
    FIGHTER_JOINT_RIGHT_HAND = 6,
    FIGHTER_JOINT_LEFT_HAND = 10,
    FIGHTER_JOINT_PELVIS = 11,
    FIGHTER_JOINT_RIGHT_THIGH = 12,
    FIGHTER_JOINT_RIGHT_FOOT = 14,
    FIGHTER_JOINT_LEFT_THIGH = 15,
    FIGHTER_JOINT_LEFT_FOOT = 17
};

/* A joint's node in a skeleton array whose first node is the root. */
#define FIGHTER_NODE_RIGHT_HAND (FIGHTER_JOINT_RIGHT_HAND + 1)
#define FIGHTER_NODE_LEFT_HAND  (FIGHTER_JOINT_LEFT_HAND + 1)
#define FIGHTER_NODE_RIGHT_FOOT (FIGHTER_JOINT_RIGHT_FOOT + 1)
#define FIGHTER_NODE_LEFT_FOOT  (FIGHTER_JOINT_LEFT_FOOT + 1)

/* Character archive IDs (DATA0\M00..M17.DAT, DATA6\A07..A09.DAT). */
enum CharacterArchiveId {
    CHAR_ARCHIVE_KANNUKI = 0,
    CHAR_ARCHIVE_MIKADO = 1,
    CHAR_ARCHIVE_BLACK_LOTUS = 2,
    CHAR_ARCHIVE_TATSUMI = 3,
    CHAR_ARCHIVE_RED_SHADOW = 4,
    CHAR_ARCHIVE_UTSUSEMI = 5,
    CHAR_ARCHIVE_KANNUKI_ALT = 6,
    CHAR_ARCHIVE_MIKADO_ALT = 7,
    CHAR_ARCHIVE_BLACK_LOTUS_ALT = 8,
    CHAR_ARCHIVE_TATSUMI_ALT = 9,
    CHAR_ARCHIVE_RED_SHADOW_ALT = 10,
    CHAR_ARCHIVE_UTSUSEMI_ALT = 11,
    CHAR_ARCHIVE_FINAL_BOSS = 12,
    CHAR_ARCHIVE_FINAL_BOSS_ALT = 13,
    CHAR_ARCHIVE_KATZE = 14,
    CHAR_ARCHIVE_BOUT8_BOSS = 15,
    CHAR_ARCHIVE_BOUT6_BOSS = 16,
    CHAR_ARCHIVE_BOUT7_BOSS = 17,
    CHAR_ARCHIVE_SELECT_HOST = 18,
    CHAR_ARCHIVE_DEMO_KANNUKI = 32,
    CHAR_ARCHIVE_DEMO_BLACK_LOTUS = 33,
    CHAR_ARCHIVE_DEMO_RED_SHADOW = 34
};

int SetupFighterSkeletonAndModel(int slot, int characterId, struct SceneNode *joints, int modelSlot);
void InitFighterShadowParts(struct SceneNode *joints, struct SceneNode *shadows, int modelSlot);
void AppendWeaponShadowPart(struct SceneNode *joints, struct SceneNode *shadows, int modelSlot);
void SelectFighterCharAndWeapon(int slot, int characterId, int weaponId);
void ReloadFighterWeaponModel(int slot, struct SceneNode *joints, int modelSlot, int weaponId);
void AttachSingleWeaponClone(int slot, struct SceneNode *joints, int modelSlot);
struct SceneNode *GetSkeletonCloneParts(int slot);
int GetFighterHeightScale(int slot);
struct SceneNode *GetSkeletonPartByMeshIndex(int slot, int meshIndex);
void SetShadowPartsFloorY(struct SceneNode *shadows, int floorY);

extern short gCharHeightScales[42];

void InitPoseParts(struct SceneNode *joints, int characterId, int modelSlot, int lightSlot);
void ApplyPosePartOverrides(const unsigned short *overrides, int slot, struct SceneNode *joints);

#endif
