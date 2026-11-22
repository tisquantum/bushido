/* Stage 12's ocean: a 9x19 spring mesh of wave fronts rolling onto the shore, drawn as one
 * scene node by the subdividing renderer, and the shoreline queries fighters and decals use. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "light.h"
#include "scene_node.h"
#include "render_scratch.h"
#include "ocean.h"
#include "gpu_code.h"
#include "model_arena.h"
#include "scene_math.h"
#include "render.h"

#define OCEAN_BAND_COUNT    9
#define OCEAN_QUAD_BANDS    8
#define OCEAN_COL_COUNT     19
#define OCEAN_QUAD_COLS     18
#define OCEAN_ROW_STRIDE    21 /* 7 RotTransPers3 triplets per ping-pong row */
#define OCEAN_ROW_TRIPLETS  (OCEAN_ROW_STRIDE / 3)
#define OCEAN_CELL_STEP     3200
#define OCEAN_SURF_BAND     5  /* and the bands after it */

typedef struct SceneNode SceneNode;

/* Built in the 3,712 bytes below GPU packet buffer 0. */
static short *gOceanWaveQuadDescs;

static SceneNode gOceanWaveNode;
/* Only rows 0 and 1 are used. */
static SVECTOR gOceanWaveRowVerts[OCEAN_QUAD_BANDS][OCEAN_ROW_STRIDE];
static int gOceanWavePhases[OCEAN_COL_COUNT];
static int gOceanWaveGridX[OCEAN_BAND_COUNT][OCEAN_COL_COUNT];
static int gOceanWaveEdgeVel[OCEAN_COL_COUNT];

int InitOceanWaveMesh(void)
{
    short *desc;
    u_short shoreTpage;
    u_short seaTpage;
    u_short shoreClut;
    u_short seaClut;
    int band, col;

    gOceanWaveQuadDescs = (short *)((char *)GetGpuPacketBuffer(0) - 3712);
    desc = gOceanWaveQuadDescs;
    shoreTpage = GetTPage(0, 0, 0x2c0, 0x180);
    seaTpage = GetTPage(0, 0, 0x2c0, 0x1c0);
    shoreClut = GetClut(0x120, 0x1e0);
    seaClut = GetClut(0x120, 0x1e0);

    for (band = 0; band < OCEAN_QUAD_BANDS; band++) {
        for (col = 0; col < OCEAN_QUAD_COLS; col++) {
            if (band >= OCEAN_SURF_BAND) {
                *desc++ = shoreTpage;
                *desc++ = GPU_CODE_POLY_FT4 << 8;
                *desc++ = shoreClut;
                *desc++ = 0xbf3f;
                *desc++ = 0xbf00;
                *desc++ = 0x803f;
                *desc++ = 0x8000;
            } else {
                *desc++ = seaTpage;
                *desc++ = GPU_CODE_POLY_FT4 << 8;
                *desc++ = seaClut;
                *desc++ = 0xff3f;
                *desc++ = 0xff00;
                *desc++ = 0xc03f;
                *desc++ = 0xc000;
            }
            if (band & 1) {
                *desc++ = (col + OCEAN_ROW_STRIDE + 1) | ((col + OCEAN_ROW_STRIDE) << 8);
                *desc++ = (col + 1) | (col << 8);
            } else {
                *desc++ = (col + 1) | (col << 8);
                *desc++ = (col + OCEAN_ROW_STRIDE + 1) | ((col + OCEAN_ROW_STRIDE) << 8);
            }
            *desc++ = 0;
            *desc++ = 0;
            *desc++ = -ONE;
        }
    }

    gOceanWaveNode.renderType = SCENE_NODE_OCEAN;
    gOceanWaveNode.lightSlot = LIGHT_SLOT_STAGE;
    gOceanWaveNode.localMatrix.t[0] = -19200;
    gOceanWaveNode.localMatrix.t[1] = 0;
    gOceanWaveNode.localMatrix.t[2] = -6400;
    gOceanWaveNode.rotation.vx = 0;
    gOceanWaveNode.rotation.vy = 0;
    gOceanWaveNode.rotation.vz = 0;
    gOceanWaveNode.rotOrder = SCENE_ROT_ORDER_FAST;
    gOceanWaveNode.parent = 0;
    gOceanWaveNode.matrixState = SCENE_MATRIX_DIRTY;
    UpdateNodeWorldMatrix(&gOceanWaveNode);

    for (band = 0; band < OCEAN_BAND_COUNT; band++) {
        for (col = 0; col < OCEAN_COL_COUNT; col++) {
            gOceanWaveGridX[band][col] = band * OCEAN_CELL_STEP;
        }
    }

    for (col = 0; col < OCEAN_COL_COUNT; col++) {
        gOceanWavePhases[col] = ((col * 2048) / OCEAN_QUAD_COLS) & 0xfff;
    }

    return (int)desc - (int)gOceanWaveQuadDescs;
}

