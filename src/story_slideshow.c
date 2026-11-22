/* Story mode 2D slideshows: the Story Prologue (a timed state machine of fading picture cards,
 * a wipe and the character's subtitle) and the bad-ending slides, drawn from tiled POLY_FT4 grids. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "display_buffer.h"
#include "gpu_code.h"
#include "screen_fade.h"
#include "packet_build.h"
#include "fighter.h"
#include "model_arena.h"
#include "asset_load.h"
#include "tim.h"
#include "scene_light.h"
#include "subtitle.h"
#include "sprite2d.h"
#include "story_slideshow.h"
#include "main.h"
#include "stage_light.h"
#include "globals.h"

static int UpdateStoryPrologue(void);
static void RecordPrologueBufferHighWater(void *bufferEnd);
static int DrawBadEndingSlide(void);
static POLY_FT4 *BuildTiledPolyFt4Grid(POLY_FT4 *packets, int texX, int texY, int width, int height,
                                       int tpageMode, int clutX, int clutY, int screenX, int screenY);
static void SubmitTiledPolyFt4Grid(u_long *otEntry, POLY_FT4 *gridPackets);

#define CD_FILE_PROLOGUE_COMMON    0x56
#define CD_FILE_PROLOGUE_CHAR_BASE 0x57
#define CD_FILE_BAD_ENDING_TIMS    0x60

#define PROLOGUE_BG_OT_INDEX         4095
#define PROLOGUE_CARD_BRIGHTNESS_STEP 4
#define PROLOGUE_CARD_BRIGHTNESS_MAX  128 /* a texture at its own brightness */
#define TILED_GRID_END_TAG            ((u_long)-1)
#define CLAMP_MAX(value, limit)       ((value) > (limit) ? (limit) : (value))

typedef enum TPageColorMode {
    TPAGE_MODE_4BIT = 0,
    TPAGE_MODE_8BIT = 1,
    TPAGE_MODE_15BIT = 2
} TPageColorMode;

typedef enum PrologueState {
    PROLOGUE_STATE_CUE_AUDIO = 0,
    PROLOGUE_STATE_WAIT_INTRO = 1,
    PROLOGUE_STATE_INTRO_FADE_IN = 2,
    PROLOGUE_STATE_CARD0_FADE_IN = 3,
    PROLOGUE_STATE_CARD1_FADE_IN = 4,
    PROLOGUE_STATE_CARD2_FADE_IN = 5,
    PROLOGUE_STATE_CARDS_FADE_OUT = 6,
    PROLOGUE_STATE_WIPE_SCROLL = 7,
    PROLOGUE_STATE_WIPE_HOLD = 8,
    PROLOGUE_STATE_FLASH_TO_CHAR = 9,
    PROLOGUE_STATE_CHAR_SUBTITLE_IN = 10,
    PROLOGUE_STATE_CHAR_SUBTITLE_OUT = 11,
    PROLOGUE_STATE_DONE = 12,
    PROLOGUE_STATE_SKIP_FLASH = 13,
    PROLOGUE_STATE_COUNT = 14
} PrologueState;

typedef struct PrologueStateDurations {
    short durations[PROLOGUE_STATE_COUNT];
    short sceneDurations[PLAYABLE_CHARACTER_COUNT][2];
} PrologueStateDurations;

extern PrologueStateDurations gPrologueStateDurations;
static int gSlideshowMode;
static void *gPrologueBufferBase;
static POLY_FT4 *gProloguePackets;
static int gSlideshowScene;
static int gPrologueState;
static int gPrologueStateFrame;
static int gPrologueUnreadFlag; /* cleared when a prologue starts; nothing reads it */
static int gPrologueSkipRequested;
static int gPrologueWipeOffset;
static unsigned short *gPrologueCommonTims;
static unsigned short *gPrologueCharacterTims;
#ifndef VERSION_JP
static int *gPrologueSubtitleBank;
#endif

