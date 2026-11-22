#ifndef DISPLAY_BUFFER_H
#define DISPLAY_BUFFER_H

#define DISPLAY_BUFFER_COUNT 2
#define DISPLAY_WIDTH 640
#define DISPLAY_HEIGHT 240
#define DISPLAY_VRAM_HEIGHT 480
#define DISPLAY_OT_LEN 0x1005

#ifdef VERSION_EU
/* PAL: the 240 lines are shown 28 lines down the taller PAL screen. */
#define DISPLAY_SCREEN_Y 28
#endif

/* Double-buffered frame environment and reverse ordering table (0x4084 bytes). */
typedef struct DisplayBuffer {
    DRAWENV draw;              /* +0x0000: 640x240 draw buffer (Y=0 or Y=240) */
    DISPENV disp;              /* +0x005C: 640x240 display buffer (Y=240 or Y=0) */
    u_long ot[DISPLAY_OT_LEN]; /* +0x0070: 4101-entry reverse OT (ClearOTagR / DrawOTag from tail) */
} DisplayBuffer;

extern DisplayBuffer db[DISPLAY_BUFFER_COUNT];

#endif
