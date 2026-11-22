/*
 * The point-light flash: a preset color fading from start to end at a world position,
 * lighting the two fighters from its direction while they are within range.
 */
#include <sys/types.h>
#include <libgte.h>
#include "light.h"
#include "light_init.h"
#include "point_light.h"
#include "render.h"
#include "tim.h"
static WorldVector gPointLightOrigin; /* where the flash is, for the targets' light directions */
/* 444 bytes; nothing reads or writes them. */
static unsigned char gPointLightUnusedBss[444];

#define POINT_LIGHT_PRESET_COUNT      20
#define POINT_LIGHT_PRESET_FIELDS     7
#define POINT_LIGHT_ALT_PRESET_OFFSET 15
#define POINT_LIGHT_FADE_MAX          4096
#define POINT_LIGHT_TARGET_COUNT      2
#define POINT_LIGHT_GTE_DELTA_LIMIT   0x7000
#define POINT_LIGHT_MAX_RANGE_SQ      25000000 /* 5000^2 world units */

/* Each row is { startR, startG, startB, endR, endG, endB, fadeRate } in Q12
 * (4096 = 1.0); fadeRate < 0 marks an unused preset. Rows 0..14 match the
 * billboard effect kinds (TriggerPointLightFlash) and rows 15..19 the 3D
 * combat effect types (TriggerAltPointLightFlash). */
short gPointLightFlashPresets[POINT_LIGHT_PRESET_COUNT][POINT_LIGHT_PRESET_FIELDS] = {
    { 4096, 0, 0, 0, 0, 0, 204 },
    { 0, 4096, 0, 0, 0, 0, 204 },
    { 4096, 0, 0, 0, 0, 0, 227 },       /* limb wound */
    { 3968, 2688, 1024, 0, 0, 0, 227 },
    { 1408, 128, 3968, 0, 0, 0, 227 },  /* fatal hit (primary) */
    { 2176, 2816, 3200, 0, 0, 0, 227 }, /* weapon clash */
    { 4096, 0, 0, 0, 0, 0, 227 },       /* fatal hit (secondary) */
    { 4096, 384, 640, 0, 0, 0, 227 },   /* torso wound */
    { 0, 0, 0, 0, 0, 0, -1 },
    { 0, 0, 0, 0, 0, 0, -1 },
    { 0, 0, 0, 0, 0, 0, -1 },
    { 0, 0, 0, 0, 0, 0, -1 },
    { 0, 0, 0, 0, 0, 0, -1 },
    { 0, 0, 0, 0, 0, 0, -1 },
    { 0, 0, 0, 0, 0, 0, -1 },
    { 128, 3968, 3968, 0, 0, 0, 227 },  /* parry spark */
    { 3968, 2944, 0, 0, 0, 0, 227 },    /* wall hit spark */
    { 0, 0, 0, 0, 0, 0, -1 },           /* dust puff (no flash) */
    { 3968, 2944, 1408, 0, 0, 0, 819 }, /* blade lock flash */
    { 0, 0, 0, 0, 0, 0, -1 },
};

int gPointLightFadeRate = 0; /* Q12 fade step per frame (0 = inactive) */
/* Start and end Q12 RGB flash colors (interpolated with LoadAverageShort12). */
static SVECTOR gPointLightStartColor;
static SVECTOR gPointLightEndColor;
static int gPointLightFadeProgress;

int TriggerPointLightFlash(int presetIndex, int x, int y, int z)
{
    short *preset;
    int fadeRate;
    int red, green, blue;

    preset = gPointLightFlashPresets[presetIndex];
    fadeRate = preset[6];
    if (fadeRate < 0)
        return fadeRate;

    gPointLightOrigin.x = x;
    gPointLightOrigin.y = y;
    gPointLightOrigin.z = z;

    if (IsRendererMonochrome() == 0) {
        gPointLightStartColor.vx = *preset++;
        gPointLightStartColor.vy = *preset++;
        gPointLightStartColor.vz = *preset++;
        gPointLightEndColor.vx = *preset++;
        gPointLightEndColor.vy = *preset++;
        gPointLightEndColor.vz = *preset++;
    } else {
        red = preset[0];
        green = preset[1];
        blue = preset[2];
        preset += 3;
        gPointLightStartColor.vx = gPointLightStartColor.vy = gPointLightStartColor.vz =
            ConvertRgbToLuminance(red, green, blue);

        red = preset[0];
        green = preset[1];
        blue = preset[2];
        preset += 3;
        gPointLightEndColor.vx = gPointLightEndColor.vy = gPointLightEndColor.vz =
            ConvertRgbToLuminance(red, green, blue);
    }

    fadeRate = *preset;
    gPointLightFadeProgress = 0;
    gPointLightFadeRate = fadeRate;
    return fadeRate;
}

