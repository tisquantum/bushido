/* Ground decals: a 16-entry ring of horizontal textured quads (fighter footprints) from a 2x2 atlas,
 * each living 300 frames and fading over the last 128, projected into deferred POLY_FT4 packets each frame. */
#include <sys/types.h>
#include <libgte.h>
#include <inline_o.h>
#include <libgpu.h>
#include "billboard.h"
#include "gpu_code.h"
#include "stage_id.h"
#include "decal.h"
#include "ocean.h"
#include "rot_matrix.h"
#include "globals.h"

#define GROUND_DECAL_MAX        16
#define GROUND_DECAL_MASK       (GROUND_DECAL_MAX - 1)
#define GROUND_DECAL_LIFETIME   300
#define GROUND_DECAL_FULL_SHADE 128
#define GROUND_DECAL_SIZE       200
#define GROUND_DECAL_HALF_SIZE  100
#define SHORELINE_WAVE_DECAY    8

int gGroundDecalWriteIndex;

struct GroundDecal {
    struct {
        int x, y, z;
    } corners[4];
    short uvIndex;
    short life;
};

struct GroundDecal gGroundDecals[GROUND_DECAL_MAX];
u_short gGroundDecalTileUvs[4][4];

void ClearGroundDecals(void)
{
    int i;

    gGroundDecalWriteIndex = 0;
    for (i = 0; i < GROUND_DECAL_MAX; i++) {
        gGroundDecals[i].life = 0;
    }
}

void InitGroundDecalAtlas(EffectTextureEntry *texture)
{
    int tileY, tileX, row, col;
    int u, v;

    gGroundDecalTexture = *texture;

    /* 2x2 grid of 8x8-texel footprint tiles; pack each corner as (u | (v << 8)). */
    for (tileY = 0; tileY < 2; tileY++) {
        for (tileX = 0; tileX < 2; tileX++) {
            for (row = 0; row < 2; row++) {
                for (col = 0; col < 2; col++) {
                    u = texture->u + col * 7 + tileX * 8;
                    v = texture->v + row * 7 + tileY * 8;
                    gGroundDecalTileUvs[tileY * 2 + tileX][row * 2 + col] =
                        u | (v << 8);
                }
            }
        }
    }

    ClearGroundDecals();
}

void SpawnGroundDecal(int uvIndex, int *position, int angle)
{
    struct GroundDecal *decal;
    int sinA;
    int cosA;
    int row;
    int col;

    decal = &gGroundDecals[gGroundDecalWriteIndex & GROUND_DECAL_MASK];
    decal->uvIndex = uvIndex;
    gGroundDecalWriteIndex++;
    decal->life = GROUND_DECAL_LIFETIME;
    sinA = rsin(angle);
    cosA = rcos(angle);
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 2; col++) {
            decal->corners[row * 2 + col].x = position[0] +
                (cosA * (col * GROUND_DECAL_SIZE - GROUND_DECAL_HALF_SIZE) +
                 sinA * (row * GROUND_DECAL_SIZE - GROUND_DECAL_HALF_SIZE)) / ONE;
            decal->corners[row * 2 + col].y = position[1];
            decal->corners[row * 2 + col].z = position[2] +
                (-sinA * (col * GROUND_DECAL_SIZE - GROUND_DECAL_HALF_SIZE) +
                 cosA * (row * GROUND_DECAL_SIZE - GROUND_DECAL_HALF_SIZE)) / ONE;
        }
    }
}

int UpdateAndBuildGroundDecals(GroundDecalScratch *scratch)
{
    long depths[GROUND_DECAL_MAX];
    POLY_FT4 *polys[GROUND_DECAL_MAX];
    struct GroundDecal *decal;
    POLY_FT4 *poly;
    int count;
    int i;
    int j;
    int k;
    int shade;
    EffectTextureEntry *texture;
    long *depthList;

    texture = &gGroundDecalTexture;
    SetRotMatrix(&scratch->matrix);
    count = 0;
    SetTransMatrix(&gZeroTransMatrix);
    decal = gGroundDecals;
    poly = scratch->packets;
    for (i = 0; i < GROUND_DECAL_MAX; i++, decal++) {
        if (decal->life == 0)
            continue;
        decal->life--;
        if (gCurrentStageId == STAGE_ID_EXECUTIONERS_COVE &&
            decal->corners[0].x < SampleOceanWaveEdgeX(decal->corners[0].z)) {
            decal->life -= SHORELINE_WAVE_DECAY;
            if (decal->life < 0)
                decal->life = 0;
        }
        for (j = 0; j < 4; j++) {
            scratch->corners[j].vx = decal->corners[j].x - scratch->translation[0];
            scratch->corners[j].vy = decal->corners[j].y - scratch->translation[1];
            scratch->corners[j].vz = decal->corners[j].z - scratch->translation[2];
        }
        gte_ldv3(&scratch->corners[0], &scratch->corners[1], &scratch->corners[2]);
        gte_rtpt();
        SetPolyFT4(poly);
        polys[count] = poly;
        poly->tpage = texture->tpage;
        poly->clut = texture->clut;
        gte_nclip();
        gte_stopz(&scratch->opz);
        if (scratch->opz < 0) {
            gte_stsxy3(&poly->x0, &poly->x1, &poly->x2);
            gte_ldv0(&scratch->corners[3]);
            gte_rtps();
            *(u_short *)&poly->u0 = gGroundDecalTileUvs[decal->uvIndex][0];
            *(u_short *)&poly->u1 = gGroundDecalTileUvs[decal->uvIndex][1];
            *(u_short *)&poly->u2 = gGroundDecalTileUvs[decal->uvIndex][2];
            *(u_short *)&poly->u3 = gGroundDecalTileUvs[decal->uvIndex][3];
            gte_stsxy(&poly->x3);
            gte_avsz4();
            shade = decal->life;
            if (shade > GROUND_DECAL_FULL_SHADE)
                shade = GROUND_DECAL_FULL_SHADE;
            *(long *)&poly->r0 = GPU_CODE_WORD(GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS) |
                shade << 16 | shade << 8 | shade;
            gte_stotz(&depths[count++]);
            poly++;
        }
    }
    /* Append deferred packet pointers and GTE OTZ depths immediately after the POLY_FT4s. */
    scratch->packets = poly;
    depthList = (long *)poly + count;
    for (k = 0; k < count; k++)
        ((POLY_FT4 **)poly)[k] = polys[k];
    for (k = 0; k < count; k++)
        depthList[k] = depths[k];
    return count;
}
