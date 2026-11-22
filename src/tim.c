/* TIM textures: uploads TIM images and their CLUTs from group archives to VRAM (grey when the
 * renderer is monochrome), reads a TIM's texture page and CLUT, and tints CLUTs in VRAM. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "globals.h"
#include "tim.h"

/* A TIM image: its id word, its flags (pixel mode, and whether a CLUT block follows), then the
 * CLUT block and the pixel block, each led by its length, position and size. */
enum { TIM_ID = 0x10, TIM_MODE_MASK = 7, TIM_HAS_CLUT = 8 };

typedef struct TextureInfo {
    short mode;
    short x, y, width, height;
    short clutX, clutY, clutWidth, clutHeight;
    unsigned short tpage, clut;
    unsigned long *pixels;
    unsigned long *palette;
} TextureInfo;

/* TIM group archive layout: leading table of byte offsets to each group; each group holds a
 * 32-bit entry count followed by 12-byte entries { timOffset, x, y, clutX, clutY }. */
void LoadTimGroup(cursor, groupIndex)
    unsigned short *cursor;
    short groupIndex;
{
    TextureInfo info; /* not used */
    unsigned long *base;
    unsigned long groupOffset;
    unsigned long count;

    base = (unsigned long *)cursor;
    cursor = (unsigned short *)((unsigned long *)cursor + groupIndex);
    groupOffset = *(unsigned long *)cursor;
    cursor = (unsigned short *)(base + groupOffset / sizeof(*base));
    count = *(unsigned long *)cursor;
    cursor += 2;

    if (count != 0) {
        while (count-- > 0) {
            unsigned long timOffset;
            unsigned long *tim;
            unsigned short x, y, clutX, clutY;

            timOffset = *(unsigned long *)cursor;
            cursor += 2;
            tim = base + timOffset / sizeof(*base);
            x = *cursor++;
            y = *cursor++;
            clutX = *cursor++;
            clutY = *cursor++;
            UploadTimToVram(tim, (short)x, (short)y, (short)clutX, (short)clutY);
        }
    }
}

void LoadTimGroupWithOffset(const void *cursor, int groupIndex, int xOffset, int yOffset)
{
    TextureInfo info; /* not used */
    const unsigned long *base = cursor;
    unsigned int count;
    unsigned int groupOffset;

    cursor = (const unsigned long *)cursor + (short)groupIndex;
    groupOffset = *(unsigned long *)cursor;
    cursor = base + groupOffset / sizeof(*base);
    count = *(const unsigned int *)cursor;
    cursor = (const unsigned short *)cursor + 2;

    if (count != 0) {
        while (count-- > 0) {
            unsigned int timOffset;
            unsigned long *tim;
            unsigned short x, y, clutX, clutY;

            timOffset = *(const unsigned int *)cursor;
            cursor = (const unsigned short *)cursor + 2;
            tim = (unsigned long *)(base + timOffset / sizeof(*base));
            x = *(const unsigned short *)cursor;
            cursor = (const unsigned short *)cursor + 1;
            y = *(const unsigned short *)cursor;
            cursor = (const unsigned short *)cursor + 1;
            clutX = *(const unsigned short *)cursor;
            cursor = (const unsigned short *)cursor + 1;
            clutY = *(const unsigned short *)cursor;
            cursor = (const unsigned short *)cursor + 1;
            UploadTimToVram(tim,
                            (short)x + (short)xOffset, (short)y + (short)yOffset,
                            (short)clutX + (short)xOffset, (short)clutY + (short)yOffset);
        }
    }
}

void LoadTimGroupWithSeparateOffsets(const void *cursor, int groupIndex, int xOffset, int yOffset,
                                     int clutXOffset, int clutYOffset)
{
    TextureInfo info; /* not used */
    const unsigned long *base = cursor;
    unsigned int count;
    unsigned int groupOffset;

    cursor = (const unsigned long *)cursor + (short)groupIndex;
    groupOffset = *(unsigned long *)cursor;
    cursor = base + groupOffset / sizeof(*base);
    count = *(const unsigned int *)cursor;
    cursor = (const unsigned short *)cursor + 2;

    if (count != 0) {
        while (count-- > 0) {
            unsigned int timOffset;
            unsigned long *tim;
            unsigned short x, y, clutX, clutY;

            timOffset = *(const unsigned int *)cursor;
            cursor = (const unsigned short *)cursor + 2;
            tim = (unsigned long *)(base + timOffset / sizeof(*base));
            x = *(const unsigned short *)cursor;
            cursor = (const unsigned short *)cursor + 1;
            y = *(const unsigned short *)cursor;
            cursor = (const unsigned short *)cursor + 1;
            clutX = *(const unsigned short *)cursor;
            cursor = (const unsigned short *)cursor + 1;
            clutY = *(const unsigned short *)cursor;
            cursor = (const unsigned short *)cursor + 1;
            UploadTimToVram(tim,
                            (short)x + (short)xOffset, (short)y + (short)yOffset,
                            (short)clutX + (short)clutXOffset,
                            (short)clutY + (short)clutYOffset);
        }
    }
}

