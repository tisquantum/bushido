/*
 * Texture page and CLUT relocation: moves every textured primitive of a mesh stream, a scene node's
 * mesh or a whole model slot to another texture page and palette row in place.
 */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "mesh_stream.h"
#include "model_table.h"
#include "scene_node.h"
#include "render.h"
#include "render_scratch.h"
#include "tpage_reloc.h"

/*
 * PSX GPU tpage/clut bitfields:
 *   1 texture page = 64 16-bit halfwords (X) by 256 scanlines (Y).
 *   clut X is stored in units of 16 halfwords, so 1 page = 4 clut X units.
 */
#define TPAGE_X_MASK      0x000f
#define TPAGE_Y_SHIFT     4
#define TPAGE_Y_MASK      1
#define TPAGE_XY_BITS     0x001f
#define TPAGE_ATTR_MASK   0xffe0
#define CLUT_X_MASK       0x003f
#define CLUT_Y_SHIFT      6
#define CLUT_Y_MASK       0x01ff
#define CLUT_XY_BITS      0x7fff
#define CLUT_X_PER_PAGE   4
#define CLUT_Y_PER_PAGE   256
#define CLUT_TOP_BIT_MASK 0x8000

/*
 * Common 6-byte header of compact textured polygons (MESH_STREAM_COMPACT);
 * the asm renderer also loads +0x00..+0x03 into GTE RGBC for NCS (reading code at +0x03).
 */
typedef struct CompactTexturedPolyHeader {
    short tpage;
    unsigned char pad; /* unused B byte of the GTE RGBC word; NCS reads only code */
    unsigned char code;
    short clut;
} CompactTexturedPolyHeader;

static void OffsetCompactPolyTPageClut(CompactTexturedPolyHeader *poly, short pageDx, short pageDy,
                                       short clutDx, short clutDy);

/* Nothing calls it. */
void OffsetNodeMeshTPageClut(struct SceneNode *node, short pageDx, short pageDy, short clutDx, short clutDy)
{
    short meshIndex = node->meshIndex;

    if (meshIndex != SCENE_MESH_NONE) {
        MESH_CURSOR = gModelTables[node->modelSlot][meshIndex];
        OffsetMeshStreamTPageClut(pageDx, pageDy, clutDx, clutDy);
    }
}

void OffsetModelSlotTPageClut(modelSlot, pageDx, pageDy, clutDx, clutDy)
    short modelSlot;
    short pageDx;
    short pageDy;
    short clutDx;
    short clutDy;
{
    short i;

    for (i = 0; i < gModelTableCounts[modelSlot]; i++) {
        MESH_CURSOR = gModelTables[modelSlot][i];
        OffsetMeshStreamTPageClut(pageDx, pageDy, clutDx, clutDy);
    }
}

/* Halfword stride per mesh primitive record type (44 primitive kinds; 0 = unused). */
short gPrimRecordHalfwords[44] = {
    /*  0.. 3: F3  */ 28, 22, 28, 22,
    /*  4.. 7: FT3 */ 38, 34, 38,  0,
    /*  8..11: F4  */ 32, 26,  0,  0,
    /* 12..15: FT4 */ 46, 42, 46,  0,
    /* 16..19: G3  */ 48,  0, 48,  0,
    /* 20..23: GT3 */ 52,  0, 52,  0,
    /* 24..27: G4  */ 62,  0,  0,  0,
    /* 28..31: GT4 */ 66,  0, 66,  0,
    /* 32..35      */  2,  2, 38, 46,
    /* 36..37      */  0, 40,
    /* 38..41      */ 34, 42, 42, 54,
    /* 42..43      */ 38, 46,
};

