#ifndef RIBBON_DRIVER_H
#define RIBBON_DRIVER_H

#include <sys/types.h>
#include <libgte.h>

struct SceneNode;

/* The opening demo's ribbon trails: set one up, then feed it hilt and tip each frame. */
void InitRibbonTrail(int textureSet, int trailIndex);
void UpdateAndDrawRibbonTrail(int trailIndex, VECTOR *hilt, VECTOR *tip);
void UpdateRibbonTrailFromBone(int trailIndex, struct SceneNode *bone, SVECTOR *hiltOffset, SVECTOR *tipOffset);

#endif
