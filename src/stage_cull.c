/* Stage grid and culling: parses a stage's layout into tiles, props and a 32x32 cell grid; each
 * frame queues the cells the camera heading's visibility mask lets through; runs the stage callbacks. */
#include <sys/types.h>
#include <libgte.h>
#include <inline_o.h>
#include <libgpu.h>
#include "scene_node.h"
#include "light.h"
#include "stage_cull.h"
#include "stage_callbacks.h"
#include "gpu_code.h"
#include "asset_load.h"
#include "display_list.h"
#include "effect.h"
#include "model_arena.h"
#include "rot_matrix.h"
#include "stage_id.h"
#include "globals.h"
#include "render.h"

#define STAGE_CELL_WORDS_PER_ROW     4
#define STAGE_CELL_GRID_WORDS        128
#define STAGE_CELL_NIBBLE_MASK       15
#define STAGE_CELL_LOD_STAMP_DIM     16
#define STAGE_CELL_LIST_MAX          625
#define STAGE_CELL_LIST_END          0x8000
#define STAGE_CELL_INDEX_MASK        0x7fff
#define STAGE_LAYOUT_END             (-1)
#define STAGE_TILE_CONDITIONAL_FLAG  0x8000
#define STAGE_TILE_HIDE_Z_MIN        0x3200
#define STAGE_VIS_WINDOW_DIM         31
#define STAGE_VIS_HEADING_SHIFT      6
#define STAGE_VIS_HEADING_MASK       63
#define STAGE_VIS_PRESET_COUNT       2
#define STAGE_DEFAULT_SHADE          0x80
#define STAGE_ZONE_HEADER_FLAG       0x8000
#define STAGE_ZONE_ID_MASK           0x7fff
#define STAGE_ZONE_HEADER(id)        ((short)(STAGE_ZONE_HEADER_FLAG | (id)))
#define STAGE_ZONE_NONE              (-1)
#define STAGE_ZONE_END               (-2)
#define CD_FILE_VISIBILITY_MASK_BASE 0x61 /* DATA3\O16.dat (normal), DATA3\O17.dat (POV) */

typedef struct StageCallbackEntry {
    void (*init)(void);
    void (*postCull)(void);
    void (*frameBegin)(void);
} StageCallbackEntry;

int gVisibilityMaskPreset = 0;
int gPeakStageCellListCount = 0;
static int gStageModelSlot;
static int gStageTileCount;
static SceneTile gStageTiles[430];
static struct SceneNode gStageObjects[60];
static struct SceneNode *gStagePendingObjects[60];
static short gStageCellHeads[STAGE_GRID_DIM][STAGE_GRID_DIM];
static u_short gStageCellLists[STAGE_CELL_LIST_MAX + 1]; /* index lists, each ended by 0x8000 */
static int gStageCellLodGrid[STAGE_CELL_GRID_WORDS]; /* a 4-bit value per cell */
static VECTOR gCameraStageCell; /* vx column, vz row; nothing reads it */
static short gStageUnreadPairs[4][2]; /* stage loading sets each [k][0] to -1; nothing reads them */

/* Row k is the 31x31 cell mask for heading k * 64, a word per row with column 0 in the top bit; row
 * 64 repeats row 0. DATA3\O16.dat and O17.dat (first person) replace it. */
