/* The select screens' 2D layer: portraits, cursors, weapon cards and the stage list, the confirm
 * blink and stamp shake, and the stage cursor's movement. */
#include <rand.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "display_buffer.h"
#include "gpu_code.h"
#include "packet_build.h"
#include "angle.h"
#include "sprite2d.h"
#include "select_ui.h"
#include "main.h"
#include "scene_math.h"
#include "globals.h"
#include "render.h"
#include "rot_matrix.h"
#include "font_hud.h"

static int gCursorGlowPhase;
static int gCharConfirmBlink[2];
static int gWeaponStampAngle[2];
static int gWeaponCardX[2];
static int gWeaponCardY[2];
static int gWeaponStampShakeP1;
static int gWeaponStampShakeP2;

#define SELECT_CHAR_SLOT_COUNT      6
#define SELECT_CPU_SLOT_OFFSET      7
#define SELECT_SLOT_CONFIRMED       1
#define SELECT_SLOT_HIDDEN          0xffff
#define SELECT_CONFIRM_BLINK_FRAMES 30
#define SELECT_CURSOR_PULSE_STEP    0x60
#define SELECT_CURSOR_TAG_OFFSET_X  0x38
#define SELECT_CHAR_NAME_P1_X       0x40
#define SELECT_CHAR_NAME_P2_X       0x168
#define SELECT_CHAR_NAME_Y          0x78
#define SELECT_CHAR_NAME_FONT_BANK  1
#define SELECT_OT_BACKGROUND        0xfff
#define SELECT_OT_FOREGROUND        0

#define SELECT_WEAPON_COUNT         9
#define WEAPON_CARD_P1_BASE_X       0x24
#define WEAPON_CARD_P2_BASE_X       0x154
#define WEAPON_CARD_BASE_Y          0x88
#define WEAPON_STAMP_MAX_ANGLE      0x700
#define WEAPON_STAMP_ANGLE_MASK     0x5ff
#define WEAPON_STAMP_SHAKE_UNSET    (-1)

#define STAGE_ROW_BRIGHTNESS_ACTIVE 0xc0
#define STAGE_ROW_BRIGHTNESS_DIM    0x50
#define STAGE_SELECT_ROWS_PER_COL   4

void InitSelectScreenUiState(void)
{
    gCursorGlowPhase = 0;
    gCharConfirmBlink[0] = -1;
    gCharConfirmBlink[1] = -1;

    gWeaponCardX[0] = WEAPON_CARD_P1_BASE_X;
    gWeaponCardX[1] = WEAPON_CARD_P2_BASE_X;
    gWeaponCardY[0] = WEAPON_CARD_BASE_Y;
    gWeaponCardY[1] = WEAPON_CARD_BASE_Y;

    gWeaponStampAngle[0] = WEAPON_STAMP_MAX_ANGLE - (rand() & WEAPON_STAMP_ANGLE_MASK);
    gWeaponStampAngle[1] = WEAPON_STAMP_MAX_ANGLE - (rand() & WEAPON_STAMP_ANGLE_MASK);
    gWeaponStampShakeP2 = WEAPON_STAMP_SHAKE_UNSET;
    gWeaponStampShakeP1 = WEAPON_STAMP_SHAKE_UNSET;
}

typedef struct {
    short x, y;
} SelectSlotPos;

TexturedRectEntry gSelectHeaderPlateSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 8, 186, GET_CLUT(1008, 153), 136, 186,
        GET_TPAGE(0, 0, 960, 256), 8, 222, 136, 222 }, 1, 192, 12, 128, 36 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 135, 221, GET_CLUT(1008, 153), 7, 221,
        GET_TPAGE(0, 0, 960, 256), 135, 185, 7, 185 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 320, 12, 128, 36 },
};

TexturedRectEntry gSelectTopBorderSprites[6] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 152, 170, GET_CLUT(1008, 144), 248, 170,
        GET_TPAGE(0, 0, 960, 256), 152, 194, 248, 194 }, 1, 0, 16, 96, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 247, 170, GET_CLUT(1008, 144), 151, 170,
        GET_TPAGE(0, 0, 960, 256), 247, 194, 151, 194 }, 1, 96, 16, 96, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 152, 170, GET_CLUT(1008, 144), 248, 170,
        GET_TPAGE(0, 0, 960, 256), 152, 194, 248, 194 }, 1, 448, 16, 96, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 247, 170, GET_CLUT(1008, 144), 151, 170,
        GET_TPAGE(0, 0, 960, 256), 247, 194, 151, 194 }, 1, 544, 16, 96, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 57, GET_CLUT(1008, 148), 128, 57,
        GET_TPAGE(0, 0, 960, 256), 0, 61, 128, 61 }, 1, 192, 16, 128, 4 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 127, 57, GET_CLUT(1008, 148), 255, 57,
        GET_TPAGE(0, 0, 960, 256), 127, 61, 255, 61 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 320, 16, 128, 4 },
};

TexturedRectEntry gCharSelectBottomFrameSprites[6] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 8, 180, GET_CLUT(1008, 145), 160, 180,
        GET_TPAGE(0, 0, 896, 256), 8, 214, 160, 214 }, 1, 168, 209, 152, 34 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 159, 180, GET_CLUT(1008, 145), 7, 180,
        GET_TPAGE(0, 0, 896, 256), 159, 214, 7, 214 }, 1, 320, 209, 152, 34 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 176, 146, GET_CLUT(1008, 146), 208, 146,
        GET_TPAGE(0, 0, 896, 256), 176, 203, 208, 203 }, 1, 0, 152, 32, 57 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 207, 146, GET_CLUT(1008, 146), 175, 146,
        GET_TPAGE(0, 0, 896, 256), 207, 203, 175, 203 }, 1, 608, 152, 32, 57 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 8, 146, GET_CLUT(1008, 147), 176, 146,
        GET_TPAGE(0, 0, 896, 256), 8, 180, 176, 180 }, 1, 0, 209, 168, 34 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 175, 146, GET_CLUT(1008, 147), 7, 146,
        GET_TPAGE(0, 0, 896, 256), 175, 180, 7, 180 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 472, 209, 168, 34 },
};

