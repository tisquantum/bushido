/* Fighter garment cloth: each fighter's cloth parts, built from its character's mesh weight list
 * into cloth models, the limb capsules they collide with, and the per-frame step that drives them
 * with the three wind channels (the solver itself lives in cloth.c). */

#include <sys/types.h>
#include <libgte.h>
#include <rand.h>
#include "scene_node.h"
#include "cloth.h"
#include "model_arena.h"
#include "fighter_cloth.h"
#include "fighter_skeleton.h"
#include "fighter_cloth_setup.h"
#include "tpage_reloc.h"
#include "mesh_stream.h"
#include "model_table.h"
#include "render.h"

#define MESH_PRIM_TYPE_QUAD 0x08
#define CLOTH_WEIGHT_END_MESH (-1)
#define CLOTH_WEIGHT_END_LIST (-2)
#define FIGHTER_CLOTH_STIFFNESS_DIV 2
#define CLOTH_WIND_DEFAULT_MAGNITUDE 1024
#define CLOTH_VERTEX_WEIGHT_FREE (-256)

#define CLOTH_WEIGHTS_EMPTY (&gClothWeightsDemoRedShadow[86])

/* Each character's cloth weights: for each of its meshes in order, runs of { count, weight,
 * vertex index... } ended by CLOTH_WEIGHT_END_MESH; the list ended by CLOTH_WEIGHT_END_LIST. */
short gClothWeightsKannuki[44] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 2, 4, 6, 8, 10, 12, 14, 8, 128, 1, 3, 5, 7, 9, 11, 13, 15, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsMikado[74] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    10, 0, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 3, -170, 3, 4, 5, 3, -80, 0, 1, 2, 1, -25, 16, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH,
    3, 0, 1, 3, 5, 3, -170, 0, 2, 4, 3, -80, 6, 7, 8, CLOTH_WEIGHT_END_MESH,
    3, 0, 1, 3, 5, 3, -170, 0, 2, 4, 3, -80, 6, 7, 8, CLOTH_WEIGHT_END_MESH,
    3, 0, 0, 11, 12, CLOTH_WEIGHT_END_MESH,
    2, 0, 0, 2, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsBlackLotus[48] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    4, 0, 0, 1, 2, 3, CLOTH_WEIGHT_END_MESH,
    6, 0, 0, 1, 2, 3, 10, 11, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, 4, -170, 8, 9, 10, 11, 4, -80, 12, 13, 14, 15, 1, -25, 16, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
    0,
};

/* 12 bytes; nothing reads them. */
short gClothWeightsUnused[6] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsRedShadow[82] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    6, 0, 0, 1, 2, 3, 4, 5, 6, -170, 6, 7, 8, 9, 10, 11, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, 4, -100, 8, 9, 10, 11, 4, -100, 12, 13, 14, 15, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsUtsusemi[24] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    3, 0, 0, 1, 2, 3, -170, 3, 4, 5, CLOTH_WEIGHT_END_MESH,
    6, 0, 4, 5, 6, 7, 12, 13, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsKannukiAlt[32] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 2, 4, 6, 8, 10, 12, 14, 8, 128, 1, 3, 5, 7, 9, 11, 13, 15,
        5, -170, 16, 20, 21, 22, 23, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsMikadoAlt[68] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, 3, -170, 8, 9, 10, 3, -80, 12, 13, 14, 1, -25, 11, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH,
    2, 0, 0, 1, 3, -170, 2, 3, 6, 3, -25, 4, 5, 7, CLOTH_WEIGHT_END_MESH,
    2, 0, 0, 1, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsBlackLotusAlt[54] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    10, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 12, 15, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 12, 15, CLOTH_WEIGHT_END_MESH,
    8, 0, 8, 9, 10, 11, 12, 13, 14, 15, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
    0,
};

short gClothWeightsTatsumiAlt[22] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    12, 0, 0, 1, 10, 11, 20, 21, 22, 23, 24, 25, 26, 27, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsRedShadowAlt[74] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    16, 0, 0, 1, 2, 3, 4, 5, 6, 7, 22, 23, 24, 25, 26, 27, 28, 29,
        4, -170, 18, 19, 20, 21, 4, -170, 8, 10, 13, 30, 4, -256, 11, 12, 14, 31, 4, -256, 9, 15, 16, 32,
        1, -256, 17, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsUtsusemiAlt[34] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 3, 5, 6, CLOTH_WEIGHT_END_MESH,
    18, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 15, 16, 17, 18, 19, 20, 21, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsFinalBoss[46] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    25, 0, 0, 8, 4, 5, 6, 7, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
        CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
    0,
};

