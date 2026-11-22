/* Billboard effects: a 16-slot pool of camera-facing animated sprites and a delayed ambient sound
 * (kind 15), with each kind's descriptor, UV tables and the quad builder. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "combat_state.h"
#include <inline_o.h>
#include "billboard.h"
#include "rot_matrix.h"

BillboardEffectSlot gBillboardEffects[BILLBOARD_EFFECT_POOL_SIZE];

EffectTextureTable gBillboardTextures;

void ClearBillboardEffects(void)
{
    int i;

    for (i = 0; i < BILLBOARD_EFFECT_POOL_SIZE; i++) {
        gBillboardEffects[i].kind = BILLBOARD_EFFECT_FREE;
    }
}

/* The per-kind tables are indexed by kind - 1. */
u_char gBillboardFrameCounts[16] = { 7, 6, 6, 6, 3, 8, 8, 7, 6, 5, 5, 7, 5, 5, 0, 0 };
u_char gBillboardFrameWidths[16] = {
    0x20, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x20, 0x30, 0x30, 0x20, 0x20, 0x20, 0x00, 0x00,
};
u_char gBillboardFrameHeights[16] = {
    0x10, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x10, 0x18, 0x18, 0x10, 0x10, 0x10, 0x00, 0x00,
};
u_short gBillboard1Uvs[7][4];
u_short gLimbWoundBillboardUvs[6][4];
u_short gOrangeSparkBillboardUvs[6][4];
u_short gFatalHitPrimaryBillboardUvs[6][4];
u_short gWeaponClashBillboardUvs[3][4];
u_short gFatalHitSecondaryBillboardUvs[8][4];
u_short gTorsoWoundBillboardUvs[8][4];
u_short gBillboard8Uvs[7][4];
u_short gBillboard9Uvs[6][4];
u_short gBillboard10Uvs[5][4];
u_short gBillboard11Uvs[5][4];
u_short gBillboard12Uvs[7][4];
u_short gBillboard13Uvs[5][4];
u_short gBillboard14Uvs[5][4];
u_short *gBillboardUvBuffers[BILLBOARD_EFFECT_KIND_COUNT] = {
    gBillboard1Uvs[0], gLimbWoundBillboardUvs[0], gOrangeSparkBillboardUvs[0], gFatalHitPrimaryBillboardUvs[0],
    gWeaponClashBillboardUvs[0], gFatalHitSecondaryBillboardUvs[0], gTorsoWoundBillboardUvs[0], gBillboard8Uvs[0],
    gBillboard9Uvs[0], gBillboard10Uvs[0], gBillboard11Uvs[0], gBillboard12Uvs[0],
    gBillboard13Uvs[0], gBillboard14Uvs[0],
};
u_char gBillboardSheetLayouts[16] = {
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_HORIZONTAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL,
    BILLBOARD_SHEET_VERTICAL_ALT,
    0,
    0,
};
unsigned char gBillboardLinearFrames[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

void BuildEffectTextureUVs(EffectTextureEntry *textures)
{
    int i, frame, y, x;
    int u, v;
    int base;

    gBillboardTextures = *(EffectTextureTable *)textures;

    for (i = 0; i < 15; i++, textures++) { /* one pass more than the kinds: frame count 14 is 0 */
        for (frame = 0; frame < gBillboardFrameCounts[i]; frame++) {
            base = frame * 4;
            for (y = 0; y < 2; y++) {
                for (x = 0; x < 2; x++) {
                    switch (gBillboardSheetLayouts[i]) {
                    case BILLBOARD_SHEET_HORIZONTAL:
                        u = textures->u + (1 - x) * (gBillboardFrameWidths[i] - 1) + frame * gBillboardFrameWidths[i];
                        v = textures->v + (1 - y) * (gBillboardFrameHeights[i] - 1);
                        break;
                    case BILLBOARD_SHEET_VERTICAL:
                        u = textures->u + (1 - x) * (gBillboardFrameWidths[i] - 1);
                        v = textures->v + (1 - y) * (gBillboardFrameHeights[i] - 1) + frame * gBillboardFrameHeights[i];
                        break;
                    case BILLBOARD_SHEET_VERTICAL_ALT:
                        /* both arms are the same in the original */
                        u = textures->u + (1 - x) * (gBillboardFrameWidths[i] - 1);
                        v = textures->v + (1 - y) * (gBillboardFrameHeights[i] - 1) + frame * gBillboardFrameHeights[i];
                        break;
                    }
                    gBillboardUvBuffers[i][base + y * 2 + x] = u | (v << 8);
                }
            }
        }
    }
    ClearBillboardEffects();
}

int SpawnBillboardEffect(int kind, const VECTOR *position)
{
    int i;
    BillboardEffectSlot *slot = gBillboardEffects;

    for (i = 0; i < BILLBOARD_EFFECT_POOL_SIZE; i++, slot++) {
        if (slot->kind == BILLBOARD_EFFECT_FREE) {
            break;
        }
    }
    if (i == BILLBOARD_EFFECT_POOL_SIZE) {
        return -1;
    }
    slot->kind = kind;
    slot->age = 0;
    slot->pos = *position;
    return i;
}

int UpdateAndDrawBillboardEffect(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                 POLY_FT4 **nextPrim, SpriteDesc *desc)
{
    POLY_FT4 *prim;
    int turn;
    int sine, cosine;
    int row, col;
    int offset;
    int frame;
    long *depths;
    POLY_FT4 **prims;

    prim = *nextPrim;
    depths = work->depths;
    prims = work->prims;
    work->delta.vx = slot->pos.vx - work->eye.vx;
    work->delta.vy = slot->pos.vy - work->eye.vy;
    work->delta.vz = slot->pos.vz - work->eye.vz;
    turn = (ratan2(work->delta.vz, work->delta.vx) - work->cameraAngle + 1024) & 0xfff;
    sine = rsin(work->cameraAngle + turn);
    cosine = rcos(work->cameraAngle + turn);
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 2; col++) {
            work->corners[row * 2 + col].vy = work->delta.vy - desc->height * row + desc->yOffset;
            offset = desc->width * col - desc->width / 2;
            work->corners[row * 2 + col].vx = work->delta.vx + offset * cosine / 4096;
            work->corners[row * 2 + col].vz = work->delta.vz + offset * sine / 4096;
        }
    }
    gte_ldv3(&work->corners[0], &work->corners[1], &work->corners[2]);
    gte_rtpt();
    SetPolyFT4(prim);
    prims[primCount] = prim;
    *(u_long *)&prim->r0 = desc->colorAndCode;
    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
    gte_ldv0(&work->corners[3]);
    gte_rtps();
    frame = desc->frames[slot->age];
    *(u_short *)&prim->u0 = desc->uvs[frame][0];
    *(u_short *)&prim->u1 = desc->uvs[frame][1];
    *(u_short *)&prim->u2 = desc->uvs[frame][2];
    *(u_short *)&prim->u3 = desc->uvs[frame][3];
    gte_stsxy(&prim->x3);
    gte_avsz4();
    prim->tpage = desc->texture->tpage;
    prim->clut = desc->texture->clut;
    gte_stotz(&depths[primCount++]);
    prim++;
    if (++slot->age >= desc->frameCount) {
        slot->kind = BILLBOARD_EFFECT_FREE;
    }
    *nextPrim = prim;
    return primCount;
}

