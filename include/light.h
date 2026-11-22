#ifndef LIGHT_H
#define LIGHT_H

#include <sys/types.h>
#include <libgte.h>

#define LIGHT_DIRECTION_COUNT 3
#define LIGHT_SLOT_COUNT      8

/* Slots of gLightSlots with a fixed user; the opening demo rebuilds slot 2 each frame. */
typedef enum LightSlotIndex {
    LIGHT_SLOT_P1 = 0,      /* Player 1 fighter (and point-light target 0) */
    LIGHT_SLOT_P2 = 1,      /* Player 2 fighter (and point-light target 1) */
    LIGHT_SLOT_STAGE = 4,   /* Stage geometry and stage props */
    LIGHT_SLOT_SPECIAL = 6  /* Dim/override lighting slot (the moat, stage 19, via InitSpecialLightSlot) */
} LightSlotIndex;

typedef enum LightDirectionIndex {
    LIGHT_DIR_PRIMARY = 0,  /* Primary / stage directional light */
    LIGHT_DIR_POINT = 1,    /* Transient dynamic point-light flash */
    LIGHT_DIR_AUX = 2       /* Auxiliary directional light */
} LightDirectionIndex;

/* 8-byte directional light source specification converted into one row of LightSlot.lightMatrix
 * by RebuildLightDirectionMatrix (src/light_init.c). */
typedef struct LightDirection {
    short pitch;  /* +0x00: 4096-unit elevation angle (0x1000 = 360 deg, passed to rcos/rsin) */
    short yaw;    /* +0x02: 4096-unit azimuth angle (0x1000 = 360 deg, passed to rsin/rcos) */
    short active; /* +0x04: nonzero enables this light; 0 zeroes its row in lightMatrix */
    short unused; /* +0x06: nothing reads it */
} LightDirection;

/* 96-byte (0x60) GTE 3-light lighting bank entry in gLightSlots[LIGHT_SLOT_COUNT]. */
typedef struct LightSlot {
    LightDirection directions[LIGHT_DIRECTION_COUNT]; /* +0x00: [0] primary/stage, [1] dynamic point light,
                                                         [2] auxiliary */
    MATRIX lightMatrix;                               /* +0x18: GTE light direction matrix (LLM, SetLightMatrix) */
    MATRIX colorMatrix;                               /* +0x38: GTE light RGB color matrix
                                                         (LCM, SetColorMatrix; col i = light i Q12 RGB) */
    u_char ambientRgb[3];                             /* +0x58: GTE ambient/back RGB (0..255, SetBackColor / RBK..BBK);
                                                         +0x5B implicit pad */
    short scale;                                      /* +0x5C: Q12 directional light length scale (0x1000 default);
                                                         +0x5E implicit pad */
} LightSlot;

/* A light slot as a preset holds it, read in this order by InitLightSlots and LoadLightSlotFromPreset. */
typedef struct LightDirectionPreset {
    short pitch;
    short yaw;
    short active;
} LightDirectionPreset;

typedef struct LightSlotPreset {
    short lightColors[3][3]; /* light i's RGB in row i: LightSlot.colorMatrix transposed */
    LightDirectionPreset directions[LIGHT_DIRECTION_COUNT];
    unsigned short ambientRgb[3];
    short scale;
} LightSlotPreset;

extern LightSlot *gActiveLightSlot;

extern LightSlot gLightSlots[LIGHT_SLOT_COUNT];

#endif