short gClothWeightsFinalBossAlt[50] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    11, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 7, -25, 14, 15, 16, 17, 19, 20, 21, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsBout8Boss[16] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsBout6Boss[54] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    12, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 3, 4, 5, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
    0,
};

short gClothWeightsBout7Boss[40] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    29, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 19, 20, 21, 26, 27, 28, 31, 33, 37, 38, 39, 40, 47, 48,
        CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
    0,
};

short gClothWeightsFourthSlashEnemy[22] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    12, 0, 0, 1, 6, 7, 12, 13, 14, 15, 16, 17, 18, 19, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
};

short gClothWeightsDemoKannuki[48] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 2, 4, 6, 8, 10, 12, 14, 8, 128, 1, 3, 5, 7, 9, 11, 13, 15, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
    0,
};

short gClothWeightsDemoBlackLotus[52] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 6, CLOTH_WEIGHT_END_MESH,
    6, 0, 0, 1, 2, 3, 10, 11, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, 4, -170, 8, 9, 10, 11, 4, -80, 12, 13, 14, 15, 1, -25, 16, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
    0,
};

/* Character 34's list, then the empty list every character without cloth shares. */
/* kept: as its own object the empty list is 4 bytes, which go to small data and move the data after them */
short gClothWeightsDemoRedShadow[88] = {
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    5, 0, 0, 1, 2, 3, 4, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH, CLOTH_WEIGHT_END_MESH,
    6, 0, 0, 1, 2, 3, 4, 5, 6, -170, 6, 7, 8, 9, 10, 11, CLOTH_WEIGHT_END_MESH,
    8, 0, 0, 1, 2, 3, 4, 5, 6, 7, 4, -100, 8, 9, 10, 11, 4, -100, 12, 13, 14, 15, CLOTH_WEIGHT_END_MESH,
    CLOTH_WEIGHT_END_LIST,
    0,
    CLOTH_WEIGHT_END_LIST, /* CLOTH_WEIGHTS_EMPTY */
    0,
};

static ClothItem gFighterClothItems[FIGHTER_CLOTH_MAX_ITEMS];
static ClothColliderSet gFighterClothColliders[FIGHTER_CLOTH_SLOT_COUNT][FIGHTER_CLOTH_MAX_ITEMS];
static int gFighterClothColliderCounts[FIGHTER_CLOTH_SLOT_COUNT];
static int gFighterClothItemStarts[FIGHTER_CLOTH_SLOT_COUNT];
static int gClothWindTargetMagnitudes[CLOTH_WIND_SLOT_COUNT];
static int gClothWindTargetAngles[CLOTH_WIND_SLOT_COUNT];
static struct SceneNode *gFighterClothNodes[FIGHTER_CLOTH_SLOT_COUNT][FIGHTER_CLOTH_MAX_ITEMS];
static int gFighterClothNodeCounts[FIGHTER_CLOTH_SLOT_COUNT];
static VECTOR sPrevNodeForward[4];
static int sNodeForwardDelta[4];

static char *sClothWorkBufferBase;
static char *gClothWorkBufferCursor;
static int sUnusedClothWorkWord; /* 4 bytes; nothing reads or writes them */
static int gActiveFighterClothCount;
static int gClothSnapResetPending;
static int gClothWindBaseMagnitude;
static int gClothWindBaseAngle;
static int sUnusedClothWindWord; /* 4 bytes; nothing reads or writes them */

