/* Model tables: the 20 registered mesh offset tables and their relocation when the model arena
 * compacts, copying part of an archive, the GPU primitive codes the renderer stamps, and the Japanese
 * build's billboard and flat drawers and depth helpers. */
#include <sys/types.h>
#include <stdarg.h>
#include <libgte.h>
#include <libgpu.h>
#include "globals.h"
#include "main.h"
#include "mesh_stream.h"
#include "model_table.h"
#include "otz.h"
#include "packet_build.h"
#include "scene_node.h"
#include "render.h"
#include "render_scratch.h"
#include "tpage_reloc.h"
#include "display_list.h"

#ifdef VERSION_JP
/* Scales both double-buffered meshes of a pair by Q12 fixed-point factors (4096 = 1.0). Nothing calls it. */
void ScaleModelPairVertices(short slot, short pairIndex, int scaleX, int scaleY, int scaleZ)
{
    MESH_CURSOR = gModelTables[slot][pairIndex * 2];
    ScaleModelMeshVertices(scaleX, scaleY, scaleZ);
    MESH_CURSOR = gModelTables[slot][pairIndex * 2 + 1];
    ScaleModelMeshVertices(scaleX, scaleY, scaleZ);
}

/* Reached only from ScaleModelPairVertices, which nothing calls. */
void ScaleModelMeshVertices(int scaleX, int scaleY, int scaleZ)
{
    int count;
    short type;
    short skip;

    do {
        count = *(*(int **)&MESH_CURSOR)++;
        count *= 3;
        while (count--) {
            *(short *)MESH_CURSOR = (*(short *)MESH_CURSOR * scaleX) >> 12;
            MESH_CURSOR++;
            *(short *)MESH_CURSOR = (*(short *)MESH_CURSOR * scaleY) >> 12;
            MESH_CURSOR++;
            *(short *)MESH_CURSOR = (*(short *)MESH_CURSOR * scaleZ) >> 12;
            MESH_CURSOR += 2;
        }
        while ((count = *MESH_CURSOR++) != 0) {
            type = *MESH_CURSOR++;
            skip = count * gPrimRecordHalfwords[type];
            MESH_CURSOR += skip;
        }
    } while (*MESH_CURSOR++ != 0);
}
#endif

void RegisterModelTable(table, slot)
    int *table;
    short slot;
{
    int *base = table;
    int header;
    unsigned short count;
    int i;

    header = *table;
    *table++ = (header | MODEL_TABLE_RELOCATED) & 0xffff;
    gModelTables[slot] = (unsigned short **)table;
    gModelTableCounts[slot] = header & MODEL_TABLE_COUNT_MASK;
    if (!(header & MODEL_TABLE_RELOCATED)) {
        count = header;
        for (i = 0; i < count; i++) {
            *table++ += (int)base;
        }
    }
}

void RelocateModelTable(int slot, int delta)
{
    unsigned short **cursor = gModelTables[slot];
    int remaining = gModelTableCounts[slot];

    cursor += delta / 4;
    gModelTables[slot] = cursor;
    while (remaining--) {
        *cursor = (unsigned short *)((char *)*cursor + delta);
        ++cursor;
    }
}

/* CompactArchiveSubset(archive, count, index, ...): rewrites an archive of offset-addressed entries
 * in place to keep only the listed entries, in that order; returns the end of what it kept. */
void *CompactArchiveSubset(void *archive, ...)
{
    va_list args;
    int remaining;
    char *base;
    int oldCount;
    int *out;
    char *dest;

    va_start(args, archive);
    remaining = va_arg(args, int);
    base = (char *)archive;
    oldCount = *(int *)base;
    archive = (int *)archive + 1;
    *(int *)base = remaining;
    out = (int *)archive;
    dest = base + *out;

    while (--remaining != -1) {
        int index;
        int end, start;
        unsigned int length;

        index = va_arg(args, int);
        if (index >= oldCount)
            TriggerDebugTrap();

        end = ((int *)archive)[index + 1];
        start = ((int *)archive)[index];
        *out++ = dest - base;
        length = end - start;
        CopyWordsFast(base + start, dest, length);
        dest += (length / 4) * 4;
    }

    *out = dest - base;
    return dest;
}

#ifdef VERSION_JP

/* PlayStation GPU tpage semi-transparency rate bits (abr << 5). */
#define TPAGE_ABR_ADD (1 << 5)
#define TPAGE_ABR_SUB (2 << 5)

