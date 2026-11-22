/* Stage lighting: each stage's day and night HSV light presets and sky colours, applied to the
 * fighter and stage light slots; HSV and RGB conversion in Q12; the special light slot. */
#include <sys/types.h>
#include <libgte.h>
#include "fighter_pose.h"
#include "fighter_skeleton.h"
#include "fog.h"
#include "globals.h"
#include "light.h"
#include "light_init.h"
#include "main.h"
#include "model_arena.h"
#include "scene_node.h"
#include "scene_light.h"
#include "stage_light.h"
#include "tim.h"

SceneLightPreset gStageLightPresetsNight[STAGE_LIGHT_PRESET_COUNT][STAGE_LIGHT_TARGET_COUNT] = {
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 3207, 205, 775, 2474, 521, 1987, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 7, 3585, 3297, 2842, 1827, 2799, 3650 },
        { 181, 1912, 2799, 3741, 858, 2176, 3650 },
    },
    {
        { 507, 569, 2431, 1975, 427, 1547, 3650 },
        { 2739, 578, 2225, 2446, 681, 1852, 3650 },
    },
    {
        { 3207, 260, 2034, 1399, 971, 1547, 3650 },
        { 2305, 2273, 2784, 1589, 1404, 2389, 3650 },
    },
    {
        { 1137, 1227, 2491, 1527, 1158, 1709, 3650 },
        { 2318, 558, 1724, 1501, 1218, 2887, 3650 },
    },
    {
        { 2952, 526, 2559, 245, 1834, 1710, 3072 },
        { 459, 1325, 1740, 343, 793, 1817, 3072 },
    },
    {
        { 547, 586, 2048, 3842, 734, 1979, 3650 },
        { 682, 924, 2048, 2730, 1066, 2769, 3650 },
    },
    {
        { 1222, 555, 2390, 2622, 1250, 2011, 3650 },
        { 2685, 821, 2562, 1274, 984, 2555, 3650 },
    },
    { { 0, 0, 2048, 0, 0, 2048, 3650 }, { 0, 0, 2048, 0, 0, 2048, 3650 } },
    {
        { 4064, 716, 1640, 288, 704, 1560, 3650 },
        { 1726, 393, 2303, 3139, 1217, 2233, 3650 },
    },
    { { 0, 0, 2193, 0, 0, 1818, 3650 }, { 2730, 1, 2049, 2730, 35, 2062, 3650 } },
    {
        { 615, 327, 2151, 2761, 1548, 3020, 3650 },
        { 2730, 166, 2135, 2730, 580, 2068, 3650 },
    },
    {
        { 3601, 458, 2516, 2064, 672, 1672, 3650 },
        { 2726, 1379, 2387, 2809, 976, 1800, 3650 },
    },
    {
        { 571, 2078, 3011, 2884, 1001, 2024, 3650 },
        { 661, 1989, 1785, 2780, 1432, 1018, 3650 },
    },
    {
        { 669, 1742, 1926, 2574, 883, 1967, 3650 },
        { 647, 1139, 1962, 2786, 122, 2056, 3650 },
    },
    {
        { 1841, 1959, 2771, 233, 376, 1490, 3650 },
        { 2730, 905, 2629, 1927, 1797, 2609, 3650 },
    },
    { { 0, 0, 2048, 0, 0, 2048, 3650 }, { 0, 0, 2048, 0, 0, 2048, 3650 } },
};
SceneLightPreset gStageLightPresetsDay[STAGE_LIGHT_PRESET_COUNT][STAGE_LIGHT_TARGET_COUNT] = {
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 2048, 1368, 4096, 1886, 136, 1468, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    {
        { 7, 3585, 4064, 2842, 1827, 1796, 3650 },
        { 181, 1912, 2799, 3741, 858, 2176, 3650 },
    },
    {
        { 612, 3151, 4096, 2935, 455, 1760, 3908 },
        { 276, 941, 2392, 117, 1138, 2130, 3908 },
    },
    { { 753, 570, 2476, 1736, 497, 1497, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 1365, 155, 2129, 2010, 258, 1441, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    {
        { 4092, 975, 2577, 682, 1141, 1362, 3072 },
        { 1016, 2900, 2048, 264, 1853, 1229, 3072 },
    },
    { { 779, 337, 2829, 2611, 1203, 2362, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 1972, 666, 3668, 2487, 1153, 2379, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 0, 0, 2048, 0, 0, 2048, 3650 }, { 0, 0, 2048, 0, 0, 2048, 3650 } },
    {
        { 214, 996, 2648, 1659, 685, 1370, 3650 },
        { 10, 757, 1125, 1641, 1393, 1534, 3650 },
    },
    {
        { 682, 1185, 2341, 615, 492, 1016, 3650 },
        { 2730, 1, 2049, 2730, 35, 2062, 3650 },
    },
    { { 2738, 387, 2647, 2338, 582, 2033, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    { { 3605, 442, 2320, 2930, 629, 1392, 3650 }, { 0, 0, 0, 0, 0, 2174, 3650 } },
    {
        { 571, 2078, 3011, 2877, 1000, 1616, 3650 },
        { 661, 1989, 1785, 2780, 1432, 1018, 3650 },
    },
    {
        { 779, 1897, 2536, 2405, 883, 1808, 3650 },
        { 650, 1173, 2081, 567, 1611, 2550, 3650 },
    },
    {
        { 672, 3119, 2505, 2753, 1587, 2849, 3650 },
        { 3224, 1513, 2595, 3013, 1018, 2332, 3650 },
    },
    { { 0, 0, 2048, 0, 0, 2048, 3650 }, { 0, 0, 2048, 0, 0, 2048, 3650 } },
};
/* Per-stage daytime sky/fog clear colors packed as 24-bit 0xRRGGBB. */
int gStageSkyClearRgb[STAGE_LIGHT_PRESET_COUNT] = {
    0x9d9d9d, 0x9d9d9d, 0x9d9d9d, 0x9d9d9d, 0x9d9d9d, 0x9d9d9d, 0x9d9d9d, 0x9d9d9d, 0x9d9d9d,
    0x9d9d9d, 0x9d9d9d, 0x000000, 0x8b3006, 0x9d9d9d, 0x9d9d9d, 0x000000, 0x9d9d9d, 0x9d9d9d,
    0x000000, 0x222424, 0x000000, 0x9d9d9d, 0x9d9d9d, 0x000000, 0x140400, 0x0e0014, 0x000000,
};
/* Per-stage primary directional light azimuth angles (4096 = 360 deg). */
unsigned short gStageLightYaws[STAGE_LIGHT_YAW_COUNT] = {
    0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180,
    0x0400, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0200, 0x0180, 0x0180, 0x0180,
    0x0180, 0x0180, 0x0a00, 0x0000, 0x0000, 0x0000,
};
int gRendererDaytime = 0;

void ApplyStageLighting(int scene)
{
    SceneLightPreset *preset;
    unsigned short lightYaw;
    int clearRgb;

    if (scene < 0) {
        return;
    }
    if (gRendererDaytime) {
        preset = gStageLightPresetsDay[scene];
    } else {
        preset = gStageLightPresetsNight[scene];
    }
    lightYaw = gStageLightYaws[scene];
    gLightSlots[LIGHT_SLOT_STAGE].directions[LIGHT_DIR_PRIMARY].yaw = lightYaw;
    gLightSlots[LIGHT_SLOT_P2].directions[LIGHT_DIR_PRIMARY].yaw = lightYaw;
    gLightSlots[LIGHT_SLOT_P1].directions[LIGHT_DIR_PRIMARY].yaw = lightYaw;
    if (gRendererDaytime && (IsCharFogSlotActive(CHAR_SLOT_P1) || IsCharFogSlotActive(CHAR_SLOT_P2))) {
        SetLightSlotFromHsvPreset(LIGHT_SLOT_P1, preset[STAGE_LIGHT_STAGE]);
        SetLightSlotFromHsvPreset(LIGHT_SLOT_P2, preset[STAGE_LIGHT_STAGE]);
        SetLightSlotFromHsvPreset(LIGHT_SLOT_STAGE, preset[STAGE_LIGHT_STAGE]);
    } else {
        SetLightSlotFromHsvPreset(LIGHT_SLOT_P1, preset[STAGE_LIGHT_FIGHTER]);
        SetLightSlotFromHsvPreset(LIGHT_SLOT_P2, preset[STAGE_LIGHT_FIGHTER]);
        SetLightSlotFromHsvPreset(LIGHT_SLOT_STAGE, preset[STAGE_LIGHT_STAGE]);
    }
    if (gRendererDaytime) {
        clearRgb = gStageSkyClearRgb[scene];
    } else {
        clearRgb = 0;
    }
    SetStageClearAndFogColor(clearRgb);
}

void SetLightSlotFromHsvPreset(int slot, SceneLightValue *preset)
{
    int hsv[3];
    int rgb[3];

    hsv[0] = *preset++;
    hsv[1] = *preset++;
    hsv[2] = *preset++;
    ConvertHsvToRgbQ12(hsv, rgb);
    gLightSlots[slot].colorMatrix.m[0][LIGHT_DIR_PRIMARY] = rgb[0];
    gLightSlots[slot].colorMatrix.m[1][LIGHT_DIR_PRIMARY] = rgb[1];
    gLightSlots[slot].colorMatrix.m[2][LIGHT_DIR_PRIMARY] = rgb[2];

    hsv[0] = *preset++;
    hsv[1] = *preset++;
    hsv[2] = *preset++;
    ConvertHsvToRgbQ12(hsv, rgb);
    gLightSlots[slot].ambientRgb[0] = (rgb[0] * 255) >> 12;
    gLightSlots[slot].ambientRgb[1] = (rgb[1] * 255) >> 12;
    gLightSlots[slot].ambientRgb[2] = (rgb[2] * 255) >> 12;
    gLightSlots[slot].directions[LIGHT_DIR_PRIMARY].pitch = *preset;
    RebuildLightDirectionMatrix(&gLightSlots[slot]);
}

void SetStageClearAndFogColor(int rgb)
{
    int red = (rgb >> 16) & 255;
    int green = (rgb >> 8) & 255;
    int blue = rgb & 255;

    if (gRendererMonochrome)
        red = green = blue = ConvertRgbToLuminance(red, green, blue);
    SetBackgroundClear(1, red, green, blue);
    SetFarColor(red, green, blue);
}

void ConvertHsvToRgbQ12(int *hsv, int *rgb)
{
    int hue, saturation, value;
    int red, green, blue;
    int fraction, rising, falling;
    int p, q, t;

    hue = hsv[0];
    saturation = hsv[1];
    value = hsv[2];

    if (IsRendererMonochrome())
        saturation = 0;

    if (saturation == 0) {
        green = value;
        red = value;
        blue = value;
    } else {
        if (hue == ONE)
            hue = 0;

        p = value * (ONE - saturation);
        hue *= 6;
        fraction = hue & (ONE - 1);
        rising = (saturation * fraction) >> 12;
        falling = (saturation * (ONE - fraction)) >> 12;
        q = value * (ONE - rising);
        t = value * (ONE - falling);
        p >>= 12;
        q >>= 12;
        t >>= 12;

        switch (hue >> 12) {
        case 0:
            red = value;
            green = t;
            blue = p;
            break;
        case 1:
            red = q;
            green = value;
            blue = p;
            break;
        case 2:
            red = p;
            green = value;
            blue = t;
            break;
        case 3:
            red = p;
            green = q;
            blue = value;
            break;
        case 4:
            red = t;
            green = p;
            blue = value;
            break;
        case 5:
            red = value;
            green = p;
            blue = q;
            break;
        }

    }
    rgb[0] = red;
    rgb[1] = green;
    rgb[2] = blue;
}

/* Nothing calls it. */
void ConvertRgbToHsvQ12(const int *rgb, int *hsv)
{
    int r, g, b;
    int max, min, value, range;
    int saturation, hue;
    int dr, dg, db;

    r = rgb[0];
    g = rgb[1];
    b = rgb[2];

    if (r >= g) {
        if (r >= b)
            max = r;
        else
            max = b;
    } else {
        if (g >= b)
            max = g;
        else
            max = b;
    }

    if (g >= r) {
        if (b >= r)
            min = r;
        else
            min = b;
    } else {
        if (b >= g)
            min = g;
        else
            min = b;
    }

    range = max - min;
    value = max;
    if (value != 0)
        saturation = (range << 12) / value;
    else
        saturation = 0;

    if (saturation != 0) {
        dr = ((max - r) << 12) / range;
        dg = ((max - g) << 12) / range;
        db = ((max - b) << 12) / range;

        if (r == max) {
            hue = db - dg;
        } else if (g == max) {
            hue = 2 * ONE + dr - db;
        } else if (b == max) {
            hue = 4 * ONE + dg - dr;
        }

        hue /= 6;
        if (hue < 0)
            hue += ONE;
    } else {
        hue = 0;
    }

    hsv[0] = hue;
    hsv[1] = saturation;
    hsv[2] = value;
}

void SetRendererDaytime(int daytime)
{
    if (daytime != 0) {
        gRendererDaytime = 1;
    } else {
        gRendererDaytime = 0;
    }
}

int IsRendererDaytime(void)
{
    return gRendererDaytime;
}

/* Read as a SceneLightPreset. Japan reads seven ints: no direct light, a dim ambient (H 1683,
 * S 3384, V 725), pitch 3650. USA and Europe read the words as shorts: no light, no ambient, pitch 1683. */
int gSpecialLightPreset[7] = { 0, 0, 0, 1683, 3384, 725, 3650 };

void InitSpecialLightSlot(void)
{
    SetLightSlotFromHsvPreset(LIGHT_SLOT_SPECIAL, (SceneLightValue *)gSpecialLightPreset);
}

void SetNodeUseSpecialLight(struct SceneNode *node, int useSpecialLight)
{
    int slot;

    if (useSpecialLight == 0) {
        slot = LIGHT_SLOT_STAGE;
    } else {
        slot = LIGHT_SLOT_SPECIAL;
    }
    node->lightSlot = slot;
}

void SetFighterUseSpecialLight(int side, int useSpecialLight)
{
    int lightSlot;
    int i;
    struct SceneNode *joint;
    struct SceneNode *clone;

    lightSlot = side;
    if (useSpecialLight != 0) {
        lightSlot = LIGHT_SLOT_SPECIAL;
    }

    joint = GetFighterRootNode(side) + 1;
    for (i = 0; i < FIGHTER_JOINT_COUNT; i++) {
        if (joint->meshIndex >= 0) {
            joint->lightSlot = lightSlot;
        }
        joint++;
    }

    clone = GetSkeletonCloneParts(side);
    while (clone->param.cloneSource != 0) {
        clone->lightSlot = lightSlot;
        clone++;
    }
}
