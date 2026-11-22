/*
 * Bitmap font formatter (#X #Y #C #S ... control codes), in-game HUD overlays (Continue, VS win
 * counts, Slash mode, Demo, Replay, Pause, Now Loading), HUD banners, and the Link Lobby screen.
 */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <stdarg.h>
#include "gpu_code.h"
#include "packet_build.h"
#include "model_arena.h"
#include "tim.h"
#include "sprite2d.h"
#include "asset_load.h"
#include "font_hud.h"
#include "main.h"
#include "globals.h"

/* Bitmap font state. */
int gFontGlyphWidth = 15;
int gFontGlyphHeight = 15; /* also the line-feed step */
int gFontCursorX = 0;      /* pixels */
int gFontCursorY = 0;      /* pixels */
int gFontGlyphScale = 0;   /* Q12; 0 draws glyphs 1:1 */
#ifndef VERSION_JP
int gFontGlyphBank = 0;    /* glyph table offset: 0 = bank 0, 57 = bank 1 */
#endif
unsigned char gFontTintR = 0x80;
unsigned char gFontTintG = 0x80;
unsigned char gFontTintB = 0x80;
unsigned char gFontTintUnused = 0xff; /* nothing reads or writes it */
static char gFontGlyphBuffer[608];
static TexturedRectEntry *gFontGlyphWritePtr; /* Write cursor into gFontGlyphBuffer */

#define TEXT_FMT_LITERAL 0
#define TEXT_FMT_PERCENT 1
#define TEXT_FMT_CONTROL 2

#define FONT_BANK_ALT_OFFSET 57
#define FONT_SCALE_1X        0x1000 /* Q12 1.0x */
#define FONT_SCALE_2X        0x2000 /* Q12 2.0x */

/* The Japanese build's text keeps its tint and size between calls, places the cursor in glyph cells
 * (#x, #y) and sets the glyph size (#w, #h), and has no font bank (#F); it closes the list even when the
 * format drew no glyph, ORing the flag into memory before gFontGlyphBuffer (DrawSlashModeHud's "#X#Y#w#h#S#C"). */
void DrawFormattedText(char *format, ...)
{
    va_list args;
    unsigned char ch;
    short mode = TEXT_FMT_LITERAL;
    int operand;
    int digits;

    gFontGlyphWritePtr = (TexturedRectEntry *)gFontGlyphBuffer;
#ifndef VERSION_JP
    gFontTintR = gFontTintG = gFontTintB = 128;
#endif
    gFontGlyphScale = 0;
#ifndef VERSION_JP
    gFontGlyphBank = 0;
#endif
    va_start(args, format);

    while ((ch = *format++) != '\0') {
        switch (ch) {
        case '%':
            mode = TEXT_FMT_PERCENT;
            digits = 4;
            break;
        case '#':
            mode = TEXT_FMT_CONTROL;
            break;
        case '\n':
            gFontCursorX = 0;
            gFontCursorY += gFontGlyphHeight;
            break;
        default:
            switch (mode) {
            case TEXT_FMT_PERCENT:
                if ((unsigned char)(ch - '0') < 10) {
                    digits = ch - '0';
                } else {
                    switch (ch) {
                    case 'x': {
                        operand = va_arg(args, unsigned int) << ((8 - digits) * 4);
                        while (digits--) {
                            unsigned int nibble = (unsigned int)operand >> 28;
                            unsigned char glyph = nibble < 10 ? nibble | '0' : nibble + 'A' - 10;
                            operand = (unsigned int)operand << 4;
                            AppendBitmapGlyph(glyph);
                        }
                        break;
                    }
                    case 's': {
                        char *str = va_arg(args, char *);
                        while ((ch = *str++) != '\0') {
                            AppendBitmapGlyph(ch);
                        }
                        break;
                    }
                    }
                    mode = TEXT_FMT_LITERAL;
                }
                break;
            case TEXT_FMT_CONTROL:
                /* the original reads some arguments as short or unsigned char */
                switch (ch) {
                case 'r':
                    gFontCursorX = 0;
                    gFontCursorY = 0;
                    break;
#ifdef VERSION_JP
                case 'x':
                    gFontCursorX = va_arg(args, short) * gFontGlyphWidth;
                    break;
                case 'y':
                    gFontCursorY = va_arg(args, short) * gFontGlyphHeight;
                    break;
#endif
                case 'X':
                    gFontCursorX = va_arg(args, short);
                    break;
                case 'Y':
                    gFontCursorY = va_arg(args, short);
                    break;
#ifdef VERSION_JP
                case 'w':
                    gFontGlyphWidth = va_arg(args, short);
                    break;
                case 'h':
                    gFontGlyphHeight = va_arg(args, short);
                    break;
#endif
                case 'C': {
                    operand = va_arg(args, int);
                    gFontTintR = operand >> 16;
                    gFontTintG = operand >> 8;
                    ch = operand;
                    gFontTintB = ch;
                    if (gRendererMonochrome) {
                        operand = ConvertRgbToLuminance(gFontTintR, gFontTintG, (unsigned char)operand);
                        gFontTintR = operand;
                        gFontTintG = operand;
                        gFontTintB = operand;
                    }
                    break;
                }
                case 'R':
                    gFontTintR = va_arg(args, unsigned char);
                    break;
                case 'G':
                    gFontTintG = va_arg(args, unsigned char);
                    break;
                case 'B':
                    gFontTintB = va_arg(args, unsigned char);
                    break;
                case 'S':
                    gFontGlyphScale = va_arg(args, unsigned short);
                    break;
#ifndef VERSION_JP
                case 'F':
                    operand = va_arg(args, int);
                    if (operand == 1) {
                        gFontGlyphBank = FONT_BANK_ALT_OFFSET;
                    } else {
                        gFontGlyphBank = 0;
                    }
                    break;
                case 'y':
                    break;
#endif
                }
                mode = TEXT_FMT_LITERAL;
                break;
            case TEXT_FMT_LITERAL:
                AppendBitmapGlyph(ch);
                break;
            }
            break;
        }
    }
#ifndef VERSION_JP
    if (gFontGlyphWritePtr != (TexturedRectEntry *)gFontGlyphBuffer)
#endif
    {
        (gFontGlyphWritePtr - 1)->countAndFlags |= TEXTURED_RECT_LAST_GROUP;
        DrawTexturedRectList(gFontGlyphBuffer, 0, 0, 0);
    }
}

