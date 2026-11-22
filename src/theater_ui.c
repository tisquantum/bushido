/* The Movie Theater screen: its menu sprites, the backdrop texture window, loading the theater's
 * textures and subtitle bank from the disc, and resetting the frame's packet cursor. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "display_buffer.h"
#include "gpu_code.h"
#include "packet_build.h"
#include "fighter.h"
#include "model_arena.h"
#include "tim.h"
#include "sprite2d.h"
#include "asset_load.h"
#include "subtitle.h"
#include "main.h"
#include "theater_ui.h"
#include "globals.h"

#define CD_FILE_MOVIE_THEATER_ASSETS         0x5f /* DATA3\O14.dat */
#ifndef VERSION_JP
#define CD_FILE_MOVIE_SUBTITLES              0x65 /* DATA3\O20.dat */
#endif

#define MOVIE_THEATER_ENDING_COUNT           2
#define MOVIE_THEATER_SELECTION_OPENING      0
#define MOVIE_THEATER_ENDING1_UNLOCKED       2
#define MOVIE_THEATER_ENDING2_UNLOCKED       1
#define MOVIE_THEATER_ENDING_ROW_STEP_Y      0x13

#define MOVIE_THEATER_LABEL_SHADE_ACTIVE     0x80
#define MOVIE_THEATER_LABEL_SHADE_DIM        0x40
#define MOVIE_THEATER_PORTRAIT_SHADE_ACTIVE  0x90
#define MOVIE_THEATER_PORTRAIT_SHADE_ENABLED 0x50
#define MOVIE_THEATER_PORTRAIT_SHADE_LOCKED  0x10

#define MOVIE_THEATER_BACKDROP_TW_X          0
#define MOVIE_THEATER_BACKDROP_TW_Y          0xe0
#define MOVIE_THEATER_BACKDROP_TW_W          0x20
#define MOVIE_THEATER_BACKDROP_TW_H          0x10

/* Offset table at the start of the Movie Theater file. */
typedef struct MovieTheaterArchiveHeader {
    int sectionCount;
    int timOffset;
    int subtitleOffset;
} MovieTheaterArchiveHeader;

TexturedRectEntry gMovieTheaterHeaderSprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 162, GET_CLUT(752, 250), 232, 162,
        GET_TPAGE(0, 1, 640, 0), 0, 178, 232, 178 }, 1, 204, 22, 232, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 4, 194, GET_CLUT(752, 222), 132, 194,
        GET_TPAGE(0, 0, 640, 0), 4, 230, 132, 230 }, 1, 192, 12, 128, 36 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 131, 229, GET_CLUT(752, 222), 3, 229,
        GET_TPAGE(0, 0, 640, 0), 131, 193, 3, 193 }, TEXTURED_RECT_LAST_GROUP | 1, 320, 12, 128, 36 },
};

TexturedRectEntry gMovieTheaterBackdropSprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 224, GET_CLUT(752, 252), 32, 224,
        GET_TPAGE(0, 0, 704, 0), 0, 240, 32, 240 }, 1, 0, 0, 224, 240 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 224, GET_CLUT(752, 252), 32, 224,
        GET_TPAGE(0, 0, 704, 0), 0, 240, 32, 240 }, 1, 224, 0, 224, 240 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 224, GET_CLUT(752, 252), 32, 224,
        GET_TPAGE(0, 0, 704, 0), 0, 240, 32, 240 }, TEXTURED_RECT_LAST_GROUP | 1, 448, 0, 192, 240 },
};

TexturedRectEntry gMovieTheaterPanelSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 200, 138, GET_CLUT(752, 221), 215, 138,
        GET_TPAGE(0, 0, 640, 0), 200, 153, 215, 153 }, 1, 96, 56, 416, 32 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 200, 138, GET_CLUT(752, 221), 215, 138,
        GET_TPAGE(0, 0, 640, 0), 200, 153, 215, 153 }, TEXTURED_RECT_LAST_GROUP | 1, 96, 100, 464, 112 },
};

#ifdef VERSION_JP
TexturedRectEntry gMovieTheaterOpeningLabelSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 138, GET_CLUT(752, 249), 200, 138,
        GET_TPAGE(0, 1, 640, 0), 0, 162, 200, 162 },
    TEXTURED_RECT_LAST_GROUP | 1, 120, 60, 200, 24,
};

