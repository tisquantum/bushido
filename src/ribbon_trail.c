/* Ribbon trails: a ring buffer of hilt and tip samples drawn as a textured, semi-transparent
 * strip of POLY_GT4s, smoothed between samples and subdivided by camera distance. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <inline_o.h>

#include "ribbon_trail.h"
#include "gpu_code.h"
#include "cloth.h"
#include "rot_matrix.h"


/* Camera distance thresholds (world units) and matching width-subdivision LODs (3..0 -> 8, 4, 2, 1 slices). */
int gRibbonTrailLodDists[RIBBON_TRAIL_LOD_BANDS] = { 500, 1000, 2000, 4000 };
int gRibbonTrailLodLevels[RIBBON_TRAIL_LOD_BANDS] = { 3, 2, 1, 0 };

void InitRibbonTrailUvs(RibbonTrail *trail)
{
    int i;
    unsigned short uv;

    for (i = 0; i <= RIBBON_TRAIL_UV_MAX_INDEX; i++) {
        LoadAverageByte(&trail->u0, &trail->u1, (RIBBON_TRAIL_UV_MAX_INDEX - i) << 8, i << 8,
                        (unsigned char *)&uv);
        trail->uvs[i] = uv;
    }
    trail->flags = RIBBON_TRAIL_FLAG_INIT;
}

static int GetRibbonTrailLod(VECTOR *first, VECTOR *second, VECTOR *origin)
{
    VECTOR delta;
    VECTOR squares;
    int distance;
    int band;

    delta.vx = (first->vx + second->vx) / 2 - origin->vx;
    delta.vy = (first->vy + second->vy) / 2 - origin->vy;
    delta.vz = (first->vz + second->vz) / 2 - origin->vz;
    if (delta.vx < -8192 || delta.vx > 8192)
        return 0;
    if (delta.vy < -8192 || delta.vy > 8192)
        return 0;
    if (delta.vz < -8192 || delta.vz > 8192)
        return 0;
    Square0(&delta, &squares);
    distance = SquareRoot0(squares.vx + squares.vy + squares.vz);
    for (band = 0; band < RIBBON_TRAIL_LOD_BANDS; band++) {
        if (distance < gRibbonTrailLodDists[band])
            return gRibbonTrailLodLevels[band];
    }
    return 0;
}