/* By glyph index: 0 to 2 . ! ?, 3 to 12 the digits, 13 to 38 A to Z, then outside Japan 39 to 64 a to z and
 * 65 to 69 ` " ' , -. Bank 1 adds 57: its . ! ? and digits share entries 57 to 69 with bank 0's s to z and
 * punctuation. */
#ifdef VERSION_JP
GlyphTexture gFontGlyphTextures[39] = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 192, GET_CLUT(896, 473), 103, 192,
        GET_TPAGE(0, 1, 896, 256), 96, 207, 103, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 104, 192, GET_CLUT(896, 473), 111, 192,
        GET_TPAGE(0, 1, 896, 256), 104, 207, 111, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 192, GET_CLUT(896, 473), 127, 192,
        GET_TPAGE(0, 1, 896, 256), 112, 207, 127, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 144, GET_CLUT(896, 473), 111, 144,
        GET_TPAGE(0, 1, 896, 256), 96, 159, 111, 159 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 144, GET_CLUT(896, 473), 127, 144,
        GET_TPAGE(0, 1, 896, 256), 112, 159, 127, 159 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 144, GET_CLUT(896, 473), 143, 144,
        GET_TPAGE(0, 1, 896, 256), 128, 159, 143, 159 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 160, GET_CLUT(896, 473), 111, 160,
        GET_TPAGE(0, 1, 896, 256), 96, 175, 111, 175 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 160, GET_CLUT(896, 473), 127, 160,
        GET_TPAGE(0, 1, 896, 256), 112, 175, 127, 175 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 160, GET_CLUT(896, 473), 143, 160,
        GET_TPAGE(0, 1, 896, 256), 128, 175, 143, 175 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 176, GET_CLUT(896, 473), 111, 176,
        GET_TPAGE(0, 1, 896, 256), 96, 191, 111, 191 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 176, GET_CLUT(896, 473), 127, 176,
        GET_TPAGE(0, 1, 896, 256), 112, 191, 127, 191 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 176, GET_CLUT(896, 473), 143, 176,
        GET_TPAGE(0, 1, 896, 256), 128, 191, 143, 191 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 192, GET_CLUT(896, 473), 143, 192,
        GET_TPAGE(0, 1, 896, 256), 128, 207, 143, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 0, GET_CLUT(896, 480), 231, 0,
        GET_TPAGE(0, 1, 896, 256), 216, 15, 231, 15 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 0, GET_CLUT(896, 480), 247, 0,
        GET_TPAGE(0, 1, 896, 256), 232, 15, 247, 15 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 16, GET_CLUT(896, 480), 231, 16,
        GET_TPAGE(0, 1, 896, 256), 216, 31, 231, 31 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 16, GET_CLUT(896, 480), 247, 16,
        GET_TPAGE(0, 1, 896, 256), 232, 31, 247, 31 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 32, GET_CLUT(896, 480), 231, 32,
        GET_TPAGE(0, 1, 896, 256), 216, 47, 231, 47 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 32, GET_CLUT(896, 480), 247, 32,
        GET_TPAGE(0, 1, 896, 256), 232, 47, 247, 47 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 48, GET_CLUT(896, 480), 231, 48,
        GET_TPAGE(0, 1, 896, 256), 216, 63, 231, 63 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 48, GET_CLUT(896, 480), 247, 48,
        GET_TPAGE(0, 1, 896, 256), 232, 63, 247, 63 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 64, GET_CLUT(896, 480), 231, 64,
        GET_TPAGE(0, 1, 896, 256), 216, 79, 231, 79 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 64, GET_CLUT(896, 480), 247, 64,
        GET_TPAGE(0, 1, 896, 256), 232, 79, 247, 79 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 80, GET_CLUT(896, 480), 231, 80,
        GET_TPAGE(0, 1, 896, 256), 216, 95, 231, 95 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 80, GET_CLUT(896, 480), 247, 80,
        GET_TPAGE(0, 1, 896, 256), 232, 95, 247, 95 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 96, GET_CLUT(896, 480), 231, 96,
        GET_TPAGE(0, 1, 896, 256), 216, 111, 231, 111 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 96, GET_CLUT(896, 480), 247, 96,
        GET_TPAGE(0, 1, 896, 256), 232, 111, 247, 111 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 112, GET_CLUT(896, 480), 231, 112,
        GET_TPAGE(0, 1, 896, 256), 216, 127, 231, 127 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 112, GET_CLUT(896, 480), 247, 112,
        GET_TPAGE(0, 1, 896, 256), 232, 127, 247, 127 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 128, GET_CLUT(896, 480), 231, 128,
        GET_TPAGE(0, 1, 896, 256), 216, 143, 231, 143 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 128, GET_CLUT(896, 480), 247, 128,
        GET_TPAGE(0, 1, 896, 256), 232, 143, 247, 143 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 144, GET_CLUT(896, 480), 231, 144,
        GET_TPAGE(0, 1, 896, 256), 216, 159, 231, 159 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 144, GET_CLUT(896, 480), 247, 144,
        GET_TPAGE(0, 1, 896, 256), 232, 159, 247, 159 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 160, GET_CLUT(896, 480), 231, 160,
        GET_TPAGE(0, 1, 896, 256), 216, 175, 231, 175 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 160, GET_CLUT(896, 480), 247, 160,
        GET_TPAGE(0, 1, 896, 256), 232, 175, 247, 175 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 176, GET_CLUT(896, 480), 231, 176,
        GET_TPAGE(0, 1, 896, 256), 216, 191, 231, 191 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 176, GET_CLUT(896, 480), 247, 176,
        GET_TPAGE(0, 1, 896, 256), 232, 191, 247, 191 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 192, GET_CLUT(896, 480), 231, 192,
        GET_TPAGE(0, 1, 896, 256), 216, 207, 231, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 208, GET_CLUT(896, 480), 247, 208,
        GET_TPAGE(0, 1, 896, 256), 232, 223, 247, 223 },
};
short gFontGlyphAdvances[40] = {
    6, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 15, 12, 10, 16, 15, 7, 16, 16, 13, 16,
    16, 16, 15, 15, 16, 15, 15, 15, 16, 16, 16, 16, 15,
    0,
};
#else
GlyphTexture gFontGlyphTextures[96] = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 192, GET_CLUT(896, 473), 103, 192,
        GET_TPAGE(0, 1, 896, 256), 96, 207, 103, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 104, 192, GET_CLUT(896, 473), 111, 192,
        GET_TPAGE(0, 1, 896, 256), 104, 207, 111, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 192, GET_CLUT(896, 473), 127, 192,
        GET_TPAGE(0, 1, 896, 256), 112, 207, 127, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 144, GET_CLUT(896, 473), 111, 144,
        GET_TPAGE(0, 1, 896, 256), 96, 159, 111, 159 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 144, GET_CLUT(896, 473), 127, 144,
        GET_TPAGE(0, 1, 896, 256), 112, 159, 127, 159 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 144, GET_CLUT(896, 473), 143, 144,
        GET_TPAGE(0, 1, 896, 256), 128, 159, 143, 159 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 160, GET_CLUT(896, 473), 111, 160,
        GET_TPAGE(0, 1, 896, 256), 96, 175, 111, 175 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 160, GET_CLUT(896, 473), 127, 160,
        GET_TPAGE(0, 1, 896, 256), 112, 175, 127, 175 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 160, GET_CLUT(896, 473), 143, 160,
        GET_TPAGE(0, 1, 896, 256), 128, 175, 143, 175 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 176, GET_CLUT(896, 473), 111, 176,
        GET_TPAGE(0, 1, 896, 256), 96, 191, 111, 191 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 176, GET_CLUT(896, 473), 127, 176,
        GET_TPAGE(0, 1, 896, 256), 112, 191, 127, 191 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 176, GET_CLUT(896, 473), 143, 176,
        GET_TPAGE(0, 1, 896, 256), 128, 191, 143, 191 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 192, GET_CLUT(896, 473), 143, 192,
        GET_TPAGE(0, 1, 896, 256), 128, 207, 143, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 0, GET_CLUT(896, 480), 231, 0,
        GET_TPAGE(0, 1, 896, 256), 216, 15, 231, 15 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 0, GET_CLUT(896, 480), 247, 0,
        GET_TPAGE(0, 1, 896, 256), 232, 15, 247, 15 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 16, GET_CLUT(896, 480), 231, 16,
        GET_TPAGE(0, 1, 896, 256), 216, 31, 231, 31 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 16, GET_CLUT(896, 480), 247, 16,
        GET_TPAGE(0, 1, 896, 256), 232, 31, 247, 31 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 32, GET_CLUT(896, 480), 231, 32,
        GET_TPAGE(0, 1, 896, 256), 216, 47, 231, 47 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 32, GET_CLUT(896, 480), 247, 32,
        GET_TPAGE(0, 1, 896, 256), 232, 47, 247, 47 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 48, GET_CLUT(896, 480), 231, 48,
        GET_TPAGE(0, 1, 896, 256), 216, 63, 231, 63 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 48, GET_CLUT(896, 480), 247, 48,
        GET_TPAGE(0, 1, 896, 256), 232, 63, 247, 63 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 64, GET_CLUT(896, 480), 231, 64,
        GET_TPAGE(0, 1, 896, 256), 216, 79, 231, 79 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 64, GET_CLUT(896, 480), 247, 64,
        GET_TPAGE(0, 1, 896, 256), 232, 79, 247, 79 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 80, GET_CLUT(896, 480), 231, 80,
        GET_TPAGE(0, 1, 896, 256), 216, 95, 231, 95 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 80, GET_CLUT(896, 480), 247, 80,
        GET_TPAGE(0, 1, 896, 256), 232, 95, 247, 95 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 96, GET_CLUT(896, 480), 231, 96,
        GET_TPAGE(0, 1, 896, 256), 216, 111, 231, 111 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 96, GET_CLUT(896, 480), 247, 96,
        GET_TPAGE(0, 1, 896, 256), 232, 111, 247, 111 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 112, GET_CLUT(896, 480), 231, 112,
        GET_TPAGE(0, 1, 896, 256), 216, 127, 231, 127 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 112, GET_CLUT(896, 480), 247, 112,
        GET_TPAGE(0, 1, 896, 256), 232, 127, 247, 127 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 128, GET_CLUT(896, 480), 231, 128,
        GET_TPAGE(0, 1, 896, 256), 216, 143, 231, 143 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 128, GET_CLUT(896, 480), 247, 128,
        GET_TPAGE(0, 1, 896, 256), 232, 143, 247, 143 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 144, GET_CLUT(896, 480), 231, 144,
        GET_TPAGE(0, 1, 896, 256), 216, 159, 231, 159 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 144, GET_CLUT(896, 480), 247, 144,
        GET_TPAGE(0, 1, 896, 256), 232, 159, 247, 159 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 160, GET_CLUT(896, 480), 231, 160,
        GET_TPAGE(0, 1, 896, 256), 216, 175, 231, 175 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 160, GET_CLUT(896, 480), 247, 160,
        GET_TPAGE(0, 1, 896, 256), 232, 175, 247, 175 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 176, GET_CLUT(896, 480), 231, 176,
        GET_TPAGE(0, 1, 896, 256), 216, 191, 231, 191 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 176, GET_CLUT(896, 480), 247, 176,
        GET_TPAGE(0, 1, 896, 256), 232, 191, 247, 191 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 192, GET_CLUT(896, 480), 231, 192,
        GET_TPAGE(0, 1, 896, 256), 216, 207, 231, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 192, GET_CLUT(896, 480), 247, 192,
        GET_TPAGE(0, 1, 896, 256), 232, 207, 247, 207 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 144, GET_CLUT(896, 470), 15, 144,
        GET_TPAGE(0, 1, 896, 256), 0, 156, 15, 156 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 16, 144, GET_CLUT(896, 470), 31, 144,
        GET_TPAGE(0, 1, 896, 256), 16, 156, 31, 156 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 144, GET_CLUT(896, 470), 47, 144,
        GET_TPAGE(0, 1, 896, 256), 32, 156, 47, 156 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 144, GET_CLUT(896, 470), 63, 144,
        GET_TPAGE(0, 1, 896, 256), 48, 156, 63, 156 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 144, GET_CLUT(896, 470), 79, 144,
        GET_TPAGE(0, 1, 896, 256), 64, 156, 79, 156 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 80, 144, GET_CLUT(896, 470), 95, 144,
        GET_TPAGE(0, 1, 896, 256), 80, 156, 95, 156 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 157, GET_CLUT(896, 470), 15, 157,
        GET_TPAGE(0, 1, 896, 256), 0, 169, 15, 169 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 16, 157, GET_CLUT(896, 470), 31, 157,
        GET_TPAGE(0, 1, 896, 256), 16, 169, 31, 169 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 157, GET_CLUT(896, 470), 47, 157,
        GET_TPAGE(0, 1, 896, 256), 32, 169, 47, 169 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 157, GET_CLUT(896, 470), 63, 157,
        GET_TPAGE(0, 1, 896, 256), 48, 169, 63, 169 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 157, GET_CLUT(896, 470), 79, 157,
        GET_TPAGE(0, 1, 896, 256), 64, 169, 79, 169 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 80, 157, GET_CLUT(896, 470), 95, 157,
        GET_TPAGE(0, 1, 896, 256), 80, 169, 95, 169 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 170, GET_CLUT(896, 470), 15, 170,
        GET_TPAGE(0, 1, 896, 256), 0, 182, 15, 182 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 16, 170, GET_CLUT(896, 470), 31, 170,
        GET_TPAGE(0, 1, 896, 256), 16, 182, 31, 182 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 170, GET_CLUT(896, 470), 47, 170,
        GET_TPAGE(0, 1, 896, 256), 32, 182, 47, 182 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 170, GET_CLUT(896, 470), 63, 170,
        GET_TPAGE(0, 1, 896, 256), 48, 182, 63, 182 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 170, GET_CLUT(896, 470), 79, 170,
        GET_TPAGE(0, 1, 896, 256), 64, 182, 79, 182 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 80, 170, GET_CLUT(896, 470), 95, 170,
        GET_TPAGE(0, 1, 896, 256), 80, 182, 95, 182 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 183, GET_CLUT(896, 470), 15, 183,
        GET_TPAGE(0, 1, 896, 256), 0, 195, 15, 195 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 16, 183, GET_CLUT(896, 470), 31, 183,
        GET_TPAGE(0, 1, 896, 256), 16, 195, 31, 195 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 183, GET_CLUT(896, 470), 47, 183,
        GET_TPAGE(0, 1, 896, 256), 32, 195, 47, 195 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 183, GET_CLUT(896, 470), 63, 183,
        GET_TPAGE(0, 1, 896, 256), 48, 195, 63, 195 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 183, GET_CLUT(896, 470), 79, 183,
        GET_TPAGE(0, 1, 896, 256), 64, 195, 79, 195 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 80, 183, GET_CLUT(896, 470), 95, 183,
        GET_TPAGE(0, 1, 896, 256), 80, 195, 95, 195 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 196, GET_CLUT(896, 470), 15, 196,
        GET_TPAGE(0, 1, 896, 256), 0, 208, 15, 208 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 16, 196, GET_CLUT(896, 470), 31, 196,
        GET_TPAGE(0, 1, 896, 256), 16, 208, 31, 208 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 196, GET_CLUT(896, 470), 47, 196,
        GET_TPAGE(0, 1, 896, 256), 32, 208, 47, 208 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 196, GET_CLUT(896, 470), 63, 196,
        GET_TPAGE(0, 1, 896, 256), 48, 208, 63, 208 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 196, GET_CLUT(896, 470), 71, 196,
        GET_TPAGE(0, 1, 896, 256), 64, 208, 71, 208 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 72, 196, GET_CLUT(896, 470), 79, 196,
        GET_TPAGE(0, 1, 896, 256), 72, 208, 79, 208 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 80, 196, GET_CLUT(896, 470), 95, 196,
        GET_TPAGE(0, 1, 896, 256), 80, 208, 95, 208 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 224, GET_CLUT(1008, 174), 15, 224,
        GET_TPAGE(0, 1, 704, 256), 0, 239, 15, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 16, 224, GET_CLUT(1008, 174), 31, 224,
        GET_TPAGE(0, 1, 704, 256), 16, 239, 31, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 224, GET_CLUT(1008, 174), 47, 224,
        GET_TPAGE(0, 1, 704, 256), 32, 239, 47, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 224, GET_CLUT(1008, 174), 63, 224,
        GET_TPAGE(0, 1, 704, 256), 48, 239, 63, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 224, GET_CLUT(1008, 174), 79, 224,
        GET_TPAGE(0, 1, 704, 256), 64, 239, 79, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 80, 224, GET_CLUT(1008, 174), 95, 224,
        GET_TPAGE(0, 1, 704, 256), 80, 239, 95, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 224, GET_CLUT(1008, 174), 111, 224,
        GET_TPAGE(0, 1, 704, 256), 96, 239, 111, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 224, GET_CLUT(1008, 174), 127, 224,
        GET_TPAGE(0, 1, 704, 256), 112, 239, 127, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 224, GET_CLUT(1008, 174), 143, 224,
        GET_TPAGE(0, 1, 704, 256), 128, 239, 143, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 144, 224, GET_CLUT(1008, 174), 159, 224,
        GET_TPAGE(0, 1, 704, 256), 144, 239, 159, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 224, GET_CLUT(1008, 174), 175, 224,
        GET_TPAGE(0, 1, 704, 256), 160, 239, 175, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 176, 224, GET_CLUT(1008, 174), 191, 224,
        GET_TPAGE(0, 1, 704, 256), 176, 239, 191, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 192, 224, GET_CLUT(1008, 174), 207, 224,
        GET_TPAGE(0, 1, 704, 256), 192, 239, 207, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 208, 224, GET_CLUT(1008, 174), 223, 224,
        GET_TPAGE(0, 1, 704, 256), 208, 239, 223, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 224, 224, GET_CLUT(1008, 174), 239, 224,
        GET_TPAGE(0, 1, 704, 256), 224, 239, 239, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 240, 224, GET_CLUT(1008, 174), 255, 224,
        GET_TPAGE(0, 1, 704, 256), 240, 239, 255, 239 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 240, GET_CLUT(1008, 174), 15, 240,
        GET_TPAGE(0, 1, 704, 256), 0, 255, 15, 255 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 16, 240, GET_CLUT(1008, 174), 31, 240,
        GET_TPAGE(0, 1, 704, 256), 16, 255, 31, 255 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 240, GET_CLUT(1008, 174), 47, 240,
        GET_TPAGE(0, 1, 704, 256), 32, 255, 47, 255 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 48, 240, GET_CLUT(1008, 174), 63, 240,
        GET_TPAGE(0, 1, 704, 256), 48, 255, 63, 255 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 240, GET_CLUT(1008, 174), 79, 240,
        GET_TPAGE(0, 1, 704, 256), 64, 255, 79, 255 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 80, 240, GET_CLUT(1008, 174), 95, 240,
        GET_TPAGE(0, 1, 704, 256), 80, 255, 95, 255 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 96, 240, GET_CLUT(1008, 174), 111, 240,
        GET_TPAGE(0, 1, 704, 256), 96, 255, 111, 255 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 112, 240, GET_CLUT(1008, 174), 127, 240,
        GET_TPAGE(0, 1, 704, 256), 112, 255, 127, 255 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 128, 240, GET_CLUT(1008, 174), 143, 240,
        GET_TPAGE(0, 1, 704, 256), 128, 255, 143, 255 },
    { 128, 128, 128, GPU_CODE_POLY_FT4, 144, 240, GET_CLUT(1008, 174), 159, 240,
        GET_TPAGE(0, 1, 704, 256), 144, 255, 159, 255 },
};
short gFontGlyphAdvances[96] = {
    6, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 14, 14, 14, 14, 14, 14, 14, 14, 6, 14, 15,
    14, 16, 14, 14, 14, 14, 15, 14, 14, 14, 16, 16, 16, 14, 14, 11, 11, 11, 11, 11, 9, 11, 11, 5,
    11, 11, 5, 13, 11, 11, 11, 11, 9, 11, 11, 11, 12, 13, 10, 11, 10, 12, 12, 6, 6, 10, 14, 14,
    14, 14, 14, 14, 14, 14, 6, 14, 15, 14, 16, 14, 14, 14, 14, 15, 14, 14, 14, 16, 16, 16, 14, 14,
};
short gFontGlyphWidths[96] = {
    5, 7, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 7, 7, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
};
short gFontGlyphHeights[96] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
};
short gFontGlyphYOffsets[96] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 3, 3, 3, 3, 3, 3, 6, 3, 3,
    6, 3, 3, 3, 3, 3, 6, 6, 3, 3, 3, 3,
    3, 3, 3, 6, 3, 0, 0, 0, 4, 3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