/* Indexed by character archive (0..40). */
short *gCharClothMeshLists[41] = {
    gClothWeightsKannuki, gClothWeightsMikado,
    gClothWeightsBlackLotus, CLOTH_WEIGHTS_EMPTY,
    gClothWeightsRedShadow, gClothWeightsUtsusemi,
    gClothWeightsKannukiAlt, gClothWeightsMikadoAlt,
    gClothWeightsBlackLotusAlt, gClothWeightsTatsumiAlt,
    gClothWeightsRedShadowAlt, gClothWeightsUtsusemiAlt,
    gClothWeightsFinalBoss, gClothWeightsFinalBossAlt,
    CLOTH_WEIGHTS_EMPTY, gClothWeightsBout8Boss,
    gClothWeightsBout6Boss, gClothWeightsBout7Boss,
    CLOTH_WEIGHTS_EMPTY, CLOTH_WEIGHTS_EMPTY,
    CLOTH_WEIGHTS_EMPTY, CLOTH_WEIGHTS_EMPTY,
    gClothWeightsFourthSlashEnemy, CLOTH_WEIGHTS_EMPTY,
    CLOTH_WEIGHTS_EMPTY, CLOTH_WEIGHTS_EMPTY,
    CLOTH_WEIGHTS_EMPTY, CLOTH_WEIGHTS_EMPTY,
    CLOTH_WEIGHTS_EMPTY, CLOTH_WEIGHTS_EMPTY,
    CLOTH_WEIGHTS_EMPTY, CLOTH_WEIGHTS_EMPTY,
    gClothWeightsDemoKannuki, gClothWeightsDemoBlackLotus,
    gClothWeightsDemoRedShadow, CLOTH_WEIGHTS_EMPTY,
    CLOTH_WEIGHTS_EMPTY, CLOTH_WEIGHTS_EMPTY,
    CLOTH_WEIGHTS_EMPTY, CLOTH_WEIGHTS_EMPTY,
    CLOTH_WEIGHTS_EMPTY,
};
/* Halfword offset to the packed 4-byte vertex-index word in a mesh primitive record, indexed by (primType >> 2) & 7. */
short gPrimVertexIndexOffsets[8] = { 20, 32, 24, 40, 28, 40, 36, 52 };

int gFighterClothActive[FIGHTER_CLOTH_SLOT_COUNT] = { 0, 0, 0 };

void ResetClothWindWithJitter(void)
{
    int jitter = rand() & 15;
    int angle = gClothWindBaseAngle - 8;

    gClothWindTargetMagnitudes[CLOTH_WIND_SLOT_FIGHTER_0] = gClothWindTargetMagnitudes[CLOTH_WIND_SLOT_FIGHTER_1] =
        gClothWindTargetMagnitudes[CLOTH_WIND_SLOT_STAGE] = gClothWindBaseMagnitude = CLOTH_WIND_DEFAULT_MAGNITUDE;
    angle += jitter;
    gClothWindTargetAngles[CLOTH_WIND_SLOT_FIGHTER_0] = gClothWindTargetAngles[CLOTH_WIND_SLOT_FIGHTER_1] =
        gClothWindTargetAngles[CLOTH_WIND_SLOT_STAGE] = gClothWindBaseAngle = angle;
}

void SetClothWindTarget(int magnitude, int angle, int windSlot)
{
    gClothWindTargetMagnitudes[windSlot] = magnitude;
    gClothWindTargetAngles[windSlot] = angle;
}

void UpdateAllFighterCloth(void)
{
    StepClothWindSlot(gClothWindTargetMagnitudes[CLOTH_WIND_SLOT_FIGHTER_0],
                      gClothWindTargetAngles[CLOTH_WIND_SLOT_FIGHTER_0], CLOTH_WIND_SLOT_FIGHTER_0);
    StepClothWindSlot(gClothWindTargetMagnitudes[CLOTH_WIND_SLOT_FIGHTER_1],
                      gClothWindTargetAngles[CLOTH_WIND_SLOT_FIGHTER_1], CLOTH_WIND_SLOT_FIGHTER_1);
    StepClothWindSlot(gClothWindTargetMagnitudes[CLOTH_WIND_SLOT_STAGE],
                      gClothWindTargetAngles[CLOTH_WIND_SLOT_STAGE], CLOTH_WIND_SLOT_STAGE);
    TransformClothColliders();
    if (gClothSnapResetPending != 0) {
        SetAllClothResetFlags();
    }
    UpdateFighterClothModels(gFighterClothItems, gActiveFighterClothCount);
    if (gClothSnapResetPending != 0) {
        ClearAllClothResetFlags();
    }
    gClothSnapResetPending = 0;
}

void TransformClothColliders(void)
{
    int slot;
    int part;
    int collider;

    for (slot = 0; slot < FIGHTER_CLOTH_SLOT_COUNT; slot++) {
        for (part = 0; part < gFighterClothColliderCounts[slot]; part++) {
            ClothColliderSet *colliders = &gFighterClothColliders[slot][part];

            for (collider = 0; collider < colliders->count; collider++) {
                MATRIX *matrix = &colliders->limbs[collider]->worldMatrix;
                SVECTOR *localEnd = colliders->localEndpoints[collider];
                VECTOR *worldEnd = colliders->input.worldEndpoints[collider];

                TransformSvectorByMatrix(matrix, localEnd, worldEnd);
                TransformSvectorByMatrix(matrix, localEnd + 1, worldEnd + 1);
            }
        }
    }
}