TexturedRectEntry gCharSelectTitleSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 238, GET_CLUT(1008, 149), 232, 238,
        GET_TPAGE(0, 0, 960, 256), 0, 254, 232, 254 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 204, 22, 232, 16 },
};

struct {
    GlyphTexture texture;
    int countAndFlags;
    RECT rects[SELECT_CHAR_SLOT_COUNT];
} gCharSelectSlotFrameSprites = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 100, 0, GET_CLUT(1008, 140), 200, 0,
      GET_TPAGE(0, 0, 960, 256), 100, 57, 200, 57 },
    (int)(TEXTURED_RECT_LAST_GROUP | SELECT_CHAR_SLOT_COUNT),
    {
        { 32, 152, 100, 57 },
        { 128, 152, 100, 57 },
        { 224, 152, 100, 57 },
        { 320, 152, 100, 57 },
        { 416, 152, 100, 57 },
        { 512, 152, 100, 57 },
    },
};

TexturedRectEntry gCharPortraitRedShadowSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(1008, 128), 84, 0,
        GET_TPAGE(0, 0, 896, 256), 0, 46, 84, 46 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 40, 156, 84, 46 },
};
TexturedRectEntry gCharPortraitTatsumiSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 84, 0, GET_CLUT(1008, 129), 168, 0,
        GET_TPAGE(0, 0, 896, 256), 84, 46, 168, 46 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 136, 156, 84, 46 },
};
TexturedRectEntry gCharPortraitMikadoSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 168, 0, GET_CLUT(1008, 130), 252, 0,
        GET_TPAGE(0, 0, 896, 256), 168, 46, 252, 46 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 232, 156, 84, 46 },
};
TexturedRectEntry gCharPortraitBlackLotusSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 46, GET_CLUT(1008, 131), 84, 46,
        GET_TPAGE(0, 0, 896, 256), 0, 92, 84, 92 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 328, 156, 84, 46 },
};
TexturedRectEntry gCharPortraitUtsusemiSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 84, 46, GET_CLUT(1008, 132), 168, 46,
        GET_TPAGE(0, 0, 896, 256), 84, 92, 168, 92 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 424, 156, 84, 46 },
};
TexturedRectEntry gCharPortraitKannukiSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 168, 46, GET_CLUT(1008, 133), 252, 46,
        GET_TPAGE(0, 0, 896, 256), 168, 92, 252, 92 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 520, 156, 84, 46 },
};

TexturedRectEntry gCharSelectPortraitBadgeSprites[6] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 92, GET_CLUT(1008, 134), 40, 92,
        GET_TPAGE(0, 0, 896, 256), 0, 146, 40, 146 }, 1, 32, 140, 40, 54 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 40, 92, GET_CLUT(1008, 135), 80, 92,
        GET_TPAGE(0, 0, 896, 256), 40, 146, 80, 146 }, 1, 128, 140, 40, 54 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 80, 92, GET_CLUT(1008, 136), 120, 92,
        GET_TPAGE(0, 0, 896, 256), 80, 146, 120, 146 }, 1, 224, 140, 40, 54 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 120, 92, GET_CLUT(1008, 137), 160, 92,
        GET_TPAGE(0, 0, 896, 256), 120, 146, 160, 146 }, 1, 320, 140, 40, 54 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 92, GET_CLUT(1008, 138), 200, 92,
        GET_TPAGE(0, 0, 896, 256), 160, 146, 200, 146 }, 1, 416, 140, 40, 54 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 92, GET_CLUT(1008, 139), 240, 92,
        GET_TPAGE(0, 0, 896, 256), 200, 146, 240, 146 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 512, 140, 40, 54 },
};

TexturedRectEntry gCharSelectFooterLabelSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 214, GET_CLUT(1008, 150), 128, 214,
        GET_TPAGE(0, 0, 896, 256), 64, 232, 128, 232 }, 1, 474, 212, 63, 18 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 214, GET_CLUT(1008, 151), 64, 214,
        GET_TPAGE(0, 1, 896, 256), 0, 232, 64, 232 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 104, 212, 62, 18 },
};

TexturedRectEntry gCharSelectP1CursorSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(1008, 141), 100, 0,
        GET_TPAGE(0, 0, 960, 256), 0, 57, 100, 57 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 0, 0, 100, 57 },
};
TexturedRectEntry gCharSelectP2CursorSprites[2] = {
#ifdef VERSION_JP
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 208, GET_CLUT(1008, 142), 156, 208,
        GET_TPAGE(0, 0, 960, 0), 112, 234, 156, 234 }, 1, 0, 0, 44, 26 },
#else
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 26, GET_CLUT(1008, 142), 244, 26,
        GET_TPAGE(0, 0, 960, 256), 200, 52, 244, 52 }, 1, 0, 0, 44, 26 },
#endif
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(1008, 142), 100, 0,
        GET_TPAGE(0, 0, 960, 256), 0, 57, 100, 57 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 0, 0, 100, 57 },
};
TexturedRectEntry gCharSelectCpuCursorSprites[2] = {
#ifdef VERSION_JP
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 156, 208, GET_CLUT(1008, 173), 200, 208,
        GET_TPAGE(0, 0, 960, 0), 156, 234, 200, 234 }, 1, 0, 0, 44, 26 },
#else
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 0, GET_CLUT(1008, 173), 244, 0,
        GET_TPAGE(0, 0, 960, 256), 200, 26, 244, 26 }, 1, 0, 0, 44, 26 },
#endif
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(1008, 173), 100, 0,
        GET_TPAGE(0, 0, 960, 256), 0, 57, 100, 57 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 0, 0, 100, 57 },
};

SelectSlotPos gCharSelectSlotPositions[SELECT_CHAR_SLOT_COUNT] = {
    { 32, 152 },
    { 128, 152 },
    { 224, 152 },
    { 320, 152 },
    { 416, 152 },
    { 512, 152 },
};

TexturedRectEntry gWeaponSelectTitleSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 222, GET_CLUT(1008, 152), 232, 222,
        GET_TPAGE(0, 0, 960, 256), 0, 238, 232, 238 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 204, 22, 232, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 222, GET_CLUT(1008, 152), 232, 222,
        GET_TPAGE(0, 0, 960, 256), 0, 238, 232, 238 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 200, 16, 232, 16 },
};