/* Nothing calls SpawnBillboard1 or the thirteen Spawn...Billboard routines after it; callers pass the
 * kind to SpawnBillboardEffect. */
static int SpawnBillboard1(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_KIND_1, position);
}

SpriteDesc gBillboard1Desc = {
    400,
    400,
    7,
    0,
    BILLBOARD_FT4_OPAQUE,
    gBillboard1Uvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[0],
    0,
};

unsigned char gLimbWoundBillboardFrames[12] = { 0, 1, 2, 2, 3, 3, 4, 4, 4, 5, 5, 5 };

static int UpdateAndDrawBillboard1(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                   POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gBillboard1Desc);
}

static int SpawnLimbWoundBillboard(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_LIMB_WOUND, position);
}

SpriteDesc gLimbWoundBillboardDesc = {
    400,
    400,
    12,
    0,
    BILLBOARD_FT4_SEMITRANS,
    gLimbWoundBillboardUvs,
    gLimbWoundBillboardFrames,
    &gBillboardTextures.entries[1],
    200,
};

static int UpdateAndDrawLimbWoundBillboard(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                           POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gLimbWoundBillboardDesc);
}

static int SpawnOrangeSparkBillboard(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_ORANGE_SPARK, position);
}

SpriteDesc gOrangeSparkBillboardDesc = {
    800,
    800,
    6,
    0,
    BILLBOARD_FT4_SEMITRANS,
    gOrangeSparkBillboardUvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[2],
    400,
};

