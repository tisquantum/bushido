/* The options menu: its rows, values, message box and confirm prompt over the gradient backdrop,
 * and loading its textures. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "options_menu.h"
#include "gpu_code.h"
#include "packet_build.h"
#include "model_arena.h"
#include "tim.h"
#include "sprite2d.h"
#include "asset_load.h"
#include "scene_light.h"
#include "gradient_bg.h"
#include "menu_state.h"
#include "main.h"
#include "options_ui.h"
#include "stage_light.h"
#include "globals.h"

#define CD_FILE_OPTIONS_TIMS         0x55 /* DATA3\O04.dat */
#define OPTIONS_MSG_AUTOSAVE_ERROR   16
#define OPTIONS_MSG_COUNT            17

#define OPTIONS_SHADE_SELECTED_ROW   0xc0
#define OPTIONS_SHADE_UNSELECTED_ROW 0x50
#define OPTIONS_SHADE_ACTIVE_VALUE   0x80
#define OPTIONS_SHADE_INACTIVE_VALUE 0x40
#ifdef VERSION_JP
#define OPTIONS_SHADE_ACTIVE_CHOICE   0xff
#define OPTIONS_SHADE_INACTIVE_CHOICE 0x20
#endif

#ifdef VERSION_JP
TexturedRectEntry gOptionsRowLabelSprites[OPTIONS_ROW_COUNT] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(688, 509), 48, 0,
        GET_TPAGE(0, 1, 640, 256), 0, 144, 48, 144 }, 1, 72, 52, 48, 144 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(688, 509), 48, 0,
        GET_TPAGE(0, 1, 640, 256), 0, 144, 48, 144 }, 1, 136, 52, 48, 144 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(688, 509), 48, 0,
        GET_TPAGE(0, 1, 640, 256), 0, 144, 48, 144 }, 1, 200, 52, 48, 144 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(688, 509), 48, 0,
        GET_TPAGE(0, 1, 640, 256), 0, 144, 48, 144 }, 1, 264, 52, 48, 144 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(688, 509), 48, 0,
        GET_TPAGE(0, 1, 640, 256), 0, 144, 48, 144 }, 1, 328, 52, 48, 144 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(688, 509), 48, 0,
        GET_TPAGE(0, 1, 640, 256), 0, 144, 48, 144 }, 1, 392, 52, 48, 144 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(688, 509), 48, 0,
        GET_TPAGE(0, 1, 640, 256), 0, 144, 48, 144 }, 1, 456, 52, 48, 144 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(688, 509), 48, 0,
        GET_TPAGE(0, 1, 640, 256), 0, 144, 48, 144 }, TEXTURED_RECT_LAST_GROUP | 1, 520, 52, 48, 144 },
};
/* The Japanese options screen is laid out in columns: each row is a column panel with a vertical
 * caption and one value sprite, and the selected row's two choices are shown in the footer's boxes. */
