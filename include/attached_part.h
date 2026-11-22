#ifndef ATTACHED_PART_H
#define ATTACHED_PART_H

#include "scene_node.h"

void ClearAttachedPartPool(void);
struct SceneNode *AllocAttachedPartSlot(int kind);
void FreeAttachedPartSlot(struct SceneNode *node);
void UpdateAttachedPartPool(void);

#endif