int gStageVisibilityMask[65][32] = {
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0001e000, 0x0001f000, 0x0003f000, 0x0003f800, 0x0007f800, 0x0007fc00, 0x0007fc00, 0x000ffe00,
        0x000ffe00, 0x001fff00, 0x001fff00, 0x001fff80, 0x003fff80, 0x003fff80, 0x003ffe00, 0x0001c000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000e000,
        0x0001e000, 0x0001f000, 0x0001f000, 0x0003f800, 0x0003fc00, 0x0003fc00, 0x0007fe00, 0x0007ff00,
        0x0007ff00, 0x0007ff80, 0x000fffc0, 0x000fffc0, 0x000fffc0, 0x000fff80, 0x000ffe00, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000e000,
        0x0001e000, 0x0001f000, 0x0001f800, 0x0001fc00, 0x0001fe00, 0x0003fe00, 0x0003ff00, 0x0003ff80,
        0x0003ffc0, 0x0003ffe0, 0x0007ffe0, 0x0007ffe0, 0x0007ffc0, 0x0007ff80, 0x0007fe00, 0x0000c000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000e000,
        0x0001f000, 0x0001f800, 0x0001fc00, 0x0001fe00, 0x0001ff00, 0x0001ff80, 0x0001ff80, 0x0001ffc0,
        0x0001ffe0, 0x0001ffe0, 0x0001ffe0, 0x0003ffe0, 0x0003ffc0, 0x0003ff80, 0x0003fe00, 0x0001c000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000e000,
        0x0000f000, 0x0000f800, 0x0000fc00, 0x0000ff00, 0x0000ff80, 0x0000ffc0, 0x0000ffe0, 0x0000fff0,
        0x0000fff0, 0x0000fff0, 0x0000fff0, 0x0000ffe0, 0x0000ffc0, 0x0000ff80, 0x0000fe00, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000e000,
        0x0000f000, 0x0000fc00, 0x0000fe00, 0x0000ff80, 0x0000ffc0, 0x0000fff0, 0x0000fff8, 0x0000fff8,
        0x0000fff8, 0x0000fff8, 0x0000fff0, 0x00007fe0, 0x00007fc0, 0x00007f80, 0x00007e00, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000e000,
        0x0000f800, 0x0000fe00, 0x0000ff80, 0x0000ffc0, 0x0000fff0, 0x0000fffc, 0x00007ffc, 0x00007ffc,
        0x00007ffc, 0x00007ff8, 0x00007ff0, 0x00003fe0, 0x00003fc0, 0x00003f80, 0x00003e00, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000f000,
        0x0000fc00, 0x0000ff00, 0x0000ffe0, 0x0000fff8, 0x0000fffe, 0x00007ffe, 0x00007ffe, 0x00007ffc,
        0x00003ffc, 0x00003ff8, 0x00003ff0, 0x00001fe0, 0x00001fc0, 0x00001f80, 0x00000e00, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000f000,
        0x0000fe00, 0x0000ffe0, 0x0000fffc, 0x0000fffe, 0x00007ffe, 0x00007ffe, 0x00003ffe, 0x00003ffc,
        0x00003ffc, 0x00001ff8, 0x00001ff0, 0x00000fe0, 0x00000fc0, 0x00000780, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000fc00,
        0x0000ffc0, 0x0000fffe, 0x0000fffe, 0x00007ffe, 0x00007ffe, 0x00003ffe, 0x00003ffe, 0x00001ffc,
        0x00001ffc, 0x00000ff8, 0x00000ff0, 0x000007e0, 0x000003c0, 0x00000380, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000ff80,
        0x0000fffe, 0x0000fffe, 0x0000fffe, 0x00007ffe, 0x00003ffe, 0x00003ffe, 0x00001ffe, 0x00000ffc,
        0x00000ffc, 0x000007f8, 0x000003f0, 0x000003e0, 0x000001c0, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000fffe,
        0x0000ffff, 0x0000ffff, 0x0000fffe, 0x00007ffe, 0x00003ffe, 0x00001ffe, 0x00000ffe, 0x000007fc,
        0x000007fc, 0x000003f8, 0x000001f0, 0x000000e0, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00007ffe, 0x0000ffff,
        0x0000ffff, 0x0000ffff, 0x00007ffe, 0x00003ffe, 0x00001ffe, 0x00000ffe, 0x000007fe, 0x000003fc,
        0x000001fc, 0x000000f8, 0x00000070, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000001e, 0x000003fe, 0x0000fffe, 0x0000ffff,
        0x0000ffff, 0x0000ffff, 0x00007ffe, 0x00003ffe, 0x00001ffe, 0x00000ffe, 0x000003fe, 0x000001fc,
        0x000000fc, 0x00000078, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x0000003e, 0x000001fe, 0x00001ffe, 0x0000fffe, 0x0000fffe,
        0x0000ffff, 0x0000ffff, 0x00007ffe, 0x00001ffe, 0x00000ffe, 0x000003fe, 0x000001fe, 0x0000007c,
        0x0000003c, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x0000003e, 0x000001fe, 0x000007fe, 0x00003ffe, 0x0000fffe, 0x0000fffe,
        0x0000fffe, 0x0000fffe, 0x00007ffe, 0x00001ffe, 0x000007fe, 0x000001fe, 0x0000007e, 0x0000001c,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0000001c, 0x0000007c, 0x000001fe, 0x000007fe, 0x00001ffe, 0x00007ffe, 0x0000fffe, 0x0000ffff,
        0x0000ffff, 0x0000ffff, 0x00003ffe, 0x00000ffe, 0x000001fe, 0x0000007e, 0x0000000e, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000038,
        0x0000007c, 0x000001fc, 0x000003fe, 0x00000ffe, 0x00001ffe, 0x00007ffe, 0x0001fffe, 0x0001fffe,
        0x0001fffe, 0x0000fffe, 0x00001ffe, 0x000003fe, 0x0000003e, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000070, 0x000000f8,
        0x000001fc, 0x000003fc, 0x00000ffe, 0x00001ffe, 0x00003ffe, 0x00007ffe, 0x0001fffe, 0x0001ffff,
        0x0001ffff, 0x0000fffe, 0x000007fe, 0x0000003e, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000000f0, 0x000001f8,
        0x000007fc, 0x00000ffc, 0x00001ffe, 0x00003ffe, 0x00007ffe, 0x0000fffe, 0x0001fffe, 0x0001ffff,
        0x0001ffff, 0x0000ffff, 0x0000001e, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000001e0, 0x000003f0, 0x000007f8,
        0x00000ffc, 0x00001ffc, 0x00001ffe, 0x00003ffe, 0x00007ffe, 0x0000fffe, 0x0001fffe, 0x0001fffe,
        0x0001fffe, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000003c0, 0x000007e0, 0x000007f0, 0x00000ff8,
        0x00001ffc, 0x00001ffc, 0x00003ffe, 0x00007ffe, 0x00007ffe, 0x0000fffe, 0x0001fffe, 0x0001fffe,
        0x0001ffe0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000780, 0x000007c0, 0x00000fe0, 0x00000ff0, 0x00001ff8,
        0x00003ffc, 0x00003ffc, 0x00007ffe, 0x00007ffe, 0x0000fffe, 0x0000fffe, 0x0001fffe, 0x0001ffe0,
        0x0001fc00, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000e00, 0x00000f80, 0x00001fc0, 0x00001fe0, 0x00003ff0, 0x00003ff8,
        0x00003ffc, 0x00007ffc, 0x00007ffe, 0x0000fffe, 0x0000fffe, 0x0001fffc, 0x0001ffe0, 0x0001ff00,
        0x0001f800, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00001e00, 0x00003f80, 0x00003fc0, 0x00003fe0, 0x00007ff0, 0x00007ff8,
        0x00007ffc, 0x00007ffc, 0x0000fffc, 0x0000fffc, 0x0000fff8, 0x0001ffe0, 0x0001ff80, 0x0001fc00,
        0x0001f000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00007e00, 0x00007f80, 0x00007fc0, 0x00007fe0, 0x00007ff0, 0x0000fff8,
        0x0000fffc, 0x0000fffc, 0x0000fffc, 0x0001fff0, 0x0001ffe0, 0x0001ff80, 0x0001fe00, 0x0001f800,
        0x0001e000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x0000fe00, 0x0000ff80, 0x0000ffc0, 0x0000ffe0, 0x0000fff0, 0x0000fff8,
        0x0001fff8, 0x0001fff8, 0x0001fff0, 0x0001ffc0, 0x0001ff80, 0x0001fe00, 0x0001fc00, 0x0001f000,
        0x0001e000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x0000c000, 0x0001fe00, 0x0001ff80, 0x0001ffc0, 0x0001ffe0, 0x0001fff0, 0x0001fff0,
        0x0001fff0, 0x0001ffe0, 0x0001ffc0, 0x0001ff80, 0x0001fe00, 0x0001fc00, 0x0001f800, 0x0001f000,
        0x0001e000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x0001c000, 0x0003fe00, 0x0003ff80, 0x0003ffc0, 0x0003ffe0, 0x0003ffe0, 0x0003ffe0,
        0x0003ffc0, 0x0003ff80, 0x0003ff00, 0x0003fe00, 0x0003fc00, 0x0003f800, 0x0003f000, 0x0003e000,
        0x0001c000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x0001c000, 0x000ffe00, 0x000fff80, 0x000fffc0, 0x000fffc0, 0x0007ffc0, 0x0007ffc0,
        0x0007ff80, 0x0007ff00, 0x0007fe00, 0x0003fc00, 0x0003fc00, 0x0003f800, 0x0003f000, 0x0003e000,
        0x0003c000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x0000c000, 0x001ffe00, 0x001fff80, 0x001fff80, 0x001fff80, 0x001fff80, 0x000fff00,
        0x000ffe00, 0x000ffe00, 0x0007fc00, 0x0007f800, 0x0007f800, 0x0007f000, 0x0003e000, 0x0003e000,
        0x0003c000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x003ffe00, 0x003fff00, 0x003fff00, 0x003fff00, 0x003ffe00, 0x001ffe00,
        0x001ffc00, 0x001ffc00, 0x000ff800, 0x000ff800, 0x0007f000, 0x0007f000, 0x0007e000, 0x0003e000,
        0x0003c000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x0001c000, 0x003ffe00, 0x00fffe00, 0x00fffe00, 0x00fffc00, 0x007ffc00, 0x007ffc00,
        0x003ff800, 0x003ff800, 0x001ff000, 0x001ff000, 0x000ff000, 0x000fe000, 0x0007e000, 0x0007c000,
        0x0003c000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x003ff800, 0x00fff800, 0x01fff800, 0x01fff800, 0x01fff800, 0x00fff000,
        0x007ff000, 0x007ff000, 0x003ff000, 0x001fe000, 0x001fe000, 0x000fe000, 0x0007c000, 0x0007c000,
        0x0003c000, 0x00038000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00018000, 0x003ff000, 0x00fff000, 0x01fff000, 0x03fff000, 0x03fff000, 0x03ffe000,
        0x01ffe000, 0x00ffe000, 0x007fe000, 0x003fe000, 0x003fc000, 0x001fc000, 0x000fc000, 0x0007c000,
        0x0003c000, 0x00038000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x0001c000, 0x003fe000, 0x00ffe000, 0x01ffe000, 0x03ffe000, 0x03ffc000, 0x03ffc000,
        0x03ffc000, 0x01ffc000, 0x00ffc000, 0x00ffc000, 0x007fc000, 0x003fc000, 0x001fc000, 0x000fc000,
        0x0007c000, 0x00038000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x003f8000, 0x00ff8000, 0x01ff8000, 0x03ff8000, 0x07ff8000, 0x07ff8000,
        0x07ff8000, 0x07ff8000, 0x03ff8000, 0x01ff8000, 0x00ff8000, 0x007f8000, 0x001f8000, 0x000f8000,
        0x00078000, 0x00038000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x003f0000, 0x00ff0000, 0x01ff0000, 0x03ff0000, 0x07ff8000, 0x0fff8000,
        0x0fff8000, 0x0fff8000, 0x0fff8000, 0x07ff8000, 0x01ff8000, 0x00ff8000, 0x003f8000, 0x001f8000,
        0x00078000, 0x00038000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x003e0000, 0x00fe0000, 0x01fe0000, 0x03fe0000, 0x07ff0000, 0x0fff0000,
        0x1fff0000, 0x1fff0000, 0x1fff0000, 0x1fff8000, 0x07ff8000, 0x01ff8000, 0x00ff8000, 0x003f8000,
        0x000f8000, 0x00038000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00380000, 0x00fc0000, 0x01fc0000, 0x03fc0000, 0x07fe0000, 0x0ffe0000,
        0x1ffe0000, 0x1fff0000, 0x3fff0000, 0x3fff0000, 0x3fff8000, 0x0fff8000, 0x03ff8000, 0x007f8000,
        0x001f8000, 0x00078000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00f00000, 0x01f80000, 0x03f80000, 0x07fc0000, 0x0ffc0000,
        0x1ffe0000, 0x1ffe0000, 0x3ffe0000, 0x3fff0000, 0x3fff0000, 0x3fff8000, 0x1fff8000, 0x03ff8000,
        0x003f8000, 0x00078000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00e00000, 0x01e00000, 0x03f00000, 0x07f80000, 0x0ff80000,
        0x1ffc0000, 0x1ffc0000, 0x3ffe0000, 0x3ffe0000, 0x3fff0000, 0x3fff0000, 0x3fff8000, 0x3fff8000,
        0x01ff8000, 0x001f8000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x01c00000, 0x03e00000, 0x07e00000, 0x0ff00000,
        0x1ff80000, 0x1ff80000, 0x3ffc0000, 0x3ffe0000, 0x3ffe0000, 0x3fff0000, 0x3fff8000, 0x3fff8000,
        0x3fff8000, 0x00ff8000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03800000, 0x07c00000, 0x0fe00000,
        0x1ff00000, 0x1ff00000, 0x3ff80000, 0x3ffc0000, 0x3ffe0000, 0x3fff0000, 0x3fff8000, 0x7fff8000,
        0x7fff8000, 0x3fff8000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x07000000, 0x0f800000,
        0x1fc00000, 0x1fe00000, 0x3ff00000, 0x3ff80000, 0x3ffc0000, 0x3ffe0000, 0x3fff0000, 0x7fff8000,
        0x7fff8000, 0x7fff8000, 0x3fff0000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0f000000,
        0x1f800000, 0x1fc00000, 0x3fe00000, 0x3ff80000, 0x3ffc0000, 0x3ffe0000, 0x3fff0000, 0x7fff8000,
        0x7fff8000, 0x7fff8000, 0x3fff8000, 0x3fe00000, 0x3c000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x1e000000, 0x1f000000, 0x3fc00000, 0x3fe00000, 0x3ff80000, 0x3ffc0000, 0x3fff0000, 0x7fff8000,
        0x7fff8000, 0x3fff8000, 0x3fff8000, 0x3ffc0000, 0x3fc00000, 0x3e000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x1c000000, 0x3f000000, 0x3fc00000, 0x3ff00000, 0x3ffc0000, 0x3fff0000, 0x3fff8000,
        0x3fff8000, 0x3fff8000, 0x3fff8000, 0x3ffe0000, 0x3ff00000, 0x3fc00000, 0x3e000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x38000000, 0x3f000000, 0x3fc00000, 0x3ff80000, 0x3ffe0000, 0x7fff8000,
        0x7fff8000, 0x7fff8000, 0x3fff8000, 0x3fff0000, 0x3ffc0000, 0x3ff00000, 0x3fc00000, 0x1f000000,
        0x1c000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3e000000, 0x3fe00000, 0x3ffc0000, 0x3fff8000,
        0x3fffc000, 0x3fffc000, 0x3fffc000, 0x3fff0000, 0x3ffc0000, 0x3ff80000, 0x3fe00000, 0x1fc00000,
        0x1f000000, 0x0e000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3e000000, 0x3ff00000, 0x3fff8000,
        0x7fffc000, 0x7fffc000, 0x3fffc000, 0x3fff0000, 0x3ffe0000, 0x3ffc0000, 0x3ff80000, 0x1fe00000,
        0x1fc00000, 0x0f800000, 0x07000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3c000000, 0x7fff8000,
        0x7fffc000, 0x7fffc000, 0x3fffc000, 0x3fff8000, 0x3fff0000, 0x3ffe0000, 0x3ffc0000, 0x1ff80000,
        0x1ff00000, 0x0fc00000, 0x07800000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x3fffc000, 0x3fffc000, 0x3fffc000, 0x3fff8000, 0x3fff0000, 0x3ffe0000, 0x3ffc0000, 0x1ffc0000,
        0x1ff80000, 0x0ff00000, 0x07e00000, 0x03c00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x03ffc000, 0x3fffc000, 0x3fffc000, 0x3fff8000, 0x3fff0000, 0x3fff0000, 0x3ffe0000, 0x1ffc0000,
        0x1ffc0000, 0x0ff80000, 0x07f00000, 0x03f00000, 0x01e00000, 0x00000000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x001fc000, 0x03ffc000, 0x3fffc000, 0x3fff8000, 0x3fff8000, 0x3fff0000, 0x3fff0000, 0x1ffe0000,
        0x1ffe0000, 0x0ffc0000, 0x07f80000, 0x03f80000, 0x01f00000, 0x00f00000, 0x00000000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x000fc000, 0x007fc000, 0x03ffc000, 0x1fffc000, 0x3fff8000, 0x3fff8000, 0x3fff0000, 0x1fff0000,
        0x1ffe0000, 0x0ffe0000, 0x07fe0000, 0x03fc0000, 0x01fc0000, 0x00f80000, 0x00380000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0007c000, 0x001fc000, 0x00ffc000, 0x03ffc000, 0x0fff8000, 0x1fff8000, 0x1fff8000, 0x1fff0000,
        0x1fff0000, 0x0fff0000, 0x07ff0000, 0x03fe0000, 0x01fe0000, 0x00fe0000, 0x003c0000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0003c000, 0x000fc000, 0x003fc000, 0x00ffc000, 0x03ffc000, 0x07ffc000, 0x1fff8000, 0x1fff8000,
        0x1fff8000, 0x0fff8000, 0x07ff0000, 0x03ff0000, 0x01ff0000, 0x00ff0000, 0x003f0000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0003c000, 0x0007c000, 0x001fc000, 0x003fc000, 0x00ffc000, 0x01ffc000, 0x07ffc000, 0x0fffc000,
        0x0fffc000, 0x0fff8000, 0x07ff8000, 0x03ff8000, 0x01ff8000, 0x00ff8000, 0x003f8000, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0003c000, 0x0007c000, 0x000fc000, 0x001fc000, 0x003fc000, 0x00ffc000, 0x01ffc000, 0x03ffc000,
        0x07ffc000, 0x07ffc000, 0x07ffc000, 0x03ffc000, 0x01ffc000, 0x00ffc000, 0x003fc000, 0x00018000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0001c000, 0x0003e000, 0x0007e000, 0x000fe000, 0x001fe000, 0x003fe000, 0x007fe000, 0x00ffe000,
        0x01ffe000, 0x03ffe000, 0x03ffe000, 0x03ffe000, 0x01ffe000, 0x00ffe000, 0x003fe000, 0x0001c000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0001e000, 0x0003e000, 0x0007e000, 0x000fe000, 0x001fe000, 0x001fe000, 0x003ff000, 0x007ff000,
        0x00fff000, 0x01fff000, 0x01fff000, 0x01fff800, 0x01fff800, 0x00fff800, 0x003ff800, 0x0001c000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0001e000, 0x0003e000, 0x0003e000, 0x0007f000, 0x000ff000, 0x000ff000, 0x001ff000, 0x003ff800,
        0x003ff800, 0x007ff800, 0x00fffc00, 0x00fffc00, 0x00fffc00, 0x00fffc00, 0x003ffc00, 0x00018000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0001e000, 0x0003e000, 0x0003f000, 0x0007f000, 0x0007f000, 0x000ff800, 0x000ff800, 0x001ffc00,
        0x001ffc00, 0x003ffc00, 0x003ffe00, 0x007ffe00, 0x007ffe00, 0x007ffe00, 0x003ffe00, 0x00000000,
    },
    {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x0001e000, 0x0001f000, 0x0003f000, 0x0003f800, 0x0007f800, 0x0007fc00, 0x0007fc00, 0x000ffe00,
        0x000ffe00, 0x001fff00, 0x001fff00, 0x001fff80, 0x003fff80, 0x003fff80, 0x003ffe00, 0x0001c000,
    },
};

