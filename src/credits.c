/* The staff credits roll: TIM strips scrolled up the screen between faded top and bottom bands,
 * followed by the post-credits end logo faded in and out. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "display_buffer.h"
#include "gpu_code.h"
#include "packet_build.h"
#include "model_arena.h"
#include "tim.h"
#include "sprite2d.h"
#include "asset_load.h"
#include "scene_light.h"
#include "main.h"
#include "stage_light.h"
#include "credits.h"
#include "globals.h"

#define CD_FILE_CREDITS_TIMS        0x63 /* DATA3\O18.dat */
#define CREDITS_SCREEN_CENTER_X     (DISPLAY_WIDTH / 2)
#define CREDITS_FADE_BAND_HEIGHT    96
#define CREDITS_BOTTOM_FADE_START_Y (DISPLAY_HEIGHT - CREDITS_FADE_BAND_HEIGHT)
#define CREDITS_OT_LAYER            10
#define CREDITS_END_FADE_IN_FRAMES  128
#define CREDITS_END_HOLD_END_FRAME  192
#define CREDITS_END_TOTAL_FRAMES    320
#define CREDITS_MAX_TILE_WIDTH      256
#define CREDITS_TPAGE_4BIT          0
#define CREDITS_ABR_ADD             1
#define CREDITS_ABR_SUB             2

/* Scrolling staff credits entry describing one TIM strip in VRAM and its initial scroll Y. */
typedef struct CreditsScrollEntry {
    short vramX;  /* VRAM X in halfwords (<= 0 terminates gCreditsEntries) */
    short vramY;
    short clutX;
    short clutY;
    short width;  /* Pixel width (split into two quads when >= 256) */
    short height;
    short startY;
} CreditsScrollEntry;

/* The same strips in every build, each with its own start in the scroll: Japan's, Europe's, the
 * USA's. The USA build has three strips more. */
#if defined(VERSION_JP)
#define CREDITS_START_Y(japan, europe, usa) (japan)
#elif defined(VERSION_EU)
#define CREDITS_START_Y(japan, europe, usa) (europe)
#else
#define CREDITS_START_Y(japan, europe, usa) (usa)
#endif

CreditsScrollEntry gCreditsEntries[] = {
    { 768, 256, 1008, 239, 416, 16, 240 },
    { 768, 320, 1008, 238, 224, 16, CREDITS_START_Y(368, 306, 304) },
    { 768, 88, 1008, 255, 304, 64, CREDITS_START_Y(394, 328, 330) },
    { 732, 324, 1008, 237, 104, 16, CREDITS_START_Y(558, 425, 430) },
    { 768, 288, 1008, 254, 316, 16, CREDITS_START_Y(584, 446, 456) },
    { 712, 256, 1008, 236, 128, 32, CREDITS_START_Y(700, 530, 556) },
    { 896, 0, 1008, 252, 296, 80, CREDITS_START_Y(756, 576, 612) },
    { 640, 320, 1008, 251, 272, 80, CREDITS_START_Y(856, 660, 712) },
    { 896, 80, 1008, 250, 316, 96, CREDITS_START_Y(956, 743, 812) },
    { 704, 468, 1008, 235, 160, 16, CREDITS_START_Y(1140, 883, 964) },
    { 896, 176, 1008, 247, 320, 64, CREDITS_START_Y(1166, 905, 990) },
    { 736, 340, 1008, 234, 80, 16, CREDITS_START_Y(1330, 1028, 1138) },
    { 640, 400, 1008, 253, 248, 104, CREDITS_START_Y(1356, 1050, 1164) },
    { 708, 356, 1008, 233, 144, 16, CREDITS_START_Y(1560, 1206, 1336) },
    { 640, 0, 1008, 249, 308, 196, CREDITS_START_Y(1586, 1228, 1362) },
    { 640, 256, 1008, 248, 276, 64, CREDITS_START_Y(1794, 1433, 1570) },
    { 708, 372, 1008, 232, 160, 16, CREDITS_START_Y(1962, 1528, 1706) },
    { 708, 324, 1008, 231, 88, 16, CREDITS_START_Y(1988, 1550, 1732) },
    { 768, 388, 1008, 246, 224, 36, CREDITS_START_Y(2014, 1571, 1758) },
    { 768, 152, 1008, 245, 280, 103, CREDITS_START_Y(2068, 1616, 1812) },
    { 708, 388, 1008, 230, 136, 16, CREDITS_START_Y(2224, 1786, 1984) },
    { 704, 420, 1008, 244, 216, 16, CREDITS_START_Y(2250, 1808, 2010) },
    { 708, 340, 1008, 229, 104, 16, CREDITS_START_Y(2350, 1891, 2094) },
    { 768, 336, 1008, 243, 216, 16, CREDITS_START_Y(2376, 1913, 2120) },
    { 704, 452, 1008, 228, 168, 16, CREDITS_START_Y(2428, 1956, 2172) },
    { 768, 304, 1008, 242, 288, 16, CREDITS_START_Y(2454, 1978, 2198) },
    { 704, 404, 1008, 227, 240, 16, CREDITS_START_Y(2570, 2075, 2266) },
    { 768, 352, 1008, 241, 320, 48, CREDITS_START_Y(2596, 2096, 2292) },
#ifndef VERSION_JP
#ifndef VERSION_EU
    { 768, 480, 1008, 223, 340, 16, 2398 },
    { 964, 256, 1008, 222, 232, 180, 2420 },
    { 896, 256, 1008, 221, 264, 112, 2606 },
#endif
#endif
    { 704, 436, 1008, 226, 204, 16, CREDITS_START_Y(2744, 2326, 2744) },
    { 768, 0, 1008, 240, 396, 88, CREDITS_START_Y(2770, 2348, 2770) },
    { 768, 272, 1008, 225, 436, 16, CREDITS_START_Y(2906, 2461, 2906) },
    { -1, 0, 0, 0, 0, 0, 0 },
};

