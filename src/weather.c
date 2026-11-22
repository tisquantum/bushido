/* Weather: the camera-relative snow and rain particles the stage callbacks start and draw (snow
 * on the winter stages, rain and puddle splashes on stage 19), blown by the stage wind. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <rand.h>
#include <inline_o.h>

#include "weather.h"
#include "cloth.h"
#include "gpu_code.h"
#include "display_buffer.h"
#include "main.h"
#include "rot_matrix.h"
#include "otz.h"
#include "stage_id.h"
#include "globals.h"

typedef struct {
    unsigned short tpage;
    unsigned short clut;
    unsigned char u;
    unsigned char v;
    int (*splashEnabled)(VECTOR *groundPos);
    int (*floorHeightAt)(int x, int z);
} WeatherSettings;

/* 0x15F0-byte weather particle state shared by snow (192 flakes, 48x2 POLY_FT4 pool)
 * and rain (128 streaks + 3-frame puddle splashes). */
typedef struct {
    WeatherParticle particles[WEATHER_PARTICLE_COUNT];
    unsigned char splashState[WEATHER_PARTICLE_COUNT];
    POLY_FT4 polys[2][SNOW_MAX_QUADS];
    int bufferIndex;
    union {
        VECTOR pos; /* copied whole each frame; its fourth word then says it holds a position */
        struct {
            long vx, vy, vz;
            long valid;
        } tracked;
    } lastCamera;
    unsigned short phase;
    short restartTries; /* rain: waiting drops tried; set to the drawn count after a frame that drew fewer than 64 */
    unsigned int prngSeed[2];
    WeatherSettings settings;
} WeatherWork;

/* Screen-space quad extents indexed by depth bucket 0 (far) .. 31 (near). */
typedef struct {
    signed char minX, minY, maxX, maxY;
} SnowQuadExtent;