void InitStorySlideshow(int mode, char *data, int scene)
{
    unsigned char unused[8]; /* not used */

    gPrologueBufferBase = data;
    switch (mode) {
    case SLIDESHOW_MODE_PROLOGUE:
        ResetModelArena();
        ReadCdFileSync(CD_FILE_PROLOGUE_COMMON, data);

        /* An archive is words: an entry count N, N entry offsets, then its size in bytes. */
        gPrologueCommonTims = (unsigned short *)(data + ((int *)data)[1]);
#ifndef VERSION_JP
        gPrologueSubtitleBank = (int *)(data + ((int *)data)[2]);
#endif

        data += ((int *)data + 1)[((int *)data)[0]];
        ReadCdFileSync(scene + CD_FILE_PROLOGUE_CHAR_BASE, data);

        gPrologueCharacterTims = (unsigned short *)(data + ((int *)data)[1]);
#ifndef VERSION_JP
        RelocateSubtitleBank(gPrologueSubtitleBank, SUBTITLE_BANK_PROLOGUE);
#endif

        gProloguePackets = GetModelArenaBase();
        gPrologueBufferBase = gProloguePackets;
        RecordPrologueBufferHighWater(gPrologueBufferBase);

        gPrologueStateDurations.durations[PROLOGUE_STATE_CHAR_SUBTITLE_IN] =
            gPrologueStateDurations.sceneDurations[scene][0];
        gPrologueStateDurations.durations[PROLOGUE_STATE_CHAR_SUBTITLE_OUT] =
            gPrologueStateDurations.sceneDurations[scene][1];
        gPrologueState = PROLOGUE_STATE_CUE_AUDIO;
        gPrologueStateFrame = 0;
        gPrologueUnreadFlag = 0;
        gPrologueWipeOffset = 0;
        gPrologueSkipRequested = 0;
#ifndef VERSION_JP
        StartSubtitleById(scene);
#endif
        break;

    case SLIDESHOW_MODE_BAD_ENDING:
        ReadCdFileSync(CD_FILE_BAD_ENDING_TIMS, data);
        LoadTimGroup((unsigned short *)data, 0);
        DrawSync(0);
        break;
    }

    gSlideshowMode = mode;
    gSlideshowScene = scene;
    SetStageClearAndFogColor(0);
}

int UpdateStorySlideshow(void)
{
    int status;

    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);
    switch (gSlideshowMode) {
    case SLIDESHOW_MODE_PROLOGUE:
        status = UpdateStoryPrologue();
        break;
    case SLIDESHOW_MODE_BAD_ENDING:
        status = DrawBadEndingSlide();
        break;
    }
    return status;
}

PrologueStateDurations gPrologueStateDurations = {
#ifdef VERSION_EU
    { 0, 50, 92, 85, 82, 120, 38, 483, 25, 25, 343, 75, -1, 38 },
    { { 200, 160 }, { 180, 150 }, { 445, 150 }, { 220, 125 }, { 225, 140 }, { 250, 150 } },
#else
    { 0, 60, 111, 102, 99, 144, 45, 585, 30, 30, 414, 90, -1, 45 },
    { { 240, 195 }, { 219, 180 }, { 534, 180 }, { 264, 150 }, { 270, 171 }, { 300, 180 } },
#endif
};
/* The prologue's fades also write the entry past each card's list: past card 2 lies gBadEndingSlideRects[0]
 * in Japan and gBadEndingSlide0Sprites[0] elsewhere. */