/* Nothing calls it. */
static void SetMeshTexturedPrimsSemiTrans(int mesh, int slot, int semi)
{
    int count, type;

    MESH_CURSOR = gModelTables[slot][mesh];
    /* the original has a loop here that runs once */
    do {
        count = *MESH_CURSOR++;
        count |= *MESH_CURSOR++ << 16;
        MESH_CURSOR += count * 12;
        while ((count = *MESH_CURSOR++) != 0) {
            type = *MESH_CURSOR++;
            if (type & MESH_PRIM_TYPE_TEXTURED) {
                while (count--) {
                    switch ((type >> 3) & 3) {
                    case MESH_TEX_SHAPE_FT3:
                        SetSemiTrans(&((POLY_FT3 *)MESH_CURSOR)[0], semi);
                        SetSemiTrans(&((POLY_FT3 *)MESH_CURSOR)[1], semi);
                        ((POLY_FT3 *)MESH_CURSOR)[0].tpage |= TPAGE_ABR_SUB;
                        ((POLY_FT3 *)MESH_CURSOR)[1].tpage |= TPAGE_ABR_SUB;
                        break;
                    case MESH_TEX_SHAPE_FT4:
                        SetSemiTrans(&((POLY_FT4 *)MESH_CURSOR)[0], semi);
                        SetSemiTrans(&((POLY_FT4 *)MESH_CURSOR)[1], semi);
                        if (mesh % 2 == 1) {
                            ((POLY_FT4 *)MESH_CURSOR)[0].tpage |= TPAGE_ABR_ADD;
                            ((POLY_FT4 *)MESH_CURSOR)[1].tpage |= TPAGE_ABR_ADD;
                        } else {
                            ((POLY_FT4 *)MESH_CURSOR)[0].tpage |= TPAGE_ABR_SUB;
                            ((POLY_FT4 *)MESH_CURSOR)[1].tpage |= TPAGE_ABR_SUB;
                        }
                        break;
                    case MESH_TEX_SHAPE_GT3:
                        break;
                    }
                    MESH_CURSOR += gPrimRecordHalfwords[type];
                }
            } else {
                while (count--)
                    MESH_CURSOR += gPrimRecordHalfwords[type];
            }
        }
    } while (0);
}
#endif

void ClearModelSlotCounts(void)
{
    int i;

    for (i = MODEL_TABLE_COUNT - 1; i >= 0; i--) {
        gModelTableCounts[i] = 0;
    }
}

void InitGpuPrimCodeCache(void)
{
    POLY_F3 f3;
    POLY_G3 g3;
    POLY_FT3 ft3;
    POLY_GT3 gt3;
    POLY_F4 f4;
    POLY_G4 g4;
    POLY_FT4 ft4;
    POLY_GT4 gt4;
    LINE_F2 line;

    SetPolyF3(&f3);
    SetPolyG3(&g3);
    SetPolyFT3(&ft3);
    SetPolyGT3(&gt3);
    SetPolyF4(&f4);
    SetPolyG4(&g4);
    SetPolyFT4(&ft4);
    SetPolyGT4(&gt4);
    SetLineF2(&line);

    gPolyF3Code = f3.code;
    gPolyG3Code = g3.code;
    gPolyFT3Code = ft3.code;
    gPolyGT3Code = gt3.code;
    gPolyF4Code = f4.code;
    gPolyG4Code = g4.code;
    gPolyFT4Code = ft4.code;
    gPolyGT4Code = gt4.code;
    gLineF2Code = line.code;
}

#ifdef VERSION_JP
/* Nothing calls the routines from here to the #endif. */

#define BILLBOARD_MAX_VIEW_DEPTH 0x4000

/* First primitive group after a mesh stream's vertex triples: one textured quad + 4 packed vertex indices. */
typedef struct {
    unsigned short primCount;
    unsigned short primType;
    POLY_FT4 poly;
    unsigned int vertexIndices;
} BillboardQuadGroup;

/* What the three flat drawers below read, which nothing calls: up to four corners in view space and a
 * colour, from byte 0x74 on, where a SceneNode's param begins. */
typedef struct {
    unsigned char unread[0x74]; /* nothing reads them */
    SVECTOR corners[4];
    CVECTOR color;
} FlatPrimNode;

/* Draws a node's flat triangle (its first three corners) semi-transparent when it faces the
 * camera and lies in depth range. */