TexturedRectEntry gMovieTheaterEndingLabelSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 114, GET_CLUT(752, 248), 240, 114,
        GET_TPAGE(0, 1, 640, 0), 0, 138, 240, 138 },
    TEXTURED_RECT_LAST_GROUP | 1, 120, 104, 240, 24,
};
#else
TexturedRectEntry gMovieTheaterOpeningLabelSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 138, GET_CLUT(752, 249), 104, 138,
        GET_TPAGE(0, 1, 640, 0), 0, 162, 104, 162 },
    TEXTURED_RECT_LAST_GROUP | 1, 120, 60, 104, 24,
};

TexturedRectEntry gMovieTheaterEndingLabelSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 114, GET_CLUT(752, 248), 128, 114,
        GET_TPAGE(0, 1, 640, 0), 0, 138, 128, 138 },
    TEXTURED_RECT_LAST_GROUP | 1, 120, 104, 128, 24,
};
#endif

TexturedRectEntry gMovieTheaterEndingRowBarSprites[4] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(752, 253), 255, 0,
        GET_TPAGE(0, 1, 640, 0), 0, 21, 255, 21 }, 1, 192, 132, 256, 21 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(752, 253), 104, 0,
        GET_TPAGE(0, 1, 704, 0), 0, 21, 104, 21 }, 1, 448, 132, 104, 21 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(752, 253), 255, 0,
        GET_TPAGE(0, 1, 640, 0), 0, 21, 255, 21 }, 1, 192, 151, 256, 21 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(752, 253), 104, 0,
        GET_TPAGE(0, 1, 704, 0), 0, 21, 104, 21 }, TEXTURED_RECT_LAST_GROUP | 1, 447, 151, 104, 21 },
};

TexturedRectEntry gMovieTheaterEnding1PromptSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 181, GET_CLUT(752, 255), 128, 181,
        GET_TPAGE(0, 1, 704, 0), 0, 197, 128, 197 },
    TEXTURED_RECT_LAST_GROUP | 1, 196, 134, 128, 16,
};

TexturedRectEntry gMovieTheaterEnding2PromptSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 197, GET_CLUT(752, 254), 128, 197,
        GET_TPAGE(0, 1, 704, 0), 0, 213, 128, 213 },
    TEXTURED_RECT_LAST_GROUP | 1, 196, 153, 128, 16,
};

TexturedRectEntry gMovieTheaterLockedEndingSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 178, GET_CLUT(752, 251), 220, 178,
        GET_TPAGE(0, 1, 640, 0), 0, 194, 220, 194 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 220, 16,
};

#ifdef VERSION_JP
TexturedRectEntry gMovieTheaterNameRedShadowSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 21, GET_CLUT(752, 235), 80, 21,
        GET_TPAGE(0, 1, 704, 0), 0, 45, 80, 45 },
    TEXTURED_RECT_LAST_GROUP | 1, 456, 104, 80, 24,
};

TexturedRectEntry gMovieTheaterNameTatsumiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 80, 21, GET_CLUT(752, 234), 160, 21,
        GET_TPAGE(0, 1, 704, 0), 80, 45, 160, 45 },
    TEXTURED_RECT_LAST_GROUP | 1, 456, 104, 80, 24,
};

TexturedRectEntry gMovieTheaterNameMikadoSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 45, GET_CLUT(752, 233), 80, 45,
        GET_TPAGE(0, 1, 704, 0), 0, 69, 80, 69 },
    TEXTURED_RECT_LAST_GROUP | 1, 456, 104, 80, 24,
};

TexturedRectEntry gMovieTheaterNameBlackLotusSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 21, GET_CLUT(752, 232), 240, 21,
        GET_TPAGE(0, 1, 704, 0), 160, 45, 240, 45 },
    TEXTURED_RECT_LAST_GROUP | 1, 456, 104, 80, 24,
};

TexturedRectEntry gMovieTheaterNameUtsusemiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 80, 45, GET_CLUT(752, 231), 160, 45,
        GET_TPAGE(0, 1, 704, 0), 80, 69, 160, 69 },
    TEXTURED_RECT_LAST_GROUP | 1, 456, 104, 80, 24,
};

