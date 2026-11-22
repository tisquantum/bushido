/* Stage 19's swaying foliage: a crossed, 18-vertex hanging mesh run by the cloth simulation
 * under a pulsing stage wind, drawn as deferred POLY_FT4 quads. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <rand.h>
#include <inline_o.h>

#include "cloth.h"
#include "foliage.h"
#include "gpu_code.h"
#include "light_init.h"
#include "rot_matrix.h"

/* Rest-pose vertices of the two crossed sheets (4x simulation scale; .pad = 0 rigid pin,
 * -256 free dynamic vertex). */
SVECTOR gMoatFoliageVerts[SWAYING_FOLIAGE_VERT_COUNT] = {
    { -800, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 800, 0, 0, 0 },
    { -800, 1600, 0, -256 },
    { 0, 1600, 0, -256 },
    { 800, 1600, 0, -256 },
    { -800, 3200, 0, -256 },
    { 0, 3200, 0, -256 },
    { 800, 3200, 0, -256 },
    { 0, 0, -800, 0 },
    { 0, 0, 800, 0 },
    { 0, 1600, -800, -256 },
    { 0, 1600, 800, -256 },
    { 0, 3200, -800, -256 },
    { 0, 3200, 800, -256 },
    { 0, 3200, 0, -256 },
    { 0, 3200, 0, -256 },
    { 0, 3200, 0, -256 },
};
/* The cloth's face links, { vertexCount, stiffnessDiv, vertices... }, ending in 0: four
 * quads across the two crossed sheets, then eight triangles down to the hanging tips. */
short gMoatFoliageFaces[66] = {
    4, 3, 0, 1, 3, 4,
    4, 3, 1, 2, 4, 5,
    4, 3, 9, 1, 11, 4,
    4, 3, 1, 10, 4, 12,
    3, 3, 3, 4, 7,
    3, 3, 3, 7, 6,
    3, 3, 4, 5, 15,
    3, 3, 5, 8, 15,
    3, 3, 11, 4, 16,
    3, 3, 11, 16, 13,
    3, 3, 4, 12, 17,
    3, 3, 12, 14, 17,
    0, 0,
};
short gMoatFoliageQuads[SWAYING_FOLIAGE_QUAD_COUNT][4] = {
    { 0, 1, 3, 4 },
    { 1, 2, 4, 5 },
    { 3, 4, 6, 7 },
    { 4, 5, 15, 8 },
    { 9, 1, 11, 4 },
    { 11, 4, 13, 16 },
    { 1, 10, 4, 12 },
    { 4, 12, 17, 14 },
};

Wind *gSwayWindPtr = &gClothWindSlots[CLOTH_WIND_SLOT_STAGE];
int gSwayWavePhase = 123456789;
SwayingFoliageTexture gMoatFoliageTexture;
VECTOR gMoatFoliageOrigins[SWAYING_FOLIAGE_COUNT];
SwayingFoliageBody *gMoatFoliageBody;

void InitSwayingFoliageMesh(SwayingFoliageInit *init)
{
    ClothModelSpec spec;
    SwayingFoliageTexture *texture;
    SwayingFoliageBody *body;
    SVECTOR *sourceVertex;
    SVECTOR *bodyVertex;
    VECTOR *origin;
    int instance;
    int vertexIndex;
    int x;
    int y;
    int u;
    int v;

    texture = &gMoatFoliageTexture;
    gMoatFoliageBody = init->storage;
    texture->tpage = init->tpage;
    texture->clut = init->clut;

    for (y = 0; y < 3; y++) {
        for (x = 0; x < 3; x++) {
            u = (init->u0 * (2 - x) + init->u1 * x) / 2;
            v = (init->v0 * (2 - y) + init->v1 * y) / 2;
            texture->uv[y * 3 + x] = (v << 8) | u;
        }
    }

    texture->uv[9] = texture->uv[0];
    texture->uv[10] = texture->uv[2];
    texture->uv[11] = texture->uv[3];
    texture->uv[12] = texture->uv[5];
    texture->uv[13] = texture->uv[6];
    texture->uv[14] = texture->uv[8];
    texture->uv[15] = texture->uv[7];
    texture->uv[16] = texture->uv[7];
    texture->uv[17] = texture->uv[7];

    body = gMoatFoliageBody;
    for (instance = 0; instance < SWAYING_FOLIAGE_COUNT; instance++, body++) {
        bodyVertex = body->verts;
        sourceVertex = gMoatFoliageVerts;
        for (vertexIndex = 0; vertexIndex < SWAYING_FOLIAGE_VERT_COUNT; vertexIndex++, bodyVertex++, sourceVertex++) {
            bodyVertex->vx = sourceVertex->vx >> 2;
            bodyVertex->vy = sourceVertex->vy >> 2;
            bodyVertex->vz = sourceVertex->vz >> 2;
            bodyVertex->pad = sourceVertex->pad;
            bodyVertex->vx += (rand() & 0x1f) - 16;
            bodyVertex->vz += (rand() & 0x1f) - 16;
        }
        InitPropIdentityMatrix3x3(&body->matrix);
        RotMatrixY((instance << 8) - 0x80, &body->matrix);
        body->matrix.t[0] = 0;
        body->matrix.t[1] = 0;
        body->matrix.t[2] = 0;

        spec.pointCount = SWAYING_FOLIAGE_VERT_COUNT;
        spec.sourceVertices = body->verts;
        spec.faceLinkLists = gMoatFoliageFaces;
        spec.initialTransform = &body->matrix;
        spec.workBuffer = body->simWork;
        spec.damping = 0xe80;
        if (CalcClothWorkSize(SWAYING_FOLIAGE_VERT_COUNT, SWAYING_FOLIAGE_LINK_COUNT) <= SWAYING_FOLIAGE_WORK_BYTES) {
            body->modelIndex = AllocClothModel(&spec);
        } else {
            body->modelIndex = -1;
        }
    }

    origin = gMoatFoliageOrigins;
    for (instance = 0; instance < SWAYING_FOLIAGE_COUNT; instance++, origin++) {
        *origin = init->pos[instance];
    }
}

