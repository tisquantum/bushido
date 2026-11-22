#ifndef STAGE_CULL_H
#define STAGE_CULL_H

#include <sys/types.h>
#include <libgte.h>
#include "scene_node.h"

/* The stage floor grid: 32 by 32 cells of 3200 units, centred on the world origin. */
#define STAGE_GRID_DIM           32
#define STAGE_GRID_HALF_DIM      16
#define STAGE_CELL_SIZE          3200
#define STAGE_CELL_HALF_SIZE     1600
#define STAGE_GRID_ORIGIN_OFFSET (STAGE_GRID_HALF_DIM * STAGE_CELL_SIZE)

/* SceneTile.flags (+0x07) bitmask. */
typedef enum SceneTileFlag {
    SCENE_TILE_FLAG_SEMI_TRANS = 1,
    SCENE_TILE_FLAG_CONDITIONAL = 8
} SceneTileFlag;

#define SCENE_TILE_HIDDEN 0x8000

/* 16-byte type-12 stage floor tile record in gStageTiles. */
typedef struct SceneTile {
    short renderType;   /* +0x00: SCENE_NODE_TILE (12); bit 15 (0x8000) hides tile */
    short modelSlot;    /* +0x02: stage model table slot */
    u_short meshIndex;  /* +0x04: tile mesh index within stage model table */
    u_char lodLevel;    /* +0x06: low 2 bits of cell grid nibble / subdivision depth */
    u_char flags;       /* +0x07: SceneTileFlag (bit 0 semi-trans, bit 3 conditional) */
    int worldX;         /* +0x08: column * 3200 - 51200 */
    int worldZ;         /* +0x0C: row * 3200 - 51200 */
} SceneTile;

/* Stage cell grid nibble flags (bits 0..1 = LOD detail 0..2, bit 2 = semi-trans, bit 3 = occluded). */
typedef enum StageCellLodLevel {
    CELL_LOD_COARSE = 0,
    CELL_LOD_MEDIUM = 1,
    CELL_LOD_HIGH   = 2
} StageCellLodLevel;

#define CELL_LOD_MASK       3
#define CELL_FLAG_SEMITRANS 4
#define CELL_FLAG_OCCLUDED  8

/* Stage tile and ocean mipmap/debug render flags (gStageRenderFlags). */
typedef enum StageRenderFlag {
    STAGE_RENDER_MIP_MODE_MASK     = 0x03, /* bits 0..1: mipmap level selection rule */
    STAGE_RENDER_MIP_MODE_OFF      = 0x00, /* always full-resolution level 0 */
    STAGE_RENDER_MIP_MODE_LOD      = 0x01, /* level = 2 - tile subdivision depth */
    STAGE_RENDER_MIP_MODE_AREA     = 0x02, /* level from projected polygon area */
    STAGE_RENDER_MIP_MODE_AREA_LOD = 0x03, /* level = max(0, area level - tile depth) */
    STAGE_RENDER_DEBUG_MIP_TINT    = 0x04, /* bit 2: debug-tint polygons by mip/subdivision level */
    STAGE_RENDER_TRI_MIPMAP        = 0x20  /* bit 5: enable mipmap UV halving on triangles */
} StageRenderFlag;

void CullAndEnqueueStageScene(void);
void ParseStageSceneLayout(u_short *layoutData, int modelSlot);
void RunSceneCallback(void);
void ApplyLightSlotToGte(short lightSlot, short scale);
int GetCameraHeadingAngle(void);
struct SceneNode *GetStageObjectsBase(void);
void LoadVisibilityMaskPreset(int preset);
int GetVisibilityMaskPreset(void);
void HideStageTilesBeyondZ(void);
void StampPackedCellGrid(void);
void MarkCellSemiTrans(SVECTOR *cell);
void MarkCellOccludedIfOpaque(SVECTOR *cell);
void MarkCellsHighDetailAroundPoint(int x, int z);
void SetPackedCellLod(int column, int row, unsigned int value);
int GetPackedCellValue(int column, int row);

extern SVECTOR gAuxViewAngles;
extern CVECTOR gStageLitColor;  /* written and rescaled, read by nothing else */

#endif
