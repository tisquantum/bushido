#ifndef MESH_STREAM_H
#define MESH_STREAM_H

/* A mesh stream, as model archives hold it and the renderer (render.s) walks it: a header word,
 * the vertices, then groups of primitives (count, type, records) until a zero count. */
#define MESH_HEADER_INDEXED_FLAG    0x8000
#define MESH_HEADER_COUNT_MASK      0x7fff
#define MESH_STREAM_STANDARD        0
#define MESH_STREAM_COMPACT         1
#define MESH_INDEXED_LIST_END       0xffff
#define MESH_INDEXED_MAX_LISTS      12 /* 13 bone sub-lists (0..12) on skinned fighter meshes */

#define MESH_PRIM_TYPE_TEXTURED     0x04
#define MESH_PRIM_SPECIAL_TEX_BASE  38 /* special textured types 38..41 (FT3, FT4, GT3, GT4) */
#define MESH_PRIM_SPECIAL_TEX_COUNT 4
#define MESH_PRIM_FLAT_TRI          0
#define MESH_PRIM_FLAT_QUAD         8
#define MESH_PRIM_DRAWMODE_F3       36 /* unused flat tri with DR_MODE */
#define MESH_PRIM_DRAWMODE_F4       37 /* flat quad with DR_MODE (DrawPolyF4WithDrawMode) */

typedef enum MeshTexShape {
    MESH_TEX_SHAPE_FT3 = 0,
    MESH_TEX_SHAPE_FT4 = 1,
    MESH_TEX_SHAPE_GT3 = 2,
    MESH_TEX_SHAPE_GT4 = 3
} MeshTexShape;

#endif
