/* Depth fog by CLUT swap: the stage CLUT (and one fighter's, when its fog is on) blended toward
 * the GTE far color in steps, each step a DR_MOVE linked at its depth in the ordering table. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "display_buffer.h"
#include "model_arena.h"
#include "fog.h"
#include "main.h"
#include "otz.h"
#include "stage_id.h"
#include "render.h"

unsigned int gStageFogDepthSpans[STAGE_COUNT] = {
    32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000,
    32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000,
};

FogStageConfig gStageFogConfigs[STAGE_COUNT] = {
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 1, 0 },
    { 0, 0 }, /* stage 23: opening demo */
    { 1, 0 },
    { 1, 0 },
};

int gCharFogEnabled = 0;
/* The fighter slot drawn with the fogged CLUT (-1 none, 0/1 fighter). */
int gCharFogFighterSlot = FOG_CHAR_SLOT_NONE;
int gCharFogTableReady = 0;
static int gMainFogStepCount;
static int gFogFarOtzBucket;           /* OTZ bucket of the last linked fog move, FOG_FAR_OTZ_NONE if none */

static DR_MOVE gMainFogMoves[DISPLAY_BUFFER_COUNT][FOG_MAIN_STEPS_FULL];
static DR_MOVE gCharFogMoves[DISPLAY_BUFFER_COUNT][FOG_CHAR_MOVE_SLOTS];

void QueueStageFogClutMoves(int stageId, int startDepth)
{
    gFogFarOtzBucket = FOG_FAR_OTZ_NONE;
    if (gStageFogConfigs[stageId].enabled != 0) {
        LinkFogClutMoves(startDepth, gStageFogDepthSpans[stageId], gStageFogConfigs[stageId].directLastLink);
        if (gCharFogTableReady != 0) {
            LinkCharFogClutMoves(startDepth, gStageFogDepthSpans[stageId], gStageFogConfigs[stageId].directLastLink);
        }
    }
}

void LinkFogClutMoves(unsigned int depth, unsigned int depthSpan, int directLastLink)
{
    int moveCount;
    int depthStep;
    int moveIndex;
    int bucket;

    if (directLastLink != 0) {
        moveCount = gMainFogStepCount;
    } else {
        moveCount = gMainFogStepCount - 1;
    }

    depthStep = FOG_MAX_DEPTH - depth;
    if (depthStep < 0) {
        return;
    }

    if (depthSpan < depthStep) {
        depthStep = depthSpan / gMainFogStepCount;
    } else {
        depthStep = depthStep / gMainFogStepCount;
    }

    for (moveIndex = 0; moveIndex < moveCount; moveIndex++) {
        bucket = CompressDepthToOtz(depth >> 2, 2);
        if (bucket < OTZ_BUCKET_COUNT) {
            addPrim(&gCurrentOt[bucket], &gMainFogMoves[gFrameCounter & 1][moveIndex]);
            if (moveIndex == moveCount - 1) {
                gFogFarOtzBucket = bucket;
            }
        }
        depth += depthStep;
    }

    if (directLastLink != 0) {
        LinkFinalFogClutDirect();
    } else {
        addPrim(&gCurrentOt[FOG_FINAL_OT_BUCKET], &gMainFogMoves[gFrameCounter & 1][gMainFogStepCount - 1]);
    }
}

void LinkCharFogClutMoves(unsigned int depth, unsigned int depthSpan, int directLastLink)
{
    int moveCount;
    int depthStep;
    int moveIndex;
    int bucket;

    moveCount = FOG_CHAR_STEPS - 1;
    if (directLastLink != 0) {
        moveCount = FOG_CHAR_STEPS;
    }

    depthStep = FOG_MAX_DEPTH - depth;
    if (depthStep < 0) {
        return;
    }

    if (depthSpan < depthStep) {
        depthStep = depthSpan / FOG_CHAR_STEPS;
    } else {
        depthStep = depthStep / FOG_CHAR_STEPS;
    }

    for (moveIndex = 0; moveIndex < moveCount; moveIndex++) {
        bucket = CompressDepthToOtz(depth >> 2, 2);
        if (bucket < OTZ_BUCKET_COUNT) {
            addPrim(&gCurrentOt[bucket], &gCharFogMoves[gFrameCounter & 1][moveIndex]);
        }
        depth += depthStep;
    }

    if (directLastLink == 0) {
        addPrim(&gCurrentOt[FOG_FINAL_OT_BUCKET], &gCharFogMoves[gFrameCounter & 1][FOG_CHAR_STEPS - 1]);
    }
}

