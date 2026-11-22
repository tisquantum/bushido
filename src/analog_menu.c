/* The Japanese build's Analog Controller menu (rumble on/off and a stick test for both pads); the
 * US and European builds omit this unit. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "analog_menu.h"
#include "asset_load.h"
#include "display_buffer.h"
#include "globals.h"
#include "gpu_code.h"
#include "main.h"
#include "model_arena.h"
#include "packet_build.h"
#include "sprite2d.h"
#include "tim.h"

#ifdef VERSION_JP

#define CD_FILE_ANALOG_CONTROLLER 0x5e /* DATA3\O13.dat */

#define ANALOG_MENU_SHADE_ACTIVE 0x80
#define ANALOG_MENU_SHADE_DIM    0x40
#define ANALOG_MENU_P2_OFFSET_X  296
#define ANALOG_MENU_BACKDROP_Y   32
#define ANALOG_MENU_TW_X         64
#define ANALOG_MENU_TW_Y         224
#define ANALOG_MENU_TW_W         32
#define ANALOG_MENU_TW_H         16

TexturedRectEntry gAnalogMenuHeaderSprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 124, GET_CLUT(672, 252), 232, 124,
        GET_TPAGE(0, 1, 640, 0), 0, 140, 232, 140 }, 1, 204, 22, 232, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 8, 140, GET_CLUT(688, 251), 136, 140,
        GET_TPAGE(0, 0, 640, 0), 8, 176, 136, 176 }, 1, 192, 12, 128, 36 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 135, 175, GET_CLUT(688, 251), 7, 175,
        GET_TPAGE(0, 0, 640, 0), 135, 139, 7, 139 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 320, 12, 128, 36 },
};
TexturedRectEntry gAnalogMenuBackdropSprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 224, GET_CLUT(656, 252), 96, 224,
        GET_TPAGE(0, 0, 640, 0), 64, 240, 96, 240 }, 1, 0, 0, 200, 240 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 224, GET_CLUT(656, 252), 96, 224,
        GET_TPAGE(0, 0, 640, 0), 64, 240, 96, 240 }, 1, 200, 0, 200, 240 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 224, GET_CLUT(656, 252), 96, 224,
        GET_TPAGE(0, 0, 640, 0), 64, 240, 96, 240 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 400, 0, 240, 240 },
};
/* Three 144x40 footer panels across the bottom bar (x = 48, 248, 448; y = 200). */
struct {
    GlyphTexture texture;
    int countAndFlags;
    RECT rects[3];
} gAnalogMenuFooterPanelSprites = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 176, GET_CLUT(672, 251), 144, 176,
        GET_TPAGE(0, 1, 640, 0), 0, 216, 144, 216 },
    (int)(TEXTURED_RECT_LAST_GROUP | 3),
    {
        { 48, 200, 144, 40 },
        { 248, 200, 144, 40 },
        { 448, 200, 144, 40 },
    },
};
/* Mirrored 96x24 header wings flanking the title plate across y = 16..40. */
struct {
    GlyphTexture texture;
    int countAndFlags;
    RECT rects[2];
} gAnalogMenuHeaderWingSprites[2] = {
    {
        { 128, 128, 128, GPU_CODE_POLY_FT4, 136, 140, GET_CLUT(688, 252), 232, 140,
            GET_TPAGE(0, 1, 640, 0), 136, 164, 232, 164 },
        2,
        {
            { 0, 16, 96, 24 },
            { 448, 16, 96, 24 },
        },
    },
    {
        { 128, 128, 128, GPU_CODE_POLY_FT4, 231, 140, GET_CLUT(688, 252), 135, 140,
            GET_TPAGE(0, 1, 640, 0), 231, 164, 135, 164 },
        (int)(TEXTURED_RECT_LAST_GROUP | 2),
        {
            { 96, 16, 96, 24 },
            { 544, 16, 96, 24 },
        },
    },
};
/* 48x40 footer tiles flanking and separating the footer panels at y = 200 (x = 592 repeated 3x). */
struct {
    GlyphTexture texture;
    int countAndFlags;
    RECT rects[6];
} gAnalogMenuFooterTileSprites = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 144, 176, GET_CLUT(656, 251), 192, 176,
        GET_TPAGE(0, 1, 640, 0), 144, 216, 192, 216 },
    (int)(TEXTURED_RECT_LAST_GROUP | 6),
    {
        { 0, 200, 48, 40 },
        { 192, 200, 48, 40 },
        { 400, 200, 48, 40 },
        { 592, 200, 48, 40 },
        { 592, 200, 48, 40 },
        { 592, 200, 48, 40 },
    },
};
/* 8x40 trim strips flanking the center footer panel (x = 240, 392); omits LAST_GROUP so it runs
 * into gAnalogMenuRumbleLabelSprites, which DrawAnalogControllerMenu redraws with tint right after. */