TexturedRectEntry gOptionsRowCaptionSprites[OPTIONS_ROW_COUNT] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 144, GET_CLUT(688, 506), 160, 144,
        GET_TPAGE(0, 0, 640, 256), 128, 191, 160, 191 }, 1, 80, 68, 32, 48 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 192, GET_CLUT(688, 505), 160, 192,
        GET_TPAGE(0, 0, 640, 256), 128, 239, 160, 239 }, 1, 144, 68, 32, 48 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 224, GET_CLUT(688, 504), 96, 224,
        GET_TPAGE(0, 0, 640, 256), 64, 255, 96, 255 }, 1, 208, 68, 32, 32 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 144, GET_CLUT(688, 503), 64, 144,
        GET_TPAGE(0, 1, 640, 256), 32, 239, 64, 239 }, 1, 272, 68, 32, 96 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 144, GET_CLUT(688, 502), 128, 144,
        GET_TPAGE(0, 1, 640, 256), 96, 239, 128, 239 }, 1, 336, 68, 32, 96 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 224, GET_CLUT(688, 501), 192, 224,
        GET_TPAGE(0, 0, 640, 256), 160, 255, 192, 255 }, 1, 400, 68, 32, 32 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 144, GET_CLUT(688, 499), 32, 144,
        GET_TPAGE(0, 1, 640, 256), 0, 239, 32, 239 }, 1, 464, 68, 32, 96 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 144, GET_CLUT(688, 500), 96, 144,
        GET_TPAGE(0, 0, 640, 256), 64, 223, 96, 223 }, TEXTURED_RECT_LAST_GROUP | 1, 528, 68, 32, 80 },
};
TexturedRectEntry gOptionsValueSprites[OPTIONS_ROW_COUNT] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 0, 640, 256), 160, 176, 200, 176 }, 1, 76, 172, 40, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 0, 640, 256), 160, 176, 200, 176 }, 1, 140, 172, 40, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 0, 640, 256), 160, 176, 200, 176 }, 1, 204, 172, 40, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 0, 640, 256), 160, 176, 200, 176 }, 1, 268, 172, 40, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 0, 640, 256), 160, 176, 200, 176 }, 1, 332, 172, 40, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 0, 640, 256), 160, 176, 200, 176 }, 1, 396, 172, 40, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 0, 640, 256), 160, 176, 200, 176 }, 1, 460, 172, 40, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 0, 640, 256), 160, 176, 200, 176 }, TEXTURED_RECT_LAST_GROUP | 1, 524, 172, 40, 16 },
};
#else
TexturedRectEntry gOptionsRowLabelSprites[OPTIONS_ROW_COUNT] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(688, 506), 176, 0,
        GET_TPAGE(0, 1, 832, 256), 0, 14, 176, 14 }, 1, 96, 54, 176, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 42, GET_CLUT(688, 505), 152, 42,
        GET_TPAGE(0, 1, 832, 256), 0, 56, 152, 56 }, 1, 96, 72, 152, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 152, 14, GET_CLUT(688, 504), 224, 14,
        GET_TPAGE(0, 1, 832, 256), 152, 28, 224, 28 }, 1, 96, 90, 72, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 28, GET_CLUT(688, 503), 160, 28,
        GET_TPAGE(0, 1, 832, 256), 0, 42, 160, 42 }, 1, 96, 108, 160, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 84, GET_CLUT(688, 502), 104, 84,
        GET_TPAGE(0, 1, 832, 256), 0, 98, 104, 98 }, 1, 96, 126, 104, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 56, GET_CLUT(688, 501), 144, 56,
        GET_TPAGE(0, 1, 832, 256), 0, 70, 144, 70 }, 1, 96, 144, 144, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 14, GET_CLUT(688, 499), 152, 14,
        GET_TPAGE(0, 1, 832, 256), 0, 28, 152, 28 }, 1, 96, 162, 152, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 70, GET_CLUT(688, 500), 120, 70,
        GET_TPAGE(0, 1, 832, 256), 0, 84, 120, 84 }, TEXTURED_RECT_LAST_GROUP | 1, 96, 180, 120, 14 },
};
TexturedRectEntry gOptionsValueSprites[OPTIONS_ROW_COUNT * 2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 120, 70, GET_CLUT(688, 497), 176, 70,
        GET_TPAGE(0, 1, 832, 256), 120, 84, 176, 84 }, 1, 392, 54, 56, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 144, 56, GET_CLUT(688, 498), 200, 56,
        GET_TPAGE(0, 1, 832, 256), 144, 70, 200, 70 }, 1, 480, 54, 56, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 160, 28, GET_CLUT(688, 496), 216, 28,
        GET_TPAGE(0, 1, 832, 256), 160, 42, 216, 42 }, 1, 392, 72, 56, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 160, 84, GET_CLUT(688, 495), 208, 84,
        GET_TPAGE(0, 1, 832, 256), 160, 98, 208, 98 }, 1, 480, 72, 48, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 176, 70, GET_CLUT(688, 494), 208, 70,
        GET_TPAGE(0, 1, 832, 256), 176, 84, 208, 84 }, 1, 392, 90, 32, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 56, 98, GET_CLUT(688, 493), 92, 98,
        GET_TPAGE(0, 1, 832, 256), 56, 112, 92, 112 }, 1, 480, 90, 36, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 176, 70, GET_CLUT(688, 494), 208, 70,
        GET_TPAGE(0, 1, 832, 256), 176, 84, 208, 84 }, 1, 392, 108, 32, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 56, 98, GET_CLUT(688, 493), 92, 98,
        GET_TPAGE(0, 1, 832, 256), 56, 112, 92, 112 }, 1, 480, 108, 36, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 176, 70, GET_CLUT(688, 494), 208, 70,
        GET_TPAGE(0, 1, 832, 256), 176, 84, 208, 84 }, 1, 392, 126, 32, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 56, 98, GET_CLUT(688, 493), 92, 98,
        GET_TPAGE(0, 1, 832, 256), 56, 112, 92, 112 }, 1, 480, 126, 36, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 176, 0, GET_CLUT(688, 489), 248, 0,
        GET_TPAGE(0, 1, 832, 256), 176, 14, 248, 14 }, 1, 392, 144, 72, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 152, 42, GET_CLUT(688, 490), 208, 42,
        GET_TPAGE(0, 1, 832, 256), 152, 56, 208, 56 }, 1, 480, 144, 56, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 98, GET_CLUT(688, 487), 56, 98,
        GET_TPAGE(0, 1, 832, 256), 0, 112, 56, 112 }, 1, 392, 162, 56, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 104, 84, GET_CLUT(688, 488), 160, 84,
        GET_TPAGE(0, 1, 832, 256), 104, 98, 160, 98 }, 1, 480, 162, 56, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 176, 70, GET_CLUT(688, 494), 208, 70,
        GET_TPAGE(0, 1, 832, 256), 176, 84, 208, 84 }, 1, 392, 180, 32, 14 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 56, 98, GET_CLUT(688, 493), 92, 98,
        GET_TPAGE(0, 1, 832, 256), 56, 112, 92, 112 }, 1, 480, 180, 36, 14 },
};
#endif
TexturedRectEntry gOptionsSideBorderTopSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 152, 40, GET_CLUT(688, 486), 200, 40,
        GET_TPAGE(0, 1, 640, 256), 152, 80, 200, 80 }, 1, 0, 24, 48, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 199, 40, GET_CLUT(688, 486), 151, 40,
        GET_TPAGE(0, 1, 640, 256), 199, 80, 151, 80 }, TEXTURED_RECT_LAST_GROUP | 1, 592, 24, 48, 40 },
};
struct {
    GlyphTexture texture;
    int countAndFlags;
    RECT rects[3];
} gOptionsSideBorderMidSprites[2] = {
    {
        { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 40, GET_CLUT(688, 485), 248, 40,
            GET_TPAGE(0, 0, 640, 256), 200, 80, 248, 80 },
        3,
        {
            { 0, 64, 48, 40 },
            { 0, 104, 48, 40 },
            { 0, 144, 48, 40 },
        },
    },
    {
        { 128, 128, 128, GPU_CODE_POLY_FT4, 247, 40, GET_CLUT(688, 485), 199, 40,
            GET_TPAGE(0, 0, 640, 256), 247, 80, 199, 80 },
        TEXTURED_RECT_LAST_GROUP | 3,
        {
            { 592, 64, 48, 40 },
            { 592, 104, 48, 40 },
            { 592, 144, 48, 40 },
        },
    },
};
TexturedRectEntry gOptionsSideBorderBottomSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 144, 80, GET_CLUT(688, 484), 192, 80,
        GET_TPAGE(0, 0, 640, 256), 144, 96, 192, 96 }, 1, 0, 184, 48, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 191, 80, GET_CLUT(688, 484), 143, 80,
        GET_TPAGE(0, 0, 640, 256), 191, 96, 143, 96 }, TEXTURED_RECT_LAST_GROUP | 1, 592, 184, 48, 16 },
};
#ifdef VERSION_JP
TexturedRectEntry gOptionsFooterBarSprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 104, GET_CLUT(688, 483), 240, 104,
        GET_TPAGE(0, 0, 640, 256), 48, 144, 240, 144 }, 1, 0, 200, 192, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 239, 104, GET_CLUT(688, 483), 47, 104,
        GET_TPAGE(0, 0, 640, 256), 239, 144, 47, 144 }, 1, 448, 200, 192, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 176, 0, GET_CLUT(688, 482), 224, 0,
        GET_TPAGE(0, 0, 640, 256), 176, 40, 224, 40 }, TEXTURED_RECT_LAST_GROUP | 1, 296, 200, 48, 40 },
};
TexturedRectEntry gOptionsChoiceBoxSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 40, GET_CLUT(688, 507), 152, 40,
        GET_TPAGE(0, 0, 640, 256), 48, 80, 152, 80 }, 1, 192, 200, 104, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 151, 40, GET_CLUT(688, 507), 47, 40,
        GET_TPAGE(0, 0, 640, 256), 151, 80, 47, 80 }, TEXTURED_RECT_LAST_GROUP | 1, 344, 200, 104, 40 },
};
#else
TexturedRectEntry gOptionsFooterBarSprites[4] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 184, GET_CLUT(688, 507), 255, 184,
        GET_TPAGE(0, 0, 640, 256), 0, 224, 255, 224 }, 1, 140, 200, 255, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 184, GET_CLUT(688, 507), 104, 184,
        GET_TPAGE(0, 0, 704, 256), 0, 224, 104, 224 }, 1, 395, 200, 104, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 104, GET_CLUT(688, 483), 240, 104,
        GET_TPAGE(0, 0, 640, 256), 48, 144, 240, 144 }, 1, 0, 200, 192, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 239, 104, GET_CLUT(688, 483), 47, 104,
        GET_TPAGE(0, 0, 640, 256), 239, 144, 47, 144 }, TEXTURED_RECT_LAST_GROUP | 1, 448, 200, 192, 40 },
};
#endif
struct {
    GlyphTexture texture;
    int countAndFlags;
    RECT rects[2];
} gOptionsHeaderWingSprites[2] = {
    {
        { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 80, GET_CLUT(688, 511), 144, 80,
            GET_TPAGE(0, 0, 640, 256), 48, 104, 144, 104 },
        2,
        {
            { 0, 8, 96, 24 },
            { 448, 8, 96, 24 },
        },
    },
    {
        { 128, 128, 128, GPU_CODE_POLY_FT4, 143, 80, GET_CLUT(688, 511), 47, 80,
            GET_TPAGE(0, 0, 640, 256), 143, 104, 47, 104 },
        TEXTURED_RECT_LAST_GROUP | 2,
        {
            { 96, 8, 96, 24 },
            { 544, 8, 96, 24 },
        },
    },
};
TexturedRectEntry gOptionsHeaderRuleSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 144, 96, GET_CLUT(688, 510), 207, 96,
        GET_TPAGE(0, 0, 640, 256), 144, 100, 207, 100 }, 1, 192, 8, 128, 4 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 206, 96, GET_CLUT(688, 510), 143, 96,
        GET_TPAGE(0, 0, 640, 256), 206, 100, 143, 100 }, TEXTURED_RECT_LAST_GROUP | 1, 320, 8, 128, 4 },
};
TexturedRectEntry gOptionsTitleBannerSprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 228, GET_CLUT(688, 481), 232, 228,
        GET_TPAGE(0, 1, 768, 256), 0, 244, 232, 244 }, 1, 204, 22, 232, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 4, GET_CLUT(688, 508), 176, 4,
        GET_TPAGE(0, 0, 640, 256), 48, 40, 176, 40 }, 1, 192, 12, 128, 36 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 175, 39, GET_CLUT(688, 508), 47, 39,
        GET_TPAGE(0, 0, 640, 256), 175, 3, 47, 3 }, TEXTURED_RECT_LAST_GROUP | 1, 320, 12, 128, 36 },
};
#ifdef VERSION_JP
TexturedRectEntry gOptionsChoiceTextSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 144, GET_CLUT(688, 498), 168, 144,
        GET_TPAGE(0, 0, 640, 256), 128, 160, 168, 160 }, 1, 224, 212, 40, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 168, 144, GET_CLUT(688, 497), 208, 144,
        GET_TPAGE(0, 0, 640, 256), 168, 160, 208, 160 }, TEXTURED_RECT_LAST_GROUP | 1, 376, 212, 40, 16 },
};
/* Each row's two choice captions, [row * 2 + choice]. */
GlyphTexture gOptionsChoiceTextures[OPTIONS_ROW_COUNT * 2] = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 144, GET_CLUT(688, 497), 200, 144,
        GET_TPAGE(0, 1, 640, 256), 160, 160, 200, 160 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 208, GET_CLUT(688, 498), 200, 208,
        GET_TPAGE(0, 1, 640, 256), 160, 224, 200, 224 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 144, GET_CLUT(688, 496), 240, 144,
        GET_TPAGE(0, 1, 640, 256), 200, 160, 240, 160 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 208, GET_CLUT(688, 495), 240, 208,
        GET_TPAGE(0, 1, 640, 256), 200, 224, 240, 224 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 1, 640, 256), 160, 176, 200, 176 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 160, GET_CLUT(688, 493), 240, 160,
        GET_TPAGE(0, 1, 640, 256), 200, 176, 240, 176 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 1, 640, 256), 160, 176, 200, 176 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 160, GET_CLUT(688, 493), 240, 160,
        GET_TPAGE(0, 1, 640, 256), 200, 176, 240, 176 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 1, 640, 256), 160, 176, 200, 176 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 160, GET_CLUT(688, 493), 240, 160,
        GET_TPAGE(0, 1, 640, 256), 200, 176, 240, 176 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 176, GET_CLUT(688, 490), 240, 176,
        GET_TPAGE(0, 1, 640, 256), 200, 192, 240, 192 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 192, GET_CLUT(688, 489), 240, 192,
        GET_TPAGE(0, 1, 640, 256), 200, 208, 240, 208 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 176, GET_CLUT(688, 487), 200, 176,
        GET_TPAGE(0, 1, 640, 256), 160, 192, 200, 192 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 192, GET_CLUT(688, 488), 200, 192,
        GET_TPAGE(0, 1, 640, 256), 160, 208, 200, 208 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 160, GET_CLUT(688, 494), 200, 160,
        GET_TPAGE(0, 1, 640, 256), 160, 176, 200, 176 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 160, GET_CLUT(688, 493), 240, 160,
        GET_TPAGE(0, 1, 640, 256), 200, 176, 240, 176 },
};
#endif
TexturedRectEntry gOptionsMsgBoxSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 224, 228, GET_CLUT(736, 507), 231, 228,
        GET_TPAGE(0, 0, 704, 256), 224, 235, 231, 235 }, TEXTURED_RECT_LAST_GROUP | 1, 120, 98, 400, 44 },
};
#ifdef VERSION_JP
TexturedRectEntry gOptionsMsgSavingSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 80, GET_CLUT(704, 511), 255, 80,
        GET_TPAGE(0, 1, 704, 256), 0, 116, 255, 116 }, 1, 176, 102, 256, 36 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 80, GET_CLUT(704, 511), 32, 80,
        GET_TPAGE(0, 1, 768, 256), 0, 116, 32, 116 }, TEXTURED_RECT_LAST_GROUP | 1, 432, 102, 32, 36 },
};
TexturedRectEntry gOptionsMsgSaveOkSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 32, 140, GET_CLUT(720, 511), 212, 140,
        GET_TPAGE(0, 1, 768, 256), 32, 156, 212, 156 }, TEXTURED_RECT_LAST_GROUP | 1, 230, 112, 180, 16 },
};
TexturedRectEntry gOptionsMsgSaveErrorSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 20, 196, GET_CLUT(736, 511), 244, 196,
        GET_TPAGE(0, 1, 768, 256), 20, 212, 244, 212 }, TEXTURED_RECT_LAST_GROUP | 1, 208, 112, 224, 16 },
};
TexturedRectEntry gOptionsMsgOverwriteConfirmSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 180, GET_CLUT(752, 511), 255, 180,
        GET_TPAGE(0, 1, 704, 256), 0, 204, 255, 204 }, 1, 182, 102, 256, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 180, GET_CLUT(752, 511), 20, 180,
        GET_TPAGE(0, 1, 768, 256), 0, 204, 20, 204 }, TEXTURED_RECT_LAST_GROUP | 1, 438, 102, 20, 24 },
};
TexturedRectEntry gOptionsMsgNoFreeBlocksSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 48, GET_CLUT(704, 510), 255, 48,
        GET_TPAGE(0, 1, 704, 256), 0, 64, 255, 64 }, 1, 140, 112, 256, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 48, GET_CLUT(704, 510), 104, 48,
        GET_TPAGE(0, 1, 768, 256), 0, 64, 104, 64 }, TEXTURED_RECT_LAST_GROUP | 1, 396, 112, 104, 16 },
};
TexturedRectEntry gOptionsMsgCorruptDataSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 32, 172, GET_CLUT(720, 510), 248, 172,
        GET_TPAGE(0, 1, 768, 256), 32, 196, 248, 196 }, TEXTURED_RECT_LAST_GROUP | 1, 212, 108, 216, 24 },
};
TexturedRectEntry gOptionsMsgFormatConfirmSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(736, 510), 255, 0,
        GET_TPAGE(0, 1, 704, 256), 0, 24, 255, 24 }, 1, 128, 102, 256, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(736, 510), 128, 0,
        GET_TPAGE(0, 1, 768, 256), 0, 24, 128, 24 }, TEXTURED_RECT_LAST_GROUP | 1, 384, 102, 128, 24 },
};
TexturedRectEntry gOptionsMsgFormattingSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 116, GET_CLUT(752, 510), 255, 116,
        GET_TPAGE(0, 1, 704, 256), 0, 148, 255, 148 }, 1, 180, 104, 256, 32 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 116, GET_CLUT(752, 510), 24, 116,
        GET_TPAGE(0, 1, 768, 256), 0, 148, 24, 148 }, TEXTURED_RECT_LAST_GROUP | 1, 436, 104, 24, 32 },
};
TexturedRectEntry gOptionsMsgFormatOkSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 32, 156, GET_CLUT(704, 509), 212, 156,
        GET_TPAGE(0, 1, 768, 256), 32, 172, 212, 172 }, TEXTURED_RECT_LAST_GROUP | 1, 230, 112, 180, 16 },
};
TexturedRectEntry gOptionsMsgFormatErrorSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 228, GET_CLUT(720, 509), 224, 228,
        GET_TPAGE(0, 1, 704, 256), 0, 244, 224, 244 }, TEXTURED_RECT_LAST_GROUP | 1, 208, 112, 224, 16 },
};
TexturedRectEntry gOptionsMsgNoCardSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 64, GET_CLUT(736, 509), 255, 64,
        GET_TPAGE(0, 1, 704, 256), 0, 80, 255, 80 }, 1, 160, 112, 256, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 64, GET_CLUT(736, 509), 64, 64,
        GET_TPAGE(0, 1, 768, 256), 0, 80, 64, 80 }, TEXTURED_RECT_LAST_GROUP | 1, 416, 112, 64, 16 },
};
TexturedRectEntry gOptionsMsgLoadingSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 148, GET_CLUT(752, 509), 255, 148,
        GET_TPAGE(0, 1, 704, 256), 0, 180, 255, 180 }, 1, 180, 104, 256, 32 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 148, GET_CLUT(752, 509), 24, 148,
        GET_TPAGE(0, 1, 768, 256), 0, 180, 24, 180 }, TEXTURED_RECT_LAST_GROUP | 1, 436, 104, 24, 32 },
};
TexturedRectEntry gOptionsMsgLoadOkSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 32, 124, GET_CLUT(704, 508), 216, 124,
        GET_TPAGE(0, 1, 768, 256), 32, 140, 216, 140 }, TEXTURED_RECT_LAST_GROUP | 1, 228, 112, 184, 16 },
};
TexturedRectEntry gOptionsMsgLoadErrorSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 20, 212, GET_CLUT(720, 508), 244, 212,
        GET_TPAGE(0, 1, 768, 256), 20, 228, 244, 228 }, TEXTURED_RECT_LAST_GROUP | 1, 208, 112, 224, 16 },
};
TexturedRectEntry gOptionsMsgNoDataSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 24, GET_CLUT(736, 508), 255, 24,
        GET_TPAGE(0, 1, 704, 256), 0, 48, 255, 48 }, 1, 140, 108, 256, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 24, GET_CLUT(736, 508), 104, 24,
        GET_TPAGE(0, 1, 768, 256), 0, 48, 104, 48 }, TEXTURED_RECT_LAST_GROUP | 1, 396, 108, 104, 24 },
};
TexturedRectEntry gOptionsMsgCheckingCardSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 204, GET_CLUT(752, 508), 255, 204,
        GET_TPAGE(0, 1, 704, 256), 0, 228, 255, 228 }, 1, 182, 108, 256, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 204, GET_CLUT(752, 508), 20, 204,
        GET_TPAGE(0, 1, 768, 256), 0, 228, 20, 228 }, TEXTURED_RECT_LAST_GROUP | 1, 438, 108, 20, 24 },
};
TexturedRectEntry gOptionsConfirmPromptSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 128, 0, GET_CLUT(704, 507), 232, 0,
        GET_TPAGE(0, 1, 768, 256), 128, 16, 232, 16 }, TEXTURED_RECT_LAST_GROUP | 1, 268, 124, 104, 16 },
};
TexturedRectEntry gOptionsMsgAutoSaveErrorSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(752, 507), 240, 0,
        GET_TPAGE(0, 1, 832, 256), 0, 16, 240, 16 }, TEXTURED_RECT_LAST_GROUP | 1, 200, 112, 240, 16 },
};
TexturedRectEntry gOptionsConfirmYesHighlight[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 232, 0, GET_CLUT(720, 507), 255, 0,
        GET_TPAGE(0, 1, 768, 256), 232, 15, 255, 15 }, TEXTURED_RECT_LAST_GROUP | 1, 268, 126, 24, 11 },
};
TexturedRectEntry gOptionsConfirmNoHighlight[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 232, 0, GET_CLUT(720, 507), 255, 0,
        GET_TPAGE(0, 1, 768, 256), 232, 15, 255, 15 }, TEXTURED_RECT_LAST_GROUP | 1, 348, 126, 24, 11 },
};
#else
TexturedRectEntry gOptionsMsgSavingSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 80, GET_CLUT(704, 511), 255, 80,
        GET_TPAGE(0, 1, 704, 256), 0, 112, 255, 112 }, 1, 172, 104, 256, 32 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 80, GET_CLUT(704, 511), 40, 80,
        GET_TPAGE(0, 1, 768, 256), 0, 112, 40, 112 }, TEXTURED_RECT_LAST_GROUP | 1, 428, 104, 40, 32 },
};
TexturedRectEntry gOptionsMsgSaveOkSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 32, 140, GET_CLUT(720, 511), 104, 140,
        GET_TPAGE(0, 1, 768, 256), 32, 156, 104, 156 }, TEXTURED_RECT_LAST_GROUP | 1, 284, 112, 72, 16 },
};
TexturedRectEntry gOptionsMsgSaveErrorSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 152, GET_CLUT(736, 511), 160, 152,
        GET_TPAGE(0, 1, 832, 256), 0, 168, 160, 168 }, TEXTURED_RECT_LAST_GROUP | 1, 240, 112, 160, 16 },
};
TexturedRectEntry gOptionsMsgOverwriteConfirmSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 160, GET_CLUT(752, 511), 255, 160,
        GET_TPAGE(0, 1, 704, 256), 0, 184, 255, 184 }, 1, 188, 102, 256, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 160, GET_CLUT(752, 511), 8, 160,
        GET_TPAGE(0, 1, 768, 256), 0, 184, 8, 184 }, TEXTURED_RECT_LAST_GROUP | 1, 444, 102, 8, 24 },
};
TexturedRectEntry gOptionsMsgNoFreeBlocksSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 48, GET_CLUT(704, 510), 255, 48,
        GET_TPAGE(0, 1, 704, 256), 0, 64, 255, 64 }, 1, 172, 112, 256, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 48, GET_CLUT(704, 510), 40, 48,
        GET_TPAGE(0, 1, 768, 256), 0, 64, 40, 64 }, TEXTURED_RECT_LAST_GROUP | 1, 428, 112, 40, 16 },
};
TexturedRectEntry gOptionsMsgCorruptDataSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 212, GET_CLUT(720, 510), 255, 212,
        GET_TPAGE(0, 1, 768, 256), 0, 228, 255, 228 }, 1, 184, 112, 256, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 212, GET_CLUT(720, 510), 16, 212,
        GET_TPAGE(0, 1, 832, 256), 0, 228, 16, 228 }, TEXTURED_RECT_LAST_GROUP | 1, 440, 112, 16, 16 },
};
TexturedRectEntry gOptionsMsgFormatConfirmSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(736, 510), 255, 0,
        GET_TPAGE(0, 1, 704, 256), 0, 24, 255, 24 }, 1, 168, 102, 256, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(736, 510), 48, 0,
        GET_TPAGE(0, 1, 768, 256), 0, 24, 48, 24 }, TEXTURED_RECT_LAST_GROUP | 1, 424, 102, 48, 24 },
};
TexturedRectEntry gOptionsMsgFormattingSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 112, GET_CLUT(752, 510), 255, 112,
        GET_TPAGE(0, 1, 704, 256), 0, 136, 255, 136 }, 1, 176, 108, 256, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 112, GET_CLUT(752, 510), 32, 112,
        GET_TPAGE(0, 1, 768, 256), 0, 136, 32, 136 }, TEXTURED_RECT_LAST_GROUP | 1, 432, 108, 32, 24 },
};
TexturedRectEntry gOptionsMsgFormatOkSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 136, GET_CLUT(704, 509), 248, 136,
        GET_TPAGE(0, 1, 832, 256), 0, 152, 248, 152 }, TEXTURED_RECT_LAST_GROUP | 1, 196, 112, 248, 16 },
};
TexturedRectEntry gOptionsMsgFormatErrorSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 228, GET_CLUT(720, 509), 184, 228,
        GET_TPAGE(0, 1, 704, 256), 0, 244, 184, 244 }, TEXTURED_RECT_LAST_GROUP | 1, 228, 112, 184, 16 },
};
TexturedRectEntry gOptionsMsgNoCardSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 64, GET_CLUT(736, 509), 255, 64,
        GET_TPAGE(0, 1, 704, 256), 0, 80, 255, 80 }, 1, 160, 112, 256, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 64, GET_CLUT(736, 509), 64, 64,
        GET_TPAGE(0, 1, 768, 256), 0, 80, 64, 80 }, TEXTURED_RECT_LAST_GROUP | 1, 416, 112, 64, 16 },
};
TexturedRectEntry gOptionsMsgLoadingSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 136, GET_CLUT(752, 509), 255, 136,
        GET_TPAGE(0, 1, 704, 256), 0, 160, 255, 160 }, 1, 176, 108, 256, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 136, GET_CLUT(752, 509), 32, 136,
        GET_TPAGE(0, 1, 768, 256), 0, 160, 32, 160 }, TEXTURED_RECT_LAST_GROUP | 1, 432, 108, 32, 24 },
};
TexturedRectEntry gOptionsMsgLoadOkSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 32, 124, GET_CLUT(704, 508), 232, 124,
        GET_TPAGE(0, 1, 768, 256), 32, 140, 232, 140 }, TEXTURED_RECT_LAST_GROUP | 1, 220, 112, 200, 16 },
};
TexturedRectEntry gOptionsMsgLoadErrorSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 168, GET_CLUT(720, 508), 160, 168,
        GET_TPAGE(0, 1, 832, 256), 0, 184, 160, 184 }, TEXTURED_RECT_LAST_GROUP | 1, 240, 112, 160, 16 },
};
TexturedRectEntry gOptionsMsgNoDataSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 24, GET_CLUT(736, 508), 255, 24,
        GET_TPAGE(0, 1, 704, 256), 0, 48, 255, 48 }, 1, 140, 108, 256, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 24, GET_CLUT(736, 508), 104, 24,
        GET_TPAGE(0, 1, 768, 256), 0, 48, 104, 48 }, TEXTURED_RECT_LAST_GROUP | 1, 396, 108, 104, 24 },
};
TexturedRectEntry gOptionsMsgCheckingCardSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 112, GET_CLUT(752, 508), 240, 112,
        GET_TPAGE(0, 1, 832, 256), 0, 136, 240, 136 }, TEXTURED_RECT_LAST_GROUP | 1, 200, 108, 240, 24 },
};
TexturedRectEntry gOptionsConfirmPromptSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 48, 0, GET_CLUT(704, 507), 184, 0,
        GET_TPAGE(0, 1, 768, 256), 48, 16, 184, 16 }, TEXTURED_RECT_LAST_GROUP | 1, 252, 124, 136, 16 },
};
TexturedRectEntry gOptionsMsgAutoSaveErrorSprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 40, 80, GET_CLUT(752, 507), 252, 80,
        GET_TPAGE(0, 1, 768, 256), 40, 96, 252, 96 }, TEXTURED_RECT_LAST_GROUP | 1, 214, 112, 212, 16 },
};
TexturedRectEntry gOptionsConfirmYesHighlight[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 232, 0, GET_CLUT(720, 507), 255, 0,
        GET_TPAGE(0, 1, 768, 256), 232, 15, 255, 15 }, TEXTURED_RECT_LAST_GROUP | 1, 257, 126, 35, 11 },
};
TexturedRectEntry gOptionsConfirmNoHighlight[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 232, 0, GET_CLUT(720, 507), 255, 0,
        GET_TPAGE(0, 1, 768, 256), 232, 15, 255, 15 }, TEXTURED_RECT_LAST_GROUP | 1, 307, 126, 72, 11 },
};
#endif
/* One sprite list per memory-card message, indexed by OPTIONS_MSG_*; no card status shows 15. */
TexturedRectEntry *gOptionsMsgSprites[OPTIONS_MSG_COUNT] = {
    gOptionsMsgSavingSprites, gOptionsMsgSaveOkSprites, gOptionsMsgSaveErrorSprites,
    gOptionsMsgOverwriteConfirmSprites, gOptionsMsgNoFreeBlocksSprites, gOptionsMsgCorruptDataSprites,
    gOptionsMsgFormatConfirmSprites, gOptionsMsgFormattingSprites, gOptionsMsgFormatOkSprites,
    gOptionsMsgFormatErrorSprites, gOptionsMsgNoCardSprites, gOptionsMsgLoadingSprites,
    gOptionsMsgLoadOkSprites, gOptionsMsgLoadErrorSprites, gOptionsMsgNoDataSprites,
    gOptionsMsgCheckingCardSprites, gOptionsMsgAutoSaveErrorSprites,
};
/* Per row: the choice a zero value shows; a nonzero value shows the other. */
unsigned char gOptionsInvertMask[] = { 0, 1, 1, 0, 1, 0, 0, 1 };