TexturedRectEntry gWeaponCardFrameSprites[4] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 4, 61, GET_CLUT(1008, 183), 132, 61,
        GET_TPAGE(0, 1, 960, 256), 4, 101, 132, 101 }, 1, 0, 0, 128, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 131, 61, GET_CLUT(1008, 183), 3, 61,
        GET_TPAGE(0, 1, 960, 256), 131, 101, 3, 101 }, 1, 128, 0, 128, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 4, 100, GET_CLUT(1008, 183), 132, 100,
        GET_TPAGE(0, 1, 960, 256), 4, 60, 132, 60 }, 1, 0, 40, 128, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 131, 100, GET_CLUT(1008, 183), 3, 100,
        GET_TPAGE(0, 1, 960, 256), 131, 60, 3, 60 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 128, 40, 128, 40 },
};

TexturedRectEntry gWeaponCardShadowSprites[4] = {
#ifdef VERSION_JP
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 4, 244, GET_CLUT(1008, 184), 132, 244,
        GET_TPAGE(0, 0, 960, 0), 4, 252, 132, 252 }, 1, 8, 78, 128, 8 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 131, 244, GET_CLUT(1008, 184), 3, 244,
        GET_TPAGE(0, 0, 960, 0), 131, 252, 3, 252 }, 1, 136, 78, 128, 8 },
#else
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 8, 102, GET_CLUT(1008, 184), 136, 102,
        GET_TPAGE(0, 0, 960, 256), 8, 110, 136, 110 }, 1, 8, 78, 128, 8 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 135, 102, GET_CLUT(1008, 184), 7, 102,
        GET_TPAGE(0, 0, 960, 256), 135, 110, 7, 110 }, 1, 136, 78, 128, 8 },
#endif
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 160, 180, GET_CLUT(1008, 185), 168, 180,
        GET_TPAGE(0, 0, 896, 256), 160, 220, 168, 220 }, 1, 256, 6, 8, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 160, 219, GET_CLUT(1008, 185), 168, 219,
        GET_TPAGE(0, 0, 896, 256), 160, 187, 168, 187 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 256, 46, 8, 32 },
};

/* Indexed by weapon; 8 is the gun. */
#ifdef VERSION_JP
TexturedRectEntry gKatanaCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 192, GET_CLUT(1008, 177), 224, 192,
        GET_TPAGE(0, 0, 704, 256), 0, 255, 224, 255 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 8, 224, 64,
};
TexturedRectEntry gLongSwordCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(1008, 175), 224, 0,
        GET_TPAGE(0, 0, 704, 256), 0, 63, 224, 63 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 8, 224, 64,
};
TexturedRectEntry gBroadswordCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 192, GET_CLUT(1008, 181), 224, 192,
        GET_TPAGE(0, 0, 832, 256), 0, 255, 224, 255 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 8, 224, 64,
};
TexturedRectEntry gSaberCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 128, GET_CLUT(1008, 180), 224, 128,
        GET_TPAGE(0, 0, 832, 256), 0, 191, 224, 191 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 8, 224, 64,
};
TexturedRectEntry gNaginataCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 128, GET_CLUT(1008, 178), 224, 128,
        GET_TPAGE(0, 0, 768, 0), 0, 191, 224, 191 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 8, 224, 64,
};
TexturedRectEntry gRapierCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 192, GET_CLUT(1008, 179), 224, 192,
        GET_TPAGE(0, 0, 768, 0), 0, 255, 224, 255 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 8, 224, 64,
};
TexturedRectEntry gNodachiCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 64, GET_CLUT(1008, 182), 224, 64,
        GET_TPAGE(0, 0, 704, 256), 0, 127, 224, 127 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 8, 224, 64,
};
TexturedRectEntry gSledgehammerCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 128, GET_CLUT(1008, 178), 224, 128,
        GET_TPAGE(0, 0, 704, 256), 0, 191, 224, 191 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 8, 224, 64,
};
TexturedRectEntry gGunCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(1008, 188), 224, 0,
        GET_TPAGE(0, 0, 896, 256), 0, 63, 224, 63 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 8, 224, 64,
};
#else
TexturedRectEntry gKatanaCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 168, GET_CLUT(1008, 177), 224, 168,
        GET_TPAGE(0, 0, 704, 256), 0, 224, 224, 224 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 14, 224, 56,
};
TexturedRectEntry gLongSwordCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 184, GET_CLUT(1008, 181), 224, 184,
        GET_TPAGE(0, 0, 832, 256), 0, 240, 224, 240 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 12, 224, 56,
};
TexturedRectEntry gBroadswordCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(1008, 175), 224, 0,
        GET_TPAGE(0, 0, 704, 256), 0, 55, 224, 55 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 12, 224, 55,
};
TexturedRectEntry gSaberCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 128, GET_CLUT(1008, 180), 224, 128,
        GET_TPAGE(0, 0, 832, 256), 0, 184, 224, 184 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 12, 224, 56,
};
TexturedRectEntry gNaginataCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 128, GET_CLUT(1008, 178), 224, 128,
        GET_TPAGE(0, 0, 768, 0), 0, 184, 224, 184 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 12, 224, 56,
};
TexturedRectEntry gRapierCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 184, GET_CLUT(1008, 179), 224, 184,
        GET_TPAGE(0, 0, 768, 0), 0, 240, 224, 240 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 12, 224, 56,
};
TexturedRectEntry gNodachiCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 55, GET_CLUT(1008, 182), 224, 55,
        GET_TPAGE(0, 0, 704, 256), 0, 111, 224, 111 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 12, 224, 56,
};
TexturedRectEntry gSledgehammerCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 111, GET_CLUT(1008, 176), 224, 111,
        GET_TPAGE(0, 0, 704, 256), 0, 168, 224, 168 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 12, 224, 57,
};
TexturedRectEntry gGunCardArtSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(1008, 188), 224, 0,
        GET_TPAGE(0, 0, 896, 256), 0, 55, 224, 55 },
    (int)(TEXTURED_RECT_LAST_GROUP | 1), 16, 12, 224, 55,
};
#endif