#endif

/* The Japanese font has no lowercase or punctuation beyond . ! ?, no second bank and no per-glyph
 * size: each glyph takes the size #w and #h set, and unscaled text advances by that width. */
void AppendBitmapGlyph(unsigned char glyph)
{
    TexturedRectEntry *record;

    switch (glyph) {
    case '.': glyph = 0; break;
    case '!': glyph = 1; break;
    case '?': glyph = 2; break;
#ifndef VERSION_JP
    case '`': glyph = 65; break;
    case '"': glyph = 66; break;
    case '\'': glyph = 67; break;
    case ',': glyph = 68; break;
    case '-': glyph = 69; break;
#endif
    default:
        if ((unsigned char)(glyph - '0') < 10) {
            glyph -= '0' - 3;
        } else if ((unsigned char)(glyph - 'A') < 26) {
            glyph -= 'A' - 13;
#ifndef VERSION_JP
        } else if ((unsigned char)(glyph - 'a') < 26) {
            glyph -= 'a' - 39;
#endif
        } else {
            glyph = FONT_GLYPH_SPACE;
        }
        break;
    }

    if (glyph != FONT_GLYPH_SPACE) {
#ifndef VERSION_JP
        if (gFontGlyphScale == 0) {
            gFontGlyphWidth = gFontGlyphWidths[glyph + gFontGlyphBank];
            gFontGlyphHeight = gFontGlyphHeights[glyph + gFontGlyphBank];
        } else {
            gFontGlyphWidth = (((gFontGlyphWidths[glyph + gFontGlyphBank] + 1) * gFontGlyphScale) >> 12) - 1;
            gFontGlyphHeight = (((gFontGlyphHeights[glyph + gFontGlyphBank] + 1) * gFontGlyphScale) >> 12) - 1;
        }
#endif
#ifdef VERSION_JP
        gFontGlyphWritePtr->texture = gFontGlyphTextures[glyph];
#else
        gFontGlyphWritePtr->texture = gFontGlyphTextures[glyph + gFontGlyphBank];
#endif
        gFontGlyphWritePtr->texture.r = gFontTintR;
        gFontGlyphWritePtr->texture.g = gFontTintG;
        gFontGlyphWritePtr->texture.b = gFontTintB;
        record = gFontGlyphWritePtr;
        record->x = gFontCursorX;
#ifdef VERSION_JP
        record->y = gFontCursorY;
#else
        record->y = gFontCursorY + gFontGlyphYOffsets[glyph + gFontGlyphBank];
#endif
        record->w = gFontGlyphWidth;
        record->h = gFontGlyphHeight;
        record->countAndFlags = 1;
        gFontGlyphWritePtr = record + 1;
    }

    if (gFontGlyphScale == 0) {
#ifdef VERSION_JP
        gFontCursorX += gFontGlyphWidth;
#else
        if (glyph == FONT_GLYPH_SPACE) {
            gFontCursorX += 8;
        } else {
            gFontCursorX += gFontGlyphAdvances[glyph + gFontGlyphBank];
        }
#endif
    } else if (glyph == FONT_GLYPH_SPACE) {
        gFontCursorX += (gFontGlyphScale << 4) >> 12;
    } else {
#ifdef VERSION_JP
        gFontCursorX += (gFontGlyphAdvances[glyph] * gFontGlyphScale) >> 12;
#else
        gFontCursorX += (gFontGlyphAdvances[glyph + gFontGlyphBank] * gFontGlyphScale) >> 12;
#endif
    }
    if (gFontCursorX >= 640) {
        gFontCursorX = 0;
        gFontCursorY += gFontGlyphHeight;
    }
}