StageCallbackEntry gStageCallbacks[STAGE_COUNT] = {
    { InitStageSnowAndDecals, UpdateStageSnowAndDecals, 0 },
    { InitStage1Waterfall, UpdateStage1Waterfall, 0 },
    { InitStageSnowAndDecals, UpdateStageSnowAndDecals, 0 },
    { InitStage3SnowAndDecals, UpdateStage3PropsAndSnow, 0 },
    { InitStage4SnowDecalsAndGlows, UpdateStage4SnowAndDecals, 0 },
    { InitStage5SnowAndDecals, UpdateStage5SnowAndDecals, 0 },
    { InitStageSnowAndDecals, UpdateStageSnowAndDecals, 0 },
    { InitStageSnowAndDecals, UpdateStageSnowAndDecals, 0 },
    { InitStageSnowAndDecals, UpdateStageSnowAndDecals, 0 },
    { InitStage9PropsAndCloth, UpdateStage9ClothStrips, 0 },
    { InitStage10SnowAndDecals, UpdateStage10SnowAndVramAnim, 0 },
    { InitStage11Props, UpdateStage11PropsAndLight, 0 },
    { InitCoveShoreline, UpdateCoveShoreline, 0 },
    { 0, 0, 0 },
    { InitBambooThicketNoOp, UpdateBambooThicketCuttableProps, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { InitYaguraPointProps, UpdateYaguraPointNightProps, 0 },
    { 0, 0, 0 },
    { InitMoatRainAndFoliage, UpdateMoatRainAndFoliage, BeginMoatFrame },
    { InitStage20NoOp, UpdateStage20NoOp, 0 },
    { 0, 0, 0 },
    { InitStage22SnowAndDecals, UpdateStage22SnowAndVramAnim, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { InitGrottoStream, UpdateGrottoStream, 0 },
};

static int gCameraHeadingAngle;
static void RunStagePreCullCallback(void);

void CullAndEnqueueStageScene(void)
{
    MATRIX rotation;
    SVECTOR forward;
    VECTOR heading;
    struct SceneNode *camera;
    short baseColumn;
    short baseRow;
    short angle;
    int *maskRow;
    struct SceneNode **pending;
    int i;
    int j;
    short row;
    short column;
    int mask;
    short link;
    short *links;
    int cellNibble;
    u_short entry;
    int index;

    camera = gCameraNodePtr;
    baseColumn = (camera->localMatrix.t[0] + STAGE_GRID_ORIGIN_OFFSET) / STAGE_CELL_SIZE;
    baseRow = (camera->localMatrix.t[2] + STAGE_GRID_ORIGIN_OFFSET) / STAGE_CELL_SIZE;
    gCameraStageCell.vx = baseColumn;
    gCameraStageCell.vz = baseRow;
    ratan2(camera->localMatrix.t[0], camera->localMatrix.t[2]); /* result not used */
    RunStagePreCullCallback();
    BuildRotMatrixYXZ(&gCameraNodePtr->rotation, &rotation);
    forward.vz = ONE;
    forward.vx = 0;
    forward.vy = 0;
    gte_SetRotMatrix(&rotation);
    gte_ldv0(&forward);
    gte_rtv0();
    gte_stlvnl(&heading);
    angle = ratan2(heading.vx, heading.vz);
    maskRow = &gStageVisibilityMask[(angle >> STAGE_VIS_HEADING_SHIFT) & STAGE_VIS_HEADING_MASK][1];
    pending = gStagePendingObjects;
    gCameraHeadingAngle = angle;
    baseColumn -= STAGE_GRID_HALF_DIM;
    baseRow -= STAGE_GRID_HALF_DIM - 1;
    for (i = 0; i < STAGE_VIS_WINDOW_DIM; i++) {
        row = baseRow + i;
        if (row < 0) {
            maskRow++;
            continue;
        }
        if (row >= STAGE_GRID_DIM)
            break;
        mask = *maskRow++;
        if (mask == 0)
            continue;
        for (j = 0; j < STAGE_VIS_WINDOW_DIM; j++) {
            column = baseColumn + j;
            if (column < 0) {
                mask <<= 1;
                continue;
            }
            if (column >= STAGE_GRID_DIM)
                break;
            if (mask < 0) {
                links = gStageCellHeads[row];
                link = links[column];
                if (link >= 0) {
                    cellNibble = (gStageCellLodGrid[row * STAGE_CELL_WORDS_PER_ROW + column / 8] >>
                                  ((7 - column % 8) * 4)) & STAGE_CELL_NIBBLE_MASK;
                    do {
                        entry = gStageCellLists[link++];
                        index = entry & STAGE_CELL_INDEX_MASK;
                        if (index < gStageTileCount) {
                            SceneTile *tile = &gStageTiles[index];

                            tile->lodLevel = cellNibble & CELL_LOD_MASK;
                            if (!((u_short)tile->renderType & SCENE_TILE_HIDDEN)) {
                                if ((tile->flags & SCENE_TILE_FLAG_CONDITIONAL) && (cellNibble & CELL_FLAG_OCCLUDED)) {
                                    if (cellNibble & CELL_FLAG_SEMITRANS) {
                                        tile->flags |= SCENE_TILE_FLAG_SEMI_TRANS;
                                        *gRenderQueueCursor++ = (struct SceneNode *)tile;
                                    }
                                } else {
                                    if ((cellNibble & (CELL_FLAG_SEMITRANS | CELL_FLAG_OCCLUDED)) ==
                                        CELL_FLAG_SEMITRANS) {
                                        tile->flags |= SCENE_TILE_FLAG_SEMI_TRANS;
                                    } else {
                                        tile->flags &= ~SCENE_TILE_FLAG_SEMI_TRANS;
                                    }
                                    *gRenderQueueCursor++ = (struct SceneNode *)tile;
                                }
                            }
                        } else {
                            struct SceneNode *object = &gStageObjects[index - gStageTileCount];

                            if (object->param.queued == 0) {
                                *pending++ = object;
                                object->param.queued = 1;
                            }
                        }
                    } while (!(entry & STAGE_CELL_LIST_END));
                }
            }
            mask <<= 1;
        }
    }
    while (pending != gStagePendingObjects) {
        pending--;
        *gRenderQueueCursor = *pending;
        (*gRenderQueueCursor)->param.queued = 0;
        gRenderQueueCursor++;
    }
    if (gStageCallbacks[GetLoadedStageId()].postCull != 0) {
        gStageCallbacks[GetLoadedStageId()].postCull();
    }
}

void ParseStageSceneLayout(u_short *layoutData, int modelSlot)
{
    short count = 0;
    u_short entry;
    int rotZ;
    int clearColumn;
    int linkCount;
    int peakLinkCount;

    gStageModelSlot = modelSlot;
    while ((short)(entry = *layoutData++) != STAGE_LAYOUT_END) {
        SceneTile *tile = &gStageTiles[count++];
        u_short meshWord = *layoutData++;
        short column;
        short row;

        tile->meshIndex = meshWord;
        if (meshWord & STAGE_TILE_CONDITIONAL_FLAG) {
            tile->meshIndex = meshWord & STAGE_CELL_INDEX_MASK;
            tile->flags = SCENE_TILE_FLAG_CONDITIONAL;
        } else {
            tile->flags = 0;
        }
        tile->lodLevel = CELL_LOD_COARSE;
        tile->renderType = SCENE_NODE_TILE;
        tile->modelSlot = gStageModelSlot;
        column = (short)entry % STAGE_GRID_DIM;
        tile->worldX = column * STAGE_CELL_SIZE - STAGE_GRID_ORIGIN_OFFSET;
        row = (short)entry / STAGE_GRID_DIM;
        tile->worldZ = row * STAGE_CELL_SIZE - STAGE_GRID_ORIGIN_OFFSET;
    }
    gStageTileCount = count;

    count = 0;
    while ((short)(entry = *layoutData++) != STAGE_LAYOUT_END) {
        struct SceneNode *object = &gStageObjects[count++];

        object->parent = 0;
        object->rotOrder = SCENE_ROT_ORDER_FAST;
        object->lightSlot = LIGHT_SLOT_STAGE;
        object->meshIndex = entry;
        object->modelSlot = gStageModelSlot;
        object->worldMatrix.t[0] = *layoutData++;
        object->worldMatrix.t[0] |= (u_int)*layoutData++ << 16;
        object->worldMatrix.t[1] = *layoutData++;
        object->worldMatrix.t[1] |= (u_int)*layoutData++ << 16;
        object->worldMatrix.t[2] = *layoutData++;
        object->worldMatrix.t[2] |= (u_int)*layoutData++ << 16;
        object->rotation.vx = *layoutData++;
        object->rotation.vy = *layoutData++;
        rotZ = (short)(object->rotation.vz = *layoutData++);
        if (object->rotation.vx == object->rotation.vy) {
            if (rotZ != 1)
                object->renderType = SCENE_NODE_WORLD;
            else
                object->renderType = SCENE_NODE_STANDARD;
        } else {
            if (rotZ != 0)
                object->renderType = SCENE_NODE_WORLD;
            else
                object->renderType = SCENE_NODE_STANDARD;
        }
        gRotMatrixBuilders[object->rotOrder](&object->rotation, &object->worldMatrix);
        object->param.queued = 0;
    }
    for (linkCount = 0; linkCount < STAGE_GRID_DIM; ++linkCount) {
        short *row = gStageCellHeads[linkCount];
        for (clearColumn = STAGE_GRID_DIM - 1; clearColumn >= 0; --clearColumn) {
            row[clearColumn] = STAGE_LAYOUT_END;
        }
    }
    linkCount = 0;
    while ((short)(entry = *layoutData++) != STAGE_LAYOUT_END) {
        short column = (short)entry % STAGE_GRID_DIM;
        int row = (short)entry / STAGE_GRID_DIM;
        short *gridRow = gStageCellHeads[row];

        gridRow[column] = linkCount;
        do {
            entry = *layoutData++;
            gStageCellLists[linkCount] = entry;
            ++linkCount;
        } while (!(entry & STAGE_CELL_LIST_END));
    }
    peakLinkCount = gPeakStageCellListCount;
    if (peakLinkCount < linkCount) {
        peakLinkCount = linkCount;
    }
    gPeakStageCellListCount = peakLinkCount;
    if (peakLinkCount >= STAGE_CELL_LIST_MAX) {
        TriggerDebugTrap();
    }
    ClearEffectPool();
    if (gStageCallbacks[GetLoadedStageId()].init != 0) {
        gStageCallbacks[GetLoadedStageId()].init();
    }
    gStageLitColor.b = gStageLitColor.g = gStageLitColor.r = STAGE_DEFAULT_SHADE;
    gStageLitColor.cd = GPU_CODE_POLY_FT4;
    gAuxViewAngles.vx = 0;
    gAuxViewAngles.vy = 0;
    gAuxViewAngles.vz = 0;
    gStageUnreadPairs[3][0] = -1;
    gStageUnreadPairs[2][0] = -1;
    gStageUnreadPairs[1][0] = -1;
    gStageUnreadPairs[0][0] = -1;
}

void RunSceneCallback(void)
{
    if (gStageCallbacks[GetLoadedStageId()].frameBegin != 0) {
        gStageCallbacks[GetLoadedStageId()].frameBegin();
    }
}

void ApplyLightSlotToGte(short lightSlot, short scale)
{
    SVECTOR upNormal;

    upNormal.vx = 0;
    upNormal.vy = -ONE;
    upNormal.vz = 0;
    SetLightMatrix(&gLightSlots[lightSlot].lightMatrix);
    SetColorMatrix(&gLightSlots[lightSlot].colorMatrix);
    SetBackColor(gLightSlots[lightSlot].ambientRgb[0],
                 gLightSlots[lightSlot].ambientRgb[1],
                 gLightSlots[lightSlot].ambientRgb[2]);
    NormalColor(&upNormal, &gStageLitColor);
    gStageLitColor.cd = GPU_CODE_POLY_FT4;
    gStageLitColor.r = (gStageLitColor.r * scale) >> 12;
    gStageLitColor.g = (gStageLitColor.g * scale) >> 12;
    gStageLitColor.b = (gStageLitColor.b * scale) >> 12;
}

int GetCameraHeadingAngle(void)
{
    return gCameraHeadingAngle;
}

struct SceneNode *GetStageObjectsBase(void)
{
    return gStageObjects;
}

void LoadVisibilityMaskPreset(int preset)
{
    if ((u_int)preset < STAGE_VIS_PRESET_COUNT && preset != gVisibilityMaskPreset) {
        gVisibilityMaskPreset = preset;
        ReadCdFileSync(preset + CD_FILE_VISIBILITY_MASK_BASE, gStageVisibilityMask);
    }
}

int GetVisibilityMaskPreset(void)
{
    return gVisibilityMaskPreset;
}

void HideStageTilesBeyondZ(void)
{
    int i;
    int count = gStageTileCount;

    for (i = 0; i < count; i++) {
        if (gStageTiles[i].worldZ >= STAGE_TILE_HIDE_Z_MIN) {
            gStageTiles[i].renderType = (short)((u_short)gStageTiles[i].renderType | SCENE_TILE_HIDDEN);
        }
    }
}

/* 16x16 cells around the camera, two bits each, first cell in the top bits (0 coarse, 1 medium,
 * 2 fine); words 16 to 31 are not read. */
int gCameraCellLodStamp[32] = {
    0x00000000, 0x00000000, 0x00155500, 0x01555550,
    0x01555550, 0x0556a554, 0x055aa954, 0x056aaa54,
    0x056aaa54, 0x056aaa54, 0x055aa954, 0x0556a554,
    0x01555550, 0x01555550, 0x00155500, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

void StampPackedCellGrid(void)
{
    int word;
    int centerColumn, centerRow;
    int stampColumn;
    struct SceneNode *camera;

    for (word = STAGE_CELL_GRID_WORDS - 1; word >= 0; --word) {
        gStageCellLodGrid[word] = 0;
    }

    camera = gCameraNodePtr;
    centerColumn = (camera->localMatrix.t[0] + STAGE_GRID_ORIGIN_OFFSET) / STAGE_CELL_SIZE;
    centerRow = (camera->localMatrix.t[2] + STAGE_GRID_ORIGIN_OFFSET) / STAGE_CELL_SIZE;
    for (word = 0; word < STAGE_CELL_LOD_STAMP_DIM; ++word) {
        int rowOffset = word - 8;
        int row = centerRow + rowOffset;
        if ((u_int)row < STAGE_GRID_DIM) {
            int packedRow = gCameraCellLodStamp[word];
            for (stampColumn = 0; stampColumn < STAGE_CELL_LOD_STAMP_DIM; ++stampColumn) {
                int columnOffset = stampColumn - 8;
                int column = centerColumn + columnOffset;
                if ((u_int)column < STAGE_GRID_DIM) {
                    int value = (packedRow >> ((15 - stampColumn) * 2)) & CELL_LOD_MASK;
                    gStageCellLodGrid[row * STAGE_CELL_WORDS_PER_ROW + column / 8] |= value << ((7 - column % 8) * 4);
                }
            }
        }
    }
}

void MarkCellSemiTrans(SVECTOR *cell)
{
    int column = cell->vx + STAGE_GRID_HALF_DIM;
    int row = cell->vz + STAGE_GRID_HALF_DIM;

    if ((u_int)column < STAGE_GRID_DIM && (u_int)row < STAGE_GRID_DIM) {
        int word = column / 8;
        int index = row * STAGE_CELL_WORDS_PER_ROW + word;
        u_int bits = gStageCellLodGrid[index];
        int shift = (7 - (column - word * 8)) * 4;
        u_int mask = (u_int)CELL_FLAG_SEMITRANS << shift;
        gStageCellLodGrid[index] = bits | mask;
    }
}

void MarkCellOccludedIfOpaque(SVECTOR *cell)
{
    int column = cell->vx + STAGE_GRID_HALF_DIM;
    int row = cell->vz + STAGE_GRID_HALF_DIM;

    if ((u_int)column < STAGE_GRID_DIM && (u_int)row < STAGE_GRID_DIM) {
        u_int testMask = (u_int)CELL_FLAG_SEMITRANS << ((7 - (column % 8)) * 4);
        int word = column / 8;
        int wordIndex = row * STAGE_CELL_WORDS_PER_ROW + word;
        u_int packedCells = gStageCellLodGrid[wordIndex];
        if (!(packedCells & testMask)) {
            testMask <<= 1;
            gStageCellLodGrid[wordIndex] = packedCells | testMask;
        }
    }
}

/* Nothing calls it. */
void MarkCellsHighDetailAroundPoint(int x, int z)
{
    int column;
    int row;
    int dx;
    int dz;
    int nextColumn;

    x += STAGE_GRID_ORIGIN_OFFSET;
    z += STAGE_GRID_ORIGIN_OFFSET;
    column = x / STAGE_CELL_SIZE;
    dx = x - column * STAGE_CELL_SIZE;
    dz = z / STAGE_CELL_SIZE;
    row = dz;
    dz = z - row * STAGE_CELL_SIZE;

    if (dx < STAGE_CELL_HALF_SIZE)
        dx = -1;
    else
        dx = 1;

    if (dz < STAGE_CELL_HALF_SIZE)
        dz = -1;
    else
        dz = 1;

    SetPackedCellLod(column, row, CELL_LOD_HIGH);
    nextColumn = column + dx;
    SetPackedCellLod(nextColumn, row, CELL_LOD_HIGH);
    SetPackedCellLod(column, row + dz, CELL_LOD_HIGH);
    SetPackedCellLod(nextColumn, row + dz, CELL_LOD_HIGH);
}

void SetPackedCellLod(int column, int row, unsigned int value)
{
    if ((u_int)column < STAGE_GRID_DIM && (u_int)row < STAGE_GRID_DIM) {
        int rowOffset = row * STAGE_CELL_WORDS_PER_ROW;
        int word = column / 8;
        int index = rowOffset + word;
        int shift = (7 - (column - word * 8)) * 4;
        u_int data = (value & CELL_LOD_MASK) << shift;
        u_int mask = (u_int)CELL_LOD_MASK << shift;
        gStageCellLodGrid[index] = (gStageCellLodGrid[index] & ~mask) | data;
    }
}

/* Nothing calls it. */
int GetPackedCellValue(int column, int row)
{
    int word = column / 8;
    return (gStageCellLodGrid[row * STAGE_CELL_WORDS_PER_ROW + word] >> ((7 - (column % 8)) * 4)) &
           STAGE_CELL_NIBBLE_MASK;
}

static void HideStageObjectZone(short *zoneList, int activeZone)
{
    int currentZone;
    int entry;
    int objectIndex;
    struct SceneNode *objects;

    currentZone = STAGE_ZONE_END;
    entry = *zoneList++;
    if (entry != currentZone) {
        do {
            objects = gStageObjects;
            if ((entry & STAGE_ZONE_HEADER_FLAG) != 0) {
                currentZone = entry & STAGE_ZONE_ID_MASK;
            } else {
                objectIndex = entry - gStageTileCount;
                if (currentZone == activeZone) {
                    objects[objectIndex].param.queued = 1;
                } else {
                    objects[objectIndex].param.queued = 0;
                }
            }
        } while ((entry = *zoneList++) != STAGE_ZONE_END);
    }
}

/* Stage 10 hideable-object zones: each zone's header, then the indices of the objects it hides. */
short gStage10ObjectZones[20] = {
    STAGE_ZONE_HEADER(0), 0x013c, 0x013d, 0x013e, 0x013f, 0x0140, 0x0141, 0x014a, 0x014b, 0x014c, 0x014d,
    STAGE_ZONE_HEADER(1), 0x0142, 0x0143, 0x0144, 0x0146, 0x0147, 0x0148, 0x0149,
    STAGE_ZONE_END,
};

static void UpdateStage10ObjectZones(void)
{
    int cameraX;
    int hideZone;

    cameraX = gCameraNode.worldMatrix.t[0];
    hideZone = STAGE_ZONE_NONE;
    if (cameraX >= 6401) {
        hideZone = 1;
    } else if (cameraX < -224) {
        hideZone = 0;
    }
    HideStageObjectZone(gStage10ObjectZones, hideZone);
}

/* Two slots past the last stage, which no stage id reaches. */
void (*gStagePreCullCallbacks[STAGE_COUNT + 2])(void) = {
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0, UpdateStage10ObjectZones, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0,
};

static void RunStagePreCullCallback(void)
{
    void (*callback)(void);

    callback = gStagePreCullCallbacks[GetLoadedStageId()];
    if (callback != 0) {
        callback();
    }
}