TexturedRectEntry *gWeaponCardArtSprites[SELECT_WEAPON_COUNT] = {
    &gKatanaCardArtSprite, &gLongSwordCardArtSprite, &gBroadswordCardArtSprite, &gSaberCardArtSprite,
    &gNaginataCardArtSprite, &gRapierCardArtSprite, &gNodachiCardArtSprite, &gSledgehammerCardArtSprite,
    &gGunCardArtSprite,
};

struct {
    GlyphTexture texture;
    int countAndFlags;
    RECT rects[12];
} gStageSelectSideBorderSprites = {
#ifdef VERSION_JP
    { 128, 128, 128, GPU_CODE_POLY_FT4, 204, 208, GET_CLUT(1008, 166), 252, 208,
        GET_TPAGE(0, 0, 960, 0), 204, 248, 252, 248 },
#else
    { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 52, GET_CLUT(1008, 166), 248, 52,
        GET_TPAGE(0, 0, 960, 256), 200, 92, 248, 92 },
#endif
    (int)(TEXTURED_RECT_LAST_GROUP | 12),
    {
        { 0, 40, 48, 40 },
        { 0, 80, 48, 40 },
        { 0, 120, 48, 40 },
        { 0, 160, 48, 40 },
        { 0, 200, 48, 40 },
        { 192, 200, 48, 40 },
        { 400, 200, 48, 40 },
        { 592, 40, 48, 40 },
        { 592, 80, 48, 40 },
        { 592, 120, 48, 40 },
        { 592, 160, 48, 40 },
        { 592, 200, 48, 40 },
    },
};

struct {
    GlyphTexture texture;
    int countAndFlags;
    RECT rects[3];
} gStageSelectBottomBarSprites = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 8, 146, GET_CLUT(1008, 167), 152, 146,
        GET_TPAGE(0, 0, 960, 256), 8, 186, 152, 186 },
    (int)(TEXTURED_RECT_LAST_GROUP | 3),
    {
        { 48, 200, 144, 40 },
        { 248, 200, 144, 40 },
        { 448, 200, 144, 40 },
    },
};

struct {
    GlyphTexture normalTexture;
    int normalCount;
    RECT normalRects[2];
    GlyphTexture mirroredTexture;
    int mirroredCountAndFlags;
    RECT mirroredRects[2];
} gStageSelectTopBorderSprites = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 152, 146, GET_CLUT(1008, 164), 248, 146,
        GET_TPAGE(0, 0, 960, 256), 152, 170, 248, 170 },
    2,
    {
        { 0, 16, 96, 24 },
        { 448, 16, 96, 24 },
    },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 247, 146, GET_CLUT(1008, 164), 151, 146,
        GET_TPAGE(0, 0, 960, 256), 247, 170, 151, 170 },
    (int)(TEXTURED_RECT_LAST_GROUP | 2),
    {
        { 96, 16, 96, 24 },
        { 544, 16, 96, 24 },
    },
};

TexturedRectEntry gStageSelectTitleSprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 238, GET_CLUT(1008, 171), 232, 238,
        GET_TPAGE(0, 0, 896, 256), 0, 254, 232, 254 }, 1, 204, 22, 232, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 8, 186, GET_CLUT(1008, 153), 136, 186,
        GET_TPAGE(0, 0, 960, 256), 8, 222, 136, 222 }, 1, 192, 12, 128, 36 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 135, 221, GET_CLUT(1008, 153), 7, 221,
        GET_TPAGE(0, 0, 960, 256), 135, 185, 7, 185 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 320, 12, 128, 36 },
};

TexturedRectEntry gStageSelectFrameTrimSprites[4] = {
#ifdef VERSION_JP
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 104, 192, GET_CLUT(1008, 168), 112, 192,
        GET_TPAGE(0, 0, 960, 0), 104, 232, 112, 232 }, 1, 240, 200, 8, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 192, GET_CLUT(1008, 169), 104, 192,
        GET_TPAGE(0, 0, 960, 0), 96, 232, 104, 232 }, 1, 392, 200, 8, 40 },
#else
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 106, GET_CLUT(1008, 168), 168, 106,
        GET_TPAGE(0, 0, 960, 256), 160, 146, 168, 146 }, 1, 240, 200, 8, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 168, 106, GET_CLUT(1008, 169), 176, 106,
        GET_TPAGE(0, 0, 960, 256), 168, 146, 176, 146 }, 1, 392, 200, 8, 40 },
#endif
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 136, 58, GET_CLUT(1008, 170), 184, 58,
        GET_TPAGE(0, 0, 960, 256), 136, 74, 184, 74 }, 1, 0, 184, 48, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 136, 58, GET_CLUT(1008, 170), 184, 58,
        GET_TPAGE(0, 0, 960, 256), 136, 74, 184, 74 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 592, 184, 48, 16 },
};

#ifdef VERSION_JP
TexturedRectEntry gStageSelectRowPlateSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 200, 0, GET_CLUT(1008, 163), 248, 0,
        GET_TPAGE(0, 0, 960, 256), 200, 144, 248, 144 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 0, 52, 48, 144 },
};
TexturedRectEntry gStageSelectNameSprites[9] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 128, GET_CLUT(1008, 154), 32, 128,
        GET_TPAGE(0, 0, 960, 0), 0, 192, 32, 192 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 8, 64, 32, 64 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 128, GET_CLUT(1008, 162), 192, 128,
        GET_TPAGE(0, 1, 960, 0), 160, 208, 192, 208 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 8, 64, 32, 80 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 128, GET_CLUT(1008, 156), 96, 128,
        GET_TPAGE(0, 0, 960, 0), 64, 192, 96, 192 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 8, 64, 32, 64 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 80, GET_CLUT(1008, 157), 192, 80,
        GET_TPAGE(0, 0, 960, 256), 160, 144, 192, 144 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 8, 64, 32, 64 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 192, GET_CLUT(1008, 158), 64, 192,
        GET_TPAGE(0, 0, 960, 0), 32, 240, 64, 240 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 8, 64, 32, 48 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 192, GET_CLUT(1008, 159), 96, 192,
        GET_TPAGE(0, 0, 960, 0), 64, 240, 96, 240 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 8, 64, 32, 48 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 128, GET_CLUT(1008, 160), 128, 128,
        GET_TPAGE(0, 0, 960, 0), 96, 192, 128, 192 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 8, 64, 32, 64 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 128, GET_CLUT(1008, 161), 160, 128,
        GET_TPAGE(0, 0, 960, 0), 128, 176, 160, 176 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 8, 64, 32, 48 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 128, GET_CLUT(1008, 155), 64, 128,
        GET_TPAGE(0, 0, 960, 0), 32, 192, 64, 192 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 8, 64, 32, 64 },
};
/* Japanese Stage Select layout tables: { rowCount, { stageNameIndex, x }... }. */
short gStageSelectLayoutVs[] = {
    8,
    0, 72,
    1, 136,
    2, 200,
    3, 264,
    4, 328,
    5, 392,
    6, 456,
    7, 520,
    0,
};

