#ifndef RENDER_SCRATCH_H
#define RENDER_SCRATCH_H

#include <libetc.h>
#include <libgpu.h>

/* Stage-tile near-clip compressed OTZ thresholds tested in RenderSceneDisplayList. */
#define STAGE_TILE_NEAR_CLIP_OTZ    0x182
#define STAGE_TILE_NO_NEAR_CLIP_OTZ 1

/* Assembly primitive-group rasterizer stored in the dispatch tables of src/otz.c. */
typedef void (*PrimRenderFn)(void);

/* The renderer's scratchpad at 0x1F800000 with the 60-entry screen buffers of the stage tile batch and the
 * ocean; the mesh drawers lay the same header with longer buffers (MeshScratch). */
typedef struct RenderScratch {
    SVECTOR *vertices;          /* +0x000: active mesh vertex buffer */
    int dispatchReturn;         /* +0x004: DispatchMeshPrimitiveGroups's return address (render.s) */
    int scaleShift;             /* +0x008: the drawers' depth shift, 3 less the node's coordinate shift */
    u_long *orderingTable;      /* +0x00C: active frame reverse ordering table (gCurrentOt) */
    int fogFarOtz;              /* +0x010: far-clip OTZ limit from GetFogFarOtzBucket() */
    int doubleBufferMask;       /* +0x014: -1 on odd frames, 0 on even frames (selects packet bank) */
    int stageTileMinOtz;        /* +0x018: minimum compressed OTZ for stage tiles (0x182 or 1) */
    PrimRenderFn *primTable;    /* +0x01C: primitive rasterizer dispatch table (gPrimRenderDispatch) */
    long screenXy[60];          /* +0x020 (0x1F800020): RotTransPers packed (sx, sy) */
    unsigned short screenZ[60]; /* +0x110 (0x1F800110): ReadSZfifo3 screen Z depths */
} RenderScratch;

#define RENDER_SCRATCH ((RenderScratch *)getScratchAddr(0))

/* The same word at 0x1F800000 seen by the routines that walk a mesh stream in place: their cursor. */
typedef struct MeshStreamScratch {
    unsigned short *cursor;
} MeshStreamScratch;

#define MESH_STREAM_SCRATCH ((MeshStreamScratch *)getScratchAddr(0))
#define MESH_CURSOR         (MESH_STREAM_SCRATCH->cursor)

#define MESH_SCRATCH_MAX_VERTICES 165

/* The same header with the 165-entry screen buffers of every mesh drawer (render_scratch.inc), of the
 * Japanese build's double-sided drawers in C and of the effect meshes. */
typedef struct MeshScratch {
    POLY_FT3 *packetCursor;                   /* +0x000: active GPU packet cursor
                                                 (the word of RenderScratch.vertices) */
    int dispatchReturn;                       /* +0x004: DispatchMeshPrimitiveGroups's return address */
    int scaleShift;                           /* +0x008: the drawers' depth shift */
    u_long *orderingTable;                    /* +0x00C: active frame reverse ordering table */
    int fogFarOtz;                            /* +0x010: far-clip OTZ limit */
    int doubleBufferMask;                     /* +0x014: packet bank selector mask */
    int stageTileMinOtz;                      /* +0x018: stage tile near-clip OTZ */
    PrimRenderFn *primTable;                  /* +0x01C: primitive dispatch table */
    long screenXy[MESH_SCRATCH_MAX_VERTICES]; /* +0x020 (0x1F800020): packed screen (sx, sy) */
    short screenZ[MESH_SCRATCH_MAX_VERTICES]; /* +0x2B4 (0x1F8002B4): screen Z depths */
} MeshScratch;

#define MESH_SCRATCH ((MeshScratch *)getScratchAddr(0))

/* From 0x1F800020 (getScratchAddr(8)) the scratchpad holds, while each runs, one effect drawer's working
 * record: StageClothRenderContext, SwayingFoliageScratch, WeatherScratch, GroundDecalScratch, RibbonDraw or
 * BillboardEffectWork. */

/* Scratchpad slots at 0x1F800020 / 0x1F800028 used by DrawBillboardQuadNode. */
#define BILLBOARD_CAMERA_OFFSET ((SVECTOR *)getScratchAddr(8))
#define BILLBOARD_ROT_MATRIX    ((MATRIX *)getScratchAddr(10))

#endif
