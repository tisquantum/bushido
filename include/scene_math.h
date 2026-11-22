#ifndef SCENE_MATH_H
#define SCENE_MATH_H

#include "scene_node.h"

struct SceneNode;

void BuildNegatedNodeRotation(struct SceneNode *node);
void UpdateNodeWorldMatrix(struct SceneNode *node);

extern short gSinLutQ12[4097];

void Init3dEngineNoOp(void);
int ComputeAcosQ12(int ratio);

#endif