TexturedRectEntry gMovieTheaterNameKannukiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 45, GET_CLUT(752, 230), 240, 45,
        GET_TPAGE(0, 1, 704, 0), 160, 69, 240, 69 },
    TEXTURED_RECT_LAST_GROUP | 1, 456, 104, 80, 24,
};

TexturedRectEntry *gMovieTheaterNameSprites[PLAYABLE_CHARACTER_COUNT] = {
    &gMovieTheaterNameRedShadowSprite,
    &gMovieTheaterNameTatsumiSprite,
    &gMovieTheaterNameMikadoSprite,
    &gMovieTheaterNameBlackLotusSprite,
    &gMovieTheaterNameUtsusemiSprite,
    &gMovieTheaterNameKannukiSprite,
};
#else
TexturedRectEntry gMovieTheaterNameRedShadowSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 1, GET_CLUT(752, 235), 144, 1,
        GET_TPAGE(0, 1, 768, 0), 0, 25, 144, 25 },
    TEXTURED_RECT_LAST_GROUP | 1, 408, 104, 144, 24,
};

TexturedRectEntry gMovieTheaterNameTatsumiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 73, GET_CLUT(752, 234), 144, 73,
        GET_TPAGE(0, 1, 768, 0), 0, 97, 144, 97 },
    TEXTURED_RECT_LAST_GROUP | 1, 408, 104, 144, 24,
};

TexturedRectEntry gMovieTheaterNameMikadoSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 120, GET_CLUT(752, 233), 144, 120,
        GET_TPAGE(0, 1, 768, 0), 0, 144, 144, 144 },
    TEXTURED_RECT_LAST_GROUP | 1, 408, 104, 144, 24,
};

TexturedRectEntry gMovieTheaterNameBlackLotusSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 25, GET_CLUT(752, 232), 144, 25,
        GET_TPAGE(0, 1, 768, 0), 0, 49, 144, 49 },
    TEXTURED_RECT_LAST_GROUP | 1, 408, 104, 144, 24,
};

TexturedRectEntry gMovieTheaterNameUtsusemiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 49, GET_CLUT(752, 231), 144, 49,
        GET_TPAGE(0, 1, 768, 0), 0, 73, 144, 73 },
    TEXTURED_RECT_LAST_GROUP | 1, 408, 104, 144, 24,
};

TexturedRectEntry gMovieTheaterNameKannukiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 97, GET_CLUT(752, 230), 144, 97,
        GET_TPAGE(0, 1, 768, 0), 0, 121, 144, 121 },
    TEXTURED_RECT_LAST_GROUP | 1, 408, 104, 144, 24,
};

TexturedRectEntry *gMovieTheaterNameSprites[PLAYABLE_CHARACTER_COUNT] = {
    &gMovieTheaterNameRedShadowSprite,
    &gMovieTheaterNameTatsumiSprite,
    &gMovieTheaterNameMikadoSprite,
    &gMovieTheaterNameBlackLotusSprite,
    &gMovieTheaterNameUtsusemiSprite,
    &gMovieTheaterNameKannukiSprite,
};
#endif

TexturedRectEntry gMovieTheaterOpeningThumbSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 132, 194, GET_CLUT(752, 223), 212, 194,
        GET_TPAGE(0, 1, 640, 0), 132, 240, 212, 240 },
    TEXTURED_RECT_LAST_GROUP | 1, 512, 44, 80, 46,
};

TexturedRectEntry gMovieTheaterPortraitRedShadowSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 84, 22, GET_CLUT(752, 229), 168, 22,
        GET_TPAGE(0, 0, 640, 0), 84, 68, 168, 68 },
    TEXTURED_RECT_LAST_GROUP | 1, 32, 176, 84, 46,
};

TexturedRectEntry gMovieTheaterPortraitTatsumiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 168, 22, GET_CLUT(752, 228), 252, 22,
        GET_TPAGE(0, 0, 640, 0), 168, 68, 252, 68 },
    TEXTURED_RECT_LAST_GROUP | 1, 128, 176, 84, 46,
};

