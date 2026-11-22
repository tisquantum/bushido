#ifndef GPU_CODE_H
#define GPU_CODE_H

/* GPU primitive codes. A primitive's colour word holds the code in its top byte above R, G, B. */
#define GPU_CODE_POLY_FT3      0x24
#define GPU_CODE_POLY_F4       0x28
#define GPU_CODE_POLY_FT4      0x2c
#define GPU_CODE_POLY_GT3      0x34
#define GPU_CODE_POLY_G4       0x38
#define GPU_CODE_POLY_GT4      0x3c
#define GPU_CODE_LINE_G2       0x50
#define GPU_CODE_SPRT          0x64
#define GPU_CODE_SEMITRANS     0x02
#define GPU_CODE_RAW_TEXTURE   0x01

/* A textured primitive's palette word: the palette's VRAM position, x in 16-halfword steps. */
#define GET_CLUT(x, y)         (((y) << 6) | ((x) >> 4))
/* A textured primitive's texture page word: the page's VRAM position (x in 64-halfword steps, y 0 or 256), its
 * blend mode (0 average, 1 add, 2 subtract, 3 add a quarter) and its depth (0 4-bit, 1 8-bit, 2 15-bit). */
#define GET_TPAGE(depth, blend, x, y) ((((y) & 0x100) >> 4) | ((blend) << 5) | ((depth) << 7) | ((x) >> 6))

#define GPU_CODE_WORD(code)    ((unsigned long)(code) << 24)
#define GPU_RGB_MASK           0x00ffffffUL
#define GPU_RGB_NEUTRAL        0x00808080UL /* texture colour 128, 128, 128: the texel unchanged */

#endif
