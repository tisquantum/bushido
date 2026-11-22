#ifndef FIGHTER_HIT_H
#define FIGHTER_HIT_H

#include "fighter.h"
#include "world_vector.h"
#include "stage_tables.h"

void ScaleFighterHitSpheres(int side, int statureScale);
void UpdateFighterHitGeometry(Fighter *fighter, WorldVector *sphereCenters, WorldVector *weaponPoints);
StageTransition *FindActiveStageTransition(void);

#endif