short gStageSelectLayoutLink[] = {
    7,
    8, 80,
    1, 152,
    3, 224,
    4, 296,
    5, 368,
    6, 440,
    7, 512,
    0,
};

/* Entries 8 and 9 are the empty layout of the modes without a stage select. */
short gStageSelectLayoutPractice[] = {
    3,
    7, 148,
    4, 296,
    6, 444,
    0,
    -1, 0,
};

short *gStageSelectLayouts[8] = {
    &gStageSelectLayoutPractice[8], gStageSelectLayoutVs,
    &gStageSelectLayoutPractice[8], gStageSelectLayoutVs,
    gStageSelectLayoutPractice, gStageSelectLayoutLink,
    &gStageSelectLayoutPractice[8], gStageSelectLayoutVs,
};
#else
TexturedRectEntry gStageSelectRowPlateSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 224, GET_CLUT(1008, 163), 248, 224,
        GET_TPAGE(0, 0, 960, 0), 0, 248, 248, 248 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 0, 0, 248, 24 },
};

TexturedRectEntry gStageSelectNameSprites[9] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 128, GET_CLUT(1008, 154), 136, 128,
        GET_TPAGE(0, 0, 960, 0), 0, 144, 136, 144 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 56, 4, 136, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 208, GET_CLUT(1008, 162), 128, 208,
        GET_TPAGE(0, 1, 960, 0), 0, 224, 128, 224 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 64, 4, 128, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 240, GET_CLUT(1008, 156), 208, 240,
        GET_TPAGE(0, 0, 832, 256), 0, 255, 208, 255 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 20, 4, 208, 15 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 160, GET_CLUT(1008, 157), 168, 160,
        GET_TPAGE(0, 0, 960, 0), 0, 176, 168, 176 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 40, 4, 168, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 240, GET_CLUT(1008, 158), 228, 240,
        GET_TPAGE(0, 0, 768, 0), 0, 255, 228, 255 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 6, 4, 228, 15 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 176, GET_CLUT(1008, 159), 168, 176,
        GET_TPAGE(0, 0, 960, 0), 0, 192, 168, 192 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 40, 4, 168, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 192, GET_CLUT(1008, 160), 196, 192,
        GET_TPAGE(0, 0, 960, 0), 0, 208, 196, 208 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 26, 4, 196, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 208, GET_CLUT(1008, 161), 248, 208,
        GET_TPAGE(0, 0, 960, 0), 128, 224, 248, 224 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 64, 4, 120, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 144, GET_CLUT(1008, 154), 192, 144,
        GET_TPAGE(0, 0, 960, 0), 0, 160, 192, 160 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 28, 4, 192, 16 },
};

/* Stage Select layout tables: { rowCount, { stageNameIndex, x, y }... }. */
short gStageSelectLayoutVs[] = {
    8,
    0, 64, 56,
    1, 64, 92,
    2, 64, 128,
    3, 64, 164,
    4, 328, 56,
    5, 328, 92,
    6, 328, 128,
    7, 328, 164,
    0,
};

short gStageSelectLayoutLink[] = {
    7,
    8, 64, 56,
    1, 64, 92,
    3, 64, 128,
    4, 64, 164,
    5, 328, 56,
    6, 328, 92,
    7, 328, 128,
};

/* Entries 10 and 11 are the empty layout of the modes without a stage select. */
short gStageSelectLayoutPractice[] = {
    3,
    7, 64, 56,
    4, 64, 92,
    6, 64, 128,
    -1, 0,
};

short *gStageSelectLayouts[8] = {
    &gStageSelectLayoutPractice[10], gStageSelectLayoutVs,
    &gStageSelectLayoutPractice[10], gStageSelectLayoutVs,
    gStageSelectLayoutPractice, gStageSelectLayoutLink,
    &gStageSelectLayoutPractice[10], gStageSelectLayoutVs,
};

/* The fighter's name under the cursor, as DrawFormattedText takes it; the Japanese build draws none. */
char *gCharacterNameLabels[SELECT_CHAR_SLOT_COUNT] = {
    "#X#Y#FRED SHADOW", "#X#Y#FTATSUMI", "#X#Y#FMIKADO",
    "#X#Y#FBLACK LOTUS", "#X#Y#FUTSUSEMI", "#X#Y#FKANNUKI",
};
#endif

/*
 * Updates timers/pulse phase and draws the 2D HUD for the active select phase:
 *   SELECT_UI_CHARACTER (0): leftArg/rightArg = (flags << 16) | slot (0..5 P1/P2, 7..12 CPU; flags bit 0 = confirmed,
 *                            0xffff = hidden)
 *   SELECT_UI_WEAPON    (1): leftArg/rightArg = P1/P2 weaponId (0..8, or -1 inactive)
 *   SELECT_UI_STAGE     (2): leftArg = (selectMode << 8) | (u_char)stageIndex, rightArg = SelectStageSubState
 */