unsigned int ConvertUintToBcd(unsigned int value)
{
    unsigned int divisor = 100000000;
    unsigned int packed = 0;
    value %= divisor;
    divisor /= 10;
    do {
        packed = (packed << 4) | ((value / divisor) & 15);
        value %= divisor;
        divisor /= 10;
    } while (divisor);
    return packed;
}

/* The Japanese build's HUD text sets its glyph size (#w, #h) and has no WIN caption; its slash-mode
 * kill count is shown as passed, where the US/EU builds show one less. */
void DrawContinueCountdown(int countdown)
{
#ifdef VERSION_JP
    DrawFormattedText("#X#Y#w#h#S#CCONTINUE? %1x", 157, 104, 31, 31, FONT_SCALE_2X, 0x808000, countdown);
#else
    DrawFormattedText("#X#Y#S#CCONTINUE? %1x", 157, 104, FONT_SCALE_2X, 0x808000, countdown);
#endif
}

/* Nothing calls it. */
void DrawTwoPlayerContinueHud(int countdown, int p1Wins, int p2Wins)
{
#ifdef VERSION_JP
    DrawFormattedText("#X#Y#w#h#S#C1P %1x%1x", 56, 32, 15, 15, FONT_SCALE_1X, 0x101060, p1Wins / 10, p1Wins % 10);
    DrawFormattedText("#X#Y#w#h#S#C2P %1x%1x", 464, 32, 15, 15, FONT_SCALE_1X, 0x101060, p2Wins / 10, p2Wins % 10);
    DrawFormattedText("#X#Y#w#h#S#CCONTINUE? %1x", 157, 104, 31, 31, FONT_SCALE_2X, 0x808000, countdown);
#else
    DrawFormattedText("#X#Y#S#C1P %1x%1x", 56, 32, FONT_SCALE_1X, 0x101060, p1Wins / 10, p1Wins % 10);
    DrawFormattedText("#X#Y#S#C2P %1x%1x", 464, 32, FONT_SCALE_1X, 0x101060, p2Wins / 10, p2Wins % 10);
    DrawFormattedText("#X#Y#S#CCONTINUE? %1x", 157, 104, FONT_SCALE_2X, 0x808000, countdown);
#endif
}