int TriggerAltPointLightFlash(int presetIndex, int x, int y, int z)
{
    return TriggerPointLightFlash(presetIndex + POINT_LIGHT_ALT_PRESET_OFFSET, x, y, z);
}

/* When an offset is 0x7000 or more, UpdatePointLight skips its range test and lights the fighter however far it is. */
#define DELTA_IN_RANGE(x) (((x) < 0 ? -(x) : (x)) < POINT_LIGHT_GTE_DELTA_LIMIT)

static void UpdatePointLight(WorldVector *firstTarget, WorldVector *secondTarget)
{
    SVECTOR color;
    LightSlot *slot;
    WorldVector *position;
    int i;
    int dx, dy, dz;
    int yaw, cosine, sine;
    LightSlot *primarySlot = &gLightSlots[LIGHT_SLOT_P1];

    if (gPointLightFadeRate == 0) {
        primarySlot->directions[LIGHT_DIR_POINT].active = 0;
        gLightSlots[LIGHT_SLOT_P2].directions[LIGHT_DIR_POINT].active = 0;
        RebuildLightDirectionMatrix(primarySlot);
    } else {
        if (gPointLightFadeProgress >= POINT_LIGHT_FADE_MAX) {
            gPointLightFadeProgress = POINT_LIGHT_FADE_MAX;
            gPointLightFadeRate = 0;
        }
        LoadAverageShort12(&gPointLightStartColor, &gPointLightEndColor,
                           POINT_LIGHT_FADE_MAX - gPointLightFadeProgress,
                           gPointLightFadeProgress, &color);
        gPointLightFadeProgress += gPointLightFadeRate;
        for (i = 0; i < POINT_LIGHT_TARGET_COUNT; i++) {
            if (i == 0) {
                position = firstTarget;
                slot = primarySlot;
            } else {
                position = secondTarget;
                slot = &gLightSlots[LIGHT_SLOT_P2];
            }
            if (position != 0) {
                dx = position->x - gPointLightOrigin.x;
                dy = position->y - gPointLightOrigin.y;
                dz = position->z - gPointLightOrigin.z;
                if (DELTA_IN_RANGE(dx) && DELTA_IN_RANGE(dy) && DELTA_IN_RANGE(dz) &&
                    ComputeVectorLengthSq3D(dx, dy, dz) > POINT_LIGHT_MAX_RANGE_SQ) {
                    goto disable;
                }
                yaw = ratan2(dx, dz);
                cosine = rcos(yaw);
                sine = rsin(yaw);
                slot->directions[LIGHT_DIR_POINT].pitch =
                    -ratan2(dy, (cosine * dz + sine * dx) >> 12);
                slot->directions[LIGHT_DIR_POINT].yaw = yaw;
                slot->directions[LIGHT_DIR_POINT].active = 1;
                slot->colorMatrix.m[0][LIGHT_DIR_POINT] = color.vx;
                slot->colorMatrix.m[1][LIGHT_DIR_POINT] = color.vy;
                slot->colorMatrix.m[2][LIGHT_DIR_POINT] = color.vz;
                continue;
            }
        disable:
            slot->directions[LIGHT_DIR_POINT].active = 0;
        }
        RebuildLightDirectionMatrix(primarySlot);
    }
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P2]);
}

static int gPointLightTargetActive[POINT_LIGHT_TARGET_COUNT]; /* per fighter: registered this frame */

void ResetPointLightTargets(void)
{
    gPointLightTargetActive[1] = 0;
    gPointLightTargetActive[0] = 0;
}

static WorldVector *gPointLightTargets[POINT_LIGHT_TARGET_COUNT]; /* per fighter: position to light */

void RegisterPointLightTarget(int side, WorldVector *position)
{
    if (side < POINT_LIGHT_TARGET_COUNT) {
        gPointLightTargetActive[side] = 1;
        gPointLightTargets[side] = position;
    }
}

/* 8 bytes; nothing reads or writes them. */
static unsigned char gPointLightUnusedSbss[8];

void ApplyPointLightToTargets(void)
{
    switch (gPointLightTargetActive[0] * 2 + gPointLightTargetActive[1]) {
    case 1:
        UpdatePointLight(0, gPointLightTargets[1]);
        return;
    case 2:
        UpdatePointLight(gPointLightTargets[0], 0);
        return;
    case 3:
        UpdatePointLight(gPointLightTargets[0], gPointLightTargets[1]);
    default:
        return;
    }
}

void StopPointLightFlash(void)
{
    gPointLightFadeRate = 0;
    UpdatePointLight(0, 0);
}