int UpdateAndDrawSelectScreenUi(int mode, int leftArg, int rightArg)
{
    int brightness;
    int slotFlags;
    int sineSample;
    int blinkTimer;
    int selectMode;
    int stageIndex;
    int row;
    int rowCount;
    int cursorState;
    short *layoutCursor;
    int stageNameIndex;
    int rowX;
    int rowY;

    switch (mode) {
    case SELECT_UI_CHARACTER:
        DrawTexturedRectList(gCharSelectTitleSprite, 0, 0, SELECT_OT_BACKGROUND);
        DrawTexturedRectList(gSelectHeaderPlateSprites, 0, 0, SELECT_OT_BACKGROUND);
        DrawTexturedRectList(gSelectTopBorderSprites, 0, 0, SELECT_OT_BACKGROUND);
        DrawTexturedRectList(gCharSelectFooterLabelSprites, 0, 0, SELECT_OT_BACKGROUND);
        DrawTexturedRectList(gCharSelectBottomFrameSprites, 0, 0, SELECT_OT_BACKGROUND);
        DrawTexturedRectList(gCharSelectPortraitBadgeSprites, 0, 0, SELECT_OT_FOREGROUND);

        slotFlags = leftArg >> 16;
        cursorState = slotFlags & 0xffff;
        leftArg &= 0xffff;
        if ((slotFlags & SELECT_SLOT_CONFIRMED) == 0) {
            sineSample = gSinLutQ12[gCursorGlowPhase & ANGLE_MASK];
            if (sineSample < 0) {
                brightness = -sineSample >> 4;
            } else {
                brightness = sineSample >> 4;
            }
        } else {
            if (gCharConfirmBlink[0] == -1) {
                gCharConfirmBlink[0] = SELECT_CONFIRM_BLINK_FRAMES;
            }
            blinkTimer = gCharConfirmBlink[0] - 1;
            if (blinkTimer < 0) {
                blinkTimer = 0;
            }
            gCharConfirmBlink[0] = blinkTimer;
            brightness = ((blinkTimer & 1) == 0) << 7;
        }
        if (brightness >= 0x100) {
            brightness = 0xff;
        }
        if (cursorState != SELECT_SLOT_HIDDEN && (unsigned int)leftArg < SELECT_CHAR_SLOT_COUNT) {
            gCharSelectP1CursorSprite[0].texture.r = gCharSelectP1CursorSprite[0].texture.g =
                gCharSelectP1CursorSprite[0].texture.b = brightness;
            gCharSelectP1CursorSprite[0].x = gCharSelectSlotPositions[leftArg].x;
            gCharSelectP1CursorSprite[0].y = gCharSelectSlotPositions[leftArg].y;
            DrawTexturedRectList(gCharSelectP1CursorSprite, 0, 0, SELECT_OT_FOREGROUND);
#ifndef VERSION_JP
            DrawFormattedText(gCharacterNameLabels[leftArg],
                              SELECT_CHAR_NAME_P1_X, SELECT_CHAR_NAME_Y, SELECT_CHAR_NAME_FONT_BANK);
#endif
        }

        slotFlags = rightArg >> 16;
        cursorState = slotFlags & 0xffff;
        rightArg &= 0xffff;
        if ((slotFlags & SELECT_SLOT_CONFIRMED) == 0) {
            sineSample = gSinLutQ12[gCursorGlowPhase & ANGLE_MASK];
            if (sineSample < 0) {
                brightness = -sineSample >> 4;
            } else {
                brightness = sineSample >> 4;
            }
        } else {
            if (gCharConfirmBlink[1] == -1) {
                gCharConfirmBlink[1] = SELECT_CONFIRM_BLINK_FRAMES;
            }
            blinkTimer = gCharConfirmBlink[1] - 1;
            if (blinkTimer < 0) {
                blinkTimer = 0;
            }
            gCharConfirmBlink[1] = blinkTimer;
            brightness = ((blinkTimer & 1) == 0) << 7;
        }
        if (brightness >= 0x100) {
            brightness = 0xff;
        }
        if (cursorState != SELECT_SLOT_HIDDEN) {
            if ((unsigned int)rightArg < SELECT_CHAR_SLOT_COUNT) {
                gCharSelectP2CursorSprites[0].texture.r = gCharSelectP2CursorSprites[0].texture.g =
                    gCharSelectP2CursorSprites[0].texture.b = gCharSelectP2CursorSprites[1].texture.r =
                        gCharSelectP2CursorSprites[1].texture.g = gCharSelectP2CursorSprites[1].texture.b = brightness;
                gCharSelectP2CursorSprites[0].x = gCharSelectSlotPositions[rightArg].x + SELECT_CURSOR_TAG_OFFSET_X;
                gCharSelectP2CursorSprites[0].y = gCharSelectSlotPositions[rightArg].y;
                gCharSelectP2CursorSprites[1].x = gCharSelectSlotPositions[rightArg].x;
                gCharSelectP2CursorSprites[1].y = gCharSelectSlotPositions[rightArg].y;
                DrawTexturedRectList(gCharSelectP2CursorSprites, 0, 0, SELECT_OT_FOREGROUND);
#ifndef VERSION_JP
                DrawFormattedText(gCharacterNameLabels[rightArg],
                                  SELECT_CHAR_NAME_P2_X, SELECT_CHAR_NAME_Y, SELECT_CHAR_NAME_FONT_BANK);
#endif
            } else if ((unsigned int)(rightArg - SELECT_CPU_SLOT_OFFSET) < SELECT_CHAR_SLOT_COUNT) {
                gCharSelectCpuCursorSprites[0].texture.r = gCharSelectCpuCursorSprites[0].texture.g =
                    gCharSelectCpuCursorSprites[0].texture.b = gCharSelectCpuCursorSprites[1].texture.r =
                        gCharSelectCpuCursorSprites[1].texture.g = gCharSelectCpuCursorSprites[1].texture.b =
                            brightness;
                gCharSelectCpuCursorSprites[0].x = gCharSelectSlotPositions[rightArg - SELECT_CPU_SLOT_OFFSET].x +
                    SELECT_CURSOR_TAG_OFFSET_X;
                gCharSelectCpuCursorSprites[0].y = gCharSelectSlotPositions[rightArg - SELECT_CPU_SLOT_OFFSET].y;
                gCharSelectCpuCursorSprites[1].x = gCharSelectSlotPositions[rightArg - SELECT_CPU_SLOT_OFFSET].x;
                gCharSelectCpuCursorSprites[1].y = gCharSelectSlotPositions[rightArg - SELECT_CPU_SLOT_OFFSET].y;
                DrawTexturedRectList(gCharSelectCpuCursorSprites, 0, 0, SELECT_OT_FOREGROUND);
#ifndef VERSION_JP
                DrawFormattedText(gCharacterNameLabels[rightArg - SELECT_CPU_SLOT_OFFSET],
                                  SELECT_CHAR_NAME_P2_X, SELECT_CHAR_NAME_Y, SELECT_CHAR_NAME_FONT_BANK);
#endif
            }
        }
        gCursorGlowPhase += SELECT_CURSOR_PULSE_STEP;
        DrawTexturedRectList(gCharPortraitRedShadowSprite, 0, 0, SELECT_OT_FOREGROUND);
        DrawTexturedRectList(gCharPortraitTatsumiSprite, 0, 0, SELECT_OT_FOREGROUND);
        DrawTexturedRectList(gCharPortraitMikadoSprite, 0, 0, SELECT_OT_FOREGROUND);
        DrawTexturedRectList(gCharPortraitBlackLotusSprite, 0, 0, SELECT_OT_FOREGROUND);
        DrawTexturedRectList(gCharPortraitUtsusemiSprite, 0, 0, SELECT_OT_FOREGROUND);
        DrawTexturedRectList(gCharPortraitKannukiSprite, 0, 0, SELECT_OT_FOREGROUND);
        DrawTexturedRectList(&gCharSelectSlotFrameSprites, 0, 0, SELECT_OT_FOREGROUND);
        break;

    case SELECT_UI_WEAPON:
        DrawTexturedRectList(gWeaponSelectTitleSprites, 0, 0, SELECT_OT_BACKGROUND);
        DrawTexturedRectList(gSelectHeaderPlateSprites, 0, 0, SELECT_OT_BACKGROUND);
        DrawTexturedRectList(gSelectTopBorderSprites, 0, 0, SELECT_OT_BACKGROUND);
        if ((unsigned int)leftArg < SELECT_WEAPON_COUNT) {
            DrawTexturedRectList(gWeaponCardArtSprites[leftArg], gWeaponCardX[0], gWeaponCardY[0],
                                 SELECT_OT_FOREGROUND);
            DrawTexturedRectList(gWeaponCardFrameSprites, gWeaponCardX[0], gWeaponCardY[0], SELECT_OT_FOREGROUND);
            DrawTexturedRectList(gWeaponCardShadowSprites, gWeaponCardX[0], gWeaponCardY[0], SELECT_OT_FOREGROUND);
        }
        if ((unsigned int)rightArg < SELECT_WEAPON_COUNT) {
            DrawTexturedRectList(gWeaponCardArtSprites[rightArg], gWeaponCardX[1], gWeaponCardY[1],
                                 SELECT_OT_FOREGROUND);
            DrawTexturedRectList(gWeaponCardFrameSprites, gWeaponCardX[1], gWeaponCardY[1], SELECT_OT_FOREGROUND);
            DrawTexturedRectList(gWeaponCardShadowSprites, gWeaponCardX[1], gWeaponCardY[1], SELECT_OT_FOREGROUND);
        }
        break;

    case SELECT_UI_STAGE:
        stageIndex = leftArg & 0xff;
        selectMode = leftArg >> 8;
        if (leftArg & 0x80) {
            stageIndex |= -256;
        }
        if (rightArg == SELECT_STAGE_NAVIGATE || (rightArg == SELECT_STAGE_CONFIRM_DELAY && (gFrameCounter & 1))) {
            cursorState = 1;
        } else {
            cursorState = 0;
        }
        layoutCursor = gStageSelectLayouts[selectMode];
        rowCount = *layoutCursor++;
#ifdef VERSION_JP
        if (stageIndex < 0) {
            stageIndex = rowCount - 1;
        }
        if (stageIndex >= rowCount) {
            stageIndex = 0;
        }
#endif
        for (row = 0; row < rowCount; ++row) {
            stageNameIndex = *layoutCursor++;
#ifdef VERSION_JP
            rowX = *layoutCursor++;
            rowY = 0;
#else
            rowX = *layoutCursor++;
            rowY = *layoutCursor++;
#endif
            if (stageIndex == row && cursorState != 0) {
                gStageSelectRowPlateSprite[0].texture.r = gStageSelectRowPlateSprite[0].texture.g =
                    gStageSelectRowPlateSprite[0].texture.b = STAGE_ROW_BRIGHTNESS_ACTIVE;
                gStageSelectNameSprites[stageNameIndex].texture.r = gStageSelectNameSprites[stageNameIndex].texture.g =
                    gStageSelectNameSprites[stageNameIndex].texture.b = STAGE_ROW_BRIGHTNESS_ACTIVE;
            } else {
                gStageSelectRowPlateSprite[0].texture.r = gStageSelectRowPlateSprite[0].texture.g =
                    gStageSelectRowPlateSprite[0].texture.b = STAGE_ROW_BRIGHTNESS_DIM;
                gStageSelectNameSprites[stageNameIndex].texture.r = gStageSelectNameSprites[stageNameIndex].texture.g =
                    gStageSelectNameSprites[stageNameIndex].texture.b = STAGE_ROW_BRIGHTNESS_DIM;
            }
            DrawTexturedRectList(&gStageSelectNameSprites[stageNameIndex], rowX, rowY, SELECT_OT_FOREGROUND);
            DrawTexturedRectList(gStageSelectRowPlateSprite, rowX, rowY, SELECT_OT_FOREGROUND);
        }
        DrawTexturedRectList(gStageSelectFrameTrimSprites, 0, 0, SELECT_OT_FOREGROUND);
        DrawTexturedRectList(&gStageSelectSideBorderSprites, 0, 0, SELECT_OT_FOREGROUND);
        DrawTexturedRectList(&gStageSelectBottomBarSprites, 0, 0, SELECT_OT_FOREGROUND);
        DrawTexturedRectList(&gStageSelectTopBorderSprites, 0, 0, SELECT_OT_FOREGROUND);
        DrawTexturedRectList(gStageSelectTitleSprites, 0, 0, SELECT_OT_FOREGROUND);
        return stageIndex;
    }
    return 0;
}

