#ifndef SPRITE2D_H
#define SPRITE2D_H

#include <sys/types.h>
#include <libgte.h>

/* 16-byte GPU POLY_FT4 color/UV/CLUT/TPage header used by DrawTexturedRectList and gFontGlyphTextures. */
typedef struct GlyphTexture {
    u_char r, g, b, code;
    u_char u0, v0;
    u_short clut;
    u_char u1, v1;
    u_short tpage;
    u_char u2, v2, u3, v3;
} GlyphTexture;

/* Single-rectangle entry for DrawTexturedRectList (countAndFlags low 31 bits = rect count, bit 31 = last group). */
#define TEXTURED_RECT_LAST_GROUP 0x80000000U

typedef struct TexturedRectEntry {
    GlyphTexture texture;
    int countAndFlags;
    short x, y, w, h;
} TexturedRectEntry;

/* Textured quad for DrawRotatedTexturedQuad (16-byte GPU attributes + 4 local SVECTOR corners). */
typedef struct RotatedTexturedQuad {
    GlyphTexture texture;
    SVECTOR vertices[4];
} RotatedTexturedQuad;

void DrawTexturedRectList(void *list, int xOffset, int yOffset, int layer);
void DrawTexturedRectListImmediate(void *list, int xOffset, int yOffset);

#endif
