#ifndef RIBBON_TRAIL_H
#define RIBBON_TRAIL_H

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#define RIBBON_TRAIL_HISTORY_LEN      8
#define RIBBON_TRAIL_HISTORY_MASK     (RIBBON_TRAIL_HISTORY_LEN - 1)
#define RIBBON_TRAIL_VISIBLE_SEGMENTS 6
#define RIBBON_TRAIL_LOD_BANDS        4
#define RIBBON_TRAIL_UV_STEPS         17
#define RIBBON_TRAIL_UV_MAX_INDEX     (RIBBON_TRAIL_UV_STEPS - 1)

/* One sampled position pair of a ribbon trail: the hilt and the tip. */
typedef struct RibbonSample {
    VECTOR first;  /* hilt */
    VECTOR second; /* tip */
} RibbonSample;

/* RibbonTrail.flags bitmask. */
typedef enum RibbonTrailFlag {
    RIBBON_TRAIL_FLAG_INIT = 1,  /* needs initial ring-buffer fill */
    RIBBON_TRAIL_FLAG_PAUSE = 2  /* pauses sample insertion */
} RibbonTrailFlag;

/* Opening-demo ribbon trail: a ring buffer of the last eight samples, drawn as a textured strip. */
typedef struct RibbonTrail {
    u_short flags;                      /* RibbonTrailFlag bitmask */
    short head;                         /* ring-buffer head 0..7 */
    RibbonSample samples[RIBBON_TRAIL_HISTORY_LEN];
    u_short uvs[RIBBON_TRAIL_UV_STEPS]; /* packed UVs interpolated from u0..v1 */
    u_char u0, v0, u1, v1;              /* source texture bounds */
    u_short tpage;
    u_short clut;
} RibbonTrail;

/* UpdateAndBuildRibbonPackets's working record in the scratchpad (render_scratch.h), filled by
 * UpdateAndDrawRibbonTrail. */
typedef struct RibbonDraw {
    MATRIX matrix;       /* camera view rotation matrix */
    VECTOR origin;       /* camera world origin */
    POLY_GT4 *packets;   /* GPU packet write cursor */
    RibbonTrail *ribbon; /* active RibbonTrail slot */
    RibbonSample latest; /* current frame hilt/tip endpoints */
    SVECTOR corners[4];  /* quad's four camera-relative vertices */
    VECTOR firstDelta;   /* GTE temp (first segment delta / LoadAverage12 vertex output) */
    VECTOR secondDelta;  /* GTE temp (second segment delta) */
    VECTOR blend;        /* GTE temp (cosine-weighted blend / squared vector) */
} RibbonDraw;

void InitRibbonTrailUvs(RibbonTrail *trail);
int UpdateAndBuildRibbonPackets(RibbonDraw *work);

#endif