void DrawVersusWinCounts(int p1Wins, int p2Wins)
{
    p1Wins %= 100;
    p2Wins %= 100;
#ifdef VERSION_JP
    DrawFormattedText("#X#Y#w#h#C%1x%1x", 64, 32, 15, 15, 0x808000, p1Wins / 10, p1Wins % 10);
    DrawFormattedText("#X#Y#w#h#C%1x%1x", 544, 32, 15, 15, 0x808000, p2Wins / 10, p2Wins % 10);
#else
    DrawFormattedText("#X#Y#C%1x%1x", 64, 32, 0x808000, p1Wins / 10, p1Wins % 10);
    DrawFormattedText("#X#Y#C%1x%1x", 544, 32, 0x808000, p2Wins / 10, p2Wins % 10);
    DrawFormattedText("#X#Y#CWIN", 304, 32, 0x808000);
#endif
}

char gSlashKillsFmt3Digit[] = "%3x";
char gSlashKillsFmt2Digit[] = " %2x";
char gSlashKillsFmt1Digit[] = "  %1x";
/* 2 bytes; nothing reads or writes them. */
unsigned char gFontHudUnused0 = 0;
unsigned char gFontHudUnused1 = 0;

/* Slash-mode kill-count label sprite (drawn on JP; unused on US/EU, which draw "KILLS" as text). */
TexturedRectEntry gSlashKillsLabelSprite = {
    { 128, 128, 128, GPU_CODE_POLY_FT4, 216, 208, GET_CLUT(896, 474), 248, 208,
        GET_TPAGE(0, 0, 896, 256), 216, 224, 248, 224 },
    TEXTURED_RECT_LAST_GROUP | 1, 516, 31, 32, 16,
};