void DrawOptionsMenu(int selectedRow, short **optionValues, int msgIndex, int confirmSelection)
{
    int row, selectedChoice;
#ifdef VERSION_JP
    TexturedRectEntry *choiceText;
#else
    int choiceIndex;
#endif
    TexturedRectEntry *highlightSprite;

    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);
    if (msgIndex >= 0) {
        DrawTexturedRectList(gOptionsMsgSprites[msgIndex], 0, 0, 0);
        if (msgIndex == OPTIONS_MSG_OVERWRITE_CONFIRM || msgIndex == OPTIONS_MSG_FORMAT_CONFIRM) {
            highlightSprite = gOptionsConfirmNoHighlight;
            if (confirmSelection == OPTIONS_CONFIRM_YES) {
                highlightSprite = gOptionsConfirmYesHighlight;
            }
            DrawTexturedRectList(highlightSprite, 0, 0, 0);
            DrawTexturedRectList(gOptionsConfirmPromptSprites, 0, 0, 0);
        }
        DrawTexturedRectList(gOptionsMsgBoxSprite, 0, 0, 0);
    }
#ifdef VERSION_JP
    choiceText = gOptionsChoiceTextSprites;
    choiceText[0].texture = gOptionsChoiceTextures[selectedRow << 1];
    choiceText[1].texture = gOptionsChoiceTextures[(selectedRow << 1) + 1];
    selectedChoice = gOptionsInvertMask[selectedRow] ^ (*optionValues[selectedRow] != 0);
    choiceText[selectedChoice].texture.r =
        choiceText[selectedChoice].texture.g =
        choiceText[selectedChoice].texture.b = OPTIONS_SHADE_ACTIVE_CHOICE;
    gOptionsChoiceBoxSprites[selectedChoice].texture.r =
        gOptionsChoiceBoxSprites[selectedChoice].texture.g =
        gOptionsChoiceBoxSprites[selectedChoice].texture.b = OPTIONS_SHADE_ACTIVE_CHOICE;
    choiceText[1 - selectedChoice].texture.r =
        choiceText[1 - selectedChoice].texture.g =
        choiceText[1 - selectedChoice].texture.b = OPTIONS_SHADE_INACTIVE_CHOICE;
    gOptionsChoiceBoxSprites[1 - selectedChoice].texture.r =
        gOptionsChoiceBoxSprites[1 - selectedChoice].texture.g =
        gOptionsChoiceBoxSprites[1 - selectedChoice].texture.b = OPTIONS_SHADE_INACTIVE_CHOICE;
    for (row = 0; row < OPTIONS_ROW_COUNT; ++row) {
        selectedChoice = gOptionsInvertMask[row] ^ (*optionValues[row] != 0);
        gOptionsValueSprites[row].texture = gOptionsChoiceTextures[(row << 1) + selectedChoice];
        /* Both arms set only the value sprite's red; green and blue keep their values. */
        if (row == selectedRow) {
            gOptionsRowLabelSprites[row].texture.r =
                gOptionsRowLabelSprites[row].texture.g =
                gOptionsRowLabelSprites[row].texture.b = OPTIONS_SHADE_SELECTED_ROW;
            gOptionsRowCaptionSprites[row].texture.r =
                gOptionsRowCaptionSprites[row].texture.g =
                gOptionsRowCaptionSprites[row].texture.b = OPTIONS_SHADE_SELECTED_ROW;
            gOptionsValueSprites[row].texture.r =
                gOptionsValueSprites[row].texture.r =
                gOptionsValueSprites[row].texture.r = OPTIONS_SHADE_ACTIVE_VALUE;
        } else {
            gOptionsRowLabelSprites[row].texture.r =
                gOptionsRowLabelSprites[row].texture.g =
                gOptionsRowLabelSprites[row].texture.b = OPTIONS_SHADE_UNSELECTED_ROW;
            gOptionsRowCaptionSprites[row].texture.r =
                gOptionsRowCaptionSprites[row].texture.g =
                gOptionsRowCaptionSprites[row].texture.b = OPTIONS_SHADE_UNSELECTED_ROW;
            gOptionsValueSprites[row].texture.r =
                gOptionsValueSprites[row].texture.r =
                gOptionsValueSprites[row].texture.r = OPTIONS_SHADE_INACTIVE_VALUE;
        }
    }
    DrawTexturedRectList(gOptionsChoiceTextSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsValueSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsRowCaptionSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsRowLabelSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsChoiceBoxSprites, 0, 0, 0);