static int UpdateAndDrawOrangeSparkBillboard(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                             POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gOrangeSparkBillboardDesc);
}

static int SpawnFatalHitPrimaryBillboard(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_FATAL_HIT_PRIMARY, position);
}

SpriteDesc gFatalHitPrimaryBillboardDesc = {
    800,
    800,
    6,
    0,
    BILLBOARD_FT4_SEMITRANS,
    gFatalHitPrimaryBillboardUvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[3],
    400,
};

static int UpdateAndDrawFatalHitPrimaryBillboard(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                                 POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gFatalHitPrimaryBillboardDesc);
}

static int SpawnWeaponClashBillboard(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_WEAPON_CLASH, position);
}

SpriteDesc gWeaponClashBillboardDesc = {
    800,
    800,
    3,
    0,
    BILLBOARD_FT4_SEMITRANS,
    gWeaponClashBillboardUvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[4],
    400,
};

static int UpdateAndDrawWeaponClashBillboard(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                             POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gWeaponClashBillboardDesc);
}

static int SpawnFatalHitSecondaryBillboard(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_FATAL_HIT_SECONDARY, position);
}

SpriteDesc gFatalHitSecondaryBillboardDesc = {
    800,
    800,
    8,
    0,
    BILLBOARD_FT4_SEMITRANS,
    gFatalHitSecondaryBillboardUvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[5],
    400,
};

static int UpdateAndDrawFatalHitSecondaryBillboard(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                                   POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gFatalHitSecondaryBillboardDesc);
}

static int SpawnTorsoWoundBillboard(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_TORSO_WOUND, position);
}

SpriteDesc gTorsoWoundBillboardDesc = {
    800,
    800,
    8,
    0,
    BILLBOARD_FT4_SEMITRANS,
    gTorsoWoundBillboardUvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[6],
    400,
};

static int UpdateAndDrawTorsoWoundBillboard(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                            POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gTorsoWoundBillboardDesc);
}

static int SpawnBillboard8(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_KIND_8, position);
}

SpriteDesc gBillboard8Desc = {
    800,
    800,
    7,
    0,
    BILLBOARD_FT4_SEMITRANS,
    gBillboard8Uvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[7],
    400,
};

static int UpdateAndDrawBillboard8(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                   POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gBillboard8Desc);
}

static int SpawnBillboard9(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_KIND_9, position);
}

SpriteDesc gBillboard9Desc = {
    400,
    400,
    6,
    0,
    BILLBOARD_FT4_OPAQUE,
    gBillboard9Uvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[8],
    0,
};

static int UpdateAndDrawBillboard9(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                   POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gBillboard9Desc);
}

static int SpawnBillboard10(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_KIND_10, position);
}

SpriteDesc gBillboard10Desc = {
    800,
    800,
    5,
    0,
    BILLBOARD_FT4_SEMITRANS,
    gBillboard10Uvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[9],
    0,
};

static int UpdateAndDrawBillboard10(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                    POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gBillboard10Desc);
}

static int SpawnBillboard11(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_KIND_11, position);
}

SpriteDesc gBillboard11Desc = {
    800,
    800,
    5,
    0,
    BILLBOARD_FT4_SEMITRANS,
    gBillboard11Uvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[10],
    0,
};

static int UpdateAndDrawBillboard11(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                    POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gBillboard11Desc);
}

static int SpawnBillboard12(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_KIND_12, position);
}

