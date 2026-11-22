#ifndef MAIN_MENU_UI_H
#define MAIN_MENU_UI_H

#include "sprite2d.h"

int UpdateAndDrawMainMenu(int move, int confirm, int analog);
void SetMenuSpriteListYAndHeight(TexturedRectEntry *spriteList, int height, int y);
void InitMainMenu(int initialIndex);

#endif