#else
    for (row = 0; row < OPTIONS_ROW_COUNT; ++row) {
        selectedChoice = gOptionsInvertMask[row] ^ (*optionValues[row] != 0);
        if (row == selectedRow) {
            gOptionsRowLabelSprites[row].texture.r =
                gOptionsRowLabelSprites[row].texture.g =
                gOptionsRowLabelSprites[row].texture.b = OPTIONS_SHADE_SELECTED_ROW;
        } else {
            gOptionsRowLabelSprites[row].texture.r =
                gOptionsRowLabelSprites[row].texture.g =
                gOptionsRowLabelSprites[row].texture.b = OPTIONS_SHADE_UNSELECTED_ROW;
        }
        choiceIndex = (row << 1) + selectedChoice;
        gOptionsValueSprites[choiceIndex].texture.r =
            gOptionsValueSprites[choiceIndex].texture.g =
            gOptionsValueSprites[choiceIndex].texture.b = OPTIONS_SHADE_ACTIVE_VALUE;
        choiceIndex = (row << 1) + (1 - selectedChoice);
        gOptionsValueSprites[choiceIndex].texture.r =
            gOptionsValueSprites[choiceIndex].texture.g =
            gOptionsValueSprites[choiceIndex].texture.b = OPTIONS_SHADE_INACTIVE_VALUE;
    }
    DrawTexturedRectList(gOptionsValueSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsRowLabelSprites, 0, 0, 0);