void InitClothSystem(void)
{
    int slot;

    ClearClothModels();
    ResetClothWindSlots();
    gClothWindBaseMagnitude = 0;
    gClothWindBaseAngle = 0;
    gActiveFighterClothCount = 0;
    gClothSnapResetPending = 1;
    for (slot = 0; slot < FIGHTER_CLOTH_SLOT_COUNT; slot++) {
        gFighterClothActive[slot] = 0;
        SetFighterClothDirty(slot, 0);
        gFighterClothColliderCounts[slot] = 0;
    }
}

void FreeAllFighterCloth(void)
{
    int slot;

    for (slot = 0; slot < FIGHTER_CLOTH_SLOT_COUNT; slot++) {
        FreeFighterClothSlot(slot);
        gFighterClothActive[slot] = 0;
        gFighterClothColliderCounts[slot] = 0;
    }
    gActiveFighterClothCount = 0;
}

int CalcFighterClothWorkSize(int slot)
{
    int size = 0;
    int character = GetFighterCharacterId(slot);
    short *weightStream = gCharClothMeshLists[character];
    int modelSlot = GetFighterModelTableSlot(slot);
    int meshIndex = 0;
    unsigned short *meshCursor;
    int count;
    unsigned short primType;
    int pointCount;
    int maxLinkCount;

    while (*weightStream != CLOTH_WEIGHT_END_LIST) {
        if (*weightStream != CLOTH_WEIGHT_END_MESH) {
            meshCursor = gModelTables[modelSlot][meshIndex];
            count = *meshCursor++;
            meshCursor++;
            if (count & MESH_HEADER_INDEXED_FLAG) {
                TriggerDebugTrap(character, weightStream);
            }
            pointCount = count * 3;
            meshCursor += pointCount * 4;
            maxLinkCount = 0;
            while ((count = *meshCursor++) != 0) {
                primType = *meshCursor++;
                if (primType & MESH_PRIM_TYPE_QUAD) {
                    maxLinkCount += count * 6;
                    meshCursor += gPrimRecordHalfwords[primType] * count;
                } else {
                    maxLinkCount += count * 3;
                    meshCursor += gPrimRecordHalfwords[primType] * count;
                }
            }
            size += CalcClothWorkSize(pointCount, maxLinkCount);
            size = AlignUp4(size);
        }
        while (*weightStream++ != CLOTH_WEIGHT_END_MESH) {
        }
        meshIndex++;
    }
    return size;
}

int BuildFighterClothParts(int slot)
{
    struct SceneNode **slotNodes = gFighterClothNodes[slot];
    int meshIndex;
    int count;
    short *weightStream;
    struct SceneNode *node;

    gFighterClothItemStarts[slot] = gActiveFighterClothCount;
    gClothWorkBufferCursor = sClothWorkBufferBase = GetFighterClothBufferBase(slot);
    meshIndex = 0;
    count = 0;
    weightStream = gCharClothMeshLists[GetFighterCharacterId(slot)];
    while (*weightStream != CLOTH_WEIGHT_END_LIST) {
        if (*weightStream != CLOTH_WEIGHT_END_MESH) {
            node = GetSkeletonPartByMeshIndex(slot, meshIndex);
            InitClothPartFromMesh(slot, node, weightStream);
            node->renderType = SCENE_NODE_CLOTH;
            *slotNodes++ = node;
            count++;
        }
        while (*weightStream++ != CLOTH_WEIGHT_END_MESH) {
        }
        meshIndex++;
    }
    gFighterClothNodeCounts[slot] = count;
    gFighterClothActive[slot] = 1;
    return count;
}

