#ifndef FOLIAGE_H
#define FOLIAGE_H

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>


#define SWAYING_FOLIAGE_COUNT      1
#define SWAYING_FOLIAGE_VERT_COUNT 18
#define SWAYING_FOLIAGE_LINK_COUNT 37
#define SWAYING_FOLIAGE_QUAD_COUNT 8
#define SWAYING_FOLIAGE_WORK_BYTES 1920

/*
 * 2388-byte (0x954) Stage-19 crossed hanging cloth body storage (gMoatFoliageStorage),
 * pointed to by gMoatFoliageBody. points holds the 18 simulated output vertices (1/4 world
 * scale); simWork holds the 1920-byte cloth solver work buffer.
 */
typedef struct SwayingFoliageBody {
    SVECTOR verts[SWAYING_FOLIAGE_VERT_COUNT];
    int modelIndex;
    MATRIX matrix;
    VECTOR points[SWAYING_FOLIAGE_VERT_COUNT];
    char simWork[SWAYING_FOLIAGE_WORK_BYTES];
} SwayingFoliageBody;

/*
 * Initialization parameters for InitSwayingFoliageMesh (Stage-19 crossed
 * 18-vertex cloth-simulated hanging mesh).
 */
typedef struct SwayingFoliageInit {
    u_char u0, v0, u1, v1;
    u_short tpage;
    u_short clut;
    VECTOR pos[SWAYING_FOLIAGE_COUNT];
    SwayingFoliageBody *storage;
} SwayingFoliageInit;

/*
 * 40-byte Stage-19 swaying foliage texture descriptor and packed per-vertex
 * (v << 8 | u) table (gMoatFoliageTexture), populated by InitSwayingFoliageMesh.
 */
typedef struct SwayingFoliageTexture {
    u_short tpage;
    u_short clut;
    u_short uv[SWAYING_FOLIAGE_VERT_COUNT];
} SwayingFoliageTexture;

/* UpdateAndDrawSwayingFoliage's working record in the scratchpad (render_scratch.h). */
typedef struct SwayingFoliageScratch {
    MATRIX viewMatrix;       /* +0x00: camera view matrix */
    VECTOR cameraPos;        /* +0x20: camera world position (copied from viewMatrix.t) */
    POLY_FT4 *packetCursor;  /* +0x30: in/out GPU packet buffer cursor (gPacketCursor) */
    int cameraHeading;       /* +0x34: (0x800 - GetCameraHeadingAngle()) & 0xfff */
    union {
        CVECTOR color;       /* as the stage callback sets it */
        int rgb;             /* as the renderer reads it, one word */
    } tint;                  /* +0x38: RGB tint (0x808080) combined with GPU_CODE_POLY_FT4 */
    SVECTOR corners[4];      /* +0x3C: 4 camera-relative quad corner vertices for GTE */
    VECTOR offset;           /* +0x5C: mesh world origin minus cameraPos */
} SwayingFoliageScratch;

void InitSwayingFoliageMesh(SwayingFoliageInit *init);
int UpdateAndDrawSwayingFoliage(SwayingFoliageScratch *scratch);

#endif
