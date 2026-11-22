#ifndef SCENE_H
#define SCENE_H

#include <sys/types.h>
#include <libgte.h>
#include "scene_node.h"

extern struct SceneNode *gRenderQueue[512];

void Init3dEngine(void);
void BeginSceneFrame(VECTOR *target, SVECTOR *rotation, int distance);
void EndSceneFrame(void);

extern short gSceneUnreadFlag; /* written, never read */
extern unsigned short gSceneFrameCounter;

#endif