void DrawSlashModeHud(int level, int kills)
{
    level %= 100;
    kills %= 1000;
#ifdef VERSION_JP
    DrawTexturedRectList(&gSlashKillsLabelSprite, 0, 0, 0);
    if (level >= 10) {
        DrawFormattedText("#X#Y#w#h#S#CLEVEL %1x%1x", 64, 32, 15, 15, FONT_SCALE_1X, 0x808000, level / 10,
                          level % 10);
    } else {
        DrawFormattedText("#X#Y#w#h#S#CLEVEL %1x", 64, 32, 15, 15, FONT_SCALE_1X, 0x808000, level % 10);
    }
    DrawFormattedText("#X#Y#w#h#S#C", 464, 32, 15, 15, FONT_SCALE_1X, 0x808000);
#else
    kills--;
    DrawFormattedText("#X#Y#S#CKILLS", 516, 32, FONT_SCALE_1X, 0x808000);
    if (level >= 10) {
        DrawFormattedText("#X#Y#S#CLEVEL %1x%1x", 64, 32, FONT_SCALE_1X, 0x808000, level / 10, level % 10);
    } else {
        DrawFormattedText("#X#Y#S#CLEVEL %1x", 64, 32, FONT_SCALE_1X, 0x808000, level % 10);
    }
    DrawFormattedText("#X#Y#S#C", 464, 32, FONT_SCALE_1X, 0x808000); /* the next call resets the tint */
#endif
    if (kills >= 100) {
        DrawFormattedText(gSlashKillsFmt3Digit, ConvertUintToBcd(kills));
    } else if (kills >= 10) {
        DrawFormattedText(gSlashKillsFmt2Digit, ConvertUintToBcd(kills));
    } else {
        DrawFormattedText(gSlashKillsFmt1Digit, ConvertUintToBcd(kills));
    }
}