void InitClothPartFromMesh(int slot, struct SceneNode *node, short *weightStream)
{
    ClothModelSpec spec;
    unsigned short *meshCursor;
    unsigned short *primStream;
    short *faceLinkOut;
    short count;
    short primType;
    int maxLinkCount;
    int packedVertices;
    ClothItem *item;
    int *vertexIndexWord;

    meshCursor = gModelTables[node->modelSlot][node->meshIndex];
    faceLinkOut = (short *)GetArenaFreePtr() + 16;
    count = *meshCursor++;
    meshCursor++;
    if (count >> 15) {
        TriggerDebugTrap();
    }
    spec.sourceVertices = (SVECTOR *)meshCursor;
    spec.pointCount = count * 3;
    meshCursor += count * 3 * 4;
    primStream = meshCursor;
    maxLinkCount = 0;
    for (count = *meshCursor++; count != 0; count = *meshCursor++) {
        primType = *meshCursor++;
        if (primType & MESH_PRIM_TYPE_QUAD) {
            maxLinkCount += count * 6;
            meshCursor += gPrimRecordHalfwords[primType] * count;
        } else {
            maxLinkCount += count * 3;
            meshCursor += gPrimRecordHalfwords[primType] * count;
        }
    }
    spec.workBuffer = gClothWorkBufferCursor;
    gClothWorkBufferCursor += CalcClothWorkSize(spec.pointCount, maxLinkCount);
    gClothWorkBufferCursor = (char *)AlignUp4((int)gClothWorkBufferCursor);
    spec.faceLinkLists = faceLinkOut;
    meshCursor = primStream;
    while ((count = *meshCursor++) != 0) {
        primType = *meshCursor++;
        if (primType & MESH_PRIM_TYPE_QUAD) {
            while (--count != -1) {
                vertexIndexWord = (int *)(meshCursor + gPrimVertexIndexOffsets[(primType >> 2) & 7]);
                packedVertices = *vertexIndexWord;
                *faceLinkOut++ = 4;
                *faceLinkOut++ = FIGHTER_CLOTH_STIFFNESS_DIV;
                *faceLinkOut++ = packedVertices & 0xff;
                *faceLinkOut++ = (packedVertices >> 8) & 0xff;
                *faceLinkOut++ = (packedVertices >> 16) & 0xff;
                *faceLinkOut++ = (packedVertices >> 24) & 0xff;
                meshCursor += gPrimRecordHalfwords[primType];
            }
        } else {
            while (--count != -1) {
                vertexIndexWord = (int *)(meshCursor + gPrimVertexIndexOffsets[(primType >> 2) & 7]);
                packedVertices = *vertexIndexWord;
                *faceLinkOut++ = 3;
                *faceLinkOut++ = FIGHTER_CLOTH_STIFFNESS_DIV;
                *faceLinkOut++ = packedVertices & 0xff;
                *faceLinkOut++ = (packedVertices >> 8) & 0xff;
                *faceLinkOut++ = (packedVertices >> 16) & 0xff;
                meshCursor += gPrimRecordHalfwords[primType];
            }
        }
    }
    *faceLinkOut++ = 0;
    UpdateArenaPeakUsage(faceLinkOut);
    if (*meshCursor != 0) {
        TriggerDebugTrap("Multipul Model");
    }
    ApplyClothVertexWeights(spec.sourceVertices, spec.pointCount, weightStream);
    spec.initialTransform = &node->worldMatrix;
    spec.damping = CLOTH_FIGHTER_DAMPING;
    node->clothSlotOrHeading.clothSlot = AllocClothModel(&spec);
    node->clothWorkOrFade.clothWork = spec.workBuffer;
    /* the original writes this field through a pointer */
    *(ClothColliderSet **)&node->clothColliders = &gFighterClothColliders[slot][gFighterClothColliderCounts[slot]++];
    node->clothColliders->count = 0;
    node->clothColliders->input.activeCount = 0;
    if (gActiveFighterClothCount >= FIGHTER_CLOTH_MAX_ITEMS) {
        TriggerDebugTrap();
    }
    item = &gFighterClothItems[gActiveFighterClothCount++];
    item->modelIndex = *(int *)&node->clothSlotOrHeading.clothSlot; /* the original reads this field
                                                                       through a pointer */
    item->transform = &node->worldMatrix;
    item->outputPositions = (VECTOR *)node->clothWorkOrFade.clothWork;
    item->colliderInput = &node->clothColliders->input;
    item->flags = CLOTH_ITEM_FLAG_NONE;
    item->windSlot = slot;
}

void ApplyClothVertexWeights(SVECTOR *vertices, int vertexCount, short *weightStream)
{
    int i;
    int count;
    int weight;

    for (i = 0; i < vertexCount; i++) {
        vertices[i].pad = CLOTH_VERTEX_WEIGHT_FREE;
    }

    for (count = *weightStream++; count != CLOTH_WEIGHT_END_MESH; count = *weightStream++) {
        weight = *weightStream++;
        while (--count != -1) {
            i = *weightStream++;
            vertices[i].pad = weight;
        }
    }
}

int AlignUp4(int size)
{
    if (size & 3)
        size = (size + 3) & ~3;

    return size;
}

/* Nothing calls it. */
static void FighterClothNoOp(void)
{
}