static void DrawFlatTriangleNode(FlatPrimNode *tri)
{
    SVECTOR corners[4];
    long p;
    long otz;
    long flag;

    corners[0] = tri->corners[0];
    corners[1] = tri->corners[1];
    corners[2] = tri->corners[2];
    corners[3] = tri->corners[3];
    if (RotAverageNclip3(&corners[0], &corners[1], &corners[2],
                         (long *)&((POLY_F3 *)gPacketCursor)->x0, (long *)&((POLY_F3 *)gPacketCursor)->x1,
                         (long *)&((POLY_F3 *)gPacketCursor)->x2,
                         &p, &otz, &flag) > 0) {
        *(CVECTOR *)&((POLY_F3 *)gPacketCursor)->r0 = tri->color;
        if (otz > 0 && otz < BILLBOARD_MAX_VIEW_DEPTH) {
            ((POLY_F3 *)gPacketCursor)->tag = 0x04000000;
            ((POLY_F3 *)gPacketCursor)->code = gPolyF3Code;
            SetSemiTrans(gPacketCursor, 1);
            AddPrim(&RENDER_SCRATCH->orderingTable[CompressDepthToOtz(otz, 0)], ((POLY_F3 *)gPacketCursor)++);
        }
    }
}

/* Draws a node's flat quad semi-transparent when it faces the camera and lies in depth range. */
static void DrawFlatQuadNode(FlatPrimNode *quad)
{
    SVECTOR corners[4];
    long p;
    long otz;
    long flag;

    corners[0] = quad->corners[0];
    corners[1] = quad->corners[1];
    corners[2] = quad->corners[2];
    corners[3] = quad->corners[3];
    if (RotAverageNclip4(&corners[0], &corners[1], &corners[2], &corners[3],
                         (long *)&((POLY_F4 *)gPacketCursor)->x2, (long *)&((POLY_F4 *)gPacketCursor)->x0,
                         (long *)&((POLY_F4 *)gPacketCursor)->x1, (long *)&((POLY_F4 *)gPacketCursor)->x3,
                         &p, &otz, &flag) > 0) {
        *(CVECTOR *)&((POLY_F4 *)gPacketCursor)->r0 = quad->color;
        otz <<= 2;
        if (otz > 0 && otz < BILLBOARD_MAX_VIEW_DEPTH) {
            ((POLY_F4 *)gPacketCursor)->tag = 0x05000000;
            ((POLY_F4 *)gPacketCursor)->code = gPolyF4Code;
            SetSemiTrans(gPacketCursor, 1);
            AddPrim(&RENDER_SCRATCH->orderingTable[CompressDepthToOtz(otz, 0)], ((POLY_F4 *)gPacketCursor)++);
        }
    }
}

/* Draws a node's flat line between its first two corners at the head of the ordering table. */
static void DrawFlatLineNode(FlatPrimNode *line)
{
    SVECTOR corners[2];
    long p;
    long flag;

    corners[0] = line->corners[0];
    corners[1] = line->corners[1];
    SetLineF2(gPacketCursor);
    RotTransPers(&corners[0], (long *)&((LINE_F2 *)gPacketCursor)->x0, &p, &flag);
    RotTransPers(&corners[1], (long *)&((LINE_F2 *)gPacketCursor)->x1, &p, &flag);
    *(CVECTOR *)&((LINE_F2 *)gPacketCursor)->r0 = line->color;
    ((LINE_F2 *)gPacketCursor)->code = gLineF2Code;
    AddPrim(RENDER_SCRATCH->orderingTable, ((LINE_F2 *)gPacketCursor)++);
}