#define WEAPON_STAMP_OFFSET_X    0x78
#define WEAPON_STAMP_OFFSET_Y    0x28

RotatedTexturedQuad gWeaponConfirmStampQuad = {
    { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 152, 194, GET_CLUT(1008, 187), 231, 194,
        GET_TPAGE(0, 0, 960, 256), 152, 217, 231, 217 },
    {
        { -60, -18, 0, -1 },
        {  60, -18, 0, -1 },
        { -60,  18, 0, -1 },
        {  60,  18, 0, -1 },
    },
};

void UpdateAndDrawWeaponConfirmStamp(int side)
{
    if (side == 0) {
        if (gWeaponStampShakeP1 == WEAPON_STAMP_SHAKE_UNSET) {
            gWeaponStampShakeP1 = ONE;
        }
        gWeaponCardX[0] = (((rand() & 3) - 1) * gWeaponStampShakeP1 >> 12) + WEAPON_CARD_P1_BASE_X;
        gWeaponCardY[0] = (((rand() & 3) - 1) * gWeaponStampShakeP1 >> 12) + WEAPON_CARD_BASE_Y;
        DrawRotatedTexturedQuad(&gWeaponConfirmStampQuad, 0, 0, gWeaponStampAngle[0],
                                gWeaponCardX[0] + WEAPON_STAMP_OFFSET_X, gWeaponCardY[0] + WEAPON_STAMP_OFFSET_Y);
        gWeaponStampShakeP1 >>= 1;
    } else {
        if (gWeaponStampShakeP2 == WEAPON_STAMP_SHAKE_UNSET) {
            gWeaponStampShakeP2 = ONE;
        }
        gWeaponCardX[1] = (((rand() & 3) - 1) * gWeaponStampShakeP2 >> 12) + WEAPON_CARD_P2_BASE_X;
        gWeaponCardY[1] = (((rand() & 3) - 1) * gWeaponStampShakeP2 >> 12) + WEAPON_CARD_BASE_Y;
        DrawRotatedTexturedQuad(&gWeaponConfirmStampQuad, 0, 0, gWeaponStampAngle[1],
                                gWeaponCardX[1] + WEAPON_STAMP_OFFSET_X, gWeaponCardY[1] + WEAPON_STAMP_OFFSET_Y);
        gWeaponStampShakeP2 >>= 1;
    }
}

