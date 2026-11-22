#ifndef DECAL_DRIVER_H
#define DECAL_DRIVER_H

struct SceneNode;

/* What the stage callbacks and the animation events call of the ground decal driver. */
void InitStageGroundDecals(void);
void UpdateAndDrawStageGroundDecals(void);
void SpawnFighterFootprint(int side, int footprintIndex, int *position);
void UpdateFootDecalContact(int fighterIndex, int footIndex, struct SceneNode *foot);

#endif