TexturedRectEntry gMovieTheaterPortraitMikadoSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 168, 68, GET_CLUT(752, 227), 252, 68,
        GET_TPAGE(0, 0, 640, 0), 168, 114, 252, 114 },
    TEXTURED_RECT_LAST_GROUP | 1, 224, 176, 84, 46,
};

TexturedRectEntry gMovieTheaterPortraitBlackLotusSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 22, GET_CLUT(752, 226), 84, 22,
        GET_TPAGE(0, 0, 640, 0), 0, 68, 84, 68 },
    TEXTURED_RECT_LAST_GROUP | 1, 320, 176, 84, 46,
};

TexturedRectEntry gMovieTheaterPortraitUtsusemiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 68, GET_CLUT(752, 225), 84, 68,
        GET_TPAGE(0, 0, 640, 0), 0, 114, 84, 114 },
    TEXTURED_RECT_LAST_GROUP | 1, 416, 176, 84, 46,
};

TexturedRectEntry gMovieTheaterPortraitKannukiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 84, 68, GET_CLUT(752, 224), 168, 68,
        GET_TPAGE(0, 0, 640, 0), 84, 114, 168, 114 },
    TEXTURED_RECT_LAST_GROUP | 1, 512, 176, 84, 46,
};

TexturedRectEntry *gMovieTheaterPortraitSprites[PLAYABLE_CHARACTER_COUNT] = {
    &gMovieTheaterPortraitRedShadowSprite,
    &gMovieTheaterPortraitTatsumiSprite,
    &gMovieTheaterPortraitMikadoSprite,
    &gMovieTheaterPortraitBlackLotusSprite,
    &gMovieTheaterPortraitUtsusemiSprite,
    &gMovieTheaterPortraitKannukiSprite,
};

#ifdef VERSION_JP
TexturedRectEntry gMovieTheaterEnding1RedShadowSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 164, 101, GET_CLUT(752, 247), 236, 101,
        GET_TPAGE(0, 0, 704, 0), 164, 117, 236, 117 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 72, 16,
};

TexturedRectEntry gMovieTheaterEnding2RedShadowSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 85, GET_CLUT(752, 246), 208, 85,
        GET_TPAGE(0, 0, 704, 0), 0, 101, 208, 101 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 208, 16,
};

TexturedRectEntry gMovieTheaterEnding1TatsumiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 168, 69, GET_CLUT(752, 245), 232, 69,
        GET_TPAGE(0, 0, 704, 0), 168, 85, 232, 85 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 64, 16,
};

TexturedRectEntry gMovieTheaterEnding2TatsumiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 133, GET_CLUT(752, 244), 160, 133,
        GET_TPAGE(0, 0, 704, 0), 0, 149, 160, 149 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 160, 16,
};

TexturedRectEntry gMovieTheaterEnding1MikadoSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 117, GET_CLUT(752, 243), 232, 117,
        GET_TPAGE(0, 0, 704, 0), 160, 133, 232, 133 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 72, 16,
};

TexturedRectEntry gMovieTheaterEnding2MikadoSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 149, GET_CLUT(752, 242), 232, 149,
        GET_TPAGE(0, 0, 704, 0), 128, 165, 232, 165 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 104, 16,
};

TexturedRectEntry gMovieTheaterEnding1BlackLotusSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 101, GET_CLUT(752, 241), 164, 101,
        GET_TPAGE(0, 0, 704, 0), 0, 117, 164, 117 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 164, 16,
};

TexturedRectEntry gMovieTheaterEnding2BlackLotusSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 69, GET_CLUT(752, 240), 168, 69,
        GET_TPAGE(0, 0, 704, 0), 0, 85, 168, 85 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 168, 16,
};

TexturedRectEntry gMovieTheaterEnding1UtsusemiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 133, GET_CLUT(752, 239), 240, 133,
        GET_TPAGE(0, 0, 704, 0), 160, 149, 240, 149 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 80, 16,
};

TexturedRectEntry gMovieTheaterEnding2UtsusemiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 117, GET_CLUT(752, 238), 160, 117,
        GET_TPAGE(0, 0, 704, 0), 0, 133, 160, 133 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 160, 16,
};

