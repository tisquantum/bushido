/*
 * Boot splash screen: loads the publisher TIM image(s) from disc into the shared dynamic load
 * buffer and fades them in and out before the main loop starts (one 4-bit 640x60 screen on USA,
 * two PAL-height double-buffered screens on Europe, omitted on Japan).
 */

#include <sys/types.h>
#include <libgte.h>
#include <libetc.h>
#include <libgpu.h>
#include "cd.h"
#include "display_buffer.h"
#include "load_area.h"
#include "splash.h"

/* Word offsets past the TIM header and CLUT block into the loaded TIM image. */
#define TIM_CLUT_DATA_WORDS       5
#define TIM_4BIT_PIXEL_DATA_WORDS 16
#define TIM_8BIT_PIXEL_DATA_WORDS 136

/* CD_CATEGORY_MOTION indices for the boot splash TIM files (appended after the 44 motion pairs). */
#define SPLASH_FILE_FIRST         44
#define SPLASH_FILE_SECOND        45

#define SPLASH_DURATION_FRAMES    300
#define SPLASH_FADE_IN_FRAMES     64
#define SPLASH_FADE_OUT_START     237

#define SPLASH_TPAGE_4BIT         0
#define SPLASH_TPAGE_8BIT         1
#define SPLASH_ABR_SUB            2 /* fades to and from black */