TexturedRectEntry gAnalogMenuFooterTrimSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 192, 176, GET_CLUT(656, 250), 200, 176,
        GET_TPAGE(0, 0, 640, 0), 192, 216, 200, 216 }, 1, 240, 200, 8, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 176, GET_CLUT(672, 250), 208, 176,
        GET_TPAGE(0, 0, 640, 0), 200, 216, 208, 216 }, 1, 392, 200, 8, 40 },
};
TexturedRectEntry gAnalogMenuRumbleLabelSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 108, GET_CLUT(672, 254), 255, 108,
        GET_TPAGE(0, 1, 640, 0), 0, 124, 255, 124 }, 1, 120, 56, 256, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 108, GET_CLUT(672, 254), 16, 108,
        GET_TPAGE(0, 1, 704, 0), 0, 124, 16, 124 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 376, 56, 16, 16 },
};
TexturedRectEntry gAnalogMenuStickTestLabelSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 68, GET_CLUT(672, 253), 255, 68,
        GET_TPAGE(0, 1, 640, 0), 0, 84, 255, 84 }, 1, 120, 96, 256, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 68, GET_CLUT(672, 253), 16, 68,
        GET_TPAGE(0, 1, 704, 0), 0, 84, 16, 84 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 376, 96, 16, 16 },
};
TexturedRectEntry gAnalogMenuRumbleOffSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 232, GET_CLUT(640, 252), 168, 232,
        GET_TPAGE(0, 1, 640, 0), 96, 248, 168, 248 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 496, 56, 72, 16,
};
TexturedRectEntry gAnalogMenuRumbleOnSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 216, GET_CLUT(688, 254), 160, 216,
        GET_TPAGE(0, 1, 640, 0), 96, 232, 160, 232 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 416, 56, 64, 16,
};
TexturedRectEntry gAnalogMenuRumbleSepSprite[1] = {
    { { 64, 64, 64, GPU_CODE_POLY_FT4, 176, 232, GET_CLUT(656, 253), 200, 232,
        GET_TPAGE(0, 1, 640, 0), 176, 248, 200, 248 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 480, 56, 24, 16 },
};
TexturedRectEntry gAnalogMenuStickFrameSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(640, 255), 255, 0,
        GET_TPAGE(1, 0, 640, 0), 0, 65, 255, 65 }, 1, 32, 124, 256, 65 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(640, 255), 24, 0,
        GET_TPAGE(1, 0, 768, 0), 0, 65, 24, 65 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 288, 124, 24, 65 },
};
TexturedRectEntry gAnalogMenuP1LabelSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 216, GET_CLUT(640, 254), 236, 216,
        GET_TPAGE(0, 1, 640, 0), 200, 228, 236, 228 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 32, 177, 36, 12,
};
TexturedRectEntry gAnalogMenuP2LabelSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 216, GET_CLUT(656, 254), 196, 216,
        GET_TPAGE(0, 1, 640, 0), 160, 228, 196, 228 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 328, 177, 36, 12,
};
TexturedRectEntry gAnalogMenuP1LeftStickSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 216, GET_CLUT(640, 253), 56, 216,
        GET_TPAGE(0, 1, 640, 0), 0, 244, 56, 244 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 96, 156, 56, 28 },
};
TexturedRectEntry gAnalogMenuP1RightStickSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 216, GET_CLUT(640, 253), 56, 216,
        GET_TPAGE(0, 1, 640, 0), 0, 244, 56, 244 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 191, 156, 56, 28 },
};
TexturedRectEntry gAnalogMenuP2LeftStickSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 216, GET_CLUT(640, 253), 56, 216,
        GET_TPAGE(0, 1, 640, 0), 0, 244, 56, 244 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 392, 156, 56, 28 },
};
TexturedRectEntry gAnalogMenuP2RightStickSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 216, GET_CLUT(640, 253), 56, 216,
        GET_TPAGE(0, 1, 640, 0), 0, 244, 56, 244 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 487, 156, 56, 28 },
};
TexturedRectEntry gAnalogMenuP1StickGuideSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 16, 68, GET_CLUT(640, 250), 72, 68,
        GET_TPAGE(0, 1, 704, 0), 16, 96, 72, 96 }, 1, 96, 156, 56, 28 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 16, 68, GET_CLUT(640, 250), 72, 68,
        GET_TPAGE(0, 1, 704, 0), 16, 96, 72, 96 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 191, 156, 56, 28 },
};
TexturedRectEntry gAnalogMenuP2StickGuideSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 16, 68, GET_CLUT(640, 250), 72, 68,
        GET_TPAGE(0, 1, 704, 0), 16, 96, 72, 96 }, 1, 392, 156, 56, 28 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 16, 68, GET_CLUT(640, 250), 72, 68,
        GET_TPAGE(0, 1, 704, 0), 16, 96, 72, 96 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 487, 156, 56, 28 },
};
TexturedRectEntry gAnalogMenuP1StickActiveSprites[2] = {
    { { 224, 224, 224, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 84, GET_CLUT(688, 253), 255, 84,
        GET_TPAGE(0, 0, 640, 0), 0, 108, 255, 108 }, 1, 40, 128, 256, 24 },
    { { 224, 224, 224, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 84, GET_CLUT(688, 253), 8, 84,
        GET_TPAGE(0, 0, 704, 0), 0, 108, 8, 108 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 296, 128, 8, 24 },
};
TexturedRectEntry gAnalogMenuP2StickActiveSprites[2] = {
    { { 224, 224, 224, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 84, GET_CLUT(688, 253), 255, 84,
        GET_TPAGE(0, 0, 640, 0), 0, 108, 255, 108 }, 1, 336, 128, 256, 24 },
    { { 224, 224, 224, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 84, GET_CLUT(688, 253), 8, 84,
        GET_TPAGE(0, 0, 704, 0), 0, 108, 8, 108 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 592, 128, 8, 24 },
};
/* Base (x, y) screen positions of the four stick indicators (P1 L/R, P2 L/R); nothing reads or writes them. */
short gAnalogMenuStickBasePos[4][2] = {
    { 96, 156 }, { 191, 156 }, { 392, 156 }, { 487, 156 },
};
/* 8 bytes; nothing reads or writes them. */
int gAnalogMenuUnusedWords[2] = { 0, 0 };

static DR_TWIN gAnalogMenuTexWindowSet[2];
static DR_TWIN gAnalogMenuTexWindowRestore[2];

void DrawAnalogControllerMenu(int flags, int p1LeftX, int p1LeftY, int p1RightX, int p1RightY,
                              int p2LeftX, int p2LeftY, int p2RightX, int p2RightY)
{
    RECT window;
    DRAWENV env;
    u_char shade;

    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);

    p1LeftX &= 0xff;
    p1LeftY &= 0xff;
    p1RightX &= 0xff;
    p1RightY &= 0xff;
    p2LeftX &= 0xff;
    p2LeftY &= 0xff;
    p2RightX &= 0xff;
    p2RightY &= 0xff;

    p1LeftX -= 0x80;
    p1LeftY -= 0x80;
    p1RightX -= 0x80;
    p1RightY -= 0x80;
    p2LeftX -= 0x80;
    p2LeftY -= 0x80;
    p2RightX -= 0x80;
    p2RightY -= 0x80;

    DrawTexturedRectList(gAnalogMenuHeaderSprites, 0, 0, 0);
    DrawTexturedRectList(&gAnalogMenuFooterPanelSprites, 0, 0, 0);
    DrawTexturedRectList(gAnalogMenuHeaderWingSprites, 0, 0, 0);
    DrawTexturedRectList(&gAnalogMenuFooterTileSprites, 0, 0, 0);
    DrawTexturedRectList(gAnalogMenuFooterTrimSprites, 0, 0, 0);

    shade = (flags & ANALOG_MENU_FLAG_ROW_RUMBLE) ? ANALOG_MENU_SHADE_ACTIVE : ANALOG_MENU_SHADE_DIM;
    gAnalogMenuRumbleLabelSprites[0].texture.r = gAnalogMenuRumbleLabelSprites[1].texture.r =
    gAnalogMenuRumbleLabelSprites[0].texture.g = gAnalogMenuRumbleLabelSprites[1].texture.g =
    gAnalogMenuRumbleLabelSprites[0].texture.b = gAnalogMenuRumbleLabelSprites[1].texture.b = shade;

    shade = (flags & ANALOG_MENU_FLAG_RUMBLE_ON) ? ANALOG_MENU_SHADE_ACTIVE : ANALOG_MENU_SHADE_DIM;
    gAnalogMenuRumbleOnSprite.texture.r = gAnalogMenuRumbleOnSprite.texture.g =
    gAnalogMenuRumbleOnSprite.texture.b = shade;

    shade = (flags & ANALOG_MENU_FLAG_RUMBLE_OFF) ? ANALOG_MENU_SHADE_ACTIVE : ANALOG_MENU_SHADE_DIM;
    gAnalogMenuRumbleOffSprite.texture.r = gAnalogMenuRumbleOffSprite.texture.g =
    gAnalogMenuRumbleOffSprite.texture.b = shade;

    DrawTexturedRectList(gAnalogMenuRumbleLabelSprites, 0, 0, 0);
    DrawTexturedRectList(&gAnalogMenuRumbleOnSprite, 0, 0, 0);
    DrawTexturedRectList(gAnalogMenuRumbleSepSprite, 0, 0, 0);
    DrawTexturedRectList(&gAnalogMenuRumbleOffSprite, 0, 0, 0);

    shade = (flags & ANALOG_MENU_FLAG_ROW_STICKS) ? ANALOG_MENU_SHADE_ACTIVE : ANALOG_MENU_SHADE_DIM;
    gAnalogMenuStickTestLabelSprites[0].texture.r = gAnalogMenuStickTestLabelSprites[1].texture.r =
    gAnalogMenuStickTestLabelSprites[0].texture.g = gAnalogMenuStickTestLabelSprites[1].texture.g =
    gAnalogMenuStickTestLabelSprites[0].texture.b = gAnalogMenuStickTestLabelSprites[1].texture.b = shade;
    DrawTexturedRectList(gAnalogMenuStickTestLabelSprites, 0, 0, 0);

    if (flags & ANALOG_MENU_FLAG_P1_ANALOG) {
        DrawTexturedRectList(gAnalogMenuP1StickActiveSprites, 0, 0, 0);
        DrawTexturedRectList(gAnalogMenuP1StickGuideSprites, 0, 0, 0);
        DrawTexturedRectList(gAnalogMenuP1LeftStickSprite, p1LeftX >> 3, p1LeftY >> 4, 0);
        DrawTexturedRectList(gAnalogMenuP1RightStickSprite, p1RightX >> 3, p1RightY >> 4, 0);
        shade = ANALOG_MENU_SHADE_ACTIVE;
    } else {
        shade = ANALOG_MENU_SHADE_DIM;
    }
    gAnalogMenuStickFrameSprites[0].texture.r = gAnalogMenuStickFrameSprites[0].texture.g =
    gAnalogMenuStickFrameSprites[0].texture.b = gAnalogMenuStickFrameSprites[1].texture.r =
    gAnalogMenuStickFrameSprites[1].texture.g = gAnalogMenuStickFrameSprites[1].texture.b = shade;
    gAnalogMenuP1LabelSprite.texture.r = gAnalogMenuP1LabelSprite.texture.g =
    gAnalogMenuP1LabelSprite.texture.b = shade;
    DrawTexturedRectList(&gAnalogMenuP1LabelSprite, 0, 0, 0);
    DrawTexturedRectList(gAnalogMenuStickFrameSprites, 0, 0, 0);

    if (flags & ANALOG_MENU_FLAG_P2_ANALOG) {
        DrawTexturedRectList(gAnalogMenuP2StickActiveSprites, 0, 0, 0);
        DrawTexturedRectList(gAnalogMenuP2StickGuideSprites, 0, 0, 0);
        DrawTexturedRectList(gAnalogMenuP2LeftStickSprite, p2LeftX >> 3, p2LeftY >> 4, 0);
        DrawTexturedRectList(gAnalogMenuP2RightStickSprite, p2RightX >> 3, p2RightY >> 4, 0);
        shade = ANALOG_MENU_SHADE_ACTIVE;
    } else {
        shade = ANALOG_MENU_SHADE_DIM;
    }
    gAnalogMenuStickFrameSprites[0].texture.r = gAnalogMenuStickFrameSprites[0].texture.g =
    gAnalogMenuStickFrameSprites[0].texture.b = gAnalogMenuStickFrameSprites[1].texture.r =
    gAnalogMenuStickFrameSprites[1].texture.g = gAnalogMenuStickFrameSprites[1].texture.b = shade;
    gAnalogMenuP2LabelSprite.texture.r = gAnalogMenuP2LabelSprite.texture.g =
    gAnalogMenuP2LabelSprite.texture.b = shade;
    DrawTexturedRectList(&gAnalogMenuP2LabelSprite, 0, 0, 0);
    DrawTexturedRectList(gAnalogMenuStickFrameSprites, ANALOG_MENU_P2_OFFSET_X, 0, 0);

    GetDrawEnv(&env);
    window.x = ANALOG_MENU_TW_X;
    window.y = ANALOG_MENU_TW_Y;
    window.w = ANALOG_MENU_TW_W;
    window.h = ANALOG_MENU_TW_H;
    SetTexWindow(&gAnalogMenuTexWindowRestore[gFrameCounter & 1], &env.tw);
    AddPrim(gCurrentOt, &gAnalogMenuTexWindowRestore[gFrameCounter & 1]);
    DrawTexturedRectList(gAnalogMenuBackdropSprites, 0, ANALOG_MENU_BACKDROP_Y, 0);
    SetTexWindow(&gAnalogMenuTexWindowSet[gFrameCounter & 1], &window);
    AddPrim(gCurrentOt, &gAnalogMenuTexWindowSet[gFrameCounter & 1]);
}

/* Ignores loadBuffer and loads from the reset model arena instead. */
void LoadAnalogControllerAssets(void *loadBuffer)
{
    ResetModelArena();
    loadBuffer = GetArenaFreePtr();
    ReadCdFileSync(CD_FILE_ANALOG_CONTROLLER, loadBuffer);
    LoadTimGroup(loadBuffer, 0);
    DrawSync(0);
    gAnalogMenuBackdropSprites[0].texture.u0 = 0;
    gAnalogMenuBackdropSprites[0].texture.v0 = 0;
    gAnalogMenuBackdropSprites[0].texture.u1 = 200;
    gAnalogMenuBackdropSprites[0].texture.v1 = 0;
    gAnalogMenuBackdropSprites[0].texture.u2 = 0;
    gAnalogMenuBackdropSprites[0].texture.v2 = 240;
    gAnalogMenuBackdropSprites[0].texture.u3 = 200;
    gAnalogMenuBackdropSprites[0].texture.v3 = 240;
    gAnalogMenuBackdropSprites[1].texture.u0 = 0;
    gAnalogMenuBackdropSprites[1].texture.v0 = 0;
    gAnalogMenuBackdropSprites[1].texture.u1 = 200;
    gAnalogMenuBackdropSprites[1].texture.v1 = 0;
    gAnalogMenuBackdropSprites[1].texture.u2 = 0;
    gAnalogMenuBackdropSprites[1].texture.v2 = 240;
    gAnalogMenuBackdropSprites[1].texture.u3 = 200;
    gAnalogMenuBackdropSprites[1].texture.v3 = 240;
    gAnalogMenuBackdropSprites[2].texture.u0 = 0;
    gAnalogMenuBackdropSprites[2].texture.v0 = 0;
    gAnalogMenuBackdropSprites[2].texture.u1 = 240;
    gAnalogMenuBackdropSprites[2].texture.v1 = 0;
    gAnalogMenuBackdropSprites[2].texture.u2 = 0;
    gAnalogMenuBackdropSprites[2].texture.v2 = 240;
    gAnalogMenuBackdropSprites[2].texture.u3 = 240;
    gAnalogMenuBackdropSprites[2].texture.v3 = 240;
}
#endif