void OffsetMeshStreamTPageClut(short pageDx, short pageDy, short clutDx, short clutDy)
{
    int count, streamKind, isIndexed, listIndex;
    int primType, texturedShape;
    short skipHalfwords;

    listIndex = 0;
    do {
        count = *MESH_CURSOR++;
        if (count & MESH_HEADER_INDEXED_FLAG) {
            isIndexed = 1;
            streamKind = MESH_STREAM_STANDARD;
            MESH_CURSOR = MESH_CURSOR + (count & MESH_HEADER_COUNT_MASK) + 1;
            if ((int)MESH_CURSOR & 3)
                MESH_CURSOR = (unsigned short *)((int)MESH_CURSOR + (4 - ((int)MESH_CURSOR & 3)));
        } else {
            streamKind = *MESH_CURSOR++;
            isIndexed = 0;
            if (streamKind == MESH_STREAM_STANDARD)
                MESH_CURSOR += count * 12;
            else
                MESH_CURSOR += count * 3;
        }
        if (streamKind == MESH_STREAM_COMPACT) {
            if ((unsigned int)MESH_CURSOR & 3)
                MESH_CURSOR++;
        }
        while ((count = *MESH_CURSOR++) != 0) {
            primType = *MESH_CURSOR++;
            if ((primType & MESH_PRIM_TYPE_TEXTURED) ||
                (primType >= MESH_PRIM_SPECIAL_TEX_BASE &&
                 primType < MESH_PRIM_SPECIAL_TEX_BASE + MESH_PRIM_SPECIAL_TEX_COUNT)) {
                if (primType >= MESH_PRIM_SPECIAL_TEX_BASE &&
                    primType < MESH_PRIM_SPECIAL_TEX_BASE + MESH_PRIM_SPECIAL_TEX_COUNT)
                    texturedShape = primType - MESH_PRIM_SPECIAL_TEX_BASE;
                else
                    texturedShape = (primType >> 3) & 3;
                switch (streamKind) {
                case MESH_STREAM_STANDARD:
                    while (count--) {
                        switch (texturedShape) {
                        case MESH_TEX_SHAPE_FT3:
                            OffsetFlatTexturedTriTPageClut((POLY_FT3 *)MESH_CURSOR, pageDx, pageDy, clutDx, clutDy);
                            OffsetFlatTexturedTriTPageClut(((POLY_FT3 *)MESH_CURSOR) + 1,
                                                           pageDx, pageDy, clutDx, clutDy);
                            break;
                        case MESH_TEX_SHAPE_FT4:
                            OffsetFlatTexturedQuadTPageClut((POLY_FT4 *)MESH_CURSOR, pageDx, pageDy, clutDx, clutDy);
                            OffsetFlatTexturedQuadTPageClut(((POLY_FT4 *)MESH_CURSOR) + 1,
                                                            pageDx, pageDy, clutDx, clutDy);
                            break;
                        case MESH_TEX_SHAPE_GT3:
                            OffsetGouraudTexturedTriTPageClut((POLY_GT3 *)MESH_CURSOR, pageDx, pageDy, clutDx, clutDy);
                            OffsetGouraudTexturedTriTPageClut(((POLY_GT3 *)MESH_CURSOR) + 1,
                                                              pageDx, pageDy, clutDx, clutDy);
                            break;
                        case MESH_TEX_SHAPE_GT4:
                            OffsetGouraudTexturedQuadTPageClut((POLY_GT4 *)MESH_CURSOR, pageDx, pageDy, clutDx, clutDy);
                            OffsetGouraudTexturedQuadTPageClut(((POLY_GT4 *)MESH_CURSOR) + 1,
                                                               pageDx, pageDy, clutDx, clutDy);
                            break;
                        }
                        MESH_CURSOR += gPrimRecordHalfwords[primType];
                    }
                    break;
                case MESH_STREAM_COMPACT:
                    while (count--) {
                        OffsetCompactPolyTPageClut((CompactTexturedPolyHeader *)MESH_CURSOR,
                                                   pageDx, pageDy, clutDx, clutDy);
                        switch (texturedShape) {
                        case MESH_TEX_SHAPE_FT3:
                            MESH_CURSOR += 12;
                            break;
                        case MESH_TEX_SHAPE_FT4:
                            MESH_CURSOR += 12;
                            break;
                        case MESH_TEX_SHAPE_GT3:
                            MESH_CURSOR += 18;
                            break;
                        case MESH_TEX_SHAPE_GT4:
                            MESH_CURSOR += 22;
                            break;
                        default:
                            TriggerDebugTrap();
                            break;
                        }
                    }
                    break;
                }
            } else if (!isIndexed) {
                skipHalfwords = count * gPrimRecordHalfwords[primType];
                MESH_CURSOR += skipHalfwords;
            } else {
                switch (primType) {
                case MESH_PRIM_FLAT_TRI:
                    MESH_CURSOR += count * 26;
                    break;
                case MESH_PRIM_FLAT_QUAD:
                    MESH_CURSOR += count * 32;
                    break;
                default:
                    skipHalfwords = count * gPrimRecordHalfwords[primType];
                    MESH_CURSOR += skipHalfwords;
                    break;
                }
            }
        }
        if (isIndexed) {
            if (listIndex == MESH_INDEXED_MAX_LISTS)
                return;
            listIndex++;
            while (*MESH_CURSOR != MESH_INDEXED_LIST_END)
                MESH_CURSOR += 3;
            MESH_CURSOR++;
        }
    } while (*MESH_CURSOR++ != 0);
}

void OffsetFlatTexturedTriTPageClut(POLY_FT3 *poly, int pageDx, int pageDy, int clutDx, int clutDy)
{
    unsigned int tpage, clut;
    unsigned int pageX, pageY, clutX, clutY;

    tpage = poly->tpage;
    pageX = (pageDx + (tpage & TPAGE_X_MASK)) & TPAGE_X_MASK;
    pageY = (pageDy + ((tpage >> TPAGE_Y_SHIFT) & TPAGE_Y_MASK)) & TPAGE_Y_MASK;
    poly->tpage = pageX | ((tpage & TPAGE_ATTR_MASK) | (pageY << TPAGE_Y_SHIFT));

    clut = poly->clut;
    clutX = ((clut & CLUT_X_MASK) + clutDx * CLUT_X_PER_PAGE) & CLUT_X_MASK;
    clutY = (((clut >> CLUT_Y_SHIFT) & CLUT_Y_MASK) + clutDy * CLUT_Y_PER_PAGE) & CLUT_Y_MASK;
    poly->clut = clutX | ((clut & CLUT_TOP_BIT_MASK) | (clutY << CLUT_Y_SHIFT));
}