static void InterpolateRibbonMidpoint(RibbonDraw *work, VECTOR *first, VECTOR *middle, VECTOR *last, VECTOR *out)
{
    int squared;
    int leading[3];
    unsigned char unused[24]; /* not used */
    int firstLength, secondLength, blendLength;
    int cosine, firstWeight, secondWeight;
    int evenLeading;

    work->firstDelta.vx = middle->vx - first->vx;
    work->firstDelta.vy = middle->vy - first->vy;
    work->firstDelta.vz = middle->vz - first->vz;
    gte_ldlvl(&work->firstDelta);
    gte_sqr0();
    work->secondDelta.vx = last->vx - middle->vx;
    work->secondDelta.vy = last->vy - middle->vy;
    work->secondDelta.vz = last->vz - middle->vz;
    gte_stlvnl(&work->blend);
    squared = work->blend.vx + work->blend.vy + work->blend.vz;
    if (squared < 1024) {
        firstLength = gSqrtLutQ3[squared] >> 3;
    } else {
        gte_ldlzc(squared);
        gte_nop();
        gte_nop();
        gte_stlzc(&leading[0]);
        evenLeading = leading[0] & ~1;
        squared = gSqrtLutQ3[squared >> (22 - evenLeading)];
        firstLength = (squared << 16) >> (19 - ((22 - evenLeading) >> 1));
    }

    gte_ldlvl(&work->secondDelta);
    gte_sqr0();
    gte_stlvnl(&work->blend);
    squared = work->blend.vx + work->blend.vy + work->blend.vz;
    if (squared < 1024) {
        secondLength = gSqrtLutQ3[squared] >> 3;
    } else {
        gte_ldlzc(squared);
        gte_nop();
        gte_nop();
        gte_stlzc(&leading[1]);
        evenLeading = leading[1] & ~1;
        squared = gSqrtLutQ3[squared >> (22 - evenLeading)];
        secondLength = (squared << 16) >> (19 - ((22 - evenLeading) >> 1));
    }

    if (firstLength == 0 || secondLength == 0) {
        cosine = ONE;
    } else {
        cosine = work->firstDelta.vx * work->secondDelta.vx +
                 work->firstDelta.vy * work->secondDelta.vy +
                 work->firstDelta.vz * work->secondDelta.vz;
        cosine = (cosine << 6) / firstLength;
        cosine = (cosine << 6) / secondLength;
    }
    firstWeight = (ONE - cosine) / 2;
    secondWeight = ONE - firstWeight;
    gte_lddp(firstWeight);
    gte_ldlvl(&work->firstDelta);
    gte_gpf12();
    gte_lddp(secondWeight);
    gte_ldlvl(&work->secondDelta);
    gte_gpl12();
    gte_stlvl(&work->blend);
    gte_ldlvl(&work->blend);
    gte_sqr0();
    gte_stlvnl(&work->firstDelta);
    squared = work->firstDelta.vx + work->firstDelta.vy + work->firstDelta.vz;
    if (squared < 1024) {
        blendLength = gSqrtLutQ3[squared] >> 3;
    } else {
        gte_ldlzc(squared);
        gte_nop();
        gte_nop();
        gte_stlzc(&leading[2]);
        evenLeading = leading[2] & ~1;
        squared = gSqrtLutQ3[squared >> (22 - evenLeading)];
        blendLength = (squared << 16) >> (19 - ((22 - evenLeading) >> 1));
    }
    if (blendLength == 0) {
        blendLength = 1;
    }
    out->vx = middle->vx + (work->blend.vx * secondLength / 2) / blendLength;
    out->vy = middle->vy + (work->blend.vy * secondLength / 2) / blendLength;
    out->vz = middle->vz + (work->blend.vz * secondLength / 2) / blendLength;
}

/* For each LOD 0..3, indices into RibbonTrail.uvs ended by 0xff. */
unsigned char gRibbonTrailUvSteps[4][16] = {
    {
        0x00, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff,
    },
    {
        0x00, 0x08, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff,
    },
    {
        0x00, 0x04, 0x08, 0x0c, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff,
    },
    {
        0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff,
    },
};
/* 0 to 16 by ones: the next row of the pattern above, ended by zeros, not 0xff; nothing reads these 32 bytes. */
unsigned char gRibbonTrailUvStepsUnused[2][16] = {
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
        0x0f,
    },
    {
        0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00,
    },
};

