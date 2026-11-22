/* The main menu: the scrolling item strip (one sprite list per menu entry, slid in and out over
 * 18 lines) drawn over the logo, frame, arrows and backdrop. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "gpu_code.h"
#include "packet_build.h"
#include "model_arena.h"
#include "tim.h"
#include "sprite2d.h"
#include "asset_load.h"
#include "scene_light.h"
#include "main.h"
#include "main_menu_ui.h"
#include "stage_light.h"
#include "globals.h"

#define CD_FILE_MAIN_MENU_TIMS      0x5d /* DATA3\O12.dat */

#define MAIN_MENU_ITEM_COUNT_BASE   8
#define MAIN_MENU_ITEM_COUNT_ANALOG 9
#define MAIN_MENU_ITEM_ANALOG       8
#define MAIN_MENU_ITEM_HEIGHT       18
#define MAIN_MENU_ITEM_Y            189
#define MAIN_MENU_SCROLL_STEP       4
#define MAIN_MENU_SCROLL_UP         (-1)
#define MAIN_MENU_SCROLL_DOWN       1

/* Static main-menu layers: title logo (2 halves), 8-piece item frame, inner/fullscreen backdrop, up/down arrows. */
TexturedRectEntry gMainMenuLogoSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(1008, 424), 255, 0,
        GET_TPAGE(0, 0, 896, 256), 0, 128, 255, 128 }, 1, 83, 33, 256, 128 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(1008, 424), 224, 0,
        GET_TPAGE(0, 0, 960, 256), 0, 128, 224, 128 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 339, 33, 225, 128 },
};
TexturedRectEntry gMainMenuFrameSprites[8] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 152, 142, GET_CLUT(1008, 427), 168, 142,
        GET_TPAGE(0, 0, 960, 256), 152, 150, 168, 150 }, 1, 112, 180, 16, 8 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 167, 142, GET_CLUT(1008, 427), 151, 142,
        GET_TPAGE(0, 0, 960, 256), 167, 150, 151, 150 }, 1, 512, 180, 16, 8 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 152, 149, GET_CLUT(1008, 427), 168, 149,
        GET_TPAGE(0, 0, 960, 256), 152, 141, 168, 141 }, 1, 112, 208, 16, 8 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 167, 149, GET_CLUT(1008, 427), 151, 149,
        GET_TPAGE(0, 0, 960, 256), 167, 141, 151, 141 }, 1, 512, 208, 16, 8 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 176, 142, GET_CLUT(992, 427), 183, 142,
        GET_TPAGE(0, 0, 960, 256), 176, 146, 183, 146 }, 1, 128, 180, 384, 4 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 176, 145, GET_CLUT(992, 427), 183, 145,
        GET_TPAGE(0, 0, 960, 256), 176, 141, 183, 141 }, 1, 128, 212, 384, 4 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 168, 142, GET_CLUT(1008, 428), 176, 142,
        GET_TPAGE(0, 0, 960, 256), 168, 145, 176, 145 }, 1, 112, 188, 8, 20 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 175, 142, GET_CLUT(1008, 428), 167, 142,
        GET_TPAGE(0, 0, 960, 256), 175, 145, 167, 145 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 520, 188, 8, 20 },
};
TexturedRectEntry gMainMenuBackdropSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 136, 128, GET_CLUT(992, 434), 143, 128,
        GET_TPAGE(0, 0, 960, 256), 136, 135, 143, 135 }, 1, 120, 184, 400, 28 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 144, 128, GET_CLUT(1008, 435), 151, 128,
        GET_TPAGE(0, 0, 960, 256), 144, 135, 151, 135 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 0, 0, 640, 240 },
};
TexturedRectEntry gMainMenuArrowSprites[4] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 144, 150, GET_CLUT(992, 428), 152, 150,
        GET_TPAGE(0, 0, 960, 256), 144, 162, 152, 162 }, 1, 312, 174, 8, 12 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 151, 150, GET_CLUT(992, 428), 143, 150,
        GET_TPAGE(0, 0, 960, 256), 151, 162, 143, 162 }, 1, 320, 174, 8, 12 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 144, 161, GET_CLUT(992, 428), 152, 161,
        GET_TPAGE(0, 0, 960, 256), 144, 149, 152, 149 }, 1, 312, 210, 8, 12 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 151, 161, GET_CLUT(992, 428), 143, 161,
        GET_TPAGE(0, 0, 960, 256), 151, 149, 143, 149 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 320, 210, 8, 12 },
};
/* Per-item label sprites (JP labels wider than 255 px split across two rects; US/EU each fit in one). */
#ifdef VERSION_JP
TexturedRectEntry gMainMenuStorySprite[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 182, GET_CLUT(992, 432), 255, 182,
        GET_TPAGE(0, 1, 896, 256), 0, 200, 255, 200 }, 1, 140, 189, 255, 18 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 182, GET_CLUT(992, 432), 96, 182,
        GET_TPAGE(0, 1, 960, 256), 0, 200, 96, 200 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 395, 189, 96, 18 },
};
TexturedRectEntry gMainMenuVsSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 236, GET_CLUT(992, 433), 236, 236,
        GET_TPAGE(0, 1, 896, 256), 0, 254, 236, 254 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 203, 189, 236, 18 },
};
TexturedRectEntry gMainMenuSlashSprite[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 164, GET_CLUT(992, 429), 255, 164,
        GET_TPAGE(0, 1, 896, 256), 0, 182, 255, 182 }, 1, 140, 189, 255, 18 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 164, GET_CLUT(992, 429), 96, 164,
        GET_TPAGE(0, 1, 960, 256), 0, 182, 96, 182 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 395, 189, 96, 18 },
};
TexturedRectEntry gMainMenuPovSprite[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 146, GET_CLUT(1008, 434), 255, 146,
        GET_TPAGE(0, 1, 896, 256), 0, 164, 255, 164 }, 1, 124, 189, 255, 18 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 146, GET_CLUT(1008, 434), 136, 146,
        GET_TPAGE(0, 1, 960, 256), 0, 164, 136, 164 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 379, 189, 136, 18 },
};
TexturedRectEntry gMainMenuLinkSprite[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 200, GET_CLUT(992, 430), 255, 200,
        GET_TPAGE(0, 1, 896, 256), 0, 218, 255, 218 }, 1, 153, 189, 255, 18 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 200, GET_CLUT(992, 430), 80, 200,
        GET_TPAGE(0, 1, 960, 256), 0, 218, 80, 218 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 408, 189, 80, 18 },
};
TexturedRectEntry gMainMenuPracticeSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 218, GET_CLUT(1008, 433), 236, 218,
        GET_TPAGE(0, 1, 896, 256), 0, 236, 236, 236 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 203, 189, 236, 18 },
};
TexturedRectEntry gMainMenuOptionsSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 218, GET_CLUT(1008, 431), 236, 218,
        GET_TPAGE(0, 1, 960, 256), 0, 236, 236, 236 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 203, 189, 236, 18 },
};
TexturedRectEntry gMainMenuAnalogSprite[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 128, GET_CLUT(1008, 429), 255, 128,
        GET_TPAGE(0, 1, 896, 256), 0, 146, 255, 146 }, 1, 125, 189, 255, 18 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 128, GET_CLUT(1008, 429), 136, 128,
        GET_TPAGE(0, 1, 960, 256), 0, 146, 136, 146 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 380, 189, 136, 18 },
};
TexturedRectEntry gMainMenuMovieTheaterSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 236, GET_CLUT(1008, 430), 188, 236,
        GET_TPAGE(0, 1, 960, 256), 0, 254, 188, 254 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 227, 189, 188, 18 },
};
#else
TexturedRectEntry gMainMenuStorySprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 182, GET_CLUT(992, 432), 208, 182,
        GET_TPAGE(0, 1, 896, 256), 0, 200, 208, 200 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 216, 189, 208, 18 },
};
TexturedRectEntry gMainMenuVsSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 236, GET_CLUT(992, 433), 192, 236,
        GET_TPAGE(0, 1, 896, 256), 0, 254, 192, 254 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 224, 189, 192, 18 },
};
TexturedRectEntry gMainMenuSlashSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 164, GET_CLUT(992, 429), 240, 164,
        GET_TPAGE(0, 1, 896, 256), 0, 182, 240, 182 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 200, 189, 240, 18 },
};
TexturedRectEntry gMainMenuPovSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 146, GET_CLUT(1008, 434), 208, 146,
        GET_TPAGE(0, 1, 896, 256), 0, 164, 208, 164 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 216, 189, 208, 18 },
};
TexturedRectEntry gMainMenuLinkSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 200, GET_CLUT(992, 430), 224, 200,
        GET_TPAGE(0, 1, 896, 256), 0, 218, 224, 218 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 208, 189, 224, 18 },
};
TexturedRectEntry gMainMenuPracticeSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 218, GET_CLUT(1008, 433), 255, 218,
        GET_TPAGE(0, 1, 896, 256), 0, 236, 255, 236 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 192, 189, 255, 18 },
};
TexturedRectEntry gMainMenuOptionsSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 218, GET_CLUT(1008, 431), 240, 218,
        GET_TPAGE(0, 1, 960, 256), 0, 236, 240, 236 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 200, 189, 240, 18 },
};
TexturedRectEntry gMainMenuAnalogSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 128, GET_CLUT(1008, 429), 224, 128,
        GET_TPAGE(0, 1, 896, 256), 0, 146, 224, 146 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 208, 189, 224, 18 },
};
TexturedRectEntry gMainMenuMovieTheaterSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 236, GET_CLUT(1008, 430), 128, 236,
        GET_TPAGE(0, 1, 960, 256), 0, 254, 128, 254 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 256, 189, 128, 18 },
};
#endif

