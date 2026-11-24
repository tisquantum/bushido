#ifndef WIDESCREEN_H
#define WIDESCREEN_H

/* Widescreen on PC (port/sdk/include/psxport.h): how far the picture reaches past the console's left and
 * right edges, in screen pixels, and as an angle (4096 a turn) to each side, in the 5.625-degree headings
 * the stage cull widens by. Both are a setting of the simulation, the same on both machines of an online
 * match. The console build has neither. */
#ifdef TARGET_PC
#define WIDE_SCREEN_MARGIN g_pcScreenMargin
#define WIDE_VIEW_ANGLE    (g_pcWideHeadings * 64)
#else
#define WIDE_SCREEN_MARGIN 0
#define WIDE_VIEW_ANGLE    0
#endif

#endif
