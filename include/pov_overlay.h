#ifndef POV_OVERLAY_H
#define POV_OVERLAY_H

#include "scene_node.h"

struct SceneNode *LoadPovOverlayModel(int fighterSlot, char *resource);
void UpdatePovOverlayModel(int orbitAngle);

/* pov_flat.s and pov_tex.s: dispatch-table drawers, inputs in registers. */
void DrawOverlayPolyF3(void);
void DrawOverlayPolyF4(void);
void DrawOverlayPolyFt3(void);
void DrawOverlayPolyFt4(void);

#define POV_OVERLAY_BUFFER_SIZE 0x4000
extern char gPovOverlayModelBuffer[POV_OVERLAY_BUFFER_SIZE];

#endif