/* Indexed by main-menu selection 0..8 (matching gMainMenuEntries[0..8]). */
TexturedRectEntry *gMainMenuItemSprites[MAIN_MENU_ITEM_COUNT_ANALOG] = {
    gMainMenuStorySprite,
    gMainMenuVsSprite,
    gMainMenuSlashSprite,
    gMainMenuPracticeSprite,
    gMainMenuPovSprite,
    gMainMenuLinkSprite,
    gMainMenuMovieTheaterSprite,
    gMainMenuOptionsSprite,
    gMainMenuAnalogSprite,
};
static int gMainMenuScrollRemaining;
static int gMainMenuScrollDir;
static TexturedRectEntry *gMainMenuIncomingItem;
static TexturedRectEntry *gMainMenuOutgoingItem;
static int gMainMenuIndex;
static int gMainMenuItemCount;

int UpdateAndDrawMainMenu(int move, int confirm, int analog)
{
    RECT window; /* not used */
    DRAWENV env; /* not used */
    int oldIndex;

    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);
#ifndef VERSION_JP
    analog = 0;
#endif
    gMainMenuItemCount = analog ? MAIN_MENU_ITEM_COUNT_ANALOG : MAIN_MENU_ITEM_COUNT_BASE;
    if (gMainMenuScrollRemaining <= 0) {
        if (!analog && gMainMenuIndex == MAIN_MENU_ITEM_ANALOG) {
            move = MAIN_MENU_SCROLL_UP;
        }
        switch (move) {
        case MAIN_MENU_SCROLL_DOWN:
            oldIndex = gMainMenuIndex;
            if ((gMainMenuIndex = oldIndex + 1) >= gMainMenuItemCount) {
                gMainMenuIndex = 0;
            }
            break;
        case MAIN_MENU_SCROLL_UP:
            oldIndex = gMainMenuIndex;
            if ((gMainMenuIndex = oldIndex - 1) < 0) {
                gMainMenuIndex = gMainMenuItemCount - 1;
            }
            break;
        }
        if (move != 0) {
            gMainMenuScrollDir = move;
            gMainMenuScrollRemaining = MAIN_MENU_ITEM_HEIGHT;
            gMainMenuOutgoingItem = gMainMenuItemSprites[oldIndex];
            gMainMenuIncomingItem = gMainMenuItemSprites[gMainMenuIndex];
        }
    }
    if (gMainMenuScrollRemaining <= 0) {
        SetMenuSpriteListYAndHeight(gMainMenuItemSprites[gMainMenuIndex], MAIN_MENU_ITEM_HEIGHT, MAIN_MENU_ITEM_Y);
        DrawTexturedRectList(gMainMenuItemSprites[gMainMenuIndex], 0, 0, 0);
    } else {
        if (gMainMenuScrollDir == MAIN_MENU_SCROLL_DOWN) {
            SetMenuSpriteListYAndHeight(gMainMenuIncomingItem, MAIN_MENU_ITEM_HEIGHT - gMainMenuScrollRemaining,
                                        MAIN_MENU_ITEM_Y + gMainMenuScrollRemaining);
            SetMenuSpriteListYAndHeight(gMainMenuOutgoingItem, gMainMenuScrollRemaining, MAIN_MENU_ITEM_Y);
        } else {
            SetMenuSpriteListYAndHeight(gMainMenuIncomingItem, MAIN_MENU_ITEM_HEIGHT - gMainMenuScrollRemaining,
                                        MAIN_MENU_ITEM_Y);
            SetMenuSpriteListYAndHeight(gMainMenuOutgoingItem, gMainMenuScrollRemaining,
                                        MAIN_MENU_ITEM_Y + MAIN_MENU_ITEM_HEIGHT - gMainMenuScrollRemaining);
        }
        gMainMenuScrollRemaining -= MAIN_MENU_SCROLL_STEP;
        DrawTexturedRectList(gMainMenuIncomingItem, 0, 0, 0);
        DrawTexturedRectList(gMainMenuOutgoingItem, 0, 0, 0);
    }
    DrawTexturedRectList(gMainMenuArrowSprites, 0, 0, 0);
    DrawTexturedRectList(gMainMenuFrameSprites, 0, 0, 0);
    DrawTexturedRectList(gMainMenuLogoSprites, 0, 0, 0);
    DrawTexturedRectList(gMainMenuBackdropSprites, 0, 0, 0);
    return gMainMenuIndex;
}

void SetMenuSpriteListYAndHeight(TexturedRectEntry *spriteList, int height, int y)
{
    spriteList->y = y;
    spriteList->h = height;
    while ((spriteList->countAndFlags & TEXTURED_RECT_LAST_GROUP) == 0) {
        spriteList++;
        spriteList->y = y;
        spriteList->h = height;
    }
}

void InitMainMenu(int initialIndex)
{
    void *loadBuffer;

    ResetModelArena();
    loadBuffer = GetArenaFreePtr();
    ReadCdFileSync(CD_FILE_MAIN_MENU_TIMS, loadBuffer);
    LoadTimGroup(loadBuffer, 0);
    gMainMenuItemCount = MAIN_MENU_ITEM_COUNT_ANALOG;
    if (initialIndex < 0) {
        initialIndex = 0;
    }
    if (initialIndex >= MAIN_MENU_ITEM_COUNT_ANALOG) {
        initialIndex = MAIN_MENU_ITEM_COUNT_ANALOG - 1;
    }
    gMainMenuIndex = initialIndex;
    gMainMenuScrollRemaining = 0;
    SetStageClearAndFogColor(0);
}
