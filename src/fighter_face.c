/* Fighter facial expressions: each expression is two VRAM moves (eyes, mouth) copying face regions
 * of the fighter's texture page over its face, queued at the back of the frame's ordering table. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "display_buffer.h"
#include "fighter.h"
#include "model_arena.h"
#include "fighter_face.h"
#include "main.h"

/* Double-buffered VRAM blit pool limits and layout for fighter facial expressions. */
enum {
    FACE_REGION_MODEL_COUNT = 19,
    FACE_REGION_VARIANT_NORMAL = 0,
    FACE_REGION_VARIANT_INJURED = 1,
    FACE_REGION_VARIANT_COUNT = 2,
    FACE_REGION_STRIDE = 6,
    FACE_REGION_NONE = -1,
    FACE_BLIT_BANK_COUNT = 2,
    FACE_BLIT_MAX_PACKETS = 10,
    FACE_BLIT_OT_BUCKET = 4095 /* the reverse ordering table's back bucket: drawn before the fighters */
};

/* Indices into each model's 5-entry face region table (3 eye/upper-face regions + 2 mouth regions). */
enum FaceTextureRegion {
    FACE_REGION_EYES_TENSE = 0,
    FACE_REGION_EYES_CLOSED = 1,
    FACE_REGION_MOUTH_OPEN = 2,
    FACE_REGION_MOUTH_CLOSED = 3,
    FACE_REGION_EYES_NORMAL = 4, /* Full face (80px tall) on uninjured models, upper face (44-48px) on injured models */
    FACE_REGION_COUNT = 5
};

/* One facial-expression VRAM move in P1's texture page: source x, y, width, height, then destination x, y;
 * source x is FACE_REGION_NONE where the region is absent. */
typedef short FaceBlitRegion[FACE_REGION_STRIDE];

static int gFaceBlitLastBank = -1;
static DR_MOVE *gFaceBlitCursor;
static DR_MOVE gFaceBlitPackets[FACE_BLIT_BANK_COUNT][FACE_BLIT_MAX_PACKETS];