TexturedRectEntry gMovieTheaterEnding1KannukiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 149, GET_CLUT(752, 237), 128, 149,
        GET_TPAGE(0, 0, 704, 0), 0, 165, 128, 165 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 128, 16,
};

TexturedRectEntry gMovieTheaterEnding2KannukiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 165, GET_CLUT(752, 236), 136, 165,
        GET_TPAGE(0, 0, 704, 0), 0, 181, 136, 181 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 136, 16,
};
#else
TexturedRectEntry gMovieTheaterEnding1RedShadowSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 168, 85, GET_CLUT(752, 247), 224, 85,
        GET_TPAGE(0, 0, 704, 0), 168, 101, 224, 101 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 56, 16,
};

TexturedRectEntry gMovieTheaterEnding2RedShadowSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 85, GET_CLUT(752, 246), 40, 85,
        GET_TPAGE(0, 0, 704, 0), 0, 101, 40, 101 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 40, 16,
};

TexturedRectEntry gMovieTheaterEnding1TatsumiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 144, 149, GET_CLUT(752, 245), 248, 149,
        GET_TPAGE(0, 0, 704, 0), 144, 165, 248, 165 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 104, 16,
};

TexturedRectEntry gMovieTheaterEnding2TatsumiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 133, GET_CLUT(752, 244), 208, 133,
        GET_TPAGE(0, 0, 704, 0), 0, 149, 208, 149 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 208, 16,
};

TexturedRectEntry gMovieTheaterEnding1MikadoSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 40, 85, GET_CLUT(752, 243), 168, 85,
        GET_TPAGE(0, 0, 704, 0), 40, 101, 168, 101 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 128, 16,
};

TexturedRectEntry gMovieTheaterEnding2MikadoSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 88, 165, GET_CLUT(752, 242), 200, 165,
        GET_TPAGE(0, 0, 704, 0), 88, 181, 200, 181 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 112, 16,
};

TexturedRectEntry gMovieTheaterEnding1BlackLotusSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 101, GET_CLUT(752, 241), 192, 101,
        GET_TPAGE(0, 0, 704, 0), 0, 117, 192, 117 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 192, 16,
};

TexturedRectEntry gMovieTheaterEnding2BlackLotusSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 69, GET_CLUT(752, 240), 208, 69,
        GET_TPAGE(0, 0, 704, 0), 0, 85, 208, 85 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 208, 16,
};

TexturedRectEntry gMovieTheaterEnding1UtsusemiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 152, 117, GET_CLUT(752, 239), 248, 117,
        GET_TPAGE(0, 0, 704, 0), 152, 133, 248, 133 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 96, 16,
};

TexturedRectEntry gMovieTheaterEnding2UtsusemiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 117, GET_CLUT(752, 238), 152, 117,
        GET_TPAGE(0, 0, 704, 0), 0, 133, 152, 133 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 152, 16,
};

TexturedRectEntry gMovieTheaterEnding1KannukiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 149, GET_CLUT(752, 237), 144, 149,
        GET_TPAGE(0, 0, 704, 0), 0, 165, 144, 165 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 134, 144, 16,
};

TexturedRectEntry gMovieTheaterEnding2KannukiSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 165, GET_CLUT(752, 236), 88, 165,
        GET_TPAGE(0, 0, 704, 0), 0, 181, 88, 181 },
    TEXTURED_RECT_LAST_GROUP | 1, 328, 153, 88, 16,
};
#endif

TexturedRectEntry *gMovieTheaterEndingTitleSprites[PLAYABLE_CHARACTER_COUNT][MOVIE_THEATER_ENDING_COUNT] = {
    { &gMovieTheaterEnding1RedShadowSprite, &gMovieTheaterEnding2RedShadowSprite },
    { &gMovieTheaterEnding1TatsumiSprite, &gMovieTheaterEnding2TatsumiSprite },
    { &gMovieTheaterEnding1MikadoSprite, &gMovieTheaterEnding2MikadoSprite },
    { &gMovieTheaterEnding1BlackLotusSprite, &gMovieTheaterEnding2BlackLotusSprite },
    { &gMovieTheaterEnding1UtsusemiSprite, &gMovieTheaterEnding2UtsusemiSprite },
    { &gMovieTheaterEnding1KannukiSprite, &gMovieTheaterEnding2KannukiSprite },
};