/* Scratch single-rect sprite populated per visible credits strip in DrawCreditsEntry. */
TexturedRectEntry gCreditsEntrySprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 0, 0, 0, 0,
};

/* Centered 120x56 post-credits end logo sprite (faded via texture RGB after the scroll finishes). */
TexturedRectEntry gCreditsEndLogoSprite[1] = {
    {
        { 128, 128, 128, GPU_CODE_POLY_FT4,
          0, 168, GET_CLUT(1008, 224),
          120, 168, GET_TPAGE(CREDITS_TPAGE_4BIT, CREDITS_ABR_ADD, 768, 256),
          0, 224, 120, 224 },
        (int)(TEXTURED_RECT_LAST_GROUP | 1), 260, 92, 120, 56,
    },
};

static POLY_G4 gCreditsFadeMasks[2][DISPLAY_BUFFER_COUNT]; /* [0]=top, [1]=bottom; double-buffered */
static DR_MODE gCreditsFadeModes[DISPLAY_BUFFER_COUNT];    /* subtractive blend mode; double-buffered */

static int gCreditsScrollY;
static int gCreditsEndFadeTimer;

static void DrawCreditsEntry(CreditsScrollEntry *entry);

void InitCreditsRoll(void)
{
    unsigned short *resource;
    unsigned short tpage;
    POLY_G4 *quad;

    ResetModelArena();
    resource = GetArenaFreePtr();
    ReadCdFileSync(CD_FILE_CREDITS_TIMS, resource);
    LoadTimGroup(resource, 0);
    DrawSync(0);
    gCreditsScrollY = 0;
    gCreditsEndFadeTimer = 0;

    quad = &gCreditsFadeMasks[0][0];
    SetPolyG4(quad);
    SetSemiTrans(quad, 1);
    quad->r0 = quad->r1 = 255;
    quad->g0 = quad->g1 = 255;
    quad->b0 = quad->b1 = 255;
    quad->r2 = quad->r3 = 0;
    quad->g2 = quad->g3 = 0;
    quad->b2 = quad->b3 = 0;
    setXY4(quad, 0, 0, DISPLAY_WIDTH, 0, 0, CREDITS_FADE_BAND_HEIGHT,
           DISPLAY_WIDTH, CREDITS_FADE_BAND_HEIGHT);

    gCreditsFadeMasks[0][1] = gCreditsFadeMasks[1][0] = *quad;
    quad = &gCreditsFadeMasks[1][0];
    setXY4(quad, 0, DISPLAY_HEIGHT, DISPLAY_WIDTH, DISPLAY_HEIGHT,
           0, CREDITS_BOTTOM_FADE_START_Y, DISPLAY_WIDTH, CREDITS_BOTTOM_FADE_START_Y);
    quad[1] = quad[0];

    tpage = GetTPage(CREDITS_TPAGE_4BIT, CREDITS_ABR_SUB, 0, 0);
    SetDrawMode(&gCreditsFadeModes[0], 1, 1, tpage, 0);
    gCreditsFadeModes[1] = gCreditsFadeModes[0];
    SetStageClearAndFogColor(0);
}