void LinkFinalFogClutDirect(void)
{
    gCurrentOt[FOG_FINAL_OT_BUCKET] =
        (u_long)&gMainFogMoves[gFrameCounter & 1][gMainFogStepCount - 1] & 0x00ffffff;
}

void BuildFogClutTable(void)
{
    unsigned short sourceClut[FOG_MAIN_CLUT_WIDTH];
    unsigned short blendedClut[FOG_MAIN_CLUT_WIDTH];
    RECT rect;
    CVECTOR fog;
    int step;
    int x;
    int y;
    int pixel;
    int fogWeight;
    int fogR;
    int fogG;
    int fogB;
    unsigned short fogColor;
    short greenStp;
    unsigned short *blended;
    unsigned short *source;
    unsigned short color;
    int r;
    int g;
    int b;

    if (gCharFogEnabled != 0) {
        gMainFogStepCount = FOG_MAIN_STEPS_SPLIT;
    } else {
        gMainFogStepCount = FOG_MAIN_STEPS_FULL;
    }

    DrawSync(0);
    rect.x = 0;
    rect.y = FOG_MAIN_CLUT_Y;
    rect.w = FOG_MAIN_CLUT_WIDTH;
    rect.h = 1;
    StoreImage(&rect, (u_long *)sourceClut);
    DrawSync(0);

    GetGteFarColor(&fog);
    fogR = fog.r;
    fogG = fog.g;
    fogB = fog.b;
    greenStp = ((fogG & 0xf8) << 2) | 0x8000;
    fogColor = (fog.r >> 3) | greenStp | ((fogB & 0xf8) << 7);

    /* Blend in Q12 across gMainFogStepCount steps, preserving zero (transparent) entries. */
    x = FOG_MAIN_CLUT_WIDTH;
    y = FOG_MAIN_CLUT_Y;
    for (step = 0; step < gMainFogStepCount; step++) {
        fogWeight = (step << 12) / (gMainFogStepCount - 1);
        source = sourceClut;
        blended = blendedClut;
        for (pixel = 0; pixel < FOG_MAIN_CLUT_WIDTH; pixel++) {
            if (step == 0) {
                *blended++ = *source++;
            } else if (step == gMainFogStepCount - 1) {
                if (*source == 0) {
                    *blended++ = *source++;
                } else {
                    *blended++ = fogColor;
                    source++;
                }
            } else {
                if (*source == 0) {
                    *blended++ = *source++;
                } else {
                    color = *source++;
                    r = (color & 0x1f) << 3;
                    r = (r * (ONE - fogWeight) + fogR * fogWeight) >> 12;
                    r >>= 3;
                    r &= 0x1f;
                    g = ((color >> 5) & 0x1f) << 3;
                    g = (g * (ONE - fogWeight) + fogG * fogWeight) >> 12;
                    g >>= 3;
                    g <<= 5;
                    b = ((color >> 10) & 0x1f) << 3;
                    b = (b * (ONE - fogWeight) + fogB * fogWeight) >> 12;
                    b >>= 3;
                    b <<= 10;
                    *blended++ = (color & 0x8000) | r | (g & 0x3e0) | (b & 0x7c00);
                }
            }
        }

        rect.x = x;
        rect.y = y;
        LoadImage(&rect, (u_long *)blendedClut);
        DrawSync(0);
        SetDrawMove(&gMainFogMoves[0][step], &rect, 0, FOG_MAIN_CLUT_Y);
        gMainFogMoves[1][step] = gMainFogMoves[0][step];

        if (x == 0) {
            x = FOG_MAIN_CLUT_WIDTH;
        } else {
            y++;
            x = 0;
        }
    }

    if (gCharFogEnabled != 0) {
        BuildCharFogClutTable();
    }
}