#ifndef VERSION_JP
void DrawDemoOverlayText(void)
{
    DrawFormattedText("#X#Y#S#CDEMO", 288, 104, FONT_SCALE_1X, 0x808080);
}

void DrawReplayOverlayText(void)
{
    DrawFormattedText("#X#Y#S#CReplay", 500, 32, FONT_SCALE_1X, 0xc00000);
}
#endif

#ifdef VERSION_JP
TexturedRectEntry gHudContinueBannerSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 192, GET_CLUT(896, 479), 159, 192,
        GET_TPAGE(0, 1, 960, 256), 0, 223, 159, 223 }, 1, 160, 88, 319, 63 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 192, GET_CLUT(896, 477), 175, 192,
        GET_TPAGE(0, 1, 960, 256), 160, 223, 175, 223 }, TEXTURED_RECT_LAST_GROUP | 1, 480, 88, 31, 63 },
};
TexturedRectEntry gHudChallengerBannerSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 224, GET_CLUT(896, 478), 159, 224,
        GET_TPAGE(0, 0, 960, 256), 0, 255, 159, 255 }, 1, 160, 88, 319, 63 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 192, GET_CLUT(896, 477), 175, 192,
        GET_TPAGE(0, 1, 960, 256), 160, 223, 175, 223 }, TEXTURED_RECT_LAST_GROUP | 1, 480, 88, 31, 63 },
};
TexturedRectEntry gHudChallengerSelectBannerSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 114, GET_CLUT(1008, 172), 159, 114,
        GET_TPAGE(0, 0, 960, 256), 0, 145, 159, 145 }, 1, 160, 88, 319, 63 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 192, 203, GET_CLUT(1008, 186), 207, 203,
        GET_TPAGE(0, 0, 896, 256), 192, 234, 207, 234 }, TEXTURED_RECT_LAST_GROUP | 1, 480, 88, 31, 63 },
};
TexturedRectEntry gHudSlashVictoryBannerSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(768, 304), 255, 0,
        GET_TPAGE(0, 1, 768, 256), 0, 24, 255, 24 }, 1, 64, 108, 256, 24 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(768, 304), 255, 0,
        GET_TPAGE(0, 1, 832, 256), 0, 24, 255, 24 }, TEXTURED_RECT_LAST_GROUP | 1, 320, 108, 256, 24 },
};
#else
TexturedRectEntry gHudContinueBannerSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 192, GET_CLUT(896, 479), 159, 192,
        GET_TPAGE(0, 1, 960, 256), 0, 223, 159, 223 }, 1, 160, 88, 319, 63 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 192, GET_CLUT(896, 477), 175, 192,
        GET_TPAGE(0, 1, 960, 256), 160, 223, 175, 223 }, TEXTURED_RECT_LAST_GROUP | 1, 432, 88, 31, 63 },
};
TexturedRectEntry gHudChallengerBannerSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 224, GET_CLUT(896, 478), 159, 224,
        GET_TPAGE(0, 0, 960, 256), 0, 255, 159, 255 }, 1, 160, 94, 319, 63 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 192, GET_CLUT(896, 477), 175, 192,
        GET_TPAGE(0, 1, 960, 256), 160, 223, 175, 223 }, TEXTURED_RECT_LAST_GROUP | 1, 468, 90, 31, 63 },
};
TexturedRectEntry gHudChallengerSelectBannerSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 114, GET_CLUT(1008, 172), 159, 114,
        GET_TPAGE(0, 0, 960, 256), 0, 145, 159, 145 }, 1, 160, 94, 319, 63 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 192, 203, GET_CLUT(1008, 186), 207, 203,
        GET_TPAGE(0, 0, 896, 256), 192, 234, 207, 234 }, TEXTURED_RECT_LAST_GROUP | 1, 468, 90, 31, 63 },
};
TexturedRectEntry gHudSlashVictoryBannerSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(768, 304), 255, 0,
        GET_TPAGE(0, 1, 768, 256), 0, 17, 255, 17 }, 1, 64, 108, 256, 17 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(768, 304), 255, 0,
        GET_TPAGE(0, 1, 832, 256), 0, 17, 255, 17 }, TEXTURED_RECT_LAST_GROUP | 1, 320, 108, 256, 17 },
};
#endif
TexturedRectEntry gHudSlashClearBannerSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(768, 304), 255, 0,
        GET_TPAGE(0, 1, 768, 256), 0, 48, 255, 48 }, 1, 64, 96, 256, 48 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(768, 304), 255, 0,
        GET_TPAGE(0, 1, 832, 256), 0, 48, 255, 48 }, TEXTURED_RECT_LAST_GROUP | 1, 320, 96, 256, 48 },
};
TexturedRectEntry gHudLinkLobbyBannerSprites[5] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(640, 240), 128, 0,
        GET_TPAGE(1, 1, 640, 0), 0, 240, 128, 240 }, 1, 0, 0, 128, 240 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(640, 240), 128, 0,
        GET_TPAGE(1, 1, 704, 0), 0, 240, 128, 240 }, 1, 128, 0, 128, 240 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(640, 240), 128, 0,
        GET_TPAGE(1, 1, 768, 0), 0, 240, 128, 240 }, 1, 256, 0, 128, 240 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(640, 240), 128, 0,
        GET_TPAGE(1, 1, 832, 0), 0, 240, 128, 240 }, 1, 384, 0, 128, 240 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(640, 240), 128, 0,
        GET_TPAGE(1, 1, 896, 0), 0, 240, 128, 240 }, TEXTURED_RECT_LAST_GROUP | 1, 512, 0, 128, 240 },
};