#define SCREEN_XY(v) ((unsigned short)(v).vx | ((unsigned long)(v).vy << 16))

void DrawRotatedTexturedQuad(RotatedTexturedQuad *quad, int rx, int ry, int rz, int x, int y)
{
    SVECTOR angles;
    VECTOR screen[4];
    MATRIX matrix;
    POLY_FT4 *poly = gPacketCursor;
    long *attrs = (long *)quad;
    unsigned long *ot;
    int i;

    angles.vx = rx;
    angles.vy = ry;
    angles.vz = rz;
#ifdef VERSION_JP
    BuildRotMatrixZYX(&angles, &matrix);
#else
    BuildRotationMatrixFast(&angles, &matrix);
#endif

    i = 0;
    do {
        ApplyMatrix(&matrix, &quad->vertices[i], &screen[i]);
        screen[i].vx += x;
        screen[i].vy = (screen[i].vy >> 1) + y;
    } while (++i < 4);

    setlen(poly, 9);
    *(u_long *)&poly->r0 = attrs[0];
    *(u_long *)&poly->u0 = attrs[1];
    *(u_long *)&poly->u1 = attrs[2];
    *(u_short *)&poly->u2 = attrs[3];
    *(u_short *)&poly->u3 = attrs[3] >> 16;

    ot = gCurrentOt;
    *(u_long *)&poly->x0 = SCREEN_XY(screen[0]);
    *(u_long *)&poly->x1 = SCREEN_XY(screen[1]);
    *(u_long *)&poly->x2 = SCREEN_XY(screen[2]);
    *(u_long *)&poly->x3 = SCREEN_XY(screen[3]);

    AddPrim(ot, poly);
    ++poly;
    gPacketCursor = poly;
}

#ifndef VERSION_JP
int StepStageSelectCursor(int selectMode, int stageIndex, int horizontal, int vertical)
{
    int count = *gStageSelectLayouts[selectMode];
    int prevIndex;

    switch (count) {
    case 8:
        stageIndex += horizontal * STAGE_SELECT_ROWS_PER_COL;
        if (stageIndex < 0)
            stageIndex += 8;
        if (stageIndex >= 8)
            stageIndex -= 8;
        prevIndex = stageIndex;
        stageIndex += vertical;
        if (prevIndex < STAGE_SELECT_ROWS_PER_COL) {
            if (stageIndex == -1)
                stageIndex = 3;
            if (stageIndex == STAGE_SELECT_ROWS_PER_COL)
                stageIndex = 0;
        } else {
            if (stageIndex == 3)
                stageIndex = 7;
            if (stageIndex == count)
                stageIndex = STAGE_SELECT_ROWS_PER_COL;
        }
        break;
    case 7:
        stageIndex += horizontal * STAGE_SELECT_ROWS_PER_COL;
        if (stageIndex < 0)
            stageIndex += 8;
        if (stageIndex >= 8)
            stageIndex -= 8;
        if (stageIndex == count)
            stageIndex = 6;
        prevIndex = stageIndex;
        stageIndex += vertical;
        if (prevIndex < STAGE_SELECT_ROWS_PER_COL) {
            if (stageIndex == -1)
                stageIndex = 3;
            if (stageIndex == STAGE_SELECT_ROWS_PER_COL)
                stageIndex = 0;
        } else {
            if (stageIndex == 3)
                stageIndex = 6;
            if (stageIndex == count)
                stageIndex = STAGE_SELECT_ROWS_PER_COL;
        }
        break;
    case 3:
        stageIndex += vertical;
        if (stageIndex == -1)
            stageIndex = 2;
        if (stageIndex == count)
            stageIndex = 0;
        break;
    }
    return stageIndex;
}
#endif
