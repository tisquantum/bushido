#ifndef GRADIENT_BG_H
#define GRADIENT_BG_H

typedef enum MenuGradientPaletteId {
    MENU_GRADIENT_PALETTE_RED_YELLOW = 0,
    MENU_GRADIENT_PALETTE_BLACK = 1,
    MENU_GRADIENT_PALETTE_COUNT = 2
} MenuGradientPaletteId;

void InitMenuGradientBackdrop(int palette);
void SubmitMenuGradientBackdrop(void);

#endif