void AddClothLimbCollider(struct SceneNode *node, struct SceneNode *limb,
                          int x1, int y1, int z1, int x2, int y2, int z2,
                          int radius)
{
    ClothColliderSet *colliders = node->clothColliders;
    int index = colliders->count;

    colliders->limbs[index] = limb;
    colliders->localEndpoints[index][0].vx = x1;
    colliders->localEndpoints[index][0].vy = y1;
    colliders->localEndpoints[index][0].vz = z1;
    colliders->localEndpoints[index][1].vx = x2;
    colliders->localEndpoints[index][1].vy = y2;
    colliders->localEndpoints[index][1].vz = z2;
    colliders->input.radii[index] = radius;
    index++;
    colliders->count = index;
    colliders->input.activeCount = index;
}

/* Nothing calls it. */
static void ClearClothLimbColliders(struct SceneNode *node)
{
    node->clothColliders->count = 0;
}

void RequestClothSnapReset(void)
{
    gClothSnapResetPending = 1;
}

void SetAllClothResetFlags(void)
{
    int i;

    for (i = 0; i < gActiveFighterClothCount; i++) {
        gFighterClothItems[i].flags = CLOTH_ITEM_FLAG_RESET;
    }
}

void ClearAllClothResetFlags(void)
{
    int i;

    for (i = 0; i < gActiveFighterClothCount; i++) {
        gFighterClothItems[i].flags = CLOTH_ITEM_FLAG_NONE;
    }
}

void SetSlotClothFloorY(int slot, int floorY)
{
    int i;

    for (i = 0; i < gFighterClothNodeCounts[slot]; i++) {
        gFighterClothNodes[slot][i]->clothColliders->input.floorY = floorY;
    }
}

void RelocateFighterClothSlot(int slot, int deltaBytes)
{
    int i;
    struct SceneNode *node;

    for (i = 0; i < gFighterClothNodeCounts[slot]; i++) {
        node = gFighterClothNodes[slot][i];
        RelocateClothWorkPtrs(node->clothSlotOrHeading.clothSlot, deltaBytes);
        RelocateClothVertexPtr(node->clothSlotOrHeading.clothSlot, deltaBytes);
        node->clothWorkOrFade.clothWork += deltaBytes;
    }
}

void FreeFighterClothSlot(int slot)
{
    int i;
    int writeIndex;
    int start;
    int end;

    if (gFighterClothActive[slot] != 0) {
        for (i = 0; i < gFighterClothNodeCounts[slot]; i++) {
            FreeClothModel(gFighterClothNodes[slot][i]->clothSlotOrHeading.clothSlot);
        }

        if (gFighterClothNodeCounts[slot] > 0) {
            start = gFighterClothItemStarts[slot];
            end = start + gFighterClothNodeCounts[slot] - 1;
            writeIndex = 0;
            for (i = 0; i < gActiveFighterClothCount; i++) {
                if (i < start) {
                    writeIndex++;
                }
                if (i > end) {
                    gFighterClothItems[writeIndex] = gFighterClothItems[i];
                    writeIndex++;
                }
            }
            gActiveFighterClothCount = writeIndex;
        }

        for (i = 0; i < FIGHTER_CLOTH_SLOT_COUNT; i++) {
            if (gFighterClothNodeCounts[i] > 0 && gFighterClothItemStarts[i] > start) {
                gFighterClothItemStarts[i] -= gFighterClothNodeCounts[slot];
            }
        }

        gFighterClothNodeCounts[slot] = 0;
        gFighterClothColliderCounts[slot] = 0;
        gFighterClothActive[slot] = 0;
    }
}

/* Nothing calls it. */
static void UpdateNodeForwardDelta(int slot, struct SceneNode *node)
{
    VECTOR delta;
    SVECTOR unitZ;
    VECTOR *prev;

    unitZ.vx = 0;
    unitZ.vy = 0;
    unitZ.vz = ONE;
    ApplyMatrix(&node->worldMatrix, &unitZ, &delta);
    prev = &sPrevNodeForward[slot];
    delta.vx -= prev->vx;
    delta.vy -= prev->vy;
    delta.vz -= prev->vz;
    sNodeForwardDelta[slot] = SquareRoot0(delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz);
    /* Stores worldMatrix.t[0] into all three components rather than t[0..2]. */
    prev->vx = node->worldMatrix.t[0];
    prev->vy = node->worldMatrix.t[0];
    prev->vz = node->worldMatrix.t[0];
}
