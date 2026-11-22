#ifndef COLLISION_H
#define COLLISION_H

#include "fighter.h"

void UpdateCombatCollisions(void);
void TestTriangleVsBodySpheres(Fighter *fighter, int isNonDegenerate, int checkVital);

#endif