void OffsetFlatTexturedQuadTPageClut(POLY_FT4 *poly, int pageDx, int pageDy, int clutDx, int clutDy)
{
    unsigned int tpage, clut;
    unsigned int pageX, pageY, clutX, clutY;

    tpage = poly->tpage;
    pageX = (pageDx + (tpage & TPAGE_X_MASK)) & TPAGE_X_MASK;
    pageY = (pageDy + ((tpage >> TPAGE_Y_SHIFT) & TPAGE_Y_MASK)) & TPAGE_Y_MASK;
    poly->tpage = pageX | ((tpage & TPAGE_ATTR_MASK) | (pageY << TPAGE_Y_SHIFT));

    clut = poly->clut;
    clutX = ((clut & CLUT_X_MASK) + clutDx * CLUT_X_PER_PAGE) & CLUT_X_MASK;
    clutY = (((clut >> CLUT_Y_SHIFT) & CLUT_Y_MASK) + clutDy * CLUT_Y_PER_PAGE) & CLUT_Y_MASK;
    poly->clut = clutX | ((clut & CLUT_TOP_BIT_MASK) | (clutY << CLUT_Y_SHIFT));
}

void OffsetGouraudTexturedTriTPageClut(POLY_GT3 *poly, int pageDx, int pageDy, int clutDx, int clutDy)
{
    unsigned int tpage, clut;
    unsigned int pageX, pageY, clutX, clutY;

    tpage = poly->tpage;
    pageX = (pageDx + (tpage & TPAGE_X_MASK)) & TPAGE_X_MASK;
    pageY = (pageDy + ((tpage >> TPAGE_Y_SHIFT) & TPAGE_Y_MASK)) & TPAGE_Y_MASK;
    poly->tpage = pageX | ((tpage & TPAGE_ATTR_MASK) | (pageY << TPAGE_Y_SHIFT));

    clut = poly->clut;
    clutX = ((clut & CLUT_X_MASK) + clutDx * CLUT_X_PER_PAGE) & CLUT_X_MASK;
    clutY = (((clut >> CLUT_Y_SHIFT) & CLUT_Y_MASK) + clutDy * CLUT_Y_PER_PAGE) & CLUT_Y_MASK;
    poly->clut = clutX | ((clut & CLUT_TOP_BIT_MASK) | (clutY << CLUT_Y_SHIFT));
}

void OffsetGouraudTexturedQuadTPageClut(POLY_GT4 *poly, int pageDx, int pageDy, int clutDx, int clutDy)
{
    unsigned int tpage, clut;
    unsigned int pageX, pageY, clutX, clutY;

    tpage = poly->tpage;
    pageX = (pageDx + (tpage & TPAGE_X_MASK)) & TPAGE_X_MASK;
    pageY = (pageDy + ((tpage >> TPAGE_Y_SHIFT) & TPAGE_Y_MASK)) & TPAGE_Y_MASK;
    poly->tpage = pageX | ((tpage & TPAGE_ATTR_MASK) | (pageY << TPAGE_Y_SHIFT));

    clut = poly->clut;
    clutX = ((clut & CLUT_X_MASK) + clutDx * CLUT_X_PER_PAGE) & CLUT_X_MASK;
    clutY = (((clut >> CLUT_Y_SHIFT) & CLUT_Y_MASK) + clutDy * CLUT_Y_PER_PAGE) & CLUT_Y_MASK;
    poly->clut = clutX | ((clut & CLUT_TOP_BIT_MASK) | (clutY << CLUT_Y_SHIFT));
}

static void OffsetCompactPolyTPageClut(CompactTexturedPolyHeader *poly, short pageDx, short pageDy,
                                       short clutDx, short clutDy)
{
    poly->tpage = OffsetPackedTPage(poly->tpage, pageDx, pageDy);
    poly->clut = OffsetPackedClut(poly->clut, clutDx, clutDy);
}

short OffsetPackedTPage(short tpage, int pageDx, int pageDy)
{
    int pageX = (pageDx + (tpage & TPAGE_X_MASK)) & TPAGE_X_MASK;
    int pageY = (pageDy + (((unsigned short)tpage >> TPAGE_Y_SHIFT) & TPAGE_Y_MASK)) & TPAGE_Y_MASK;

    return pageX | ((tpage & ~TPAGE_XY_BITS) | (pageY << TPAGE_Y_SHIFT));
}

short OffsetPackedClut(short clut, int clutDx, int clutDy)
{
    int clutX = ((clut & CLUT_X_MASK) + clutDx * CLUT_X_PER_PAGE) & CLUT_X_MASK;
    int clutY = ((((unsigned short)clut >> CLUT_Y_SHIFT) & CLUT_Y_MASK) + clutDy * CLUT_Y_PER_PAGE) & CLUT_Y_MASK;

    return clutX | ((clut & ~CLUT_XY_BITS) | (clutY << CLUT_Y_SHIFT));
}
