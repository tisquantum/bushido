/*
 * Soft-body cloth: up to 16 mass-spring cloth models solved on the scratchpad with gravity, the
 * wind channels, a floor plane and limb capsule colliders, for stage cloth and fighter garments.
 */
#include <sys/types.h>
#include <libetc.h>
#include <inline_o.h>
#include "cloth.h"

void ClearClothModels(void)
{
    int i;

    for (i = 0; i < CLOTH_MODEL_COUNT; ++i) {
        gClothModels[i].sourceVertices = 0;
    }
}

int CalcClothWorkSize(int pointCount, int linkCount)
{
    return pointCount * (int)sizeof(ClothPoint) + linkCount * (int)sizeof(ClothLink);
}

static int CalcVec3DistanceFine(VECTOR *a, VECTOR *b)
{
    VECTOR delta;
    VECTOR squared;

    delta.vx = (a->vx - b->vx) >> 2;
    delta.vy = (a->vy - b->vy) >> 2;
    delta.vz = (a->vz - b->vz) >> 2;
    Square12(&delta, &squared);
    return SquareRoot12(squared.vx + squared.vy + squared.vz) << 2;
}

static int CalcVec3DistanceCoarse(VECTOR *a, VECTOR *b)
{
    VECTOR delta;
    VECTOR squared;

    delta.vx = (a->vx - b->vx) >> 4;
    delta.vy = (a->vy - b->vy) >> 4;
    delta.vz = (a->vz - b->vz) >> 4;
    Square12(&delta, &squared);
    return SquareRoot12(squared.vx + squared.vy + squared.vz) << 4;
}

/* Unified 0x1F800000 scratchpad layout used by the cloth mass-spring solver. */
typedef struct ClothScratch {
    union {
        int linkDelta[3];
        int capsuleDiffStart[3];
    } diffStart;
    union {
        int linkSq[3];
        int capsuleDiffEnd[3];
    } diffEnd;
    int forceOrTarget[3];
    int rootDrift[3];
    int workVector[3];
    int pointPos[3];
    int coarsePos[3];
    int windForceX;
    int windForceZ;
    int damping;
    int gravity;
    int pushLimit;
    int colliderCount;
    int capsuleEndpoints[3][2][3];
    int capsuleRadii[3];
    int floorY;
    int windSin;
    int windCos;
    int windForceSin;
    int windForceCos;
    int windGustColumnOrigin;
    int windGustRowOrigin;
    int linkForces[67][3]; /* to the end of the 1 KB scratchpad */
} ClothScratch;

#define SCRATCHPAD_BASE  ((int *)getScratchAddr(0))
#define SCRATCHPAD_WORDS 0x100
#define CLOTH_SCRATCH    ((ClothScratch *)getScratchAddr(0))

#define CLOTH_DRIFT_MAX_DIST    500
#define CLOTH_DRIFT_MAX_DIST_SQ (CLOTH_DRIFT_MAX_DIST * CLOTH_DRIFT_MAX_DIST)
#define CLOTH_MAX_CAPSULE_PUSH  0x400000
#define CLOTH_FLOOR_MAX_DEPTH   12800 /* 100 << 7: down to this depth below the floor the push is dist / 8;
                                         deeper it is CLOTH_FLOOR_MAX_PUSH */
#define CLOTH_FLOOR_MAX_PUSH    1024
#define CLOTH_FIGHTER_GRAVITY   400
#define CLOTH_WIND_SCROLL_COLUMN     153
#define CLOTH_WIND_SCROLL_ROW     827

ClothModel gClothModels[CLOTH_MODEL_COUNT];
Wind gClothWindSlots[CLOTH_WIND_SLOT_COUNT];
int gScratchpadBackup[SCRATCHPAD_WORDS];
int gStageClothGravity = 400;
u_char gMaxClothModelSlot = 0;

static int RegisterPointConstraint(ClothModel *model, int stiffnessDiv, int from, int to)
{
    int i;
    int length;
    ClothLink *link;

    if (from == to || (model->points[from].massOrPin >= 0 && model->points[to].massOrPin >= 0)) {
        return 0;
    }
    for (i = 0; i < model->points[from].outLinkCount; ++i) {
        if (model->links[model->points[from].outLinks[i]].endpoints.indices.to == to) {
            return 0;
        }
    }
    for (i = 0; i < model->points[from].inLinkCount; ++i) {
        if (model->links[model->points[from].inLinks[i]].endpoints.indices.from == to) {
            return 0;
        }
    }

    length = CalcVec3DistanceFine((VECTOR *)&model->points[from], (VECTOR *)&model->points[to]);
    link = &model->links[model->linkCount];
    link->restLength = length;
    link->maxLength = length * 3;
    link->stiffnessDiv = stiffnessDiv;
    link->endpoints.packed = ((u_int)from << 16) | to;
    model->points[from].outLinks[model->points[from].outLinkCount++] = model->linkCount;
    model->points[to].inLinks[model->points[to].inLinkCount++] = model->linkCount;
    ++model->linkCount;
    return 1;
}

static void InitClothPointsAndLinks(ClothModelSpec *spec, ClothModel *model)
{
    VECTOR position;
    VECTOR center;
    long flags;
    ClothPoint *points;
    ClothPoint *point;
    int pinnedCount = 0;
    int i, j;
    short *faceList;
    int vertexCount, stiffnessDiv;

    center.vx = 0;
    center.vy = 0;
    center.vz = 0;
    SetRotMatrix(spec->initialTransform);
    SetTransMatrix(spec->initialTransform);
    points = model->points;
    for (i = 0; i < spec->pointCount; ++i) {
        point = &points[i];
        point->massOrPin = spec->sourceVertices[i].pad;
        RotTrans(&spec->sourceVertices[i], &position, &flags);
        position.vx *= 128;
        position.vy *= 128;
        position.vz *= 128;
        if (point->massOrPin >= 0) {
            ++pinnedCount;
            center.vx += position.vx;
            center.vy += position.vy;
            center.vz += position.vz;
        }
        point->x = position.vx;
        point->y = position.vy;
        point->z = position.vz;
        point->vx = 0;
        point->vy = 0;
        point->vz = 0;
        point->outLinkCount = 0;
        point->inLinkCount = 0;
    }

    center.vx /= pinnedCount;
    center.vy /= pinnedCount;
    center.vz /= pinnedCount;
    for (i = 0; i < spec->pointCount; ++i) {
        int weight = CalcVec3DistanceCoarse(&center, (VECTOR *)&points[i]) >> 8;
        if (weight > 256) {
            weight = 256;
        }
        points[i].windWeight = weight;
    }

    faceList = spec->faceLinkLists;
    while ((vertexCount = *faceList++) != 0) {
        stiffnessDiv = *faceList++;
        for (i = 0; i < vertexCount - 1; ++i) {
            int distance = CalcVec3DistanceCoarse(&center, (VECTOR *)&points[faceList[i]]);
            for (j = i + 1; j < vertexCount; ++j) {
                if (distance < CalcVec3DistanceCoarse(&center, (VECTOR *)&points[faceList[j]])) {
                    RegisterPointConstraint(model, stiffnessDiv, faceList[i], faceList[j]);
                } else {
                    RegisterPointConstraint(model, stiffnessDiv, faceList[j], faceList[i]);
                }
            }
        }
        faceList += vertexCount;
    }
}

int AllocClothModel(ClothModelSpec *spec)
{
    ClothModel *model;
    int slot;
    ClothPoint *workBuffer;
    int pointCount;

    for (model = gClothModels, slot = 0; slot < CLOTH_MODEL_COUNT; ++slot, ++model) {
        if (model->sourceVertices == 0) {
            break;
        }
    }
    if (slot == CLOTH_MODEL_COUNT) {
        return -1;
    }
    if (gMaxClothModelSlot < slot) {
        gMaxClothModelSlot = slot;
    }
    model->pointCount = spec->pointCount;
    model->sourceVertices = spec->sourceVertices;
    model->prevTransform = *spec->initialTransform;
    model->damping = spec->damping;
    workBuffer = spec->workBuffer;
    pointCount = model->pointCount;
    model->linkCount = 0;
    model->points = workBuffer;
    model->links = (ClothLink *)(workBuffer + pointCount);
    InitClothPointsAndLinks(spec, model);
    return slot;
}

