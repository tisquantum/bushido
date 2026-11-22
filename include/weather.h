#ifndef WEATHER_H
#define WEATHER_H

#include <sys/types.h>
#include <libgte.h>

#define WEATHER_PARTICLE_COUNT 192
#define SNOW_MAX_QUADS 48
#define RAIN_PARTICLE_COUNT 128
#define WEATHER_WRAP_HALF 0x4000
#define WEATHER_WRAP_SIZE 0x8000
#define WEATHER_WORK_BYTES 0x15F0
#define WEATHER_NO_FLOOR (-0x8000)

typedef enum RainSplashState {
    RAIN_STATE_FALLING = 0,
    RAIN_STATE_SPLASH_START = 1,
    RAIN_STATE_INACTIVE = 4
} RainSplashState;

/* Camera-relative weather particle (8 bytes, SVECTOR-compatible for gte_ldv0/gte_ldv3).
 * Snow (192 slots): x/z wrap in [-0x4000, 0x3fff]; y respawns at -6000 on hitting stage ground Y;
 *   offscreen: 0 = projected on-screen last frame (gentle flutter, +16..+23 Y),
 *              1 = clipped/initialized (fast drop, +1000 Y).
 * Rain (128 slots): offscreen starts negative (-(i & 31)) for staggered initial fast drop (+2880 Y),
 *   cleared to 0 once drawn on-screen (normal fall +720 Y with floor-height splash test). */
typedef struct WeatherParticle {
    short x, y, z, offscreen;
} WeatherParticle;

/* What InitSnowSystem is given: the flake texture page, CLUT and UV origin, and the caller-owned
 * 0x15F0-byte weather work buffer. */
typedef struct SnowInit {
    u_short tpage;
    u_short clut;
    u_char u;
    u_char v;
    void *storage;
} SnowInit;

/* The working record of UpdateAndDrawSnow and UpdateAndDrawRain in the scratchpad (render_scratch.h). */
typedef struct WeatherScratch {
    MATRIX viewMatrix;         /* +0x00: aspect-corrected camera view matrix (gAspectCorrectedCameraMatrix) */
    VECTOR cameraPos;          /* +0x20: camera world position copied from viewMatrix.t[0..2] */
    void *packetCursor;        /* +0x30: in/out gPacketCursor; advanced by rain */
    short cameraHeading;       /* +0x34: (0x800 - GetCameraHeadingAngle()) & 0xfff (rain spawn cone) */
    short density;             /* +0x36: stage density (snow: x4 clamped to 192;
                                  rain: x2 caps WeatherWork.restartTries) */
    union {
        CVECTOR color;         /* as the stage callback sets it */
        u_long rgb;            /* as the renderer reads it, one word */
    } lineColor[2];            /* +0x38: rain top/bottom streak RGB */
    VECTOR cameraDelta;        /* +0x40: per-frame camera displacement (cameraPos - the last frame's) */
    VECTOR groundPos;          /* +0x50: rain splash ground hit point */
    WeatherParticle verts[4];  /* +0x60: snow [0..2] torus-wrapped copies for gte_ldv3;
                                  rain [0..3] splash corners or [0] streak end */
    DVECTOR screenPos[3];      /* +0x80: GTE projected screen XY (gte_stsxy / gte_stsxy3) */
    long screenZ[3];           /* +0x8C: GTE projected screen Z (gte_stsz3, 4x OTZ scale) for snow verts[0..2] */
    WeatherParticle particle;  /* +0x98: working copy of current particle for update and gte_ldv0 */
    u_int prngSeed[2];         /* +0xA0: rain 64-bit PRNG state ([0] LFSR, [1] LCG) */
    int windX;                 /* +0xA8: rain per-frame wind X */
    int windZ;                 /* +0xAC: rain per-frame wind Z */
    long depth;                /* +0xB0: GTE OTZ depth (gte_stszotz / screenZ >> 2), compressed to OT bucket 0..4095 */
} WeatherScratch;

/*
 * What InitRainParticles is given. Rain streaks are untextured LINE_G2 primitives; the texture
 * fields describe the 16x16 3-frame POLY_FT4 puddle-splash strip (frames at splashV + 0/16/32).
 */
typedef struct RainInit {
    u_short splashTpage;
    u_short splashClut;
    u_char splashU;
    u_char splashV;
    int (*splashEnabled)(VECTOR *groundPos); /* non-zero spawns splash at ground impact */
    int (*floorHeightAt)(int x, int z);      /* returns floor Y, or WEATHER_NO_FLOOR */
    void *workBuffer;                        /* caller-supplied WeatherWork buffer (0x15F0 bytes) */
} RainInit;

void InitSnowSystem(SnowInit *init);
int UpdateAndDrawSnow(WeatherScratch *scratch);
void InitRainParticles(RainInit *init);
int UpdateAndDrawRain(WeatherScratch *scratch);

#endif