int UpdateAndDrawCreditsRoll(void)
{
    CreditsScrollEntry *entry = gCreditsEntries;
    int visibleCount;
    int timer;
    u_char shade;
    int buffer;

    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);
    visibleCount = 0;
    while (entry->vramX > 0) {
        if (entry->startY - gCreditsScrollY <= DISPLAY_HEIGHT &&
            entry->startY + entry->height - gCreditsScrollY >= 0) {
            DrawCreditsEntry(entry);
            visibleCount++;
        }
        entry++;
    }
    gCreditsScrollY++;
    if (visibleCount == 0) {
        timer = ++gCreditsEndFadeTimer;
        if (timer <= CREDITS_END_FADE_IN_FRAMES) {
            shade = timer;
        } else if (timer <= CREDITS_END_HOLD_END_FRAME) {
            shade = CREDITS_END_FADE_IN_FRAMES;
        } else if (timer <= CREDITS_END_TOTAL_FRAMES) {
            shade = CREDITS_END_TOTAL_FRAMES - timer;
        } else {
            shade = 0;
            gCreditsEndFadeTimer = CREDITS_END_TOTAL_FRAMES;
        }
        gCreditsEndLogoSprite->texture.b = shade;
        gCreditsEndLogoSprite->texture.g = shade;
        gCreditsEndLogoSprite->texture.r = shade;
        DrawTexturedRectList(gCreditsEndLogoSprite, 0, 0, CREDITS_OT_LAYER);
    }
    buffer = gFrameCounter & 1;
    addPrim(gCurrentOt, &gCreditsFadeMasks[0][buffer]);
    addPrim(gCurrentOt, &gCreditsFadeMasks[1][buffer]);
    addPrim(gCurrentOt, &gCreditsFadeModes[buffer]);
    return gCreditsEndFadeTimer != CREDITS_END_TOTAL_FRAMES;
}

static void DrawCreditsEntry(CreditsScrollEntry *entry)
{
    TexturedRectEntry *sprite = &gCreditsEntrySprite;
    short vramX;
    int xOffset;
    short width;

    if (entry->width >= CREDITS_MAX_TILE_WIDTH) {
        setUV4(&sprite->texture,
               entry->vramX % 64 * 4, entry->vramY % 256,
               entry->vramX % 64 * 4 + (CREDITS_MAX_TILE_WIDTH - 1), entry->vramY % 256,
               entry->vramX % 64 * 4, (entry->vramY + entry->height) % 256,
               entry->vramX % 64 * 4 + (CREDITS_MAX_TILE_WIDTH - 1), (entry->vramY + entry->height) % 256);
        sprite->texture.clut = GET_CLUT(entry->clutX, entry->clutY);
        sprite->texture.tpage = GET_TPAGE(CREDITS_TPAGE_4BIT, CREDITS_ABR_ADD, entry->vramX, entry->vramY);
        sprite->x = CREDITS_SCREEN_CENTER_X - entry->width / 2;
        sprite->y = entry->startY - gCreditsScrollY;
        sprite->w = CREDITS_MAX_TILE_WIDTH;
        sprite->h = entry->height;
        DrawTexturedRectList(sprite, 0, 0, CREDITS_OT_LAYER);
        xOffset = CREDITS_MAX_TILE_WIDTH;
        vramX = entry->vramX + 64;
        width = entry->width - CREDITS_MAX_TILE_WIDTH;
    } else {
        width = entry->width;
        vramX = entry->vramX;
        xOffset = 0;
    }
    setUV4(&sprite->texture,
           vramX % 64 * 4, entry->vramY % 256,
           width + vramX % 64 * 4, entry->vramY % 256,
           vramX % 64 * 4, (entry->vramY + entry->height) % 256,
           width + vramX % 64 * 4, (entry->vramY + entry->height) % 256);
    sprite->texture.clut = GET_CLUT(entry->clutX, entry->clutY);
    sprite->texture.tpage = GET_TPAGE(CREDITS_TPAGE_4BIT, CREDITS_ABR_ADD, vramX, entry->vramY);
    sprite->x = CREDITS_SCREEN_CENTER_X - entry->width / 2 + xOffset;
    sprite->y = entry->startY - gCreditsScrollY;
    sprite->w = width;
    sprite->h = entry->height;
    DrawTexturedRectList(&gCreditsEntrySprite, 0, 0, CREDITS_OT_LAYER);
}