void FreeClothModel(int clothSlot)
{
    gClothModels[clothSlot].sourceVertices = 0;
}

void RelocateClothWorkPtrs(int clothSlot, int deltaBytes)
{
    ClothModel *model;
    ClothPoint *oldPoints;
    int pointCount;
    ClothPoint *newPoints;

    model = &gClothModels[clothSlot];
    oldPoints = model->points;
    pointCount = model->pointCount;
    newPoints = (ClothPoint *)((char *)oldPoints + deltaBytes);
    model->points = newPoints;
    model->links = (ClothLink *)(newPoints + pointCount);
}

void RelocateClothVertexPtr(int clothSlot, int deltaBytes)
{
    ClothModel *model = &gClothModels[clothSlot];

    model->sourceVertices = (SVECTOR *)((char *)model->sourceVertices + deltaBytes);
}

/* Nothing calls it. */
static void SaveScratchpad(void)
{
    int *src = SCRATCHPAD_BASE;
    int *dst = gScratchpadBackup;
    u_int i;

    for (i = 0; i < SCRATCHPAD_WORDS; ++i) {
        *dst++ = *src++;
    }
}

/* Nothing calls it. */
static void RestoreScratchpad(void)
{
    int *src = gScratchpadBackup;
    int *dst = SCRATCHPAD_BASE;
    u_int i;

    for (i = 0; i < SCRATCHPAD_WORDS; ++i) {
        *dst++ = *src++;
    }
}

void ResetClothWindSlots(void)
{
    int i;

    for (i = 0; i < CLOTH_WIND_SLOT_COUNT; ++i) {
        gClothWindSlots[i].strength = 0;
        gClothWindSlots[i].angle = 0;
        gClothWindSlots[i].gustColumnOrigin = 0;
        gClothWindSlots[i].gustRowOrigin = 0;
    }
}

void StepClothWindSlot(int magnitude, int angle, int windSlot)
{
    int columnOrigin = gClothWindSlots[windSlot].gustColumnOrigin;
    int rowOrigin = gClothWindSlots[windSlot].gustRowOrigin;

    gClothWindSlots[windSlot].strength = magnitude;
    gClothWindSlots[windSlot].angle = angle;
    gClothWindSlots[windSlot].gustColumnOrigin = columnOrigin + CLOTH_WIND_SCROLL_COLUMN;
    gClothWindSlots[windSlot].gustRowOrigin = rowOrigin + CLOTH_WIND_SCROLL_ROW;
}

static void LoadClothWindToScratch(int windSlot)
{
    int sine;
    int cosine;

    CLOTH_SCRATCH->windGustColumnOrigin = gClothWindSlots[windSlot].gustColumnOrigin;
    CLOTH_SCRATCH->windGustRowOrigin = gClothWindSlots[windSlot].gustRowOrigin;
    CLOTH_SCRATCH->windSin = rsin(gClothWindSlots[windSlot].angle);
    cosine = rcos(gClothWindSlots[windSlot].angle);
    sine = CLOTH_SCRATCH->windSin;
    CLOTH_SCRATCH->windCos = cosine;
    CLOTH_SCRATCH->windForceSin = sine * gClothWindSlots[windSlot].strength >> 12;
    CLOTH_SCRATCH->windForceCos = cosine * gClothWindSlots[windSlot].strength >> 12;
}

static void LoadClothCollidersToScratch(ClothItem *item)
{
    ClothColliderInput *colliders;
    int i, j;

    if (item->colliderInput != 0) {
        colliders = item->colliderInput;
        CLOTH_SCRATCH->floorY = colliders->floorY * 128;
        for (i = 0; i < colliders->activeCount; ++i) {
            for (j = 0; j < 2; ++j) {
                CLOTH_SCRATCH->capsuleEndpoints[i][j][0] = colliders->worldEndpoints[i][j].vx * 4;
                CLOTH_SCRATCH->capsuleEndpoints[i][j][1] = colliders->worldEndpoints[i][j].vy * 4;
                CLOTH_SCRATCH->capsuleEndpoints[i][j][2] = colliders->worldEndpoints[i][j].vz * 4;
            }
            CLOTH_SCRATCH->capsuleRadii[i] = colliders->radii[i] * 4;
        }
        CLOTH_SCRATCH->colliderCount = item->colliderInput->activeCount;
    }
}

static void PrepareStageClothStep(ClothItem *item, ClothModel *model)
{
    int sumSq;
    VECTOR leading;
    int driftScale;
    int lzcInput;

    CLOTH_SCRATCH->gravity = gStageClothGravity;
    gte_SetRotMatrix(item->transform);
    gte_SetTransMatrix(item->transform);
    CLOTH_SCRATCH->damping = model->damping;
    CLOTH_SCRATCH->pushLimit = ((CLOTH_FIGHTER_DAMPING - model->damping) << 19) + CLOTH_MAX_CAPSULE_PUSH;
    LoadClothCollidersToScratch(item);
    CLOTH_SCRATCH->rootDrift[0] = item->transform->t[0] - model->prevTransform.t[0];
    CLOTH_SCRATCH->rootDrift[1] = item->transform->t[1] - model->prevTransform.t[1];
    CLOTH_SCRATCH->rootDrift[2] = item->transform->t[2] - model->prevTransform.t[2];
    sumSq = CLOTH_SCRATCH->rootDrift[0] * CLOTH_SCRATCH->rootDrift[0]
          + CLOTH_SCRATCH->rootDrift[1] * CLOTH_SCRATCH->rootDrift[1]
          + CLOTH_SCRATCH->rootDrift[2] * CLOTH_SCRATCH->rootDrift[2];
    if (sumSq > CLOTH_DRIFT_MAX_DIST_SQ) {
        driftScale = 256;
    } else if (sumSq < 0) {
        driftScale = 0;
    } else {
        lzcInput = sumSq;
        if (sumSq < 1024) {
            sumSq = gSqrtLutQ3[sumSq] >> 3;
        } else {
            int shift;
            gte_ldlzc(lzcInput);
            gte_nop();
            gte_nop();
            gte_stlzc(&leading.vx);
            shift = leading.vx;
            shift = 22 - (shift & ~1);
            lzcInput = gSqrtLutQ3[sumSq >> shift];
            sumSq = (lzcInput << 16) >> (19 - (shift >> 1));
        }
        driftScale = sumSq * 64 / CLOTH_DRIFT_MAX_DIST + 192;
    }
    CLOTH_SCRATCH->rootDrift[0] = CLOTH_SCRATCH->rootDrift[0] * driftScale >> 1;
    CLOTH_SCRATCH->rootDrift[1] = CLOTH_SCRATCH->rootDrift[1] * driftScale >> 1;
    CLOTH_SCRATCH->rootDrift[2] = CLOTH_SCRATCH->rootDrift[2] * driftScale >> 1;
    model->prevTransform = *item->transform;
    SolveStageClothLinks(model);
}