int UpdateAndDrawSwayingFoliage(SwayingFoliageScratch *scratch)
{
    ClothItem items[SWAYING_FOLIAGE_COUNT];
    SwayingFoliageScratch savedScratch;
    long depths[SWAYING_FOLIAGE_QUAD_COUNT];
    POLY_FT4 *polys[SWAYING_FOLIAGE_QUAD_COUNT];
    SwayingFoliageBody *body;
    SwayingFoliageTexture *texture;
    short *quadVerts;
    VECTOR *origin;
    POLY_FT4 *poly;
    long *depthList;
    int savedWindStrength;
    int windWave;
    int rgbCode;
    int polyCount;
    int instance;
    int quad;
    int corner;
    int deferredIndex;

    body = gMoatFoliageBody;
    for (instance = 0; instance < SWAYING_FOLIAGE_COUNT; instance++, body++) {
        items[instance].modelIndex = body->modelIndex;
        items[instance].flags = CLOTH_ITEM_FLAG_NONE;
        items[instance].windSlot = CLOTH_WIND_SLOT_STAGE;
        items[instance].outputPositions = body->points;
        items[instance].transform = &body->matrix;
        items[instance].colliderInput = 0;
    }

    savedWindStrength = gSwayWindPtr->strength;
    gSwayWavePhase += 13;
    windWave = rsin(gSwayWavePhase) & 0xfff;
    if (windWave >= 0x800)
        windWave -= 0x1000;
    windWave += 0x800;
    gSwayWindPtr->strength = gSwayWindPtr->strength * windWave >> 11;

    savedScratch = *scratch;
    UpdateClothModelsWithOutput(items, SWAYING_FOLIAGE_COUNT);
    *scratch = savedScratch;
    texture = &gMoatFoliageTexture;
    gSwayWindPtr->strength = savedWindStrength;

    rgbCode = (scratch->tint.rgb & GPU_RGB_MASK) | GPU_CODE_WORD(GPU_CODE_POLY_FT4);
    SetRotMatrix(&scratch->viewMatrix);
    SetTransMatrix(&gZeroTransMatrix);
    poly = scratch->packetCursor;
    body = gMoatFoliageBody;
    origin = gMoatFoliageOrigins;
    for (instance = 0, polyCount = 0; instance < SWAYING_FOLIAGE_COUNT; instance++, origin++) {
        scratch->offset.vx = origin->vx - scratch->cameraPos.vx;
        scratch->offset.vz = origin->vz - scratch->cameraPos.vz;
        if ((unsigned int)(((ratan2(scratch->offset.vz, scratch->offset.vx) - scratch->cameraHeading + 0x400) &
                            0xfff) - 384) < 3329)
            continue;
        scratch->offset.vy = origin->vy - scratch->cameraPos.vy;
        quadVerts = gMoatFoliageQuads[0];
        for (quad = 0; quad < SWAYING_FOLIAGE_QUAD_COUNT; quad++, quadVerts += 4) {
            /* The cloth works at a quarter of world scale. */
            for (corner = 0; corner < 4; corner++) {
                scratch->corners[corner].vx = scratch->offset.vx + (body->points[quadVerts[corner]].vx << 2);
                scratch->corners[corner].vy = scratch->offset.vy + (body->points[quadVerts[corner]].vy << 2);
                scratch->corners[corner].vz = scratch->offset.vz + (body->points[quadVerts[corner]].vz << 2);
            }
            gte_ldv3(&scratch->corners[0], &scratch->corners[1], &scratch->corners[2]);
            gte_rtpt();
            SetPolyFT4(poly);
            polys[polyCount] = poly;
            gte_stsxy3(&poly->x0, &poly->x1, &poly->x2);
            gte_ldv0(&scratch->corners[3]);
            gte_rtps();
            poly->tpage = texture->tpage;
            poly->clut = texture->clut;
            *(long *)&poly->r0 = rgbCode;
            gte_stsxy(&poly->x3);
            gte_avsz4();
            *(unsigned short *)&poly->u0 = texture->uv[quadVerts[0]];
            *(unsigned short *)&poly->u1 = texture->uv[quadVerts[1]];
            *(unsigned short *)&poly->u2 = texture->uv[quadVerts[2]];
            *(unsigned short *)&poly->u3 = texture->uv[quadVerts[3]];
            gte_stotz(&depths[polyCount++]);
            poly++;
        }
    }

    scratch->packetCursor = poly;
    depthList = (long *)poly + polyCount;
    for (deferredIndex = 0; deferredIndex < polyCount; deferredIndex++)
        ((POLY_FT4 **)poly)[deferredIndex] = polys[deferredIndex];
    for (deferredIndex = 0; deferredIndex < polyCount; deferredIndex++)
        depthList[deferredIndex] = depths[deferredIndex];
    return polyCount;
}