FaceBlitRegion gFaceRegionsModel06Normal[FACE_REGION_COUNT] = {
    { 724, 160, 17, 48, 708, 0 },
    { 744, 160, 17, 48, 708, 0 },
    { 724, 208, 17, 32, 708, 48 },
    { 704, 208, 17, 32, 708, 48 },
    { 704, 160, 17, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel06Injured[FACE_REGION_COUNT] = {
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { 768, 60, 33, 44, 704, 0 },
    { 724, 204, 17, 36, 708, 44 },
    { 704, 204, 17, 36, 708, 44 },
    { 704, 160, 33, 44, 704, 0 },
};

FaceBlitRegion gFaceRegionsModel07Normal[FACE_REGION_COUNT] = {
    { 720, 80, 16, 48, 708, 0 },
    { 736, 112, 16, 48, 708, 0 },
    { 720, 128, 16, 32, 708, 48 },
    { 704, 128, 16, 32, 708, 48 },
    { 704, 80, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel07Injured[FACE_REGION_COUNT] = {
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { 768, 60, 34, 44, 704, 0 },
    { 720, 124, 16, 36, 708, 44 },
    { 704, 124, 16, 36, 708, 44 },
    { 704, 80, 34, 44, 704, 0 },
};

FaceBlitRegion gFaceRegionsModel08Normal[FACE_REGION_COUNT] = {
    { 720, 80, 15, 48, 708, 0 },
    { 736, 80, 15, 48, 708, 0 },
    { 720, 128, 16, 32, 708, 48 },
    { 704, 128, 15, 32, 708, 48 },
    { 704, 80, 15, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel08Injured[FACE_REGION_COUNT] = {
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { 768, 80, 22, 48, 704, 0 },
    { 720, 128, 16, 32, 708, 48 },
    { 704, 128, 15, 32, 708, 48 },
    { 704, 80, 22, 48, 704, 0 },
};

FaceBlitRegion gFaceRegionsModel09Normal[FACE_REGION_COUNT] = {
    { 720, 140, 16, 48, 708, 0 },
    { 736, 140, 16, 48, 708, 0 },
    { 720, 188, 16, 32, 708, 48 },
    { 704, 188, 16, 32, 708, 48 },
    { 704, 140, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel09Injured[FACE_REGION_COUNT] = {
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { 768, 84, 34, 44, 704, 0 },
    { 720, 184, 16, 36, 708, 44 },
    { 704, 184, 16, 36, 708, 44 },
    { 704, 140, 34, 44, 704, 0 },
};

FaceBlitRegion gFaceRegionsModel10Normal[FACE_REGION_COUNT] = {
    { 720, 80, 16, 48, 708, 0 },
    { 736, 80, 16, 48, 708, 0 },
    { 720, 128, 16, 32, 708, 48 },
    { 704, 128, 16, 32, 708, 48 },
    { 704, 80, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel10Injured[FACE_REGION_COUNT] = {
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { 768, 84, 34, 44, 704, 0 },
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { 704, 80, 34, 44, 704, 0 },
};

FaceBlitRegion gFaceRegionsModel11Normal[FACE_REGION_COUNT] = {
    { 720, 80, 16, 48, 708, 0 },
    { 736, 80, 16, 48, 708, 0 },
    { 720, 128, 16, 32, 708, 48 },
    { 704, 128, 16, 32, 708, 48 },
    { 704, 80, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel11Injured[FACE_REGION_COUNT] = {
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { 704, 160, 34, 44, 704, 0 },
    { 720, 124, 16, 36, 708, 44 },
    { 704, 124, 16, 36, 708, 44 },
    { 704, 80, 34, 44, 704, 0 },
};

FaceBlitRegion gFaceRegionsModel00Normal[FACE_REGION_COUNT] = {
    { 724, 164, 17, 48, 708, 0 },
    { 744, 164, 17, 48, 708, 0 },
    { 724, 212, 17, 32, 708, 48 },
    { 704, 212, 17, 32, 708, 48 },
    { 704, 164, 17, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel01Normal[FACE_REGION_COUNT] = {
    { 720, 140, 16, 48, 708, 0 },
    { 736, 140, 16, 48, 708, 0 },
    { 720, 188, 16, 32, 708, 48 },
    { 704, 188, 16, 32, 708, 48 },
    { 704, 140, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel02Normal[FACE_REGION_COUNT] = {
    { 720, 160, 16, 48, 708, 0 },
    { 736, 160, 16, 48, 708, 0 },
    { 720, 208, 16, 32, 708, 48 },
    { 704, 208, 16, 32, 708, 48 },
    { 704, 160, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel02Injured[FACE_REGION_COUNT] = {
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { 768, 80, 22, 48, 704, 0 },
    { 720, 208, 16, 32, 708, 48 },
    { 704, 208, 16, 32, 708, 48 },
    { 704, 160, 22, 48, 704, 0 },
};

FaceBlitRegion gFaceRegionsModel03Normal[FACE_REGION_COUNT] = {
    { 720, 88, 16, 48, 708, 0 },
    { 736, 88, 16, 48, 708, 0 },
    { 720, 136, 16, 32, 708, 48 },
    { 704, 136, 16, 32, 708, 48 },
    { 704, 88, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel04Normal[FACE_REGION_COUNT] = {
    { 720, 80, 16, 48, 708, 0 },
    { 736, 80, 16, 48, 708, 0 },
    { 720, 128, 16, 32, 708, 48 },
    { 704, 128, 16, 32, 708, 48 },
    { 704, 80, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel05Normal[FACE_REGION_COUNT] = {
    { 720, 140, 16, 48, 708, 0 },
    { 736, 140, 16, 48, 708, 0 },
    { 720, 188, 16, 32, 708, 48 },
    { 704, 188, 16, 32, 708, 48 },
    { 704, 140, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel13Normal[FACE_REGION_COUNT] = {
    { 720, 168, 16, 48, 708, 0 },
    { 736, 168, 16, 48, 708, 0 },
    { 720, 216, 16, 32, 708, 48 },
    { 704, 216, 16, 32, 708, 48 },
    { 704, 168, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel14Normal[FACE_REGION_COUNT] = {
    { 720, 140, 16, 48, 707, 0 },
    { 736, 140, 16, 48, 707, 0 },
    { 720, 188, 16, 32, 707, 48 },
    { 704, 188, 16, 32, 707, 48 },
    { 704, 140, 16, 80, 707, 0 },
};

FaceBlitRegion gFaceRegionsModel15Normal[FACE_REGION_COUNT] = {
    { 656, 136, 16, 48, 707, 0 },
    { 672, 136, 16, 48, 707, 0 },
    { 656, 184, 16, 32, 707, 48 },
    { 640, 184, 16, 32, 707, 48 },
    { 640, 136, 16, 80, 707, 0 },
};

FaceBlitRegion gFaceRegionsModel16Normal[FACE_REGION_COUNT] = {
    { 720, 140, 16, 48, 708, 0 },
    { 736, 140, 16, 48, 708, 0 },
    { 720, 188, 16, 32, 708, 48 },
    { 704, 188, 16, 32, 708, 48 },
    { 704, 140, 16, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsModel17Normal[FACE_REGION_COUNT] = {
    { 740, 0, 13, 48, 708, 0 },
    { 754, 0, 13, 48, 708, 0 },
    { 740, 48, 13, 32, 708, 48 },
    { 726, 48, 13, 32, 708, 48 },
    { 726, 0, 13, 80, 708, 0 },
};

FaceBlitRegion gFaceRegionsEmpty[FACE_REGION_COUNT] = {
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
    { FACE_REGION_NONE, 0, 0, 0, 0, 0 },
};

/* Indexed by [fighterId][variant] (0..18; models 12 and 18 have no facial blits). */
FaceBlitRegion *gFaceRegionTables[FACE_REGION_MODEL_COUNT][FACE_REGION_VARIANT_COUNT] = {
    { gFaceRegionsModel00Normal, gFaceRegionsEmpty },
    { gFaceRegionsModel01Normal, gFaceRegionsEmpty },
    { gFaceRegionsModel02Normal, gFaceRegionsModel02Injured },
    { gFaceRegionsModel03Normal, gFaceRegionsEmpty },
    { gFaceRegionsModel04Normal, gFaceRegionsEmpty },
    { gFaceRegionsModel05Normal, gFaceRegionsEmpty },
    { gFaceRegionsModel06Normal, gFaceRegionsModel06Injured },
    { gFaceRegionsModel07Normal, gFaceRegionsModel07Injured },
    { gFaceRegionsModel08Normal, gFaceRegionsModel08Injured },
    { gFaceRegionsModel09Normal, gFaceRegionsModel09Injured },
    { gFaceRegionsModel10Normal, gFaceRegionsModel10Injured },
    { gFaceRegionsModel11Normal, gFaceRegionsModel11Injured },
    { gFaceRegionsEmpty, gFaceRegionsEmpty },
    { gFaceRegionsModel13Normal, gFaceRegionsEmpty },
    { gFaceRegionsModel14Normal, gFaceRegionsEmpty },
    { gFaceRegionsModel15Normal, gFaceRegionsEmpty },
    { gFaceRegionsModel16Normal, gFaceRegionsEmpty },
    { gFaceRegionsModel17Normal, gFaceRegionsEmpty },
    { gFaceRegionsEmpty, gFaceRegionsEmpty },
};

static void QueueFaceTextureBlit(int player, int fighterId, int regionIndex)
{
    int bank;
    short *region;
    RECT sourceRect;
    short dstX, dstY;

    if (fighterId >= FACE_REGION_MODEL_COUNT)
        return;
    bank = gFrameCounter & 1;
    if (bank != gFaceBlitLastBank) {
        gFaceBlitCursor = gFaceBlitPackets[bank];
        gFaceBlitLastBank = bank;
    }
    if (gFaceBlitCursor == gFaceBlitPackets[gFaceBlitLastBank] + FACE_BLIT_MAX_PACKETS)
        return;
    if (GetFighterInjuryOverlayMask(player) & INJURY_OVERLAY_HEAD_TORSO)
        region = gFaceRegionTables[fighterId][FACE_REGION_VARIANT_INJURED][0];
    else
        region = gFaceRegionTables[fighterId][FACE_REGION_VARIANT_NORMAL][0];
    region += regionIndex * FACE_REGION_STRIDE;
    if (*region == FACE_REGION_NONE)
        return;
    sourceRect.x = *region++;
    sourceRect.y = *region++;
    sourceRect.w = *region++;
    sourceRect.h = *region++;
    dstX = *region++;
    dstY = *region;
    if (player) {
        sourceRect.x += TPAGE_P2_X_OFFSET;
        dstX += TPAGE_P2_X_OFFSET;
    }
    SetDrawMove(gFaceBlitCursor, &sourceRect, dstX, dstY);
    addPrim(&gCurrentOt[FACE_BLIT_OT_BUCKET], gFaceBlitCursor);
    gFaceBlitCursor++;
}

void SetFighterFacialExpression(int player, int fighterId, int expression)
{
    switch (expression) {
    case FIGHTER_FACE_NORMAL:
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_EYES_NORMAL);
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_MOUTH_CLOSED);
        break;
    case FIGHTER_FACE_DEAD:
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_EYES_CLOSED);
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_MOUTH_CLOSED);
        break;
    case FIGHTER_FACE_LETHAL:
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_EYES_TENSE);
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_MOUTH_CLOSED);
        break;
    case FIGHTER_FACE_NORMAL_OPEN_MOUTH:
        /* OT insertion is LIFO: queue mouth first so EYES_NORMAL (full face when uninjured) draws before it. */
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_MOUTH_OPEN);
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_EYES_NORMAL);
        break;
    case FIGHTER_FACE_CLOSED_EYES_OPEN_MOUTH:
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_EYES_CLOSED);
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_MOUTH_OPEN);
        break;
    case FIGHTER_FACE_WOUND:
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_EYES_TENSE);
        QueueFaceTextureBlit(player, fighterId, FACE_REGION_MOUTH_OPEN);
        break;
    }
}