void UploadTimToVram(tim, x, y, clutX, clutY)
    u_long *tim;
    int x;
    int y;
    short clutX;
    unsigned short clutY;
{
    u_long *clut;
    unsigned int magic;
    unsigned int hasClut;
    u_long dimensions;
    RECT rect;
    u_long converted[256];

    magic = *(u_char *)tim;
    ++tim;
    if (magic != TIM_ID)
        return;

    hasClut = *tim & TIM_HAS_CLUT;
    ++tim;
    if (hasClut) {
        clut = tim;
        tim += *clut / sizeof(*clut);
        ++clut;
    }

    tim += 2;
    rect.x = x;
    rect.y = y;
    dimensions = *tim;
    ++tim;
    rect.h = dimensions >> 16;
    rect.w = dimensions;
    LoadImage(&rect, tim);

    if (!hasClut)
        return;

    ++clut;
    rect.x = clutX;
    rect.y = clutY;
    dimensions = *clut;
    ++clut;
    rect.h = dimensions >> 16;
    rect.w = dimensions;
    if (IsRendererMonochrome() != 0) {
        ConvertClutToMonochrome((const unsigned short *)clut, rect.w, (unsigned short *)converted);
        LoadImage(&rect, converted);
        DrawSync(0);
    } else {
        LoadImage(&rect, clut);
    }
}

void LoadTimClutGroupWithOffset(const void *cursor, int groupIndex, int xOffset, int yOffset)
{
    const unsigned short *list;
    const unsigned char *base;
    unsigned int count;

    list = (const unsigned short *)((const unsigned char *)cursor + ((int *)cursor)[(short)groupIndex]);
    base = (const unsigned char *)cursor;
    cursor = list;
    count = *(const unsigned int *)cursor;
    cursor = (const unsigned short *)cursor + 2;

    if (count != 0) {
        while (count-- > 0) {
            int timOffset;
            unsigned short clutX;
            unsigned short clutY;

            timOffset = *(const int *)cursor;
            cursor = (const unsigned short *)cursor + 4;
            clutX = *(const unsigned short *)cursor;
            cursor = (const unsigned short *)cursor + 1;
            clutY = *(const unsigned short *)cursor;
            cursor = (const unsigned short *)cursor + 1;
            UploadTimClutToVram((unsigned long *)(base + timOffset), (short)clutX + (short)xOffset,
                                (short)clutY + (short)yOffset);
        }
    }
}

void UploadTimClutToVram(unsigned long *tim, int x, int y)
{
    RECT rect;
    unsigned long clut[256];
    unsigned long dimensions;

    if (*(unsigned char *)tim == TIM_ID) {
        tim++;
        if (*tim++ & TIM_HAS_CLUT) {
            tim += 2;

            rect.x = x;
            rect.y = y;
            dimensions = *tim++;
            rect.h = dimensions >> 16;
            rect.w = dimensions;

            if (IsRendererMonochrome()) {
                ConvertClutToMonochrome((const unsigned short *)tim, rect.w, (unsigned short *)clut);
                LoadImage(&rect, clut);
            } else {
                LoadImage(&rect, tim);
            }
        }
    }
}

static void ReadTimTextureInfo(unsigned long *tim, TextureInfo *out,
                               short x, short y, short clutX, short clutY);

/* Nothing calls it. */
static int GetTimGroupEntryInfo(void *cursor, int groupIndex, unsigned int entryIndex, TextureInfo *out)
{
    unsigned int base = (unsigned int)cursor;
    unsigned int count, timOffset;
    unsigned short x, y, clutX, clutY;
    int result;

    cursor = (void *)(((unsigned int *)cursor)[groupIndex] + base);
    count = *(unsigned int *)cursor;
    cursor = (unsigned char *)cursor + 4;

    if (entryIndex < count) {
        cursor = (unsigned char *)cursor + entryIndex * 12;
        timOffset = *(unsigned int *)cursor;
        cursor = (unsigned char *)cursor + 4;
        x = *(unsigned short *)cursor;
        cursor = (unsigned char *)cursor + 2;
        y = *(unsigned short *)cursor;
        cursor = (unsigned char *)cursor + 2;
        clutX = *(unsigned short *)cursor;
        cursor = (unsigned char *)cursor + 2;
        clutY = *(unsigned short *)cursor;

        ReadTimTextureInfo((unsigned long *)(timOffset + base), out, (short)x, (short)y, clutX, clutY);
        result = count;
    } else {
        result = -1;
    }
    return result;
}

