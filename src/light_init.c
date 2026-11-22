/*
 * Scene cameras and light slots at engine start: the main and secondary camera nodes, the eight
 * GTE light slots with their default preset, a slot loaded from a stage preset, and the light
 * direction matrix rebuilt from each light's pitch and yaw.
 */
#include <sys/types.h>
#include <libgte.h>

#include "display_list.h"
#include "light.h"
#include "light_init.h"
#include "scene_node.h"
#include "scene_math.h"

struct SceneNode gSecondaryCameraNode;

void InitSceneCameras(void)
{
    gCameraNode.renderType = SCENE_NODE_CAMERA_MAIN;
    gCameraNode.rotation.vx = 0;
    gCameraNode.rotation.vy = 0;
    gCameraNode.rotation.vz = 0;
    gCameraNode.localMatrix.t[0] = 0;
    gCameraNode.localMatrix.t[1] = 0;
    gCameraNode.localMatrix.t[2] = 0;
    gCameraNode.parent = 0;
    gCameraNode.rotOrder = SCENE_ROT_ORDER_ZXY;
    BuildNegatedNodeRotation(&gCameraNode);
    gSecondaryCameraNode.renderType = SCENE_NODE_CAMERA_SECONDARY;
    gSecondaryCameraNode.rotation.vx = 0;
    gSecondaryCameraNode.rotation.vy = 0;
    gSecondaryCameraNode.rotation.vz = 0;
    gSecondaryCameraNode.localMatrix.t[0] = 0;
    gSecondaryCameraNode.localMatrix.t[1] = 0;
    gSecondaryCameraNode.localMatrix.t[2] = 0;
    gSecondaryCameraNode.parent = 0;
    gSecondaryCameraNode.rotOrder = SCENE_ROT_ORDER_ZXY;
    BuildNegatedNodeRotation(&gSecondaryCameraNode);
    gCameraNodePtr = &gCameraNode;
    gSecondaryCameraNodePtr = &gSecondaryCameraNode;
}

void InitLightSlots(void)
{
    LightSlotPreset preset = {
        {{0xE00, 0xE00, 0xE00}, {0, 0, 0}, {0, 0, 0}},
        {{0xE00, 0xA00, 1}, {0, 0, 0}, {0, 0, 0}},
        {48, 48, 48},
        ONE
    };
    int i;

    for (i = 0; i < LIGHT_SLOT_COUNT; i++) {
        gActiveLightSlot = &gLightSlots[i];
        gActiveLightSlot->colorMatrix.m[0][0] = preset.lightColors[0][0];
        gActiveLightSlot->colorMatrix.m[1][0] = preset.lightColors[0][1];
        gActiveLightSlot->colorMatrix.m[2][0] = preset.lightColors[0][2];
        gActiveLightSlot->colorMatrix.m[0][1] = preset.lightColors[1][0];
        gActiveLightSlot->colorMatrix.m[1][1] = preset.lightColors[1][1];
        gActiveLightSlot->colorMatrix.m[2][1] = preset.lightColors[1][2];
        gActiveLightSlot->colorMatrix.m[0][2] = preset.lightColors[2][0];
        gActiveLightSlot->colorMatrix.m[1][2] = preset.lightColors[2][1];
        gActiveLightSlot->colorMatrix.m[2][2] = preset.lightColors[2][2];

        gActiveLightSlot->lightMatrix.m[0][0] = 0;
        gActiveLightSlot->lightMatrix.m[0][1] = 0;
        gActiveLightSlot->lightMatrix.m[0][2] = 0;
        gActiveLightSlot->lightMatrix.m[1][0] = 0;
        gActiveLightSlot->lightMatrix.m[1][1] = 0;
        gActiveLightSlot->lightMatrix.m[1][2] = 0;
        gActiveLightSlot->lightMatrix.m[2][0] = 0;
        gActiveLightSlot->lightMatrix.m[2][1] = 0;
        gActiveLightSlot->lightMatrix.m[2][2] = 0;

        gActiveLightSlot->directions[0].pitch = preset.directions[0].pitch;
        gActiveLightSlot->directions[0].yaw = preset.directions[0].yaw;
        gActiveLightSlot->directions[0].active = preset.directions[0].active;
        gActiveLightSlot->directions[1].pitch = preset.directions[1].pitch;
        gActiveLightSlot->directions[1].yaw = preset.directions[1].yaw;
        gActiveLightSlot->directions[1].active = preset.directions[1].active;
        gActiveLightSlot->directions[2].pitch = preset.directions[2].pitch;
        gActiveLightSlot->directions[2].yaw = preset.directions[2].yaw;
        gActiveLightSlot->directions[2].active = preset.directions[2].active;
        /* The direction rebuild uses the slot's previous scale. */
        RebuildLightDirectionMatrix(gActiveLightSlot);

        gActiveLightSlot->ambientRgb[0] = preset.ambientRgb[0];
        gActiveLightSlot->ambientRgb[1] = preset.ambientRgb[1];
        gActiveLightSlot->ambientRgb[2] = preset.ambientRgb[2];
        gActiveLightSlot->scale = preset.scale;
    }
    gActiveLightSlot = gLightSlots;
    ApplyActiveLightColorToGte();
}