void UpdateOceanWaveMesh(void)
{
    int i, col;
    int sinVal, gap, step;

    for (i = 0; i < OCEAN_COL_COUNT; ++i) {
        sinVal = gSinLutQ12[gOceanWavePhases[i] & 0xfff];
        gOceanWaveGridX[0][i] = (sinVal * 925) >> 10;
        gOceanWavePhases[i] += 18;
    }

    for (i = 1; i < OCEAN_BAND_COUNT; ++i) {
        for (col = 0; col < OCEAN_COL_COUNT; ++col) {
            gap = OCEAN_CELL_STEP - (gOceanWaveGridX[i][col] -
                                     gOceanWaveGridX[i - 1][col]);
            if (gap < 0) {
                step = gap / 16;
            } else {
                step = gap / 10;
            }
            gOceanWaveGridX[i][col] += step;
            if (i == OCEAN_BAND_COUNT - 1) {
                gOceanWaveEdgeVel[col] = step;
            }
        }
    }

    *gRenderQueueCursor++ = &gOceanWaveNode;
}

/* The ocean node's draw routine, reached from the renderer's scene node dispatch (render type 14). */
void DrawOceanWaveMesh(void)
{
    short *quadDescs;
    int band;
    int (*row)[OCEAN_COL_COUNT];
    SVECTOR *vertices;
    long *screen;
    unsigned short *depth;
    SVECTOR *vertex;
    const int *waveX;
    int i;
    int z;
    long xy0, xy1, xy2;
    long p, flag;
    long sz0, sz1, sz2;

    quadDescs = gOceanWaveQuadDescs;
    for (band = 0, row = gOceanWaveGridX; band < OCEAN_BAND_COUNT; ++band, ++row) {
        if (band & 1) {
            vertices = gOceanWaveRowVerts[1];
            screen = &RENDER_SCRATCH->screenXy[OCEAN_ROW_STRIDE];
            depth = &RENDER_SCRATCH->screenZ[OCEAN_ROW_STRIDE];
        } else {
            vertices = gOceanWaveRowVerts[0];
            screen = RENDER_SCRATCH->screenXy;
            depth = RENDER_SCRATCH->screenZ;
        }

        vertex = vertices;
        for (i = 0, z = -28800, waveX = *row; i < OCEAN_COL_COUNT; ++i) {
            int x = *waveX++;
            vertex->vx = x - 12800;
            vertex->vy = 0;
            vertex->vz = z;
            ++vertex;
            z += OCEAN_CELL_STEP;
        }

        vertex = vertices;
        for (i = 0; i < OCEAN_ROW_TRIPLETS; ++i) {
            RotTransPers3(vertex, vertex + 1, vertex + 2, &xy0, &xy1, &xy2, &p, &flag);
            vertex += 3;
            ReadSZfifo3(&sz0, &sz1, &sz2);

            *screen++ = xy0;
            *screen++ = xy1;
            *screen++ = xy2;
            *depth++ = sz0;
            *depth++ = sz1;
            *depth++ = sz2;
        }

        if (band != 0)
            quadDescs = DrawSubdivPolyFt4Dynamic(OCEAN_QUAD_COLS, vertices, quadDescs, 0);
    }
}

static int gOceanSampleIndex;
static int gOceanSampleWeight;

int SampleOceanWaveEdgeX(int worldZ)
{
    unsigned int index;
    int weight;

    index = (worldZ + 32000) / OCEAN_CELL_STEP;
    gOceanSampleIndex = index;
    weight = (worldZ % OCEAN_CELL_STEP) & 1; /* 0 or 1 out of 4096:
                                                both samplers return about the next column's value */
    gOceanSampleWeight = weight;

    return index < OCEAN_QUAD_COLS
        ? ((gOceanWaveGridX[OCEAN_BAND_COUNT - 1][index] * weight +
            gOceanWaveGridX[OCEAN_BAND_COUNT - 1][index + 1] * (ONE - weight)) >> 12) - 32200
        : -100000;
}

int SampleOceanWaveVelocity(void)
{
    int index;
    int edgeVel;
    int weight;
    int velocity;

    index = gOceanSampleIndex;
    if ((unsigned int)index < OCEAN_QUAD_COLS) {
        edgeVel = gOceanWaveEdgeVel[index];
        weight = gOceanSampleWeight;
        velocity = (edgeVel * weight + gOceanWaveEdgeVel[index + 1] * (ONE - weight)) >> 12;
    } else {
        velocity = 0;
    }
    return velocity;
}
