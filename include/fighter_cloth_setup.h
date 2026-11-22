#ifndef FIGHTER_CLOTH_SETUP_H
#define FIGHTER_CLOTH_SETUP_H

#include "scene_node.h"

void RebuildFighterClothIfDirty(int slot);
void SetFighterClothDirty(int slot, int dirty);

void SetFighterClothSkeletonBase(int slot, struct SceneNode *skeleton);

#endif