static void DrawBillboardQuadNode(struct SceneNode *node)
{
    SVECTOR unused; /* not used */
    struct SceneNode *camera;
    int dx;
    int dy;
    int dz;
    int absX;
    int extent;
    int depth; /* reused, as in the original */
    short scaleShift;
    int *meshStream;
    SVECTOR *vertices;
    BillboardQuadGroup *quadGroup;
    POLY_FT4 *poly;
    unsigned int vertexIndices;
    long p;
    long flag;

    camera = gCameraNodePtr;
    dx = node->worldMatrix.t[0] - camera->worldMatrix.t[0];
    dy = node->worldMatrix.t[1] - camera->worldMatrix.t[1];
    dz = node->worldMatrix.t[2] - camera->worldMatrix.t[2];

    absX = dx >= 0 ? dx : -dx;
    if (dy >= 0) {
        extent = absX | dy;
    } else {
        extent = absX | -dy;
    }
    if (dz >= 0) {
        depth = extent | dz;
    } else {
        depth = extent | -dz;
    }

    scaleShift = GetDepthScaleShift(depth);
    BILLBOARD_CAMERA_OFFSET->vx = dx >> scaleShift;
    BILLBOARD_CAMERA_OFFSET->vy = dy >> scaleShift;
    BILLBOARD_CAMERA_OFFSET->vz = dz >> scaleShift;
    ApplyMatrix(&gCombinedViewMatrix, BILLBOARD_CAMERA_OFFSET, (VECTOR *)gCombinedViewMatrix.t);
    gCombinedViewMatrix.t[1] >>= 1; /* 640x240 aspect ratio Y-halving */
    depth = gCombinedViewMatrix.t[2];
    if (depth > 0 && depth < BILLBOARD_MAX_VIEW_DEPTH) {
        BILLBOARD_ROT_MATRIX->m[0][0] = rcos(node->rotation.vz) >> scaleShift;
        BILLBOARD_ROT_MATRIX->m[0][1] = -rsin(node->rotation.vz) >> scaleShift;
        BILLBOARD_ROT_MATRIX->m[1][0] = -BILLBOARD_ROT_MATRIX->m[0][1] >> 1;
        BILLBOARD_ROT_MATRIX->m[0][2] = BILLBOARD_ROT_MATRIX->m[1][2] =
            BILLBOARD_ROT_MATRIX->m[2][0] = BILLBOARD_ROT_MATRIX->m[2][1] = 0;
        BILLBOARD_ROT_MATRIX->m[2][2] = ONE >> scaleShift;
        BILLBOARD_ROT_MATRIX->m[1][1] = BILLBOARD_ROT_MATRIX->m[0][0] >> 1;
        SetTransMatrix(&gCombinedViewMatrix);
        SetRotMatrix(BILLBOARD_ROT_MATRIX);

        meshStream = (int *)gModelTables[node->modelSlot][(node->meshIndex << 1) + (gFrameCounter & 1)];
        vertices = (SVECTOR *)(meshStream + 1);
        RENDER_SCRATCH->vertices = vertices;
        dx = *meshStream; /* reused, as in the original */
        quadGroup = (BillboardQuadGroup *)(vertices + dx * 3);
        vertexIndices = quadGroup->vertexIndices;
        poly = &quadGroup->poly;
        RotTransPers4(
            &vertices[vertexIndices & 0xff],
            &vertices[(vertexIndices >> 8) & 0xff],
            &vertices[(vertexIndices >> 16) & 0xff],
            &vertices[vertexIndices >> 24],
            (long *)&quadGroup->poly.x0,
            (long *)&quadGroup->poly.x1,
            (long *)&quadGroup->poly.x2,
            (long *)&quadGroup->poly.x3,
            &p,
            &flag
        );
        AddPrim(&RENDER_SCRATCH->orderingTable[CompressDepthToOtz(depth, scaleShift)], poly);
    }
}

static int MinTriOtz(int sz0, int sz1, int sz2)
{
    if (sz0 < sz1) {
        if (sz0 < sz2)
            return sz0 >> 2;
        return sz2 >> 2;
    }
    if (sz1 < sz2)
        return sz1 >> 2;
    return sz2 >> 2;
}

static int MinQuadOtz(int sz0, int sz1, int sz2, int sz3)
{
    if (sz1 < sz0)
        sz0 = sz1;
    if (sz2 < sz0)
        sz0 = sz2;
    if (sz3 < sz0)
        sz0 = sz3;
    return sz0 >> 2;
}

static int MaxTriOtz(int sz0, int sz1, int sz2)
{
    if (sz0 < sz1)
        sz0 = sz1;
    if (sz0 < sz2)
        sz0 = sz2;
    return sz0 >> 2;
}

static int MaxQuadOtz(int sz0, int sz1, int sz2, int sz3)
{
    if (sz0 < sz1)
        sz0 = sz1;
    if (sz0 < sz2)
        sz0 = sz2;
    if (sz0 < sz3)
        sz0 = sz3;
    return sz0 >> 2;
}

static long AverageScreenZ3(long sz0, long sz1, long sz2)
{
    return AverageZ3(sz0, sz1, sz2) << 2;
}

static long AverageScreenZ4(long sz0, long sz1, long sz2, long sz3)
{
    return AverageZ4(sz0, sz1, sz2, sz3) << 2;
}

static long MinScreenZ3(long sz0, long sz1, long sz2)
{
    if (sz0 < sz1) {
        if (sz0 < sz2)
            return sz0;
        return sz2;
    }
    if (sz1 < sz2)
        return sz1;
    return sz2;
}

static long MinScreenZ4(long sz0, long sz1, long sz2, long sz3)
{
    if (sz1 < sz0)
        sz0 = sz1;
    if (sz2 < sz0)
        sz0 = sz2;
    if (sz3 < sz0)
        sz0 = sz3;
    return sz0;
}

static long MaxScreenZ3(long sz0, long sz1, long sz2)
{
    if (sz0 < sz1)
        sz0 = sz1;
    if (sz0 < sz2)
        sz0 = sz2;
    return sz0;
}

static long MaxScreenZ4(long sz0, long sz1, long sz2, long sz3)
{
    if (sz0 < sz1)
        sz0 = sz1;
    if (sz0 < sz2)
        sz0 = sz2;
    if (sz0 < sz3)
        sz0 = sz3;
    return sz0;
}
#endif