#ifdef VERSION_JP
TexturedRectEntry gNowLoadingOverlaySprites[2] = {
    { { 80, 80, 80, GPU_CODE_POLY_FT4, 80, 240, GET_CLUT(944, 495), 83, 240,
        GET_TPAGE(0, 1, 896, 256), 80, 243, 83, 243 }, 1, 0, 0, 641, 241 },
    { { 80, 80, 80, GPU_CODE_POLY_FT4, 80, 224, GET_CLUT(944, 495), 215, 224,
        GET_TPAGE(0, 1, 896, 256), 80, 255, 215, 255 }, TEXTURED_RECT_LAST_GROUP | 1, 340, 184, 271, 31 },
};
#else
TexturedRectEntry gNowLoadingOverlaySprites[2] = {
    { { 80, 80, 80, GPU_CODE_POLY_FT4, 80, 224, GET_CLUT(944, 495), 83, 224,
        GET_TPAGE(0, 1, 896, 256), 80, 227, 83, 227 }, 1, 0, 0, 641, 241 },
    { { 80, 80, 80, GPU_CODE_POLY_FT4, 80, 224, GET_CLUT(944, 495), 255, 224,
        GET_TPAGE(0, 1, 896, 256), 80, 239, 255, 239 }, TEXTURED_RECT_LAST_GROUP | 1, 372, 192, 175, 15 },
};
#endif
TexturedRectEntry *gHudBannerSprites[6] = {
    gHudContinueBannerSprites, gHudChallengerBannerSprites, gHudChallengerSelectBannerSprites,
    gHudSlashVictoryBannerSprites, gHudSlashClearBannerSprites, gHudLinkLobbyBannerSprites,
};

#define HUD_BANNER_LINK_LOBBY 5

void DrawHudBanner(int bannerIndex)
{
    DrawTexturedRectList(gHudBannerSprites[bannerIndex], 0, 0, 0);
}

#ifdef VERSION_JP
u_long gNowLoadingTim[560] = {
#include "content/now_loading_jp.inc"
};
#else
u_long gNowLoadingTim[368] = {
#include "content/now_loading.inc"
};
#endif

void ShowNowLoadingOverlay(void)
{
    RECT source;
    DR_MOVE move;
    int page;

    UploadTimToVram(gNowLoadingTim, 916, 480, 944, 495);
    DrawSync(0);
    DrawTexturedRectListImmediate(gNowLoadingOverlaySprites, 0, 0);
    source.x = 0;
    source.w = 640;
    source.h = 240;
    page = gFrameCounter & 1;
    source.y = (1 - page) * 240;
    SetDrawMove(&move, &source, 0, page * 240);
    DrawPrim(&move);
    DrawSync(0);
}

#ifndef VERSION_JP
/* Prebuilt "Pause" bitmap-font sprites (glyphs 'P', 'a', 'u', 's', 'e'). */
TexturedRectEntry gPauseOverlaySprites[5] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 232, 112, GET_CLUT(896, 480), 247, 112,
        GET_TPAGE(0, 1, 896, 256), 232, 127, 247, 127 }, 1, 290, 112, 15, 15 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 144, GET_CLUT(896, 470), 15, 144,
        GET_TPAGE(0, 1, 896, 256), 0, 156, 15, 156 }, 1, 304, 115, 15, 12 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 183, GET_CLUT(896, 470), 47, 183,
        GET_TPAGE(0, 1, 896, 256), 32, 195, 47, 195 }, 1, 315, 115, 15, 12 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 183, GET_CLUT(896, 470), 15, 183,
        GET_TPAGE(0, 1, 896, 256), 0, 195, 15, 195 }, 1, 326, 115, 15, 12 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 64, 144, GET_CLUT(896, 470), 79, 144,
        GET_TPAGE(0, 1, 896, 256), 64, 156, 79, 156 }, TEXTURED_RECT_LAST_GROUP | 1, 337, 115, 15, 12 },
};

int ShowPauseOverlay(void)
{
    RECT source;
    DR_MOVE move;
    int page;

    DrawTexturedRectListImmediate(gPauseOverlaySprites, 0, 0);
    source.x = 0;
    source.w = 640;
    source.h = 240;
    page = gFrameCounter & 1;
    source.y = (1 - page) * 240;
    SetDrawMove(&move, &source, 0, page * 240);
    DrawPrim(&move);
    return DrawSync(0);
}
#endif

#define CD_FILE_LINK_LOBBY_TIM 0x64

void LoadLinkLobbyScreen(void)
{
    void *arenaPtr;

    ResetModelArena();
    arenaPtr = GetArenaFreePtr();
    ReadCdFileSync(CD_FILE_LINK_LOBBY_TIM, arenaPtr);
    LoadTimGroup(arenaPtr, 0);
    DrawSync(0);
}

void DrawLinkLobbyScreen(void)
{
    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);
    DrawHudBanner(HUD_BANNER_LINK_LOBBY);
}