TexturedRectEntry gPrologueCard0Sprites[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(640, 510), 200, 0, GET_TPAGE(0, 2, 640, 256), 0, 156, 200, 156 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 212, 5, 200, 156 },
};
TexturedRectEntry gPrologueCard1Sprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(656, 510), 255, 0, GET_TPAGE(0, 2, 704, 256), 0, 141, 255, 141 }, 1, 0, 72, 256, 141 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(656, 510), 44, 0, GET_TPAGE(0, 2, 768, 256), 0, 141, 44, 141 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 256, 72, 44, 141 },
};
TexturedRectEntry gPrologueCard2Sprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(672, 510), 255, 0, GET_TPAGE(0, 2, 832, 256), 0, 140, 255, 140 }, 1, 320, 100, 256, 140 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(672, 510), 65, 0, GET_TPAGE(0, 2, 896, 256), 0, 140, 65, 140 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 576, 100, 64, 140 },
};
#ifdef VERSION_JP
TexturedRectEntry gBadEndingSlideRects[5] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(704, 255), 32, 0, GET_TPAGE(0, 1, 704, 0), 0, 216, 32, 216 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 320, 20, 32, 216 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 32, 0, GET_CLUT(720, 255), 64, 0, GET_TPAGE(0, 1, 704, 0), 32, 216, 64, 216 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 320, 20, 32, 216 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 64, 0, GET_CLUT(736, 255), 128, 0, GET_TPAGE(0, 1, 704, 0), 64, 180, 128, 180 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 320, 20, 64, 180 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 128, 0, GET_CLUT(752, 255), 192, 0, GET_TPAGE(0, 1, 704, 0), 128, 216, 192, 216 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 320, 20, 64, 216 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 192, 0, GET_CLUT(704, 254), 255, 0, GET_TPAGE(0, 1, 704, 0), 192, 204, 255, 204 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 320, 20, 63, 204 },
};
TexturedRectEntry gBadEndingFooterSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 64, 180, GET_CLUT(720, 254), 88, 180, GET_TPAGE(0, 1, 704, 0), 64, 220, 88, 220 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 160, 160, 24, 40 },
};
#else
TexturedRectEntry gBadEndingSlide0Sprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(704, 255), 255, 0, GET_TPAGE(0, 1, 704, 0), 0, 40, 255, 40 }, 1, 24, 96, 255, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(704, 255), 255, 0, GET_TPAGE(0, 1, 768, 0), 0, 40, 255, 40 }, 1, 279, 96, 255, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 0, GET_CLUT(704, 255), 80, 0, GET_TPAGE(0, 1, 832, 0), 0, 40, 80, 40 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 534, 96, 80, 40 },
};
TexturedRectEntry gBadEndingSlide1Sprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 40, GET_CLUT(720, 255), 255, 40, GET_TPAGE(0, 1, 704, 0), 0, 80, 255, 80 }, 1, 40, 96, 255, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 40, GET_CLUT(720, 255), 255, 40, GET_TPAGE(0, 1, 768, 0), 0, 80, 255, 80 }, 1, 295, 96, 255, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 40, GET_CLUT(720, 255), 48, 40, GET_TPAGE(0, 1, 832, 0), 0, 80, 48, 80 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 550, 96, 48, 40 },
};
TexturedRectEntry gBadEndingSlide2Sprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 80, GET_CLUT(736, 255), 255, 80, GET_TPAGE(0, 1, 704, 0), 0, 120, 255, 120 }, 1, 112, 96, 255, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 80, GET_CLUT(736, 255), 160, 80, GET_TPAGE(0, 1, 768, 0), 0, 120, 160, 120 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 367, 96, 160, 40 },
};
TexturedRectEntry gBadEndingSlide3Sprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 120, GET_CLUT(752, 255), 255, 120, GET_TPAGE(0, 1, 704, 0), 0, 160, 255, 160 }, 1, 24, 96, 255, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 120, GET_CLUT(752, 255), 255, 120, GET_TPAGE(0, 1, 768, 0), 0, 160, 255, 160 }, 1, 279, 96, 255, 40 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 120, GET_CLUT(752, 255), 80, 120, GET_TPAGE(0, 1, 832, 0), 0, 160, 80, 160 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 534, 96, 80, 40 },
};
TexturedRectEntry gBadEndingSlide4Sprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 160, GET_CLUT(704, 254), 255, 160, GET_TPAGE(0, 1, 704, 0), 0, 224, 255, 224 }, 1, 64, 88, 255, 64 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 160, GET_CLUT(704, 254), 255, 160, GET_TPAGE(0, 1, 768, 0), 0, 224, 255, 224 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 319, 88, 255, 64 },
};
TexturedRectEntry gBadEndingFooterSprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS, 0, 224, GET_CLUT(720, 254), 208, 224, GET_TPAGE(0, 1, 704, 0), 0, 248, 208, 248 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 216, 184, 208, 24 },
};
TexturedRectEntry *gBadEndingSlideSprites[5] = {
    gBadEndingSlide0Sprites,
    gBadEndingSlide1Sprites,
    gBadEndingSlide2Sprites,
    gBadEndingSlide3Sprites,
    gBadEndingSlide4Sprites,
};
#endif

static POLY_FT4 *gWipePacketsOdd;
static POLY_FT4 *gWipePacketsEven;
static int gPrologueCardBrightness;