void SolveStageClothLinks(ClothModel *model)
{
    ClothLink *link;
    ClothPoint *first;
    ClothPoint *second;
    int (*forceOut)[3];
    int i;
    int restLength;
    int maxDelta;
    int minDelta;
    int dx, dy, dz;
    u_int squared;
    VECTOR leading;
    int distance;

    forceOut = CLOTH_SCRATCH->linkForces;
    link = model->links;
    for (i = 0; i < model->linkCount;) {
        distance = link->endpoints.packed; /* reused, as in the original */
        first = &model->points[distance >> 16];
        second = &model->points[distance & 0xffff];
        dx = second->x - first->x;
        dy = second->y - first->y;
        dz = second->z - first->z;
        restLength = link->restLength;
        maxDelta = restLength * 3;
        minDelta = -maxDelta;
        if (dx < minDelta || maxDelta < dx || dy < minDelta || maxDelta < dy || dz < minDelta || maxDelta < dz) {
            dx = (second->x + first->x) / 2;
            dy = (second->y + first->y) / 2;
            dz = (second->z + first->z) / 2;
            if (second->massOrPin < 0) {
                second->x = dx;
                second->y = dy;
                second->z = dz;
                second->vx /= 4;
                second->vy /= 4;
                second->vz /= 4;
            } else {
                first->x = dx;
                first->y = dy;
                first->z = dz;
                first->vx /= 4;
                first->vy /= 4;
                first->vz /= 4;
            }
        } else {
            CLOTH_SCRATCH->diffStart.linkDelta[0] = dx >> 3;
            CLOTH_SCRATCH->diffStart.linkDelta[1] = dy >> 3;
            CLOTH_SCRATCH->diffStart.linkDelta[2] = dz >> 3;
            gte_ldlvl(CLOTH_SCRATCH->diffStart.linkDelta);
            gte_sqr0();
            gte_stlvnl(CLOTH_SCRATCH->diffEnd.linkSq);
            squared = CLOTH_SCRATCH->diffEnd.linkSq[0] + CLOTH_SCRATCH->diffEnd.linkSq[1] +
                CLOTH_SCRATCH->diffEnd.linkSq[2];
            if (squared < 1024) {
                distance = gSqrtLutQ3[squared];
                gte_ldlvl(CLOTH_SCRATCH->diffStart.linkDelta);
            } else {
                gte_ldlzc(squared);
                gte_ldlvl(CLOTH_SCRATCH->diffStart.linkDelta);
                gte_stlzc(&leading.vx);
                distance = leading.vx;
                distance = 22 - (distance & ~1);
                squared = gSqrtLutQ3[squared >> distance];
                distance = (squared << 16) >> (16 - (distance >> 1));
            }
            gte_lddp(((distance - restLength) << 15) / (link->stiffnessDiv * distance));
            gte_gpf12();
            gte_stlvl(forceOut);
            forceOut++;
            i++;
            link++;
        }
    }
}

/* 16x16 tiled radial wind gust profile (0..65535, symmetric around peak [8][8]). */
unsigned short gClothWindFieldLut[256] = {
        0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,   575,  1935,  3135,  3599,  3135,  1935,   575,     0,     0,     0,     0,
        0,     0,     0,   143,  2303,  5775,  9215, 11663, 12543, 11663,  9215,  5775,  2303,   143,     0,     0,
        0,     0,   143,  3135,  8463, 14399, 19599, 23103, 24335, 23103, 19599, 14399,  8463,  3135,   143,     0,
        0,     0,  2303,  8463, 16383, 24335, 30975, 35343, 36863, 35343, 30975, 24335, 16383,  8463,  2303,     0,
        0,   575,  5775, 14399, 24335, 33855, 41615, 46655, 48399, 46655, 41615, 33855, 24335, 14399,  5775,   575,
        0,  1935,  9215, 19599, 30975, 41615, 50175, 55695, 57599, 55695, 50175, 41615, 30975, 19599,  9215,  1935,
        0,  3135, 11663, 23103, 35343, 46655, 55695, 61503, 63503, 61503, 55695, 46655, 35343, 23103, 11663,  3135,
        0,  3599, 12543, 24335, 36863, 48399, 57599, 63503, 65535, 63503, 57599, 48399, 36863, 24335, 12543,  3599,
        0,  3135, 11663, 23103, 35343, 46655, 55695, 61503, 63503, 61503, 55695, 46655, 35343, 23103, 11663,  3135,
        0,  1935,  9215, 19599, 30975, 41615, 50175, 55695, 57599, 55695, 50175, 41615, 30975, 19599,  9215,  1935,
        0,   575,  5775, 14399, 24335, 33855, 41615, 46655, 48399, 46655, 41615, 33855, 24335, 14399,  5775,   575,
        0,     0,  2303,  8463, 16383, 24335, 30975, 35343, 36863, 35343, 30975, 24335, 16383,  8463,  2303,     0,
        0,     0,   143,  3135,  8463, 14399, 19599, 23103, 24335, 23103, 19599, 14399,  8463,  3135,   143,     0,
        0,     0,     0,   143,  2303,  5775,  9215, 11663, 12543, 11663,  9215,  5775,  2303,   143,     0,     0,
        0,     0,     0,     0,     0,   575,  1935,  3135,  3599,  3135,  1935,   575,     0,     0,     0,     0,
};