SnowQuadExtent gSnowQuadExtents[32] = {
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 1, 1 },
    { -1, -1, 2, 1 },
    { -1, -1, 2, 1 },
    { -1, -1, 2, 1 },
    { -2, -1, 2, 1 },
    { -2, -1, 2, 1 },
    { -2, -1, 2, 1 },
    { -2, -1, 3, 2 },
    { -2, -1, 3, 2 },
    { -3, -1, 3, 2 },
    { -3, -1, 3, 2 },
    { -3, -2, 4, 2 },
    { -3, -2, 4, 2 },
    { -4, -2, 4, 2 },
    { -4, -2, 5, 3 },
    { -6, -3, 6, 3 },
    { -7, -3, 7, 4 },
    { -8, -4, 8, 4 },
};
short gStageSnowGroundY[STAGE_COUNT] = {
    0, 2000, 2000, 0, 8000, 0, 6000, 2000, 0, 0, 2000, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
Wind *gWindState = &gClothWindSlots[CLOTH_WIND_SLOT_STAGE];
WeatherWork *gParticleWork;

void InitSnowSystem(SnowInit *init)
{
    WeatherWork *work;
    WeatherParticle *particle;
    POLY_FT4 *poly;
    int i;
    int buf;
    int seedHigh;
    int seedLow;

    gParticleWork = init->storage;
    work = gParticleWork;
    work->settings.tpage = init->tpage;
    work->settings.clut = init->clut;
    work->settings.u = init->u;
    work->settings.v = init->v;

    particle = work->particles;
    for (i = 0; i < WEATHER_PARTICLE_COUNT; i++, particle++) {
        particle->x = (rand() & 0x7fff) - WEATHER_WRAP_HALF;
        particle->y = -((i & 3) * 6000) / 4 + -(i * 1500) / WEATHER_PARTICLE_COUNT;
        particle->z = (rand() & 0x7fff) - WEATHER_WRAP_HALF;
        particle->offscreen = 1;
    }

    work->lastCamera.tracked.valid = 0;
    work->phase = 0;
    work->restartTries = 0;
    for (i = 0; i < 2; i++) {
        seedHigh = rand();
        seedLow = rand();
        work->prngSeed[i] = (seedHigh << 16) | seedLow;
    }

    work->bufferIndex = 0;
    for (buf = 0; buf < 2; buf++) {
        for (i = 0; i < SNOW_MAX_QUADS; i++) {
            poly = &work->polys[buf][i];
            SetPolyFT4(poly);
            poly->code = GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS | GPU_CODE_RAW_TEXTURE;
            poly->clut = work->settings.clut;
            poly->tpage = work->settings.tpage;
            setUVWH(poly, work->settings.u, work->settings.v, 31, 15);
        }
    }
}

int UpdateAndDrawSnow(WeatherScratch *scratch)
{
    unsigned char unused[384]; /* not used */
    WeatherWork *work;
    WeatherParticle *particle;
    POLY_FT4 *poly;
    u_int rngLfsr, rngLcg;
    int windX, windZ;
    int groundY;
    int drawnCount;
    int i, wrapIndex;
    int screenX, screenY;
    int coord;
    int sizeIndex;
    long screenZ;
    int otShift;

    work = gParticleWork;
    drawnCount = 0;
    windX = rcos(gWindState->angle) * gWindState->strength >> 18;
    windZ = rsin(gWindState->angle) * gWindState->strength >> 18;
    groundY = gStageSnowGroundY[gCurrentStageId];
    SetRotMatrix(&scratch->viewMatrix);
    SetTransMatrix(&gZeroTransMatrix);
    if (work->lastCamera.tracked.valid != 0) {
        scratch->cameraDelta.vx = scratch->cameraPos.vx - work->lastCamera.pos.vx;
        scratch->cameraDelta.vy = scratch->cameraPos.vy - work->lastCamera.pos.vy;
        scratch->cameraDelta.vz = scratch->cameraPos.vz - work->lastCamera.pos.vz;
    } else {
        scratch->cameraDelta.vz = 0;
        scratch->cameraDelta.vy = 0;
        scratch->cameraDelta.vx = 0;
    }
    work->lastCamera.pos = scratch->cameraPos;
    particle = work->particles;
    rngLfsr = work->prngSeed[0];
    rngLcg = work->prngSeed[1];
    work->lastCamera.tracked.valid = 1;
    work->phase = (work->phase + 1) & 7;
    poly = work->polys[work->bufferIndex++ & 1];
    scratch->density *= 4;
    if (scratch->density > WEATHER_PARTICLE_COUNT) {
        scratch->density = WEATHER_PARTICLE_COUNT;
    }

    for (i = 0; i < scratch->density; i++, particle++) {
        scratch->particle = *particle;
        if (scratch->particle.y + scratch->cameraPos.vy >= groundY) {
            scratch->particle.x = (rngLfsr & 0x7fff) - WEATHER_WRAP_HALF;
            scratch->particle.y = -6000;
            scratch->particle.z = ((rngLcg >> 16) & 0x7fff) - WEATHER_WRAP_HALF;
        } else if (scratch->particle.offscreen == 0) {
            rngLcg += rngLcg << 2;
            rngLcg += i;
            rngLfsr = (rngLfsr >> 16) ^ (rngLfsr << 16);
            rngLfsr = (((rngLfsr >> 31) & 1) | (rngLfsr << 1)) ^ ((rngLfsr >> 29) & 1);
            scratch->particle.x += ((rngLfsr >> 12) & 0xf) - 8 - scratch->cameraDelta.vx + windX;
            scratch->particle.y += ((rngLcg >> (rngLfsr & 3)) & 7) + 16 - scratch->cameraDelta.vy;
            scratch->particle.z += ((rngLfsr >> 20) & 0xf) - 8 - scratch->cameraDelta.vz + windZ;
        } else {
            rngLcg += rngLcg << 2;
            rngLcg += i;
            scratch->particle.x += windX - scratch->cameraDelta.vx;
            scratch->particle.y += 1000 - scratch->cameraDelta.vy;
            scratch->particle.z += windZ - scratch->cameraDelta.vz;
        }

        if (drawnCount < SNOW_MAX_QUADS) {
            scratch->particle.offscreen = 1;
            gte_ldv0(&scratch->particle);
            gte_rtps();
            gte_stsxy(&scratch->screenPos[0]);
            screenX = scratch->screenPos[0].vx;
            screenY = scratch->screenPos[0].vy;
            if ((u_int)(screenX + 8) < DISPLAY_WIDTH + 16 && (u_int)(screenY + 4) < DISPLAY_HEIGHT + 8) {
                gte_stszotz(&scratch->depth);
                sizeIndex = 31 - (scratch->depth >> 8);
                if (sizeIndex < 0) {
                    sizeIndex = 0;
                }
                poly->x0 = gSnowQuadExtents[sizeIndex].minX + screenX;
                poly->y0 = gSnowQuadExtents[sizeIndex].minY + screenY;
                poly->x1 = gSnowQuadExtents[sizeIndex].maxX + screenX;
                poly->y1 = gSnowQuadExtents[sizeIndex].minY + screenY;
                poly->x2 = gSnowQuadExtents[sizeIndex].minX + screenX;
                poly->y2 = gSnowQuadExtents[sizeIndex].maxY + screenY;
                poly->x3 = gSnowQuadExtents[sizeIndex].maxX + screenX;
                poly->y3 = gSnowQuadExtents[sizeIndex].maxY + screenY;
                otShift = scratch->depth >> 12;
                scratch->depth = (u_int)((scratch->depth >> 1) & 0x7ff | 0xfffff000) >> otShift & 0xfff;
                if (scratch->depth < OTZ_BUCKET_COUNT) {
                    AddPrim(gCurrentOt + scratch->depth, poly++);
                    drawnCount++;
                }
                scratch->particle.offscreen = 0;
            } else {
                scratch->verts[0] = scratch->particle;
                coord = scratch->verts[0].x;
                scratch->verts[0].x = coord > 0 ? coord - WEATHER_WRAP_HALF : coord + WEATHER_WRAP_HALF;
                scratch->verts[2] = scratch->verts[0];
                scratch->verts[1] = scratch->particle;
                coord = scratch->verts[1].z;
                coord = coord > 0 ? coord - WEATHER_WRAP_HALF : coord + WEATHER_WRAP_HALF;
                scratch->verts[1].z = coord;
                scratch->verts[2].z = coord;
                gte_ldv3(&scratch->verts[0], &scratch->verts[1], &scratch->verts[2]);
                gte_rtpt();
                gte_stsxy3(&scratch->screenPos[0], &scratch->screenPos[1], &scratch->screenPos[2]);
                for (wrapIndex = 0; wrapIndex < 3; wrapIndex++) {
                    screenX = scratch->screenPos[wrapIndex].vx;
                    screenY = scratch->screenPos[wrapIndex].vy;
                    if ((u_int)(screenX + 8) < DISPLAY_WIDTH + 16 && (u_int)(screenY + 4) < DISPLAY_HEIGHT + 8) {
                        gte_stsz3(&scratch->screenZ[0], &scratch->screenZ[1], &scratch->screenZ[2]);
                        screenZ = scratch->screenZ[wrapIndex];
                        sizeIndex = 31 - (screenZ >> 10);
                        scratch->depth = screenZ >> 2;
                        if (sizeIndex < 0) {
                            sizeIndex = 0;
                        }
                        poly->x0 = gSnowQuadExtents[sizeIndex].minX + screenX;
                        poly->y0 = gSnowQuadExtents[sizeIndex].minY + screenY;
                        poly->x1 = gSnowQuadExtents[sizeIndex].maxX + screenX;
                        poly->y1 = gSnowQuadExtents[sizeIndex].minY + screenY;
                        poly->x2 = gSnowQuadExtents[sizeIndex].minX + screenX;
                        poly->y2 = gSnowQuadExtents[sizeIndex].maxY + screenY;
                        poly->x3 = gSnowQuadExtents[sizeIndex].maxX + screenX;
                        poly->y3 = gSnowQuadExtents[sizeIndex].maxY + screenY;
                        otShift = scratch->depth >> 12;
                        scratch->depth = (u_int)((scratch->depth >> 1) & 0x7ff | 0xfffff000) >> otShift & 0xfff;
                        if (scratch->depth < OTZ_BUCKET_COUNT) {
                            AddPrim(gCurrentOt + scratch->depth, poly++);
                            drawnCount++;
                        }
                        scratch->particle.offscreen = 0;
                        break;
                    }
                }
            }
        }
        *particle = scratch->particle;
    }
    work->prngSeed[0] = rngLfsr ^ rand();
    work->prngSeed[1] = rngLcg ^ rand();
    return 0;
}

void InitRainParticles(RainInit *init)
{
    WeatherWork *work;
    WeatherParticle *particle;
    int i;
    int seedHigh, seedLow;

    gParticleWork = init->workBuffer;
    work = gParticleWork;
    work->settings.tpage = init->splashTpage;
    work->settings.clut = init->splashClut;
    work->settings.u = init->splashU;
    work->settings.v = init->splashV;
    work->settings.splashEnabled = init->splashEnabled;
    work->settings.floorHeightAt = init->floorHeightAt;

    particle = work->particles;
    for (i = 0; i < RAIN_PARTICLE_COUNT; i++, ++particle) {
        particle->x = (rand() & 0x7fff) - WEATHER_WRAP_HALF;
        particle->y = -(i * 4096) / RAIN_PARTICLE_COUNT;
        particle->z = (rand() & 0x7fff) - WEATHER_WRAP_HALF;
        particle->offscreen = -(i & 31);
        work->splashState[i] = RAIN_STATE_FALLING;
    }

    work->lastCamera.tracked.valid = 0;
    work->phase = 0;
    work->restartTries = 0;
    for (i = 0; i < 2; ++i) {
        seedHigh = rand();
        seedLow = rand();
        work->prngSeed[i] = ((unsigned int)seedHigh << 16) | seedLow;
    }
}

int UpdateAndDrawRain(WeatherScratch *scratch)
{
    int i;
    int phase;
    u_long lineRgb0, lineRgb1;
    u_long splashColorCode;
    WeatherWork *work;
    WeatherParticle *particle;
    LINE_G2 *line;
    POLY_FT4 *splashPoly;
    int drawnCount;
    u_char splashState;
    u_int swapped;
    int angleOrDrop, spawnRadius;
    int floorY, worldY;
    int windDx, windDz;
    int splashUv;
    int otShift;
    int screenX, screenY;

    work = gParticleWork;
    drawnCount = 0;
    scratch->windX = rcos(gWindState->angle) * gWindState->strength >> 15;
    splashColorCode = GPU_CODE_WORD(GPU_CODE_POLY_FT4) | GPU_RGB_NEUTRAL;
    scratch->windZ = rsin(gWindState->angle) * gWindState->strength >> 15;
    SetRotMatrix(&scratch->viewMatrix);
    SetTransMatrix(&gZeroTransMatrix);
    lineRgb0 = scratch->lineColor[0].rgb & GPU_RGB_MASK | GPU_CODE_WORD(GPU_CODE_LINE_G2);
    line = scratch->packetCursor;
    lineRgb1 = scratch->lineColor[1].rgb & GPU_RGB_MASK;
    if (work->lastCamera.tracked.valid != 0) {
        scratch->cameraDelta.vx = scratch->cameraPos.vx - work->lastCamera.pos.vx;
        scratch->cameraDelta.vy = scratch->cameraPos.vy - work->lastCamera.pos.vy;
        scratch->cameraDelta.vz = scratch->cameraPos.vz - work->lastCamera.pos.vz;
    } else {
        scratch->cameraDelta.vz = 0;
        scratch->cameraDelta.vy = 0;
        scratch->cameraDelta.vx = 0;
    }
    work->lastCamera.pos = scratch->cameraPos;
    work->lastCamera.tracked.valid = 1;
    work->phase = (work->phase + 1) & 7;
    phase = work->phase;
    scratch->prngSeed[0] = work->prngSeed[0];
    scratch->prngSeed[1] = work->prngSeed[1];

    particle = work->particles;
    for (i = 0; i < RAIN_PARTICLE_COUNT; i++, particle++) {
        if (work->splashState[i] != RAIN_STATE_FALLING) {
            if (work->splashState[i] == RAIN_STATE_INACTIVE) {
                if (work->restartTries < scratch->density * 2) {
                    work->restartTries++;
                    scratch->prngSeed[1] = scratch->prngSeed[1] + (scratch->prngSeed[1] << 2) + 1;
                    if ((scratch->prngSeed[1] & 0xc0008000) == 0) {
                        swapped = (scratch->prngSeed[0] >> 16) ^ (scratch->prngSeed[0] << 16);
                        scratch->prngSeed[0] = ((swapped << 1) | (swapped >> 31)) ^ ((swapped >> 29) & 1);
                        scratch->particle.y = -4096 - (scratch->prngSeed[1] & 0x3ff);
                        angleOrDrop = scratch->cameraHeading - 0x200 + (scratch->prngSeed[0] & 0x3ff);
                        spawnRadius = (scratch->prngSeed[1] >> 16) & 0x3fff;
                        scratch->particle.x = rsin(angleOrDrop) * spawnRadius / 4096 - (scratch->windX << 2);
                        scratch->particle.z = -rcos(angleOrDrop) * spawnRadius / 4096 - (scratch->windZ << 2);
                        work->splashState[i] = RAIN_STATE_FALLING;
                    }
                }
            } else {
                work->splashState[i]++;
            }
        } else {
            scratch->particle.x = particle->x;
            scratch->particle.y = particle->y;
            scratch->particle.z = particle->z;
            if (particle->offscreen < 0) {
                scratch->particle.x += scratch->windX - scratch->cameraDelta.vx;
                scratch->particle.y += 2880 - scratch->cameraDelta.vy;
                scratch->particle.z += scratch->windZ - scratch->cameraDelta.vz;
                if (scratch->particle.y + scratch->cameraPos.vy >= 0) {
                    work->splashState[i] = RAIN_STATE_INACTIVE;
                }
            } else {
                scratch->particle.x += scratch->windX - scratch->cameraDelta.vx;
                scratch->particle.y += 720 - scratch->cameraDelta.vy;
                scratch->particle.z += scratch->windZ - scratch->cameraDelta.vz;
                floorY = work->settings.floorHeightAt(scratch->particle.x + scratch->cameraPos.vx,
                                                      scratch->particle.z + scratch->cameraPos.vz);
                if (floorY == WEATHER_NO_FLOOR) {
                    if (scratch->particle.y + scratch->cameraPos.vy >= 0) {
                        work->splashState[i] = RAIN_STATE_INACTIVE;
                    }
                } else {
                    worldY = scratch->particle.y + scratch->cameraPos.vy;
                    if (worldY >= floorY) {
                        angleOrDrop = worldY - floorY;
                        windDx = scratch->windX * angleOrDrop;
                        windDz = scratch->windZ * angleOrDrop;
                        scratch->particle.y = floorY - scratch->cameraPos.vy;
                        scratch->groundPos.vy = floorY;
                        scratch->particle.x -= windDx / 720;
                        scratch->particle.z -= windDz / 720;
                        scratch->groundPos.vx = scratch->cameraPos.vx + scratch->particle.x;
                        scratch->groundPos.vz = scratch->cameraPos.vz + scratch->particle.z;
                        if (work->settings.splashEnabled(&scratch->groundPos) != 0) {
                            work->splashState[i] = RAIN_STATE_SPLASH_START;
                        } else {
                            work->splashState[i] = RAIN_STATE_INACTIVE;
                        }
                    }
                }
            }
        }

        if ((i & 7) == phase) {
            if (scratch->particle.x < -WEATHER_WRAP_HALF) {
                particle->x = scratch->particle.x = scratch->particle.x + WEATHER_WRAP_SIZE;
            }
            if (scratch->particle.x > WEATHER_WRAP_HALF) {
                particle->x = scratch->particle.x = scratch->particle.x - WEATHER_WRAP_SIZE;
            }
            if (scratch->particle.z < -WEATHER_WRAP_HALF) {
                particle->z = scratch->particle.z = scratch->particle.z + WEATHER_WRAP_SIZE;
            }
            if (scratch->particle.z > WEATHER_WRAP_HALF) {
                particle->z = scratch->particle.z = scratch->particle.z - WEATHER_WRAP_SIZE;
            }
        }

        splashState = work->splashState[i];
        if (splashState != RAIN_STATE_FALLING) {
            if (splashState < RAIN_STATE_INACTIVE) {
                splashPoly = (POLY_FT4 *)line;

                scratch->particle.x = particle->x;
                scratch->particle.y = particle->y;
                scratch->particle.z = particle->z;
                scratch->verts[0].x = scratch->particle.x - 100;
                scratch->verts[0].y = scratch->particle.y;
                scratch->verts[0].z = scratch->particle.z - 100;
                gte_ldv0(&scratch->verts[0]);
                gte_rtps();
                SetPolyFT4(splashPoly);
                *(u_long *)&splashPoly->r0 = splashColorCode;
                gte_stsxy(&scratch->screenPos[0]);
                screenX = scratch->screenPos[0].vx;
                screenY = scratch->screenPos[0].vy;
                if ((u_int)(screenX + 32) < DISPLAY_WIDTH + 64 && screenY < DISPLAY_HEIGHT + 32) {
                    gte_stszotz(&scratch->depth);
                    *(u_long *)&splashPoly->x0 = *(u_long *)&scratch->screenPos[0];
                    scratch->verts[1].x = scratch->particle.x + 100;
                    scratch->verts[1].y = scratch->particle.y;
                    scratch->verts[1].z = scratch->particle.z - 100;
                    scratch->verts[2].x = scratch->particle.x - 100;
                    scratch->verts[2].y = scratch->particle.y;
                    scratch->verts[2].z = scratch->particle.z + 100;
                    scratch->verts[3].x = scratch->particle.x + 100;
                    scratch->verts[3].y = scratch->particle.y;
                    scratch->verts[3].z = scratch->particle.z + 100;
                    gte_ldv3(&scratch->verts[1], &scratch->verts[2], &scratch->verts[3]);
                    gte_rtpt();
                    splashPoly->tpage = work->settings.tpage;
                    splashPoly->clut = work->settings.clut;
                    splashPoly->u0 = work->settings.u;
                    splashUv = splashPoly->u0 | (work->settings.v + ((work->splashState[i] - 1) << 4)) << 8;
                    gte_stsxy3(&splashPoly->x1, &splashPoly->x2, &splashPoly->x3);
                    *(u_short *)&splashPoly->u0 = splashUv;
                    *(u_short *)&splashPoly->u1 = splashUv + 0xf;
                    *(u_short *)&splashPoly->u2 = splashUv + 0xf00;
                    *(u_short *)&splashPoly->u3 = splashUv + 0xf0f;
                    otShift = scratch->depth >> 12;
                    scratch->depth = (u_int)((scratch->depth >> 1) & 0x7ff | 0xfffff000) >> otShift & 0xfff;
                    if (scratch->depth < OTZ_BUCKET_COUNT) {
                        AddPrim(gCurrentOt + scratch->depth, splashPoly++);
                        drawnCount++;
                    }
                    line = (LINE_G2 *)splashPoly;
                }
            }
        } else {
            particle->x = scratch->particle.x;
            particle->y = scratch->particle.y;
            particle->z = scratch->particle.z;
            gte_ldv0(&scratch->particle);
            gte_rtps();
            SetLineG2(line);
            *(u_long *)&line->r0 = lineRgb0;
            *(u_long *)&line->r1 = lineRgb1;
            gte_stsxy(&scratch->screenPos[0]);
            screenX = scratch->screenPos[0].vx;
            screenY = scratch->screenPos[0].vy;
            if ((u_int)(screenX + 64) < DISPLAY_WIDTH + 128 && screenY < DISPLAY_HEIGHT + 16) {
                gte_stszotz(&scratch->depth);
                scratch->verts[0].y = scratch->particle.y + 720;
                floorY = work->settings.floorHeightAt(scratch->particle.x + scratch->cameraPos.vx,
                                                      scratch->particle.z + scratch->cameraPos.vz);
                if (floorY != WEATHER_NO_FLOOR && (worldY = scratch->verts[0].y + scratch->cameraPos.vy) >= floorY) {
                    angleOrDrop = 720 - (worldY - floorY);
                    scratch->verts[0].y = floorY - scratch->cameraPos.vy;
                    scratch->verts[0].x = scratch->particle.x + scratch->windX * angleOrDrop / 720;
                    scratch->verts[0].z = scratch->particle.z + scratch->windZ * angleOrDrop / 720;
                } else {
                    scratch->verts[0].x = scratch->particle.x + scratch->windX;
                    scratch->verts[0].z = scratch->particle.z + scratch->windZ;
                }
                gte_ldv0(&scratch->verts[0]);
                gte_rtps();
                *(u_long *)&line->x0 = *(u_long *)&scratch->screenPos[0];
                particle->offscreen = 0;
                gte_stsxy(&line->x1);
                otShift = scratch->depth >> 12;
                scratch->depth = (u_int)((scratch->depth >> 1) & 0x7ff | 0xfffff000) >> otShift & 0xfff;
                if (scratch->depth < OTZ_BUCKET_COUNT) {
                    AddPrim(gCurrentOt + scratch->depth, line++);
                    drawnCount++;
                }
            }
        }
    }
    work->prngSeed[0] = scratch->prngSeed[0];
    work->prngSeed[1] = scratch->prngSeed[1];
    scratch->packetCursor = line;
    if (drawnCount < 64) {
        work->restartTries = drawnCount;
    }
    return 0;
}