static int UpdateStoryPrologue(void)
{
    int duration;
    int status;
    POLY_FT4 *packets;
    POLY_FT4 *nextPacket;

    duration = gPrologueStateDurations.durations[gPrologueState];
    if (duration < 0) {
        return PROLOGUE_STATUS_FINISHED;
    }
    if (gPrologueSkipRequested != 0 && gPrologueState < PROLOGUE_STATE_WIPE_HOLD) {
        duration = 0;
        gPrologueStateFrame = 0;
        gPrologueState = PROLOGUE_STATE_DONE;
    }
    gPrologueSkipRequested = 0;
    status = PROLOGUE_STATUS_RUNNING;
    if (gPrologueStateFrame == duration) {
        switch (gPrologueState) {
        case PROLOGUE_STATE_CUE_AUDIO:
            status = PROLOGUE_STATUS_CUE_INTRO_XA;
            break;
        case PROLOGUE_STATE_WAIT_INTRO:
            LoadTimGroup(gPrologueCommonTims, 0);
            status = PROLOGUE_STATUS_START_INTRO_XA;
            RecordPrologueBufferHighWater(BuildTiledPolyFt4Grid(gProloguePackets, 640, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                                                TPAGE_MODE_8BIT, 640, DISPLAY_HEIGHT, 0, 0));
            StartScreenFade(SCREEN_FADE_IN_FROM_BLACK);
            DrawSync(0);
            break;
        case PROLOGUE_STATE_INTRO_FADE_IN:
            gPrologueCard0Sprites[0].texture.r = gPrologueCard0Sprites[0].texture.g =
                gPrologueCard0Sprites[0].texture.b = gPrologueCard0Sprites[1].texture.r =
                    gPrologueCard0Sprites[1].texture.g = gPrologueCard0Sprites[1].texture.b = 0;
            gPrologueCardBrightness = 0;
            break;
        case PROLOGUE_STATE_CARD0_FADE_IN:
            gPrologueCard1Sprites[0].texture.r = gPrologueCard1Sprites[0].texture.g =
                gPrologueCard1Sprites[0].texture.b = gPrologueCard1Sprites[1].texture.r =
                    gPrologueCard1Sprites[1].texture.g = gPrologueCard1Sprites[1].texture.b =
                        gPrologueCard1Sprites[2].texture.r = gPrologueCard1Sprites[2].texture.g =
                            gPrologueCard1Sprites[2].texture.b = 0;
            gPrologueCardBrightness = 0;
            break;
        case PROLOGUE_STATE_CARD1_FADE_IN:
            gPrologueCard2Sprites[0].texture.r = gPrologueCard2Sprites[0].texture.g =
                gPrologueCard2Sprites[0].texture.b = gPrologueCard2Sprites[1].texture.r =
                    gPrologueCard2Sprites[1].texture.g = gPrologueCard2Sprites[1].texture.b =
                        gPrologueCard2Sprites[2].texture.r = gPrologueCard2Sprites[2].texture.g =
                            gPrologueCard2Sprites[2].texture.b = 0;
            gPrologueCardBrightness = 0;
            break;
        case PROLOGUE_STATE_CARD2_FADE_IN:
            StartScreenFade(SCREEN_FADE_OUT_TO_BLACK);
            break;
        case PROLOGUE_STATE_CARDS_FADE_OUT:
            DrawSync(0);
            LoadTimGroup(gPrologueCommonTims, 1);
            StartScreenFade(SCREEN_FADE_IN_FROM_BLACK);
            packets = gProloguePackets;
            gWipePacketsOdd = packets;
            packets = BuildTiledPolyFt4Grid(packets, 640, 0, DISPLAY_WIDTH, 304,
                                            TPAGE_MODE_8BIT, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0);
            packets = BuildTiledPolyFt4Grid(packets, 640, 256, DISPLAY_WIDTH, 304,
                                            TPAGE_MODE_8BIT, DISPLAY_WIDTH, 496, 0, 0);
            gWipePacketsEven = packets;
            RecordPrologueBufferHighWater((void *)((int)packets * 2 - (int)gProloguePackets));
            gPrologueWipeOffset = 0;
            DrawSync(0);
            break;
        case PROLOGUE_STATE_WIPE_SCROLL:
            status = PROLOGUE_STATUS_STOP_INTRO_XA;
            break;
        case PROLOGUE_STATE_SKIP_FLASH:
            gPrologueState = PROLOGUE_STATE_FLASH_TO_CHAR;
            /* fall through */
        case PROLOGUE_STATE_FLASH_TO_CHAR:
            DrawSync(0);
            status = PROLOGUE_STATUS_START_CHAR_XA;
            LoadTimGroup(gPrologueCharacterTims, 0);
#ifdef VERSION_JP
            packets = BuildTiledPolyFt4Grid(gProloguePackets, 640, 256, DISPLAY_WIDTH, 89,
                                            TPAGE_MODE_8BIT, 640, 348, 0, 85);
#else
            packets = BuildTiledPolyFt4Grid(gProloguePackets, 640, 0, DISPLAY_WIDTH, 89,
                                            TPAGE_MODE_8BIT, 640, 92, 0, 85);
#endif
            RecordPrologueBufferHighWater(packets);
            StartScreenFade(SCREEN_FADE_IN_FROM_WHITE);
            DrawSync(0);
            break;
        case PROLOGUE_STATE_CHAR_SUBTITLE_IN:
            StartScreenFade(SCREEN_FADE_OUT_TO_BLACK);
            break;
        case PROLOGUE_STATE_DONE:
            status = PROLOGUE_STATUS_STOP_INTRO_XA;
            /* fall through */
        case PROLOGUE_STATE_WIPE_HOLD:
            StartScreenFade(SCREEN_FADE_FLASH_TO_WHITE);
            break;
        }
        gPrologueStateFrame = 0;
        gPrologueState++;
    } else {
        gPrologueStateFrame++;
    }
    switch (gPrologueState) {
    case PROLOGUE_STATE_CUE_AUDIO:
    case PROLOGUE_STATE_WAIT_INTRO:
    case PROLOGUE_STATE_DONE:
        break;
    case PROLOGUE_STATE_INTRO_FADE_IN:
        UpdateAndDrawScreenFade();
        SubmitTiledPolyFt4Grid(&gCurrentOt[PROLOGUE_BG_OT_INDEX], gProloguePackets);
        break;
    case PROLOGUE_STATE_CARD0_FADE_IN:
        gPrologueCard0Sprites[0].texture.r = gPrologueCard0Sprites[0].texture.g =
            gPrologueCard0Sprites[0].texture.b = gPrologueCard0Sprites[1].texture.r =
                gPrologueCard0Sprites[1].texture.g = gPrologueCard0Sprites[1].texture.b = gPrologueCardBrightness;
        gPrologueCardBrightness = CLAMP_MAX(gPrologueCardBrightness + PROLOGUE_CARD_BRIGHTNESS_STEP,
                                            PROLOGUE_CARD_BRIGHTNESS_MAX);
        DrawTexturedRectList(gPrologueCard0Sprites, 0, 0, 0);
        SubmitTiledPolyFt4Grid(&gCurrentOt[PROLOGUE_BG_OT_INDEX], gProloguePackets);
        break;
    case PROLOGUE_STATE_CARD1_FADE_IN:
        gPrologueCard1Sprites[0].texture.r = gPrologueCard1Sprites[0].texture.g =
            gPrologueCard1Sprites[0].texture.b = gPrologueCard1Sprites[1].texture.r =
                gPrologueCard1Sprites[1].texture.g = gPrologueCard1Sprites[1].texture.b =
                    gPrologueCard1Sprites[2].texture.r = gPrologueCard1Sprites[2].texture.g =
                        gPrologueCard1Sprites[2].texture.b = gPrologueCardBrightness;
        gPrologueCardBrightness = CLAMP_MAX(gPrologueCardBrightness + PROLOGUE_CARD_BRIGHTNESS_STEP,
                                            PROLOGUE_CARD_BRIGHTNESS_MAX);
        DrawTexturedRectList(gPrologueCard1Sprites, 0, 0, 0);
        DrawTexturedRectList(gPrologueCard0Sprites, 0, 0, 0);
        SubmitTiledPolyFt4Grid(&gCurrentOt[PROLOGUE_BG_OT_INDEX], gProloguePackets);
        break;
    case PROLOGUE_STATE_CARD2_FADE_IN:
        gPrologueCard2Sprites[0].texture.r = gPrologueCard2Sprites[0].texture.g =
            gPrologueCard2Sprites[0].texture.b = gPrologueCard2Sprites[1].texture.r =
                gPrologueCard2Sprites[1].texture.g = gPrologueCard2Sprites[1].texture.b =
                    gPrologueCard2Sprites[2].texture.r = gPrologueCard2Sprites[2].texture.g =
                        gPrologueCard2Sprites[2].texture.b = gPrologueCardBrightness;
        gPrologueCardBrightness = CLAMP_MAX(gPrologueCardBrightness + PROLOGUE_CARD_BRIGHTNESS_STEP,
                                            PROLOGUE_CARD_BRIGHTNESS_MAX);
        DrawTexturedRectList(gPrologueCard2Sprites, 0, 0, 0);
        DrawTexturedRectList(gPrologueCard1Sprites, 0, 0, 0);
        DrawTexturedRectList(gPrologueCard0Sprites, 0, 0, 0);
        SubmitTiledPolyFt4Grid(&gCurrentOt[PROLOGUE_BG_OT_INDEX], gProloguePackets);
        break;
    case PROLOGUE_STATE_CARDS_FADE_OUT:
        UpdateAndDrawScreenFade();
        DrawTexturedRectList(gPrologueCard2Sprites, 0, 0, 0);
        DrawTexturedRectList(gPrologueCard1Sprites, 0, 0, 0);
        DrawTexturedRectList(gPrologueCard0Sprites, 0, 0, 0);
        SubmitTiledPolyFt4Grid(&gCurrentOt[PROLOGUE_BG_OT_INDEX], gProloguePackets);
        break;
    case PROLOGUE_STATE_WIPE_SCROLL:
    case PROLOGUE_STATE_WIPE_HOLD:
        UpdateAndDrawScreenFade();
        packets = ((gFrameCounter & 1) != 0) ? gWipePacketsOdd : gWipePacketsEven;
        nextPacket = BuildTiledPolyFt4Grid(packets, 640, gPrologueWipeOffset, DISPLAY_WIDTH,
                                           DISPLAY_HEIGHT - gPrologueWipeOffset, TPAGE_MODE_8BIT,
                                           DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0);
        BuildTiledPolyFt4Grid(nextPacket, 640, 256, DISPLAY_WIDTH, gPrologueWipeOffset, TPAGE_MODE_8BIT,
                              DISPLAY_WIDTH, 496, 0, DISPLAY_HEIGHT - gPrologueWipeOffset);
        SubmitTiledPolyFt4Grid(&gCurrentOt[PROLOGUE_BG_OT_INDEX], packets);
        SubmitTiledPolyFt4Grid(&gCurrentOt[PROLOGUE_BG_OT_INDEX], nextPacket);
        gPrologueWipeOffset = CLAMP_MAX(gPrologueWipeOffset + 1, DISPLAY_HEIGHT);
        break;
    case PROLOGUE_STATE_FLASH_TO_CHAR:
        packets = ((gFrameCounter & 1) != 0) ? gWipePacketsOdd : gWipePacketsEven;
        BuildTiledPolyFt4Grid(packets, 640, 256, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                              TPAGE_MODE_8BIT, 640, 496, 0, 0);
        SubmitTiledPolyFt4Grid(&gCurrentOt[PROLOGUE_BG_OT_INDEX], packets);
        UpdateAndDrawScreenFade();
        break;
    case PROLOGUE_STATE_CHAR_SUBTITLE_IN:
    case PROLOGUE_STATE_CHAR_SUBTITLE_OUT:
        SubmitTiledPolyFt4Grid(&gCurrentOt[PROLOGUE_BG_OT_INDEX], gProloguePackets);
#ifndef VERSION_JP
        UpdateAndDrawSubtitle(0);
#endif
        UpdateAndDrawScreenFade();
        break;
    case PROLOGUE_STATE_SKIP_FLASH:
        UpdateAndDrawScreenFade();
        break;
    default:
        break;
    }
    return status;
}

