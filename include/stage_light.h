#ifndef STAGE_LIGHT_H
#define STAGE_LIGHT_H

#include "scene_node.h"
#include "scene_light.h"

void ApplyStageLighting(int scene);
void SetLightSlotFromHsvPreset(int slot, SceneLightValue *preset);
void SetStageClearAndFogColor(int rgb);
void ConvertHsvToRgbQ12(int *hsv, int *rgb);
void ConvertRgbToHsvQ12(const int *rgb, int *hsv);
void SetRendererDaytime(int daytime);
int IsRendererDaytime(void);
void InitSpecialLightSlot(void);
void SetNodeUseSpecialLight(struct SceneNode *node, int useSpecialLight);
void SetFighterUseSpecialLight(int side, int useSpecialLight);

#endif