void BuildCharFogClutTable(void)
{
    unsigned short sourceClut[FOG_CHAR_CLUT_PIXELS];
    unsigned short blendedClut[FOG_CHAR_CLUT_PIXELS];
    RECT rect;
    CVECTOR fog;
    unsigned short fogColor;
    short greenStp;
    int step, pixel;
    int x, y;
    int fogWeight;
    unsigned short *source;
    unsigned short *blended;
    unsigned short color;
    int fogR, fogG, fogB;
    int r, g, b;

    DrawSync(0);
    rect.x = 0;
    rect.y = FOG_CHAR_CLUT_Y;
    rect.w = FOG_CHAR_CLUT_WIDTH;
    rect.h = FOG_CHAR_CLUT_HEIGHT;
    StoreImage(&rect, (u_long *)sourceClut);
    DrawSync(0);
    GetGteFarColor(&fog);

    x = FOG_CHAR_CLUT_WIDTH;
    y = FOG_CHAR_CLUT_Y;
    fogR = fog.r;
    fogG = fog.g;
    fogB = fog.b;
    greenStp = ((fogG & 0xf8) << 2) | 0x8000;
    fogColor = (fog.r >> 3) | greenStp | ((fogB & 0xf8) << 7);

    /* Blend in Q12, keeping zero (transparent) entries except at the solid endpoint. */
    for (step = 0; step < FOG_CHAR_STEPS; step++) {
        fogWeight = (step << 12) / (FOG_CHAR_STEPS - 1);
        source = sourceClut;
        blended = blendedClut;
        for (pixel = 0; pixel < FOG_CHAR_CLUT_PIXELS; pixel++) {
            if (step == 0) {
                *blended++ = *source++;
            } else if (step == FOG_CHAR_STEPS - 1) {
                *blended++ = fogColor;
            } else {
                if (*source == 0) {
                    *blended++ = *source++;
                } else {
                    color = *source++;
                    r = (color & 0x1f) << 3;
                    r = (r * (ONE - fogWeight) + fogR * fogWeight) >> 12;
                    r >>= 3;
                    r &= 0x1f;
                    g = ((color >> 5) & 0x1f) << 3;
                    g = (g * (ONE - fogWeight) + fogG * fogWeight) >> 12;
                    g >>= 3;
                    g <<= 5;
                    b = ((color >> 10) & 0x1f) << 3;
                    b = (b * (ONE - fogWeight) + fogB * fogWeight) >> 12;
                    b >>= 3;
                    b <<= 10;
                    *blended++ = (color & 0x8000) | r | (g & 0x3e0) | (b & 0x7c00);
                }
            }
        }
        rect.x = x;
        rect.y = y;
        LoadImage(&rect, (u_long *)blendedClut);
        DrawSync(0);
        SetDrawMove(&gCharFogMoves[0][step], &rect, 0, FOG_CHAR_CLUT_Y);
        gCharFogMoves[1][step] = gCharFogMoves[0][step];
        x += FOG_CHAR_CLUT_WIDTH;
        if (x >= FOG_CHAR_VRAM_LIMIT_X) {
            x = 0;
            y = FOG_CHAR_CLUT_WRAP_Y;
        }
    }
    gCharFogTableReady = 1;
}

void EnableCharFogSlot(int slot)
{
    if (gCharFogFighterSlot != slot) {
        gCharFogTableReady = 0;
        gCharFogEnabled = 1;
        gCharFogFighterSlot = slot;
    }
}

void DisableCharFogSlot(void)
{
    if (gCharFogFighterSlot == CHAR_SLOT_P1) {
        InvalidateArenaGroup(ARENA_GROUP_P1_CHARACTER);
    }
    if (gCharFogFighterSlot == CHAR_SLOT_P2) {
        InvalidateArenaGroup(ARENA_GROUP_P2_CHARACTER);
    }
    gCharFogEnabled = 0;
    gCharFogFighterSlot = FOG_CHAR_SLOT_NONE;
    gCharFogTableReady = 0;
}

int IsCharFogSlotActive(int slot)
{
    return gCharFogFighterSlot == slot;
}

int GetFogFarOtzBucket(void)
{
    return gFogFarOtzBucket;
}