/* Nothing calls it. */
static void LoadLightSlotFromPreset(LightSlot *slot, unsigned short *preset)
{
    slot->colorMatrix.m[0][0] = *preset++;
    slot->colorMatrix.m[1][0] = *preset++;
    slot->colorMatrix.m[2][0] = *preset++;
    slot->colorMatrix.m[0][1] = *preset++;
    slot->colorMatrix.m[1][1] = *preset++;
    slot->colorMatrix.m[2][1] = *preset++;
    slot->colorMatrix.m[0][2] = *preset++;
    slot->colorMatrix.m[1][2] = *preset++;
    slot->colorMatrix.m[2][2] = *preset++;

    slot->lightMatrix.m[0][0] = 0;
    slot->lightMatrix.m[0][1] = 0;
    slot->lightMatrix.m[0][2] = 0;
    slot->lightMatrix.m[1][0] = 0;
    slot->lightMatrix.m[1][1] = 0;
    slot->lightMatrix.m[1][2] = 0;
    slot->lightMatrix.m[2][0] = 0;
    slot->lightMatrix.m[2][1] = 0;
    slot->lightMatrix.m[2][2] = 0;

    slot->directions[0].pitch = *preset++;
    slot->directions[0].yaw = *preset++;
    slot->directions[0].active = *preset++;
    slot->directions[1].pitch = *preset++;
    slot->directions[1].yaw = *preset++;
    slot->directions[1].active = *preset++;
    slot->directions[2].pitch = *preset++;
    slot->directions[2].yaw = *preset++;
    slot->directions[2].active = *preset++;
    RebuildLightDirectionMatrix(slot);

    slot->ambientRgb[0] = *preset++;
    slot->ambientRgb[1] = *preset++;
    slot->ambientRgb[2] = *preset++;
    slot->scale = *preset++;
}

void RebuildLightDirectionMatrix(LightSlot *slot)
{
    LightDirection *light;
    short i;
    short pitchCos;

    for (i = 0; i < LIGHT_DIRECTION_COUNT; i++) {
        light = &slot->directions[i];
        if (light->active) {
            pitchCos = rcos(light->pitch);
            slot->lightMatrix.m[i][0] = -(((rsin(light->yaw) * pitchCos) >> 12)
                                 * slot->scale) >> 12;
            slot->lightMatrix.m[i][1] = (rsin(light->pitch) * slot->scale) >> 12;
            slot->lightMatrix.m[i][2] = -(((rcos(light->yaw) * pitchCos) >> 12)
                                 * slot->scale) >> 12;
        } else {
            slot->lightMatrix.m[i][0] = 0;
            slot->lightMatrix.m[i][1] = 0;
            slot->lightMatrix.m[i][2] = 0;
        }
    }
}

void ApplyActiveLightColorToGte(void)
{
    SetColorMatrix(&gActiveLightSlot->colorMatrix);
    SetBackColor(gActiveLightSlot->ambientRgb[0], gActiveLightSlot->ambientRgb[1],
                 gActiveLightSlot->ambientRgb[2]);
}

void InitPropIdentityMatrix3x3(MATRIX *matrix)
{
    matrix->m[0][0] = ONE;
    matrix->m[0][1] = 0;
    matrix->m[0][2] = 0;
    matrix->m[1][0] = 0;
    matrix->m[1][1] = ONE;
    matrix->m[1][2] = 0;
    matrix->m[2][0] = 0;
    matrix->m[2][1] = 0;
    matrix->m[2][2] = ONE;
}
