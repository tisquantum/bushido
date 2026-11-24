/* Fullscreen RGB screen fader: interpolates a double-buffered semi-transparent POLY_F4 + DR_MODE
 * from a 12-entry preset table in Q12 steps and links it at the head of the active ordering table. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "display_buffer.h"
#include "main.h"
#include "screen_fade.h"

ScreenFadePreset gScreenFadePresets[SCREEN_FADE_PRESET_COUNT] = {
    {   0,   0,   0, 255, 255, 255,  216, SCREEN_FADE_ABR_ADD, 0 },
    { 255, 255, 255,   0,   0,   0,  216, SCREEN_FADE_ABR_ADD, 0 },
    { 255, 255, 255,   0,   0,   0,  216, SCREEN_FADE_ABR_SUB, 0 },
    {   0,   0,   0, 255, 255, 255,  216, SCREEN_FADE_ABR_SUB, 0 },
    { 255, 255, 255,   0,   0,   0,  120, SCREEN_FADE_ABR_SUB, 0 },
    {   0,   0,   0, 255, 255, 255,  120, SCREEN_FADE_ABR_SUB, 0 },
    {   0,   0,   0, 255, 255, 255, 1024, SCREEN_FADE_ABR_ADD, 0 },
    { 255, 255, 255,   0,   0,   0,  512, SCREEN_FADE_ABR_ADD, 0 },
    { 255, 255, 255,   0,   0,   0,  384, SCREEN_FADE_ABR_SUB, 0 },
    {   0,   0,   0, 255, 255, 255,  384, SCREEN_FADE_ABR_SUB, 0 },
    {   0,   0,   0, 255, 255, 255,   68, SCREEN_FADE_ABR_ADD, 0 },
    { 255, 255, 255,   0,   0,   0,  256, SCREEN_FADE_ABR_ADD, 0 },
};

static POLY_F4 gScreenFadeQuads[SCREEN_FADE_BANK_COUNT];
static DR_MODE gScreenFadeModes[SCREEN_FADE_BANK_COUNT];

static ScreenFadePreset *gActiveScreenFade;

#ifdef TARGET_PC
/* A fade drawn before any has started reads its preset through a null pointer: RAM at address 0 on the
 * console, which the PC cannot map. The port reads a preset of zeros instead. */
static ScreenFadePreset sUnstartedScreenFade;
#define ACTIVE_SCREEN_FADE (gActiveScreenFade != NULL ? gActiveScreenFade : &sUnstartedScreenFade)
#else
#define ACTIVE_SCREEN_FADE gActiveScreenFade
#endif

static int gScreenFadeInitFrames;           /* counts down from SCREEN_FADE_BANK_COUNT so both banks initialize */
static int gScreenFadePresetId;             /* written, never read */

void StartScreenFade(int presetId)
{
    gScreenFadeInitFrames = SCREEN_FADE_BANK_COUNT;
    gScreenFadePresetId = presetId;
    gActiveScreenFade = &gScreenFadePresets[presetId];
    gActiveScreenFade->progress = 0;
}

void BuildScreenFadePrim(void)
{
    int bank;
    POLY_F4 *poly;
    DR_MODE *drawMode;

    bank = gFrameCounter & 1;
    poly = &gScreenFadeQuads[bank];
    SetPolyF4(poly);
    setRGB0(poly, ACTIVE_SCREEN_FADE->startR, ACTIVE_SCREEN_FADE->startG, ACTIVE_SCREEN_FADE->startB);
    SetSemiTrans(poly, 1);
    setXYWH(poly, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    drawMode = &gScreenFadeModes[bank];
    SetDrawMode(drawMode, 1, 1, GetTPage(0, ACTIVE_SCREEN_FADE->abr, 0, 0), 0);
}

int UpdateAndDrawScreenFade(void)
{
    POLY_F4 *poly;
    int done;

    done = 0;
    if (gScreenFadeInitFrames != 0) {
        BuildScreenFadePrim();
        gScreenFadeInitFrames--;
    }
    poly = &gScreenFadeQuads[gFrameCounter & 1];
    poly->r0 = ((ACTIVE_SCREEN_FADE->endR - ACTIVE_SCREEN_FADE->startR) * ACTIVE_SCREEN_FADE->progress) / ONE +
        ACTIVE_SCREEN_FADE->startR;
    poly->g0 = ((ACTIVE_SCREEN_FADE->endG - ACTIVE_SCREEN_FADE->startG) * ACTIVE_SCREEN_FADE->progress) / ONE +
        ACTIVE_SCREEN_FADE->startG;
    poly->b0 = ((ACTIVE_SCREEN_FADE->endB - ACTIVE_SCREEN_FADE->startB) * ACTIVE_SCREEN_FADE->progress) / ONE +
        ACTIVE_SCREEN_FADE->startB;
    AddPrim(gCurrentOt, &gScreenFadeQuads[gFrameCounter & 1]);
    AddPrim(gCurrentOt, &gScreenFadeModes[gFrameCounter & 1]);
    ACTIVE_SCREEN_FADE->progress += ACTIVE_SCREEN_FADE->step;
    if (ACTIVE_SCREEN_FADE->progress > ONE) {
        ACTIVE_SCREEN_FADE->progress = ONE;
        done = 1;
    }
    return done;
}
