#ifndef FIGHTER_POSE_H
#define FIGHTER_POSE_H

/* The fighters' skeletons and shadows (src/fighter_pose.c): what other units call. */

#include <sys/types.h>
#include <libgte.h>
#include "scene_node.h"

void UpdateAndEnqueueFighterPose(int side, int mode, long *position,
                                 PoseAngles *rotation, PoseAngles *poseAngles, int floorY);
void BlendPoseJointRotations(int side, PoseAngles *fromPose, PoseAngles *toPose,
                             int weight, MATRIX *scratch);
void LoadDefaultFighters(void);
void LoadMatchFighters(int p1ModelId, int p2ModelId, int p1WeaponId, int p2WeaponId);
struct SceneNode *LoadSingleFighterSlot(int side, int modelId, int weaponId,
                                        struct SceneNode *skeletonParts, struct SceneNode *shadowParts,
                                        int reloadTextures);
MATRIX **GetFighterWorldMatrixList(int side);
long *GetFighterRootJointLocalPos(int side);
struct SceneNode *GetFighterRootNode(int side);
void TintSlashEnemyCluts(int red, int green, int blue);
void RemoveWeaponShadowPart(int side);

#endif