/* Reached only from GetTimGroupEntryInfo, which nothing calls. */
static void ReadTimTextureInfo(unsigned long *tim, TextureInfo *out, short x, short y,
                        short clutX, short clutY)
{
    unsigned long flags, width;
    unsigned short height;
    unsigned long *block;

    if (*(unsigned char *)tim++ == TIM_ID) {
        flags = *tim++;
        out->mode = flags & TIM_MODE_MASK;
        if (flags & TIM_HAS_CLUT) {
            unsigned long size;

            block = tim;
            size = *block;
            out->clutX = clutX;
            out->clutY = clutY;
            tim = block + size / sizeof(*block);
            block += 2;
            out->clutHeight = ((unsigned short *)block)[1];
            width = *block++;
            out->palette = block;
            out->clutWidth = width;
            out->clut = GetClut(out->clutX, out->clutY);
        } else {
            out->clut = 0;
        }

        block = tim;
        block += 2;
        out->x = x;
        out->y = y;
        height = ((unsigned short *)block)[1];
        out->height = height;
        width = *block++;
        out->pixels = block;
        out->width = width;
        out->tpage = GetTPage(out->mode, 0, out->x & 0xffc0, out->y & 0xff00);
    }
}

int IsRendererMonochrome(void)
{
    return gRendererMonochrome;
}

void SetRendererMonochrome(int monochrome)
{
    if (monochrome != 0) {
        gRendererMonochrome = 1;
    } else {
        gRendererMonochrome = 0;
    }
}

/* Q12 luminance weights: 1351/4096 R + 2048/4096 G + 696/4096 B (~0.330 R + 0.500 G + 0.170 B). */
void ConvertClutToMonochrome(const unsigned short *source, int count, unsigned short *dest)
{
    int pixel, level, green, blue;
    short color;

    while (count--) {
        color = pixel = *source++;
        level = pixel & 0x1f;
        level *= 1351;
        blue = (color >> 10) & 0x1f;
        green = ((color >> 5) & 0x1f) << 11;
        blue *= 696;
        level = ((level + green + blue) >> 12) & 0x1f;

        pixel &= ~0x7fff;
        pixel += level << 10;
        pixel += level << 5;
        pixel += level;
        *dest++ = pixel;
    }
}

int ConvertRgbToLuminance(int red, int green, int blue)
{
    red *= 1351;
    green <<= 11;
    blue *= 696;

    return (red + green + blue) >> 12;
}

void TintVramClut(int mode, int sourceX, int sourceY, int count, int rScale, int gScale, int bScale, int destX,
                  int destY)
{
    u_short source[256];
    u_short dest[256];
    RECT rect;
    u_short *destPtr;
    u_short *sourcePtr;
    int i;
    int r;
    int g;
    int b;
    int stp;

    DrawSync(0);
    rect.x = sourceX;
    rect.y = sourceY;
    rect.w = count;
    rect.h = 1;
    StoreImage(&rect, (u_long *)source);
    DrawSync(0);

    sourcePtr = source;
    destPtr = dest;
    for (i = 0; i < count; i++) {
        if (*sourcePtr == 0) {
            *destPtr++ = *sourcePtr++;
        } else {
            r = (*sourcePtr & 0x1f) << 3;
            g = ((*sourcePtr >> 5) & 0x1f) << 3;
            b = ((*sourcePtr >> 10) & 0x1f) << 3;
            stp = *sourcePtr++ & 0x8000;

            switch (mode) {
            case CLUT_TINT_MODE_RGB:
                r = (r * rScale) >> 15;
                g = (g * gScale) >> 15;
                b = (b * bScale) >> 15;
                break;
            case CLUT_TINT_MODE_LUMINANCE:
                r *= 1351;
                g <<= 11;
                b *= 696;
                r = (r + g + b) >> 15;
                r = (r * rScale) >> 12;
                g = (r * gScale) >> 12; /* g and b scale the tinted r, not the luminance */
                b = (r * bScale) >> 12;
                break;
            }

            r &= 0x1f;
            g &= 0x1f;
            b &= 0x1f;
            *destPtr++ = stp | r | (g << 5) | (b << 10);
        }
    }

    rect.x = destX;
    rect.y = destY;
    LoadImage(&rect, (u_long *)dest);
    DrawSync(0);
}

void ScaleVramClutInPlace(int vramX, int vramY, int colorCount, int redScale, int greenScale, int blueScale)
{
    TintVramClut(CLUT_TINT_MODE_RGB, vramX, vramY, colorCount, redScale, greenScale, blueScale, vramX, vramY);
}