SpriteDesc gBillboard12Desc = {
    400,
    400,
    7,
    0,
    BILLBOARD_FT4_OPAQUE,
    gBillboard12Uvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[11],
    0,
};

static int UpdateAndDrawBillboard12(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                    POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gBillboard12Desc);
}

static int SpawnBillboard13(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_KIND_13, position);
}

SpriteDesc gBillboard13Desc = {
    400,
    400,
    5,
    0,
    BILLBOARD_FT4_OPAQUE,
    gBillboard13Uvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[12],
    0,
};

static int UpdateAndDrawBillboard13(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                    POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gBillboard13Desc);
}

static int SpawnBillboard14(const VECTOR *position)
{
    return SpawnBillboardEffect(BILLBOARD_EFFECT_KIND_14, position);
}

SpriteDesc gBillboard14Desc = {
    800,
    800,
    5,
    0,
    BILLBOARD_FT4_SEMITRANS,
    gBillboard14Uvs,
    gBillboardLinearFrames,
    &gBillboardTextures.entries[13],
    0,
};

static int UpdateAndDrawBillboard14(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                    POLY_FT4 **nextPrim)
{
    return UpdateAndDrawBillboardEffect(work, slot, primCount, nextPrim, &gBillboard14Desc);
}

short gAmbientTriggerCycle = 0; /* counts 0 to 2; nothing else reads it */

/* Called through gBillboardEffectHandlers, so variant is a pointer to the caller's stack: negative
 * as an int, and the sound is always ambient slot 0. */
static int UpdateDelayedAmbientTrigger(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount, int variant)
{
    short cycle;

    if (++slot->age == 2) {
        cycle = gAmbientTriggerCycle + 1;
        slot->kind = BILLBOARD_EFFECT_FREE;
        gAmbientTriggerCycle = cycle;
        if (cycle == 3) {
            gAmbientTriggerCycle = 0;
        }
        TriggerPositionalAmbientSound(variant > 0 ? variant + 5 : 0, &slot->pos);
    }
    return primCount;
}

BillboardEffectHandler gBillboardEffectHandlers[BILLBOARD_EFFECT_DELAYED_AMBIENT + 1] = {
    0,
    UpdateAndDrawBillboard1,
    UpdateAndDrawLimbWoundBillboard,
    UpdateAndDrawOrangeSparkBillboard,
    UpdateAndDrawFatalHitPrimaryBillboard,
    UpdateAndDrawWeaponClashBillboard,
    UpdateAndDrawFatalHitSecondaryBillboard,
    UpdateAndDrawTorsoWoundBillboard,
    UpdateAndDrawBillboard8,
    UpdateAndDrawBillboard9,
    UpdateAndDrawBillboard10,
    UpdateAndDrawBillboard11,
    UpdateAndDrawBillboard12,
    UpdateAndDrawBillboard13,
    UpdateAndDrawBillboard14,
    (BillboardEffectHandler)UpdateDelayedAmbientTrigger,
};

int UpdateAndBuildBillboardEffects(BillboardEffectWork *work)
{
    long depths[BILLBOARD_EFFECT_POOL_SIZE];
    POLY_FT4 *primitives[BILLBOARD_EFFECT_POOL_SIZE];
    POLY_FT4 *packets;
    BillboardEffectSlot *slot;
    int count;
    int i;
    int j;
    long *depthList;

    work->depths = depths;
    work->prims = primitives;
    SetRotMatrix(&work->matrix);
    SetTransMatrix(&gZeroTransMatrix);
    count = 0;
    packets = work->packets;
    slot = gBillboardEffects;
    for (i = 0; i < BILLBOARD_EFFECT_POOL_SIZE; ++i, ++slot) {
        if (slot->kind != BILLBOARD_EFFECT_FREE)
            count = gBillboardEffectHandlers[slot->kind](work, slot, count, &packets);
    }
    work->packets = packets;
    depthList = (long *)packets + count;
    for (j = 0; j < count; j++)
        ((POLY_FT4 **)packets)[j] = primitives[j];
    for (j = 0; j < count; j++)
        depthList[j] = depths[j];
    return count;
}