/* floor(sqrt(64 * i)): eight times the square root of i, for i < 1024. */
unsigned char gSqrtLutQ3[1024] = {
    0x00, 0x08, 0x0b, 0x0d, 0x10, 0x11, 0x13, 0x15, 0x16, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
    0x20, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x28, 0x29, 0x2a, 0x2b, 0x2b, 0x2c,
    0x2d, 0x2d, 0x2e, 0x2f, 0x30, 0x30, 0x31, 0x31, 0x32, 0x33, 0x33, 0x34, 0x35, 0x35, 0x36, 0x36,
    0x37, 0x38, 0x38, 0x39, 0x39, 0x3a, 0x3a, 0x3b, 0x3b, 0x3c, 0x3c, 0x3d, 0x3d, 0x3e, 0x3e, 0x3f,
    0x40, 0x40, 0x40, 0x41, 0x41, 0x42, 0x42, 0x43, 0x43, 0x44, 0x44, 0x45, 0x45, 0x46, 0x46, 0x47,
    0x47, 0x48, 0x48, 0x48, 0x49, 0x49, 0x4a, 0x4a, 0x4b, 0x4b, 0x4b, 0x4c, 0x4c, 0x4d, 0x4d, 0x4d,
    0x4e, 0x4e, 0x4f, 0x4f, 0x50, 0x50, 0x50, 0x51, 0x51, 0x51, 0x52, 0x52, 0x53, 0x53, 0x53, 0x54,
    0x54, 0x55, 0x55, 0x55, 0x56, 0x56, 0x56, 0x57, 0x57, 0x58, 0x58, 0x58, 0x59, 0x59, 0x59, 0x5a,
    0x5a, 0x5a, 0x5b, 0x5b, 0x5b, 0x5c, 0x5c, 0x5c, 0x5d, 0x5d, 0x5d, 0x5e, 0x5e, 0x5e, 0x5f, 0x5f,
    0x60, 0x60, 0x60, 0x60, 0x61, 0x61, 0x61, 0x62, 0x62, 0x62, 0x63, 0x63, 0x63, 0x64, 0x64, 0x64,
    0x65, 0x65, 0x65, 0x66, 0x66, 0x66, 0x67, 0x67, 0x67, 0x68, 0x68, 0x68, 0x68, 0x69, 0x69, 0x69,
    0x6a, 0x6a, 0x6a, 0x6b, 0x6b, 0x6b, 0x6b, 0x6c, 0x6c, 0x6c, 0x6d, 0x6d, 0x6d, 0x6d, 0x6e, 0x6e,
    0x6e, 0x6f, 0x6f, 0x6f, 0x70, 0x70, 0x70, 0x70, 0x71, 0x71, 0x71, 0x71, 0x72, 0x72, 0x72, 0x73,
    0x73, 0x73, 0x73, 0x74, 0x74, 0x74, 0x75, 0x75, 0x75, 0x75, 0x76, 0x76, 0x76, 0x76, 0x77, 0x77,
    0x77, 0x78, 0x78, 0x78, 0x78, 0x79, 0x79, 0x79, 0x79, 0x7a, 0x7a, 0x7a, 0x7a, 0x7b, 0x7b, 0x7b,
    0x7b, 0x7c, 0x7c, 0x7c, 0x7c, 0x7d, 0x7d, 0x7d, 0x7d, 0x7e, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f, 0x7f,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x81, 0x81, 0x81, 0x82, 0x82, 0x82, 0x82, 0x83, 0x83, 0x83,
    0x83, 0x84, 0x84, 0x84, 0x84, 0x85, 0x85, 0x85, 0x85, 0x86, 0x86, 0x86, 0x86, 0x87, 0x87, 0x87,
    0x87, 0x88, 0x88, 0x88, 0x88, 0x88, 0x89, 0x89, 0x89, 0x89, 0x8a, 0x8a, 0x8a, 0x8a, 0x8b, 0x8b,
    0x8b, 0x8b, 0x8b, 0x8c, 0x8c, 0x8c, 0x8c, 0x8d, 0x8d, 0x8d, 0x8d, 0x8d, 0x8e, 0x8e, 0x8e, 0x8e,
    0x8f, 0x8f, 0x8f, 0x8f, 0x90, 0x90, 0x90, 0x90, 0x90, 0x91, 0x91, 0x91, 0x91, 0x91, 0x92, 0x92,
    0x92, 0x92, 0x93, 0x93, 0x93, 0x93, 0x93, 0x94, 0x94, 0x94, 0x94, 0x95, 0x95, 0x95, 0x95, 0x95,
    0x96, 0x96, 0x96, 0x96, 0x96, 0x97, 0x97, 0x97, 0x97, 0x98, 0x98, 0x98, 0x98, 0x98, 0x99, 0x99,
    0x99, 0x99, 0x99, 0x9a, 0x9a, 0x9a, 0x9a, 0x9a, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b, 0x9c, 0x9c, 0x9c,
    0x9c, 0x9c, 0x9d, 0x9d, 0x9d, 0x9d, 0x9d, 0x9e, 0x9e, 0x9e, 0x9e, 0x9e, 0x9f, 0x9f, 0x9f, 0x9f,
    0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa2, 0xa2, 0xa2, 0xa2, 0xa2,
    0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa6,
    0xa6, 0xa6, 0xa6, 0xa6, 0xa7, 0xa7, 0xa7, 0xa7, 0xa7, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa9,
    0xa9, 0xa9, 0xa9, 0xa9, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xac,
    0xac, 0xac, 0xac, 0xac, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xae, 0xae, 0xae, 0xae, 0xae, 0xaf,
    0xaf, 0xaf, 0xaf, 0xaf, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb0, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1, 0xb1,
    0xb2, 0xb2, 0xb2, 0xb2, 0xb2, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb3, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4,
    0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb7, 0xb7, 0xb7, 0xb7,
    0xb7, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb8, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9, 0xba, 0xba, 0xba,
    0xba, 0xba, 0xba, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbd,
    0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc2, 0xc2, 0xc2,
    0xc2, 0xc2, 0xc2, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc4, 0xc4, 0xc4, 0xc4, 0xc4, 0xc4, 0xc5,
    0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc7, 0xc7, 0xc7, 0xc7, 0xc7,
    0xc7, 0xc8, 0xc8, 0xc8, 0xc8, 0xc8, 0xc8, 0xc8, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xc9, 0xca, 0xca,
    0xca, 0xca, 0xca, 0xca, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xcd, 0xce, 0xce, 0xce, 0xce, 0xce, 0xce, 0xcf, 0xcf,
    0xcf, 0xcf, 0xcf, 0xcf, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd1, 0xd1, 0xd1, 0xd1, 0xd1,
    0xd1, 0xd1, 0xd2, 0xd2, 0xd2, 0xd2, 0xd2, 0xd2, 0xd3, 0xd3, 0xd3, 0xd3, 0xd3, 0xd3, 0xd3, 0xd4,
    0xd4, 0xd4, 0xd4, 0xd4, 0xd4, 0xd5, 0xd5, 0xd5, 0xd5, 0xd5, 0xd5, 0xd5, 0xd6, 0xd6, 0xd6, 0xd6,
    0xd6, 0xd6, 0xd6, 0xd7, 0xd7, 0xd7, 0xd7, 0xd7, 0xd7, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8,
    0xd9, 0xd9, 0xd9, 0xd9, 0xd9, 0xd9, 0xd9, 0xda, 0xda, 0xda, 0xda, 0xda, 0xda, 0xda, 0xdb, 0xdb,
    0xdb, 0xdb, 0xdb, 0xdb, 0xdb, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdd, 0xdd, 0xdd, 0xdd,
    0xdd, 0xdd, 0xdd, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf,
    0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe1, 0xe1, 0xe1, 0xe1, 0xe1, 0xe1, 0xe1, 0xe2,
    0xe2, 0xe2, 0xe2, 0xe2, 0xe2, 0xe2, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xe4, 0xe4, 0xe4,
    0xe4, 0xe4, 0xe4, 0xe4, 0xe5, 0xe5, 0xe5, 0xe5, 0xe5, 0xe5, 0xe5, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6,
    0xe6, 0xe6, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0xe8, 0xe9, 0xe9, 0xe9, 0xe9, 0xe9, 0xe9, 0xe9, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xea, 0xeb,
    0xeb, 0xeb, 0xeb, 0xeb, 0xeb, 0xeb, 0xeb, 0xec, 0xec, 0xec, 0xec, 0xec, 0xec, 0xec, 0xed, 0xed,
    0xed, 0xed, 0xed, 0xed, 0xed, 0xed, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xef, 0xef, 0xef,
    0xef, 0xef, 0xef, 0xef, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf1, 0xf1, 0xf1, 0xf1,
    0xf1, 0xf1, 0xf1, 0xf1, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf3, 0xf3, 0xf3, 0xf3, 0xf3,
    0xf3, 0xf3, 0xf3, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf5, 0xf5, 0xf5, 0xf5, 0xf5, 0xf5,
    0xf5, 0xf5, 0xf6, 0xf6, 0xf6, 0xf6, 0xf6, 0xf6, 0xf6, 0xf6, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7,
    0xf7, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9,
    0xf9, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb,
    0xfb, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd,
    0xfd, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

void StepStageClothModel(ClothItem *item, ClothModel *model)
{
    ClothPoint *point;
    int *linkForce;
    int x;
    int y;
    int z;
    VECTOR leading;
    unsigned char unused[8]; /* not used */
    int i;
    int k;
    int windU;
    int gust;
    int fx;
    int fy;
    int fz;
    int vx;
    int vy;
    int vz;
    int radius;
    int dist;
    int sqDist;
    int lzcInput;
    int distStart;
    int distEnd;

    PrepareStageClothStep(item, model);

    point = model->points;
    for (i = 0; i < model->pointCount; i++, point++) {
        x = point->x;
        CLOTH_SCRATCH->pointPos[0] = x;
        y = point->y;
        CLOTH_SCRATCH->pointPos[1] = y;
        z = point->z;
        CLOTH_SCRATCH->pointPos[2] = z;
        if (point->massOrPin == 0) {
            CLOTH_SCRATCH->windForceX = CLOTH_SCRATCH->windForceZ = 0;
        } else {
            windU = (x + y) >> 3;
            y = (z + y) >> 3;
            CLOTH_SCRATCH->workVector[0] = (CLOTH_SCRATCH->windSin * windU + CLOTH_SCRATCH->windCos * y) >> 12;
            CLOTH_SCRATCH->workVector[2] = (-CLOTH_SCRATCH->windCos * windU + CLOTH_SCRATCH->windSin * y) >> 12;
            windU = ((CLOTH_SCRATCH->workVector[0] - CLOTH_SCRATCH->windGustColumnOrigin) >> 7) & 0xf;
            y = (CLOTH_SCRATCH->workVector[2] - CLOTH_SCRATCH->windGustRowOrigin) >> 3;
            gust = gClothWindFieldLut[(y & 0xf0) + windU];
            x = CLOTH_SCRATCH->windForceCos * gust;
            z = CLOTH_SCRATCH->windForceSin * gust;
            CLOTH_SCRATCH->windForceX = x >> 14;
            CLOTH_SCRATCH->windForceZ = z >> 14;
            if (point->massOrPin < 0) {
                CLOTH_SCRATCH->windForceX = x >> 16;
                CLOTH_SCRATCH->windForceZ = z >> 16;
            }
        }
        if (point->massOrPin >= 0) {
            gte_ldv0(&model->sourceVertices[i]);
            gte_rt();
            gte_stlvnl(CLOTH_SCRATCH->forceOrTarget);
            if (point->massOrPin > 0) {
                CLOTH_SCRATCH->pointPos[0] += CLOTH_SCRATCH->rootDrift[0];
                CLOTH_SCRATCH->pointPos[1] += CLOTH_SCRATCH->rootDrift[1];
                CLOTH_SCRATCH->pointPos[2] += CLOTH_SCRATCH->rootDrift[2];
                vx = point->vx / 2 + CLOTH_SCRATCH->windForceX +
                    (((CLOTH_SCRATCH->forceOrTarget[0] << 7) - CLOTH_SCRATCH->pointPos[0]) << 2) / point->massOrPin;
                vy = point->vy / 2 +
                    (((CLOTH_SCRATCH->forceOrTarget[1] << 7) - CLOTH_SCRATCH->pointPos[1]) << 2) / point->massOrPin +
                    point->massOrPin * CLOTH_SCRATCH->gravity / 16;
                vz = point->vz / 2 + CLOTH_SCRATCH->windForceZ +
                    (((CLOTH_SCRATCH->forceOrTarget[2] << 7) - CLOTH_SCRATCH->pointPos[2]) << 2) / point->massOrPin;
                point->vx = vx;
                point->vy = vy;
                point->vz = vz;
                point->x = CLOTH_SCRATCH->pointPos[0] + vx;
                point->y = CLOTH_SCRATCH->pointPos[1] + vy;
                point->z = CLOTH_SCRATCH->pointPos[2] + vz;
            } else {
                point->x = CLOTH_SCRATCH->forceOrTarget[0] << 7;
                point->y = CLOTH_SCRATCH->forceOrTarget[1] << 7;
                point->z = CLOTH_SCRATCH->forceOrTarget[2] << 7;
            }
        } else {
            if (point->massOrPin >= CLOTH_TETHER_MIN) {
                gte_ldv0(&model->sourceVertices[i]);
                gte_rt();
                gte_stlvnl(CLOTH_SCRATCH->forceOrTarget);
                CLOTH_SCRATCH->pointPos[0] +=
                    (((CLOTH_SCRATCH->forceOrTarget[0] << 7) - CLOTH_SCRATCH->pointPos[0]) << 2) /
                    (1 - point->massOrPin);
                CLOTH_SCRATCH->pointPos[1] +=
                    (((CLOTH_SCRATCH->forceOrTarget[1] << 7) - CLOTH_SCRATCH->pointPos[1]) << 2) /
                    (1 - point->massOrPin);
                CLOTH_SCRATCH->pointPos[2] +=
                    (((CLOTH_SCRATCH->forceOrTarget[2] << 7) - CLOTH_SCRATCH->pointPos[2]) << 2) /
                    (1 - point->massOrPin);
            }
            fx = point->vx + CLOTH_SCRATCH->windForceX;
            fy = point->vy;
            fz = point->vz + CLOTH_SCRATCH->windForceZ;
            for (k = 0; k < point->outLinkCount; k++) {
                linkForce = CLOTH_SCRATCH->linkForces[point->outLinks[k]];
                fx += linkForce[0];
                fy += linkForce[1];
                fz += linkForce[2];
            }
            for (k = 0; k < point->inLinkCount; k++) {
                linkForce = CLOTH_SCRATCH->linkForces[point->inLinks[k]];
                fx -= linkForce[0];
                fy -= linkForce[1];
                fz -= linkForce[2];
            }
            CLOTH_SCRATCH->forceOrTarget[0] = fx;
            CLOTH_SCRATCH->forceOrTarget[1] = fy;
            CLOTH_SCRATCH->forceOrTarget[2] = fz;
            CLOTH_SCRATCH->coarsePos[0] = CLOTH_SCRATCH->pointPos[0] >> 5;
            CLOTH_SCRATCH->coarsePos[1] = CLOTH_SCRATCH->pointPos[1] >> 5;
            CLOTH_SCRATCH->coarsePos[2] = CLOTH_SCRATCH->pointPos[2] >> 5;
            if (item->colliderInput != 0) {
                dist = CLOTH_SCRATCH->pointPos[1] - CLOTH_SCRATCH->floorY;
                if (dist > 0) {
                    if (dist > CLOTH_FLOOR_MAX_DEPTH) {
                        CLOTH_SCRATCH->forceOrTarget[1] = fy - CLOTH_FLOOR_MAX_PUSH;
                    } else {
                        CLOTH_SCRATCH->forceOrTarget[1] = fy - dist / 8;
                    }
                }
                for (k = 0; k < item->colliderInput->activeCount; k++) {
                    radius = CLOTH_SCRATCH->capsuleRadii[k];
                    dist = CLOTH_SCRATCH->coarsePos[1] - CLOTH_SCRATCH->capsuleEndpoints[k][0][1];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffStart.capsuleDiffStart[1] = dist;
                    dist = CLOTH_SCRATCH->coarsePos[0] - CLOTH_SCRATCH->capsuleEndpoints[k][0][0];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffStart.capsuleDiffStart[0] = dist;
                    dist = CLOTH_SCRATCH->coarsePos[2] - CLOTH_SCRATCH->capsuleEndpoints[k][0][2];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffStart.capsuleDiffStart[2] = dist;
                    gte_ldlvl(CLOTH_SCRATCH->diffStart.capsuleDiffStart);
                    gte_sqr0();
                    gte_stlvnl(CLOTH_SCRATCH->workVector);
                    sqDist = CLOTH_SCRATCH->workVector[0] + CLOTH_SCRATCH->workVector[1] + CLOTH_SCRATCH->workVector[2];
                    lzcInput = sqDist;
                    if (sqDist < 1024) {
                        distStart = gSqrtLutQ3[sqDist] >> 3;
                        if (distStart >= radius) continue;
                    } else {
                        int shift;
                        gte_ldlzc(lzcInput);
                        gte_nop();
                        gte_nop();
                        gte_stlzc(&leading.vx);
                        shift = leading.vx;
                        shift = 22 - (shift & ~1);
                        lzcInput = gSqrtLutQ3[sqDist >> shift];
                        distStart = (lzcInput << 16) >> (19 - (shift >> 1));
                        if (distStart >= radius) continue;
                    }
                    dist = CLOTH_SCRATCH->coarsePos[1] - CLOTH_SCRATCH->capsuleEndpoints[k][1][1];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffEnd.capsuleDiffEnd[1] = dist;
                    dist = CLOTH_SCRATCH->coarsePos[0] - CLOTH_SCRATCH->capsuleEndpoints[k][1][0];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffEnd.capsuleDiffEnd[0] = dist;
                    dist = CLOTH_SCRATCH->coarsePos[2] - CLOTH_SCRATCH->capsuleEndpoints[k][1][2];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffEnd.capsuleDiffEnd[2] = dist;
                    gte_ldlvl(CLOTH_SCRATCH->diffEnd.capsuleDiffEnd);
                    gte_sqr0();
                    gte_stlvnl(CLOTH_SCRATCH->workVector);
                    distEnd = CLOTH_SCRATCH->workVector[0] + CLOTH_SCRATCH->workVector[1] +
                        CLOTH_SCRATCH->workVector[2];
                    if (distEnd < 1024) {
                        distEnd = gSqrtLutQ3[distEnd] >> 3;
                    } else {
                        int shift;
                        gte_ldlzc(distEnd);
                        gte_nop();
                        gte_nop();
                        gte_stlzc(&leading.vy);
                        shift = leading.vy;
                        shift = 22 - (shift & ~1);
                        distEnd = gSqrtLutQ3[distEnd >> shift];
                        distEnd = (distEnd << 16) >> (19 - (shift >> 1));
                    }
                    dist = distStart + distEnd;
                    if (dist >= radius) continue;
                    gte_lddp(1);
                    gte_ldlvl(CLOTH_SCRATCH->forceOrTarget);
                    gte_gpf0();
                    sqDist = (radius - dist) << 17;
                    if (CLOTH_SCRATCH->pushLimit < sqDist) {
                        sqDist = CLOTH_SCRATCH->pushLimit;
                    }
                    if (distStart != 0) {
                        distStart = sqDist / distStart;
                    }
                    gte_ldlvl(CLOTH_SCRATCH->diffStart.capsuleDiffStart);
                    gte_lddp(distStart);
                    gte_gpl12();
                    if (distEnd != 0) {
                        distEnd = sqDist / distEnd;
                    }
                    gte_ldlvl(CLOTH_SCRATCH->diffEnd.capsuleDiffEnd);
                    gte_lddp(distEnd);
                    gte_gpl12();
                    gte_stlvl(CLOTH_SCRATCH->forceOrTarget);
                }
            }
            point->vx = CLOTH_SCRATCH->forceOrTarget[0] * CLOTH_SCRATCH->damping >> 12;
            point->vy = (CLOTH_SCRATCH->forceOrTarget[1] * CLOTH_SCRATCH->damping >> 12) + CLOTH_SCRATCH->gravity;
            point->vz = CLOTH_SCRATCH->forceOrTarget[2] * CLOTH_SCRATCH->damping >> 12;
            point->x = CLOTH_SCRATCH->pointPos[0] + CLOTH_SCRATCH->rootDrift[0] + point->vx;
            point->y = CLOTH_SCRATCH->pointPos[1] + CLOTH_SCRATCH->rootDrift[1] + point->vy;
            point->z = CLOTH_SCRATCH->pointPos[2] + CLOTH_SCRATCH->rootDrift[2] + point->vz;
        }
    }
}

void UpdateClothModelsWithOutput(ClothItem *items, int count)
{
    int dispatchIndex;
    int copyIndex;

    for (dispatchIndex = 0; dispatchIndex < count; ++dispatchIndex) {
        if (items[dispatchIndex].flags & CLOTH_ITEM_FLAG_RESET) {
            ResetClothModelPositions(&items[dispatchIndex], &gClothModels[items[dispatchIndex].modelIndex]);
        } else {
            LoadClothWindToScratch(items[dispatchIndex].windSlot);
            StepStageClothModel(&items[dispatchIndex], &gClothModels[items[dispatchIndex].modelIndex]);
        }
    }

    for (copyIndex = 0; copyIndex < count; ++copyIndex) {
        ClothModel *model = &gClothModels[items[copyIndex].modelIndex];
        int point;

        if (items[copyIndex].outputPositions != 0) {
            for (point = 0; point < model->pointCount; ++point) {
                items[copyIndex].outputPositions[point].vx = model->points[point].x >> 7;
                items[copyIndex].outputPositions[point].vy = model->points[point].y >> 7;
                items[copyIndex].outputPositions[point].vz = model->points[point].z >> 7;
            }
        }
    }
}

void ResetClothModelPositions(ClothItem *item, ClothModel *model)
{
    VECTOR out;
    ClothPoint *point;
    int i;

    gte_SetRotMatrix(item->transform);
    gte_SetTransMatrix(item->transform);
    point = model->points;
    for (i = 0; i < model->pointCount; i++) {
        gte_ldv0(&model->sourceVertices[i]);
        gte_rt();
        gte_stlvnl(&out);
        point->x = out.vx << 7;
        point->y = out.vy << 7;
        point->z = out.vz << 7;
        point->vx = 0;
        point->vy = 0;
        point->vz = 0;
        point++;
    }
    model->prevTransform = *item->transform;
}

static void PrepareFighterClothStep(ClothItem *item, ClothModel *model)
{
    int sumSq;
    VECTOR leading;
    int driftScale;
    int lzcInput;

    gte_SetRotMatrix(item->transform);
    gte_SetTransMatrix(item->transform);
    LoadClothCollidersToScratch(item);
    CLOTH_SCRATCH->rootDrift[0] = item->transform->t[0] - model->prevTransform.t[0];
    CLOTH_SCRATCH->rootDrift[1] = item->transform->t[1] - model->prevTransform.t[1];
    CLOTH_SCRATCH->rootDrift[2] = item->transform->t[2] - model->prevTransform.t[2];
    sumSq = CLOTH_SCRATCH->rootDrift[0] * CLOTH_SCRATCH->rootDrift[0]
          + CLOTH_SCRATCH->rootDrift[1] * CLOTH_SCRATCH->rootDrift[1]
          + CLOTH_SCRATCH->rootDrift[2] * CLOTH_SCRATCH->rootDrift[2];
    if (sumSq > CLOTH_DRIFT_MAX_DIST_SQ) {
        driftScale = 256;
    } else if (sumSq < 0) {
        driftScale = 0;
    } else {
        lzcInput = sumSq;
        if (sumSq < 1024) {
            sumSq = gSqrtLutQ3[sumSq] >> 3;
        } else {
            int shift;
            gte_ldlzc(lzcInput);
            gte_nop();
            gte_nop();
            gte_stlzc(&leading.vx);
            shift = leading.vx;
            shift = 22 - (shift & ~1);
            lzcInput = gSqrtLutQ3[sumSq >> shift];
            sumSq = (lzcInput << 16) >> (19 - (shift >> 1));
        }
        driftScale = sumSq * 64 / CLOTH_DRIFT_MAX_DIST + 192;
    }
    CLOTH_SCRATCH->rootDrift[0] = CLOTH_SCRATCH->rootDrift[0] * driftScale >> 1;
    CLOTH_SCRATCH->rootDrift[1] = CLOTH_SCRATCH->rootDrift[1] * driftScale >> 1;
    CLOTH_SCRATCH->rootDrift[2] = CLOTH_SCRATCH->rootDrift[2] * driftScale >> 1;
    model->prevTransform = *item->transform;
    SolveFighterClothLinks(model);
}

void SolveFighterClothLinks(ClothModel *model)
{
    ClothLink *link;
    ClothPoint *points;
    ClothPoint *first;
    ClothPoint *second;
    int (*forceOut)[3];
    int remaining;
    int restLength;
    int maxDelta;
    int minDelta;
    int dx, dy, dz;
    u_int squared;
    VECTOR leading;
    unsigned char unused[8]; /* not used */
    int distance;

    forceOut = CLOTH_SCRATCH->linkForces;
    remaining = model->linkCount - 1;
    link = model->links;
    points = model->points;
    maxDelta = link->endpoints.packed; /* reused, as in the original */
    restLength = link->restLength;
    first = &points[maxDelta >> 16];
    second = &points[maxDelta & 0xffff];
    maxDelta = restLength * 3;
    minDelta = -maxDelta;
    dx = second->x - first->x;
    dy = second->y - first->y;
    dz = second->z - first->z;
    while (remaining > 0) {
        while (dx < minDelta || maxDelta < dx || dy < minDelta || maxDelta < dy || dz < minDelta || maxDelta < dz) {
            int midX = (second->x + first->x) / 2;
            int midY = (second->y + first->y) / 2;
            int midZ = (second->z + first->z) / 2;
            if (second->massOrPin < 0) {
                second->x = midX;
                second->y = midY;
                second->z = midZ;
                second->vx /= 4;
                second->vy /= 4;
                second->vz /= 4;
            } else {
                first->x = midX;
                first->y = midY;
                first->z = midZ;
                first->vx /= 4;
                first->vy /= 4;
                first->vz /= 4;
            }
            dx = second->x - first->x;
            dy = second->y - first->y;
            dz = second->z - first->z;
        }
        CLOTH_SCRATCH->diffStart.linkDelta[0] = dx >> 3;
        CLOTH_SCRATCH->diffStart.linkDelta[1] = dy >> 3;
        CLOTH_SCRATCH->diffStart.linkDelta[2] = dz >> 3;
        gte_ldlvl(CLOTH_SCRATCH->diffStart.linkDelta);
        gte_sqr0();
        link++;
        remaining--;
        maxDelta = link->endpoints.packed; /* reused, as in the original */
        gte_stlvnl(CLOTH_SCRATCH->diffEnd.linkSq);
        squared = CLOTH_SCRATCH->diffEnd.linkSq[0] + CLOTH_SCRATCH->diffEnd.linkSq[1] +
            CLOTH_SCRATCH->diffEnd.linkSq[2];
        if (squared < 1024) {
            distance = gSqrtLutQ3[squared];
            first = &points[maxDelta >> 16];
            second = &points[maxDelta & 0xffff];
        } else {
            gte_ldlzc(squared);
            first = &points[maxDelta >> 16];
            second = &points[maxDelta & 0xffff];
            gte_stlzc(&leading.vx);
            distance = leading.vx;
            distance = 22 - (distance & ~1);
            squared = gSqrtLutQ3[squared >> distance];
            distance = (squared << 16) >> (16 - (distance >> 1));
        }
        gte_ldlvl(CLOTH_SCRATCH->diffStart.linkDelta);
        distance = ((distance - restLength) << 14) / distance;
        restLength = link->restLength;
        maxDelta = restLength * 3;
        minDelta = -maxDelta;
        gte_lddp(distance);
        gte_gpf12();
        dx = second->x - first->x;
        dy = second->y - first->y;
        dz = second->z - first->z;
        gte_stlvl(forceOut);
        forceOut++;
    }
    CLOTH_SCRATCH->diffStart.linkDelta[0] = dx >> 3;
    CLOTH_SCRATCH->diffStart.linkDelta[1] = dy >> 3;
    CLOTH_SCRATCH->diffStart.linkDelta[2] = dz >> 3;
    gte_ldlvl(CLOTH_SCRATCH->diffStart.linkDelta);
    gte_sqr0();
    gte_stlvnl(CLOTH_SCRATCH->diffEnd.linkSq);
    squared = CLOTH_SCRATCH->diffEnd.linkSq[0] + CLOTH_SCRATCH->diffEnd.linkSq[1] + CLOTH_SCRATCH->diffEnd.linkSq[2];
    if (squared < 1024) {
        distance = gSqrtLutQ3[squared];
    } else {
        gte_ldlzc(squared);
        gte_nop();
        gte_nop();
        gte_stlzc(&leading.vx);
        distance = leading.vx;
        distance = 22 - (distance & ~1);
        squared = gSqrtLutQ3[squared >> distance];
        distance = (squared << 16) >> (16 - (distance >> 1));
    }
    gte_ldlvl(CLOTH_SCRATCH->diffStart.linkDelta);
    distance = ((distance - restLength) << 14) / distance;
    gte_lddp(distance);
    gte_gpf12();
    gte_stlvl(forceOut);
}

void StepFighterClothModel(ClothItem *item, ClothModel *model)
{
    ClothPoint *point;
    int *linkForce;
    int x;
    int y;
    int z;
    VECTOR leading;
    unsigned char unused[8]; /* not used */
    int count;
    int i;
    int k;
    int gust;
    int lzcInput;
    int windCol;
    int scaledGust;
    int packedLinks;
    int linkCount;
    int fx;
    int fy;
    int fz;
    int radius;
    int dist;
    int distStart;
    int distEnd;

    PrepareFighterClothStep(item, model);
    count = model->pointCount;
    point = model->points;
    for (i = 0; i < count; i++, point++) {
        x = point->x;
        CLOTH_SCRATCH->pointPos[0] = x;
        y = point->y;
        CLOTH_SCRATCH->pointPos[1] = y;
        z = point->z;
        CLOTH_SCRATCH->pointPos[2] = z;
        if (point->massOrPin == 0) {
            CLOTH_SCRATCH->windForceX = CLOTH_SCRATCH->windForceZ = 0;
        } else {
            windCol = (((x >> 3) - CLOTH_SCRATCH->windGustColumnOrigin) >> 9) & 0xf;
            gust = gClothWindFieldLut[((((y >> 3) - CLOTH_SCRATCH->windGustRowOrigin) >> 5) & 0xf0) + windCol];
            gust = gust * point->windWeight;
            scaledGust = gust >> 8;
            if (point->massOrPin < 0) {
                scaledGust = gust >> 10;
            }
            CLOTH_SCRATCH->windForceX = CLOTH_SCRATCH->windForceCos * scaledGust >> 14;
            CLOTH_SCRATCH->windForceZ = CLOTH_SCRATCH->windForceSin * scaledGust >> 14;
        }
        if (point->massOrPin >= 0) {
            gte_ldv0(&model->sourceVertices[i]);
            gte_rt();
            gte_stlvnl(CLOTH_SCRATCH->forceOrTarget);
            if (point->massOrPin > 0) {
                CLOTH_SCRATCH->pointPos[0] += CLOTH_SCRATCH->rootDrift[0];
                point->vx = point->vx / 2 + CLOTH_SCRATCH->windForceX +
                    (((CLOTH_SCRATCH->forceOrTarget[0] << 7) - CLOTH_SCRATCH->pointPos[0]) * point->massOrPin >> 8);
                point->x = CLOTH_SCRATCH->pointPos[0] + point->vx;
                CLOTH_SCRATCH->pointPos[1] += CLOTH_SCRATCH->rootDrift[1];
                point->vy = point->vy / 2 +
                    (((CLOTH_SCRATCH->forceOrTarget[1] << 7) - CLOTH_SCRATCH->pointPos[1]) * point->massOrPin +
                     (256 - point->massOrPin) * 25 >> 8);
                point->y = CLOTH_SCRATCH->pointPos[1] + point->vy;
                CLOTH_SCRATCH->pointPos[2] += CLOTH_SCRATCH->rootDrift[2];
                point->vz = point->vz / 2 + CLOTH_SCRATCH->windForceZ +
                    (((CLOTH_SCRATCH->forceOrTarget[2] << 7) - CLOTH_SCRATCH->pointPos[2]) * point->massOrPin >> 8);
                point->z = CLOTH_SCRATCH->pointPos[2] + point->vz;
            } else {
                point->x = CLOTH_SCRATCH->forceOrTarget[0] << 7;
                point->y = CLOTH_SCRATCH->forceOrTarget[1] << 7;
                point->z = CLOTH_SCRATCH->forceOrTarget[2] << 7;
            }
        } else {
            if (point->massOrPin >= CLOTH_TETHER_MIN) {
                gte_ldv0(&model->sourceVertices[i]);
                gte_rt();
                gte_stlvnl(CLOTH_SCRATCH->forceOrTarget);
                CLOTH_SCRATCH->pointPos[0] -=
                    ((CLOTH_SCRATCH->forceOrTarget[0] << 7) - CLOTH_SCRATCH->pointPos[0]) * point->massOrPin >> 8;
                CLOTH_SCRATCH->pointPos[1] -=
                    ((CLOTH_SCRATCH->forceOrTarget[1] << 7) - CLOTH_SCRATCH->pointPos[1]) * point->massOrPin >> 8;
                CLOTH_SCRATCH->pointPos[2] -=
                    ((CLOTH_SCRATCH->forceOrTarget[2] << 7) - CLOTH_SCRATCH->pointPos[2]) * point->massOrPin >> 8;
            }
            fx = point->vx + CLOTH_SCRATCH->windForceX;
            fy = point->vy;
            fz = point->vz + CLOTH_SCRATCH->windForceZ;
            packedLinks = *(int *)&point->outLinks[0];
            linkCount = point->outLinkCount;
            for (k = 0; k < linkCount; k++) {
                linkForce = CLOTH_SCRATCH->linkForces[packedLinks & 0xff];
                fx += linkForce[0];
                fy += linkForce[1];
                fz += linkForce[2];
                if (k == 3) {
                    packedLinks = *(int *)&point->outLinks[4];
                } else {
                    packedLinks >>= 8;
                }
            }
            packedLinks = *(int *)&point->inLinks[0];
            linkCount = point->inLinkCount;
            for (k = 0; k < linkCount; k++) {
                linkForce = CLOTH_SCRATCH->linkForces[packedLinks & 0xff];
                fx -= linkForce[0];
                fy -= linkForce[1];
                fz -= linkForce[2];
                if (k == 3) {
                    packedLinks = *(int *)&point->inLinks[4];
                } else {
                    packedLinks >>= 8;
                }
            }
            CLOTH_SCRATCH->forceOrTarget[0] = fx;
            CLOTH_SCRATCH->forceOrTarget[1] = fy;
            CLOTH_SCRATCH->forceOrTarget[2] = fz;
            if (item->colliderInput != 0) {
                dist = CLOTH_SCRATCH->pointPos[1] - CLOTH_SCRATCH->floorY;
                if (dist > 0) {
                    if (dist > CLOTH_FLOOR_MAX_DEPTH) {
                        CLOTH_SCRATCH->forceOrTarget[1] = fy - CLOTH_FLOOR_MAX_PUSH;
                    } else {
                        CLOTH_SCRATCH->forceOrTarget[1] = fy - dist / 8;
                    }
                }
                CLOTH_SCRATCH->coarsePos[0] = CLOTH_SCRATCH->pointPos[0] >> 5;
                CLOTH_SCRATCH->coarsePos[1] = CLOTH_SCRATCH->pointPos[1] >> 5;
                CLOTH_SCRATCH->coarsePos[2] = CLOTH_SCRATCH->pointPos[2] >> 5;
                for (k = 0; k < CLOTH_SCRATCH->colliderCount; k++) {
                    radius = CLOTH_SCRATCH->capsuleRadii[k];
                    dist = CLOTH_SCRATCH->coarsePos[1] - CLOTH_SCRATCH->capsuleEndpoints[k][0][1];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffStart.capsuleDiffStart[1] = dist;
                    dist = CLOTH_SCRATCH->coarsePos[0] - CLOTH_SCRATCH->capsuleEndpoints[k][0][0];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffStart.capsuleDiffStart[0] = dist;
                    dist = CLOTH_SCRATCH->coarsePos[2] - CLOTH_SCRATCH->capsuleEndpoints[k][0][2];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffStart.capsuleDiffStart[2] = dist;
                    gte_ldlvl(CLOTH_SCRATCH->diffStart.capsuleDiffStart);
                    gte_sqr0();
                    gte_stlvnl(CLOTH_SCRATCH->workVector);
                    distStart = CLOTH_SCRATCH->workVector[0] + CLOTH_SCRATCH->workVector[1] +
                        CLOTH_SCRATCH->workVector[2];
                    lzcInput = distStart;
                    if (distStart < 1024) {
                        distStart = gSqrtLutQ3[distStart] >> 3;
                        if (distStart >= radius) continue;
                    } else {
                        int shift;
                        gte_ldlzc(lzcInput);
                        gte_nop();
                        gte_nop();
                        gte_stlzc(&leading.vx);
                        shift = leading.vx;
                        shift = 22 - (shift & ~1);
                        lzcInput = gSqrtLutQ3[distStart >> shift];
                        distStart = (lzcInput << 16) >> (19 - (shift >> 1));
                        if (distStart >= radius) continue;
                    }
                    dist = CLOTH_SCRATCH->coarsePos[1] - CLOTH_SCRATCH->capsuleEndpoints[k][1][1];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffEnd.capsuleDiffEnd[1] = dist;
                    dist = CLOTH_SCRATCH->coarsePos[0] - CLOTH_SCRATCH->capsuleEndpoints[k][1][0];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffEnd.capsuleDiffEnd[0] = dist;
                    dist = CLOTH_SCRATCH->coarsePos[2] - CLOTH_SCRATCH->capsuleEndpoints[k][1][2];
                    if (dist < -radius || radius < dist) continue;
                    CLOTH_SCRATCH->diffEnd.capsuleDiffEnd[2] = dist;
                    gte_ldlvl(CLOTH_SCRATCH->diffEnd.capsuleDiffEnd);
                    gte_sqr0();
                    gte_stlvnl(CLOTH_SCRATCH->workVector);
                    distEnd = CLOTH_SCRATCH->workVector[0] + CLOTH_SCRATCH->workVector[1] +
                        CLOTH_SCRATCH->workVector[2];
                    if (distEnd < 1024) {
                        distEnd = gSqrtLutQ3[distEnd] >> 3;
                    } else {
                        int shift;
                        gte_ldlzc(distEnd);
                        gte_nop();
                        gte_nop();
                        gte_stlzc(&leading.vy);
                        shift = leading.vy;
                        shift = 22 - (shift & ~1);
                        distEnd = gSqrtLutQ3[distEnd >> shift];
                        distEnd = (distEnd << 16) >> (19 - (shift >> 1));
                    }
                    dist = distStart + distEnd;
                    if (dist >= radius) continue;
                    gte_lddp(1);
                    gte_ldlvl(CLOTH_SCRATCH->forceOrTarget);
                    gte_gpf0();
                    dist = (radius - dist) << 17;
                    if (dist > CLOTH_MAX_CAPSULE_PUSH) {
                        dist = CLOTH_MAX_CAPSULE_PUSH;
                    }
                    if (distStart != 0) {
                        distStart = dist / distStart;
                    }
                    gte_ldlvl(CLOTH_SCRATCH->diffStart.capsuleDiffStart);
                    gte_lddp(distStart);
                    gte_gpl12();
                    if (distEnd != 0) {
                        distEnd = dist / distEnd;
                    }
                    gte_ldlvl(CLOTH_SCRATCH->diffEnd.capsuleDiffEnd);
                    gte_lddp(distEnd);
                    gte_gpl12();
                    gte_stlvl(CLOTH_SCRATCH->forceOrTarget);
                }
            }
            point->vx = CLOTH_SCRATCH->forceOrTarget[0] * 7 >> 3;
            point->x = CLOTH_SCRATCH->pointPos[0] + CLOTH_SCRATCH->rootDrift[0] + point->vx;
            point->vy = (CLOTH_SCRATCH->forceOrTarget[1] * 7 >> 3) + CLOTH_FIGHTER_GRAVITY;
            point->y = CLOTH_SCRATCH->pointPos[1] + CLOTH_SCRATCH->rootDrift[1] + point->vy;
            point->vz = CLOTH_SCRATCH->forceOrTarget[2] * 7 >> 3;
            point->z = CLOTH_SCRATCH->pointPos[2] + CLOTH_SCRATCH->rootDrift[2] + point->vz;
        }
    }
}

void UpdateFighterClothModels(ClothItem *items, int count)
{
    int i;

    for (i = 0; i < count; ++i) {
        if (items[i].flags & CLOTH_ITEM_FLAG_RESET) {
            ResetFighterClothPositions(&items[i], &gClothModels[items[i].modelIndex]);
        } else {
            LoadClothWindToScratch(items[i].windSlot);
            StepFighterClothModel(&items[i], &gClothModels[items[i].modelIndex]);
        }
    }
}

/* The same statements as ResetClothModelPositions. */
void ResetFighterClothPositions(ClothItem *item, ClothModel *model)
{
    VECTOR out;
    ClothPoint *point;
    int i;

    gte_SetRotMatrix(item->transform);
    gte_SetTransMatrix(item->transform);
    point = model->points;
    for (i = 0; i < model->pointCount; i++) {
        gte_ldv0(&model->sourceVertices[i]);
        gte_rt();
        gte_stlvnl(&out);
        point->x = out.vx << 7;
        point->y = out.vy << 7;
        point->z = out.vz << 7;
        point->vx = 0;
        point->vy = 0;
        point->vz = 0;
        point++;
    }
    model->prevTransform = *item->transform;
}