static DR_TWIN gMovieTheaterTexWindowSet[2];
static DR_TWIN gMovieTheaterTexWindowRestore[2];

void DrawMovieTheaterMenu(int selection, int options, int enabled)
{
    RECT window;
    DRAWENV env;
    u_char openingShade;
    u_char endingShade;
    int slot;

    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);
    DrawTexturedRectList(gMovieTheaterHeaderSprites, 0, 0, 0);
    if (selection == MOVIE_THEATER_SELECTION_OPENING) {
        openingShade = MOVIE_THEATER_LABEL_SHADE_ACTIVE;
        endingShade = MOVIE_THEATER_LABEL_SHADE_DIM;
    } else {
        openingShade = MOVIE_THEATER_LABEL_SHADE_DIM;
        endingShade = MOVIE_THEATER_LABEL_SHADE_ACTIVE;
    }
    gMovieTheaterOpeningLabelSprite.texture.r = gMovieTheaterOpeningLabelSprite.texture.g =
        gMovieTheaterOpeningLabelSprite.texture.b = openingShade;
    gMovieTheaterEndingLabelSprite.texture.r = gMovieTheaterEndingLabelSprite.texture.g =
        gMovieTheaterEndingLabelSprite.texture.b = endingShade;
    DrawTexturedRectList(&gMovieTheaterOpeningLabelSprite, 0, 0, 0);
    DrawTexturedRectList(&gMovieTheaterEndingLabelSprite, 0, 0, 0);

    if (selection != MOVIE_THEATER_SELECTION_OPENING) {
        DrawTexturedRectList(&gMovieTheaterEnding1PromptSprite, 0, 0, 0);
        DrawTexturedRectList(&gMovieTheaterEnding2PromptSprite, 0, 0, 0);
        if (options & MOVIE_THEATER_ENDING1_UNLOCKED) {
            DrawTexturedRectList(gMovieTheaterEndingTitleSprites[selection - 1][0], 0, 0, 0);
        } else {
            DrawTexturedRectList(&gMovieTheaterLockedEndingSprite, 0, 0, 0);
        }
        if (options & MOVIE_THEATER_ENDING2_UNLOCKED) {
            DrawTexturedRectList(gMovieTheaterEndingTitleSprites[selection - 1][1], 0, 0, 0);
        } else {
            DrawTexturedRectList(&gMovieTheaterLockedEndingSprite, 0, MOVIE_THEATER_ENDING_ROW_STEP_Y, 0);
        }
        DrawTexturedRectList(gMovieTheaterNameSprites[selection - 1], 0, 0, 0);
        DrawTexturedRectList(gMovieTheaterEndingRowBarSprites, 0, 0, 0);
    }

    for (slot = 0; slot < PLAYABLE_CHARACTER_COUNT; ++slot) {
        if (slot == selection - 1) {
            gMovieTheaterPortraitSprites[slot]->texture.r = gMovieTheaterPortraitSprites[slot]->texture.g =
                gMovieTheaterPortraitSprites[slot]->texture.b = MOVIE_THEATER_PORTRAIT_SHADE_ACTIVE;
        } else if ((enabled >> (PLAYABLE_CHARACTER_COUNT - 1 - slot)) & 1) {
            gMovieTheaterPortraitSprites[slot]->texture.r = gMovieTheaterPortraitSprites[slot]->texture.g =
                gMovieTheaterPortraitSprites[slot]->texture.b = MOVIE_THEATER_PORTRAIT_SHADE_ENABLED;
        } else {
            gMovieTheaterPortraitSprites[slot]->texture.r = gMovieTheaterPortraitSprites[slot]->texture.g =
                gMovieTheaterPortraitSprites[slot]->texture.b = MOVIE_THEATER_PORTRAIT_SHADE_LOCKED;
        }
        DrawTexturedRectList(gMovieTheaterPortraitSprites[slot], 0, 0, 0);
    }

    gMovieTheaterOpeningThumbSprite.texture.r = gMovieTheaterOpeningThumbSprite.texture.g =
        gMovieTheaterOpeningThumbSprite.texture.b = openingShade;
    DrawTexturedRectList(&gMovieTheaterOpeningThumbSprite, 0, 0, 0);
    DrawTexturedRectList(gMovieTheaterPanelSprites, 0, 0, 0);
    GetDrawEnv(&env);
    window.x = MOVIE_THEATER_BACKDROP_TW_X;
    window.y = MOVIE_THEATER_BACKDROP_TW_Y;
    window.w = MOVIE_THEATER_BACKDROP_TW_W;
    window.h = MOVIE_THEATER_BACKDROP_TW_H;
    SetTexWindow(&gMovieTheaterTexWindowRestore[gFrameCounter & 1], &env.tw);
    AddPrim(gCurrentOt, &gMovieTheaterTexWindowRestore[gFrameCounter & 1]);
    DrawTexturedRectList(gMovieTheaterBackdropSprites, 0, 0, 0);
    SetTexWindow(&gMovieTheaterTexWindowSet[gFrameCounter & 1], &window);
    AddPrim(gCurrentOt, &gMovieTheaterTexWindowSet[gFrameCounter & 1]);
}

