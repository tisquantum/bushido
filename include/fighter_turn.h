#ifndef FIGHTER_TURN_H
#define FIGHTER_TURN_H

#include "fighter.h"

void AdjustFighterTurnTarget(Fighter *fighter, int targetHeading);
void SetFighterTurnTarget(Fighter *fighter, int targetHeading);
void StartFighterPoseBlend(Fighter *fighter, int blendStep);

#endif
