/* The menus' gradient backdrop: a double-buffered POLY_G4 in one of two palettes,
 * shaded to grey when the renderer is monochrome. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "display_buffer.h"
#include "tim.h"
#include "gradient_bg.h"
#include "main.h"

static POLY_G4 gMenuBackdropQuads[DISPLAY_BUFFER_COUNT];

#define MENU_BACKDROP_OT_BUCKET 4095
#define MENU_BACKDROP_TOP_Y 16

typedef enum MenuGradientPaletteId {
    MENU_GRADIENT_PALETTE_RED_YELLOW = 0,
    MENU_GRADIENT_PALETTE_BLACK = 1,
    MENU_GRADIENT_PALETTE_COUNT = 2
} MenuGradientPaletteId;

typedef struct MenuGradientColor {
    unsigned char r, g, b;
} MenuGradientColor;

/* Vertex RGB palettes for the menu backdrop POLY_G4 (top-left, top-right, bottom-left, bottom-right):
 * palette 0 = vertical red-to-yellow gradient; palette 1 = solid black. */
typedef struct MenuGradientPalette {
    MenuGradientColor corners[4];
} MenuGradientPalette;

MenuGradientPalette gMenuGradientPalettes[MENU_GRADIENT_PALETTE_COUNT] = {
    { { { 0xc0, 0x00, 0x00 }, { 0xc0, 0x00, 0x00 }, { 0xc0, 0xc0, 0x00 }, { 0xc0, 0xc0, 0x00 } } },
    { { { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00 } } },
};

void InitMenuGradientBackdrop(int palette)
{
    int monochrome = IsRendererMonochrome();
    unsigned char *rgb = &gMenuGradientPalettes[palette].corners[0].r;
    int i;
    POLY_G4 *quad;
    unsigned char luma;

    for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {
        quad = &gMenuBackdropQuads[i];
        SetPolyG4(quad);
        quad->y0 = MENU_BACKDROP_TOP_Y;
        quad->y1 = MENU_BACKDROP_TOP_Y;
        quad->x0 = 0;
        quad->x1 = DISPLAY_WIDTH - 1;
        quad->x2 = 0;
        quad->y2 = DISPLAY_HEIGHT;
        quad->x3 = DISPLAY_WIDTH - 1;
        quad->y3 = DISPLAY_HEIGHT;
        if (monochrome) {
            luma = ConvertRgbToLuminance(rgb[0], rgb[1], rgb[2]);
            rgb += 3;
            quad->r0 = quad->g0 = quad->b0 = luma;
            luma = ConvertRgbToLuminance(rgb[0], rgb[1], rgb[2]);
            rgb += 3;
            quad->r1 = quad->g1 = quad->b1 = luma;
            luma = ConvertRgbToLuminance(rgb[0], rgb[1], rgb[2]);
            rgb += 3;
            quad->r2 = quad->g2 = quad->b2 = luma;
            luma = ConvertRgbToLuminance(rgb[0], rgb[1], rgb[2]);
            rgb += 3;
            quad->r3 = quad->g3 = quad->b3 = luma;
        } else {
            quad->r0 = *rgb++;
            quad->g0 = *rgb++;
            quad->b0 = *rgb++;
            quad->r1 = *rgb++;
            quad->g1 = *rgb++;
            quad->b1 = *rgb++;
            quad->r2 = *rgb++;
            quad->g2 = *rgb++;
            quad->b2 = *rgb++;
            quad->r3 = *rgb++;
            quad->g3 = *rgb++;
            quad->b3 = *rgb++;
        }
        rgb -= sizeof(MenuGradientPalette);
    }
}

void SubmitMenuGradientBackdrop(void)
{
    AddPrim(&gCurrentOt[MENU_BACKDROP_OT_BUCKET], &gMenuBackdropQuads[gFrameCounter & 1]);
}