#endif
    DrawTexturedRectList(gOptionsFooterBarSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsHeaderWingSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsHeaderRuleSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsTitleBannerSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsSideBorderTopSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsSideBorderMidSprites, 0, 0, 0);
    DrawTexturedRectList(gOptionsSideBorderBottomSprites, 0, 0, 0);
    SubmitMenuGradientBackdrop();
    SetRendererMonochrome(GetMonochromeOption());
    SetRendererDaytime(GetDaytimeOption());
}

void LoadOptionsMenuAssets(void)
{
    void *arenaPtr;

    DrawSync(0);
    ResetModelArena();
    arenaPtr = GetArenaFreePtr();
    ReadCdFileSync(CD_FILE_OPTIONS_TIMS, arenaPtr);
    LoadTimGroup(arenaPtr, 0);
    DrawSync(0);
    InitMenuGradientBackdrop(0);
    SetStageClearAndFogColor(0);
}

void DrawAutoSaveErrorOverlay(void)
{
    void *packetBuffer;
    TexturedRectEntry *errorSprite;

    packetBuffer = GetGpuPacketBuffer(gFrameCounter & 1);
    errorSprite = gOptionsMsgSprites[OPTIONS_MSG_AUTOSAVE_ERROR];
    gPacketCursor = packetBuffer;
    DrawTexturedRectList(errorSprite, 0, 0, 0);
    DrawTexturedRectList(gOptionsMsgBoxSprite, 0, 0, 0);
    SubmitMenuGradientBackdrop();
}