void LoadMovieTheaterAssets(void)
{
    unsigned char *resource;
#ifndef VERSION_JP
    int timOffset;
    int *subtitleBank;
#endif

    ResetModelArena();
    resource = GetArenaFreePtr();
    ReadCdFileSync(CD_FILE_MOVIE_THEATER_ASSETS, resource);
#ifdef VERSION_JP
    /* The Japanese archive is the TIM group alone; elsewhere an offset header comes first and a subtitle bank last. */
    LoadTimGroup((unsigned short *)resource, 0);
#else
    timOffset = ((MovieTheaterArchiveHeader *)resource)->timOffset;
    subtitleBank = (int *)(resource + ((MovieTheaterArchiveHeader *)resource)->subtitleOffset);
    LoadTimGroup((unsigned short *)(resource + timOffset), 0);
    RelocateSubtitleBank(subtitleBank, SUBTITLE_BANK_MOVIE);
#endif
    DrawSync(0);
    gMovieTheaterBackdropSprites[0].texture.u0 = 0;
    gMovieTheaterBackdropSprites[0].texture.v0 = 0;
    gMovieTheaterBackdropSprites[0].texture.u1 = 224;
    gMovieTheaterBackdropSprites[0].texture.v1 = 0;
    gMovieTheaterBackdropSprites[0].texture.u2 = 0;
    gMovieTheaterBackdropSprites[0].texture.v2 = 240;
    gMovieTheaterBackdropSprites[0].texture.u3 = 224;
    gMovieTheaterBackdropSprites[0].texture.v3 = 240;
    gMovieTheaterBackdropSprites[1].texture.u0 = 0;
    gMovieTheaterBackdropSprites[1].texture.v0 = 0;
    gMovieTheaterBackdropSprites[1].texture.u1 = 224;
    gMovieTheaterBackdropSprites[1].texture.v1 = 0;
    gMovieTheaterBackdropSprites[1].texture.u2 = 0;
    gMovieTheaterBackdropSprites[1].texture.v2 = 240;
    gMovieTheaterBackdropSprites[1].texture.u3 = 224;
    gMovieTheaterBackdropSprites[1].texture.v3 = 240;
    gMovieTheaterBackdropSprites[2].texture.u0 = 0;
    gMovieTheaterBackdropSprites[2].texture.v0 = 0;
    gMovieTheaterBackdropSprites[2].texture.u1 = 192;
    gMovieTheaterBackdropSprites[2].texture.v1 = 0;
    gMovieTheaterBackdropSprites[2].texture.u2 = 0;
    gMovieTheaterBackdropSprites[2].texture.v2 = 240;
    gMovieTheaterBackdropSprites[2].texture.u3 = 192;
    gMovieTheaterBackdropSprites[2].texture.v3 = 240;
}

#ifndef VERSION_JP
void LoadMovieSubtitleBank(void *buffer)
{
    ReadCdFileSync(CD_FILE_MOVIE_SUBTITLES, buffer);
    RelocateSubtitleBank(buffer, SUBTITLE_BANK_MOVIE);
}

void ResetFramePacketCursor(void)
{
    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);
}
#endif
