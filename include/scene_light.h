#ifndef SCENE_LIGHT_H
#define SCENE_LIGHT_H

#include "stage_id.h"

/* One HSV light preset word: an int in the Japanese build (28 B record), a short in the US/EU builds (14 B record). */
#ifdef VERSION_JP
typedef int SceneLightValue;
#else
typedef short SceneLightValue;
#endif

#define STAGE_LIGHT_PRESET_COUNT (STAGE_COUNT + 1) /* and the character select scene */
#define STAGE_LIGHT_YAW_COUNT    30 /* 27 scene yaw entries + 3 trailing zero entries */

typedef enum StageLightTarget {
    STAGE_LIGHT_FIGHTER = 0, /* Fighter light slots (LIGHT_SLOT_P1, LIGHT_SLOT_P2) */
    STAGE_LIGHT_STAGE = 1,   /* Stage light slot (LIGHT_SLOT_STAGE), and daytime char-fog override */
    STAGE_LIGHT_TARGET_COUNT = 2
} StageLightTarget;

/* A light preset, read in order by SetLightSlotFromHsvPreset: the light's hue, saturation and value, the
 * ambient's hue, saturation and value (each 0..4096), and the light's pitch (4096 to the turn). */
typedef SceneLightValue SceneLightPreset[7];

struct SceneNode;

#endif
