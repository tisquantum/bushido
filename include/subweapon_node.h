#ifndef SUBWEAPON_NODE_H
#define SUBWEAPON_NODE_H

#include "world_vector.h"

void InitSubWeaponModels(void);
void InitStage4SubWeaponSlot(int stageId);
void SubmitSubWeaponNode(int modelOwner, WorldVector *position, int yaw);

#endif