#ifndef VERSION_JP
void PlayBootSplashScreen(void)
{
    unsigned char unused[24]; /* not used */
#ifdef VERSION_EU
    DRAWENV draw[2];
    DISPENV disp[2];
#else
    DRAWENV draw;
    DISPENV disp;
#endif
    u_long ot[2];
    SPRT sprites[2][3];
    DR_TPAGE tpages[2][3];
    TILE fadeTiles[2];
    RECT rect;
    int frame;
    int bufferIndex;
    int strip;
    int x;
    short fadeLevel;
    u_long *otHead;
    TILE *fadeTile;
    SPRT *sprite;
    DR_TPAGE *tpage;

#ifdef VERSION_EU
    /* First PAL screen: 8-bit 640x256 TIM (file 44), double-buffered. */
    SetDispMask(0);
    StartCdFileRead(ResolveCdFileHandle(CD_CATEGORY_MOTION, SPLASH_FILE_FIRST), DYNAMIC_LOAD_BUFFER);
    WaitCdIdle();
    setRECT(&rect, 640, 256, 256, 1);
    LoadImage(&rect, (u_long *)DYNAMIC_LOAD_BUFFER + TIM_CLUT_DATA_WORDS);
    setRECT(&rect, 640, 0, 320, 256);
    LoadImage(&rect, (u_long *)DYNAMIC_LOAD_BUFFER + TIM_8BIT_PIXEL_DATA_WORDS);
    DrawSync(0);
    SetDefDispEnv(&disp[0], 0, 256, 640, 256);
    SetDefDispEnv(&disp[1], 0, 0, 640, 256);
    SetDefDrawEnv(&draw[0], 0, 0, 640, 256);
    SetDefDrawEnv(&draw[1], 0, 256, 640, 256);
    disp[0].screen.x = 0;
    disp[0].screen.y = DISPLAY_SCREEN_Y;
    disp[1].screen.x = 0;
    disp[1].screen.y = DISPLAY_SCREEN_Y;
    SetDispMask(1);
    for (frame = 0; frame < SPLASH_DURATION_FRAMES; frame++) {
        bufferIndex = frame & 1;
        otHead = &ot[bufferIndex];
        ClearOTag(otHead, 1);
        if (frame >= SPLASH_FADE_OUT_START) {
            fadeLevel = 255 - (SPLASH_DURATION_FRAMES - frame) * 4;
        } else {
            fadeLevel = 0;
        }
        if (frame < SPLASH_FADE_IN_FRAMES) {
            fadeLevel = 255 - frame * 4;
        }
        fadeTile = &fadeTiles[bufferIndex];
        SetTile(fadeTile);
        SetSemiTrans(fadeTile, 1);
        setXY0(fadeTile, 0, 37);
        setWH(fadeTile, 640, 179);
        setRGB0(fadeTile, fadeLevel, fadeLevel, fadeLevel);
        AddPrim(otHead, fadeTile);
        sprite = sprites[bufferIndex];
        tpage = tpages[bufferIndex];
        for (x = 0, strip = 0; strip < 3; strip++, x += 256) {
            SetSprt(sprite);
            SetShadeTex(sprite, 1);
            setXY0(sprite, x, 0);
            setWH(sprite, strip == 2 ? 128 : 256, 256);
            setUV0(sprite, 0, 0);
            sprite->clut = GetClut(640, 256);
            AddPrim(otHead, sprite++);
            SetDrawTPage(tpage, 0, 1, GetTPage(SPLASH_TPAGE_8BIT, SPLASH_ABR_SUB, 640 + x / 2, 0));
            AddPrim(otHead, tpage++);
        }
        DrawSync(0);
        VSync(0);
        PutDispEnv(&disp[frame & 1]);
        PutDrawEnv(&draw[frame & 1]);
        DrawOTag(&ot[frame & 1]);
    }

    /* Second PAL screen: 4-bit 320x256 TIM (file 45), double-buffered. */
    SetDispMask(0);
    StartCdFileRead(ResolveCdFileHandle(CD_CATEGORY_MOTION, SPLASH_FILE_SECOND), DYNAMIC_LOAD_BUFFER);
    WaitCdIdle();
    SetDefDispEnv(&disp[0], 0, 256, 320, 256);
    SetDefDispEnv(&disp[1], 0, 0, 320, 256);
    SetDefDrawEnv(&draw[0], 0, 0, 320, 256);
    SetDefDrawEnv(&draw[1], 0, 256, 320, 256);
    disp[0].screen.x = 0;
    disp[0].screen.y = DISPLAY_SCREEN_Y;
    disp[1].screen.x = 0;
    disp[1].screen.y = DISPLAY_SCREEN_Y;
    setRECT(&rect, 640, 256, 16, 1);
    LoadImage(&rect, (u_long *)DYNAMIC_LOAD_BUFFER + TIM_CLUT_DATA_WORDS);
    setRECT(&rect, 640, 0, 80, 256);
    LoadImage(&rect, (u_long *)DYNAMIC_LOAD_BUFFER + TIM_4BIT_PIXEL_DATA_WORDS);
    DrawSync(0);
    SetDispMask(1);
    for (frame = 0; frame < SPLASH_DURATION_FRAMES; frame++) {
        bufferIndex = frame & 1;
        otHead = &ot[bufferIndex];
        ClearOTag(otHead, 1);
        if (frame >= SPLASH_FADE_OUT_START) {
            fadeLevel = 255 - (SPLASH_DURATION_FRAMES - frame) * 4;
        } else {
            fadeLevel = 0;
        }
        if (frame < SPLASH_FADE_IN_FRAMES) {
            fadeLevel = 255 - frame * 4;
        }
        fadeTile = &fadeTiles[bufferIndex];
        SetTile(fadeTile);
        SetSemiTrans(fadeTile, 1);
        setXY0(fadeTile, 0, 0);
        setWH(fadeTile, 320, 256);
        setRGB0(fadeTile, fadeLevel, fadeLevel, fadeLevel);
        AddPrim(otHead, fadeTile);
        sprite = sprites[bufferIndex];
        tpage = tpages[bufferIndex];
        for (x = 0, strip = 0; strip < 2; strip++, x += 256) {
            SetSprt(sprite);
            SetShadeTex(sprite, 1);
            setXY0(sprite, x, 0);
            setWH(sprite, strip == 1 ? 64 : 256, 256);
            setUV0(sprite, 0, 0);
            sprite->clut = GetClut(640, 256);
            AddPrim(otHead, sprite++);
            SetDrawTPage(tpage, 0, 1, GetTPage(SPLASH_TPAGE_4BIT, SPLASH_ABR_SUB, 640 + x / 4, 0));
            AddPrim(otHead, tpage++);
        }
        DrawSync(0);
        VSync(0);
        PutDispEnv(&disp[frame & 1]);
        PutDrawEnv(&draw[frame & 1]);
        DrawOTag(&ot[frame & 1]);
    }
#else
    /* USA screen: 4-bit 640x60 TIM (file 44), vertically centered in 640x480. */
    SetDispMask(0);
    StartCdFileRead(ResolveCdFileHandle(CD_CATEGORY_MOTION, SPLASH_FILE_FIRST), DYNAMIC_LOAD_BUFFER);
    WaitCdIdle();
    setRECT(&rect, 640, 60, 16, 1);
    LoadImage(&rect, (u_long *)DYNAMIC_LOAD_BUFFER + TIM_CLUT_DATA_WORDS);
    setRECT(&rect, 640, 0, 160, 60);
    LoadImage(&rect, (u_long *)DYNAMIC_LOAD_BUFFER + TIM_4BIT_PIXEL_DATA_WORDS);
    DrawSync(0);
    SetDefDispEnv(&disp, 0, 0, 640, 480);
    SetDefDrawEnv(&draw, 0, 0, 640, 480);
    draw.isbg = 1;
    SetDispMask(1);
    for (frame = 0; frame < SPLASH_DURATION_FRAMES; frame++) {
        bufferIndex = frame & 1;
        otHead = &ot[bufferIndex];
        ClearOTag(otHead, 1);
        if (frame >= SPLASH_FADE_OUT_START) {
            fadeLevel = 255 - (SPLASH_DURATION_FRAMES - frame) * 4;
        } else {
            fadeLevel = 0;
        }
        if (frame < SPLASH_FADE_IN_FRAMES) {
            fadeLevel = 255 - frame * 4;
        }
        fadeTile = &fadeTiles[bufferIndex];
        SetTile(fadeTile);
        SetSemiTrans(fadeTile, 1);
        setXY0(fadeTile, 0, 210);
        setWH(fadeTile, 640, 60);
        setRGB0(fadeTile, fadeLevel, fadeLevel, fadeLevel);
        AddPrim(otHead, fadeTile);
        sprite = sprites[bufferIndex];
        tpage = tpages[bufferIndex];
        for (x = 0, strip = 0; strip < 3; strip++, x += 256) {
            SetSprt(sprite);
            SetShadeTex(sprite, 1);
            setXY0(sprite, x, 210);
            setWH(sprite, strip == 2 ? 128 : 256, 60);
            setUV0(sprite, 0, 0);
            sprite->clut = GetClut(640, 60);
            AddPrim(otHead, sprite++);
            SetDrawTPage(tpage, 0, 1, GetTPage(SPLASH_TPAGE_4BIT, SPLASH_ABR_SUB, 640 + x / 4, 0));
            AddPrim(otHead, tpage++);
        }
        DrawSync(0);
        VSync(0);
        PutDispEnv(&disp);
        PutDrawEnv(&draw);
        DrawOTag(&ot[frame & 1]);
    }
#endif

    SetDispMask(0);
    ResetGraph(1);
    setRECT(&rect, 0, 0, 1024, 512);
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    VSync(4);
}
#endif