int UpdateAndBuildRibbonPackets(RibbonDraw *work)
{
    RibbonTrail *ribbon;
    unsigned short flags;
    int head;
    int lod;
    int segments;
    int count;
    int row;
    int column;
    int edge;
    int cornerEnd;
    long topColor;
    long bottomColor;
    POLY_GT4 *poly;
    long *depthList;
    RibbonSample *sample;
    int i;
    int nearUv;
    int farUv;

    ribbon = work->ribbon;
    flags = ribbon->flags;
    if (flags & RIBBON_TRAIL_FLAG_INIT) {
        ribbon->flags = flags & ~RIBBON_TRAIL_FLAG_INIT;
        for (row = 0; row < RIBBON_TRAIL_HISTORY_LEN; row++) {
            ribbon->samples[row].first = work->latest.first;
            ribbon->samples[row].second = work->latest.second;
        }
        ribbon->head = 0;
    } else if (!(flags & RIBBON_TRAIL_FLAG_PAUSE)) {
        /* edge, row and cornerEnd reused, as in the original */
        head = ribbon->head;
        edge = (head - 3) & RIBBON_TRAIL_HISTORY_MASK;
        row = (head - 1) & RIBBON_TRAIL_HISTORY_MASK;
        cornerEnd = ++head & RIBBON_TRAIL_HISTORY_MASK;
        ribbon->samples[cornerEnd].first = work->latest.first;
        ribbon->samples[cornerEnd].second = work->latest.second;
        InterpolateRibbonMidpoint(work, &ribbon->samples[edge].first, &ribbon->samples[row].first,
                                  &ribbon->samples[cornerEnd].first, &ribbon->samples[ribbon->head].first);
        InterpolateRibbonMidpoint(work, &ribbon->samples[edge].second, &ribbon->samples[row].second,
                                  &ribbon->samples[cornerEnd].second, &ribbon->samples[ribbon->head].second);
        ribbon->head = (ribbon->head + 2) & RIBBON_TRAIL_HISTORY_MASK;
    }
    SetRotMatrix(&work->matrix);
    SetTransMatrix(&gZeroTransMatrix);
    lod = GetRibbonTrailLod(&ribbon->samples[(ribbon->head - 1) & RIBBON_TRAIL_HISTORY_MASK].first,
                            &ribbon->samples[ribbon->head].second, &work->origin);
    count = 0;
    segments = 1 << lod;
    poly = work->packets;
    {
        long depths[segments * RIBBON_TRAIL_VISIBLE_SEGMENTS];
        POLY_GT4 *polys[segments * RIBBON_TRAIL_VISIBLE_SEGMENTS];

        for (row = 0; row < RIBBON_TRAIL_VISIBLE_SEGMENTS; row++) {
            topColor = (RIBBON_TRAIL_VISIBLE_SEGMENTS - row) * 128 / RIBBON_TRAIL_VISIBLE_SEGMENTS;
            topColor = GPU_CODE_WORD(GPU_CODE_POLY_GT4 | GPU_CODE_SEMITRANS) |
                topColor << 16 | topColor << 8 | topColor;
            bottomColor = (RIBBON_TRAIL_VISIBLE_SEGMENTS - 1 - row) * 128 / RIBBON_TRAIL_VISIBLE_SEGMENTS;
            bottomColor = GPU_CODE_WORD(GPU_CODE_POLY_GT4 | GPU_CODE_SEMITRANS) |
                bottomColor << 16 | bottomColor << 8 | bottomColor;
            for (column = 0; column < segments; column++) {
                for (edge = 0; edge < 2; edge++) {
                    sample = &ribbon->samples[(ribbon->head - 1 - row - edge) & RIBBON_TRAIL_HISTORY_MASK];
                    for (cornerEnd = 0; cornerEnd < 2; cornerEnd++) {
                        LoadAverage12(&sample->first, &sample->second, (segments - column - cornerEnd) << (12 - lod),
                                      (column + cornerEnd) << (12 - lod), &work->firstDelta);
                        work->corners[edge * 2 + cornerEnd].vx = work->firstDelta.vx - work->origin.vx;
                        work->corners[edge * 2 + cornerEnd].vy = work->firstDelta.vy - work->origin.vy;
                        work->corners[edge * 2 + cornerEnd].vz = work->firstDelta.vz - work->origin.vz;
                    }
                }
                gte_ldv3(&work->corners[0], &work->corners[1], &work->corners[2]);
                gte_rtpt();
                nearUv = gRibbonTrailUvSteps[lod][column];
                farUv = gRibbonTrailUvSteps[lod][column + 1];
                SetPolyGT4(poly);
                polys[count] = poly;
                gte_stsxy3(&poly->x0, &poly->x1, &poly->x2);
                gte_ldv0(&work->corners[3]);
                gte_rtps();
                *(unsigned short *)&poly->u0 = ribbon->uvs[nearUv];
                *(unsigned short *)&poly->u1 = ribbon->uvs[farUv];
                *(unsigned short *)&poly->u2 = ribbon->uvs[nearUv];
                *(unsigned short *)&poly->u3 = ribbon->uvs[farUv];
                gte_stsxy(&poly->x3);
                gte_avsz4();
                poly->tpage = ribbon->tpage;
                poly->clut = ribbon->clut;
                gte_stotz(&depths[count++]);
                *(long *)&poly->r0 = topColor;
                *(long *)&poly->r1 = topColor;
                *(long *)&poly->r2 = bottomColor;
                *(long *)&poly->r3 = bottomColor;
                poly++;
            }
        }
        work->packets = poly;
        depthList = (long *)poly + count;
        for (i = 0; i < count; i++)
            ((POLY_GT4 **)poly)[i] = polys[i];
        for (i = 0; i < count; i++)
            depthList[i] = depths[i];
    }
    return count;
}
