#ifndef DISPLAY_LIST_H
#define DISPLAY_LIST_H

#include "scene_node.h"

void SubmitSceneDisplayList(void);

extern struct SceneNode *gCameraNodePtr;

void TrapInvalidPrimType(void);

#endif