void RequestSkipStoryPrologue(void)
{
    gPrologueSkipRequested = 1;
}

int gPrologueBufferHighWater = 0; /* read only by its own update */

static void RecordPrologueBufferHighWater(void *bufferEnd)
{
    int usedBytes = (char *)bufferEnd - (char *)gPrologueBufferBase;

    if ((unsigned int)gPrologueBufferHighWater < (unsigned int)usedBytes) {
        gPrologueBufferHighWater = usedBytes;
    }
}

static int DrawBadEndingSlide(void)
{
#ifdef VERSION_JP
    DrawTexturedRectList(&gBadEndingSlideRects[gSlideshowScene], 0, 0, 0);
#else
    DrawTexturedRectList(gBadEndingSlideSprites[gSlideshowScene], 0, 0, 0);
#endif
    DrawTexturedRectList(gBadEndingFooterSprite, 0, 0, 0);
    return 0;
}

static POLY_FT4 *BuildTiledPolyFt4Grid(POLY_FT4 *packets, int texX, int texY, int width, int height,
                                       int tpageMode, int clutX, int clutY, int screenX, int screenY)
{
    unsigned short clut;
    unsigned short tpage;
    int remainingCols;
    int vramX;
    int dstX;
    int tileHeight;
    int tileWidth;
    int pageX;
    int pageY;
    int vramY;
    int remainingRows;
    int dstY;

    clut = GetClut(clutX, clutY);
    switch (tpageMode) {
    case TPAGE_MODE_4BIT:
        width /= 4;
        break;
    case TPAGE_MODE_8BIT:
        width /= 2;
        break;
    case TPAGE_MODE_15BIT:
        break;
    }
    vramY = texY;
    dstY = screenY;
    for (remainingRows = height; remainingRows > 0; remainingRows -= tileHeight) {
        remainingCols = width;
        /* pageY is not yet set on the first row, so that row's height depends on whatever it holds */
        tileHeight = 64 - pageY % 64;
        if (remainingRows < tileHeight) {
            tileHeight = remainingRows;
        }
        vramX = texX;
        dstX = screenX;
        for (; remainingCols > 0; remainingCols -= tileWidth) {
            SetPolyFT4(packets);
            setRGB0(packets, 128, 128, 128);
            tpage = GetTPage(tpageMode, 0, vramX, vramY);
            packets->tpage = tpage;
            packets->clut = clut;
            pageX = vramX - ((tpage & 0x1f) << 6);
            pageY = vramY - ((tpage << 3) & 0x100);
            tileWidth = 16 - pageX % 16;
            if (remainingCols < tileWidth) {
                tileWidth = remainingCols;
            }
            switch (tpageMode) {
            case TPAGE_MODE_4BIT:
                setUV4(packets, pageX * 4, pageY, (pageX + tileWidth) * 4, pageY,
                       pageX * 4, pageY + tileHeight, (pageX + tileWidth) * 4, pageY + tileHeight);
                setXYWH(packets, dstX, dstY, tileWidth * 4, tileHeight);
                dstX += tileWidth * 4;
                break;
            case TPAGE_MODE_8BIT:
                setUV4(packets, pageX * 2, pageY, (pageX + tileWidth) * 2, pageY,
                       pageX * 2, pageY + tileHeight, (pageX + tileWidth) * 2, pageY + tileHeight);
                setXYWH(packets, dstX, dstY, tileWidth * 2, tileHeight);
                dstX += tileWidth * 2;
                break;
            case TPAGE_MODE_15BIT:
                setUVWH(packets, pageX, pageY, tileWidth, tileHeight);
                setXYWH(packets, dstX, dstY, tileWidth, tileHeight);
                dstX += tileWidth;
                break;
            }
            packets[1] = packets[0];
            packets += 2;
            vramX += tileWidth;
        }
        vramY += tileHeight;
        dstY += tileHeight;
    }
    packets->tag = TILED_GRID_END_TAG;
    packets++;
    packets->tag = TILED_GRID_END_TAG;
    packets++;
    return packets;
}

static void SubmitTiledPolyFt4Grid(u_long *otEntry, POLY_FT4 *gridPackets)
{
    POLY_FT4 *packet;

    if ((gFrameCounter & 1) != 0) {
        packet = gridPackets + 1;
    } else {
        packet = gridPackets;
    }
    while (packet->tag != TILED_GRID_END_TAG) {
        addPrim(otEntry, packet);
        packet += 2;
    }
}
