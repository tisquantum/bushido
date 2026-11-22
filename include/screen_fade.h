#ifndef SCREEN_FADE_H
#define SCREEN_FADE_H

#define SCREEN_FADE_PRESET_COUNT 12
#define SCREEN_FADE_BANK_COUNT 2

/* PlayStation GPU semi-transparency rate (abr) passed to GetTPage for the fade quad. */
#define SCREEN_FADE_ABR_ADD 1 /* fades to and from white */
#define SCREEN_FADE_ABR_SUB 2 /* fades to and from black */

/* Indices into gScreenFadePresets[12] passed to StartScreenFade. */
typedef enum ScreenFadePresetId {
    SCREEN_FADE_OUT_TO_WHITE = 0,        /* additive, step 216 (~19 frames) */
    SCREEN_FADE_IN_FROM_WHITE_MED = 1,   /* additive, step 216 (~19 frames) */
    SCREEN_FADE_IN_FROM_BLACK_MED = 2,   /* subtractive, step 216 (~19 frames) */
    SCREEN_FADE_OUT_TO_BLACK_MED = 3,    /* subtractive, step 216 (~19 frames) */
    SCREEN_FADE_IN_FROM_BLACK = 4,       /* subtractive, step 120 (~35 frames) */
    SCREEN_FADE_OUT_TO_BLACK = 5,        /* subtractive, step 120 (~35 frames) */
    SCREEN_FADE_FLASH_TO_WHITE = 6,      /* additive, step 1024 (4 frames) */
    SCREEN_FADE_IN_FROM_WHITE = 7,       /* additive, step 512 (8 frames) */
    SCREEN_FADE_IN_FROM_BLACK_FAST = 8,  /* subtractive, step 384 (~11 frames) */
    SCREEN_FADE_OUT_TO_BLACK_FAST = 9,   /* subtractive, step 384 (~11 frames) */
    SCREEN_FADE_OUT_TO_WHITE_SLOW = 10,  /* additive, step 68 (~61 frames) */
    SCREEN_FADE_IN_FROM_WHITE_FAST = 11  /* additive, step 256 (16 frames) */
} ScreenFadePresetId;

/* 36-byte fullscreen RGB fade preset (start/end RGB, Q12 step, GPU abr, mutable Q12 progress). */
typedef struct ScreenFadePreset {
    int startR;
    int startG;
    int startB;
    int endR;
    int endG;
    int endB;
    int step;
    int abr;
    int progress;
} ScreenFadePreset;

void StartScreenFade(int presetId);
void BuildScreenFadePrim(void);
int UpdateAndDrawScreenFade(void);

#endif
