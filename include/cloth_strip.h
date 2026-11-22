#ifndef CLOTH_STRIP_H
#define CLOTH_STRIP_H

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#define STAGE_CLOTH_STRIP_COUNT 3

/* What InitStageClothStrip is given: the cloth texture's UV box, page and CLUT, and the three
 * world origins of Stage 9's strips. */
typedef struct StageClothConfig {
    unsigned char u0, v0, u1, v1;
    unsigned short tpage, clut;
    VECTOR origins[STAGE_CLOTH_STRIP_COUNT];
} StageClothConfig;

/* UpdateAndDrawStageClothStrips's working record in the scratchpad (render_scratch.h). */
typedef struct StageClothRenderContext {
    MATRIX matrix;       /* +0x00: camera view matrix */
    VECTOR eye;          /* +0x20: camera world position */
    POLY_GT3 *packet;    /* +0x30: GPU packet write cursor */
    int yaw;             /* +0x34: camera yaw (0x800 - heading) & 0xfff */
    SVECTOR corner[4];   /* +0x38: quad's four camera-relative vertices */
    long screen[4];      /* +0x58: projected screen XY pairs (packed s16,s16) */
    union {
        struct {
            VECTOR edgeA;
            VECTOR edgeB;
            VECTOR normal;
        } normalCalc;
        struct {
            struct {
                long vx, vy, vz;
            } offset;
            long normalSide;        /* GTE nclip sign bit (StageClothNormalSide) */
            long pad[3]; /* only the whole-context copy reads or writes them */
            long depth;
        } draw;
    } work;              /* +0x68: scratch reused by normal calc and quad drawing */
} StageClothRenderContext;

void InitStageClothStrip(StageClothConfig *config);
int UpdateAndDrawStageClothStrips(StageClothRenderContext *ctx);

#endif
