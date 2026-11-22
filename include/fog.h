#ifndef FOG_H
#define FOG_H

/* Main stage CLUT strip at VRAM (0, 0x1e0, 320x1) and blended step budgets. */
#define FOG_MAIN_CLUT_Y 0x1e0
#define FOG_MAIN_CLUT_WIDTH 320
#define FOG_MAIN_STEPS_FULL 63  /* Full VRAM step count when character fog is disabled */
#define FOG_MAIN_STEPS_SPLIT 31 /* Reduced step count when VRAM is split with character fog */

/* Secondary character CLUT block at VRAM (0, 0x1f8, 80x8) and blended steps. */
#define FOG_CHAR_CLUT_Y 0x1f8
#define FOG_CHAR_CLUT_WRAP_Y 0x1f0 /* Second VRAM row once x reaches FOG_CHAR_VRAM_LIMIT_X */
#define FOG_CHAR_CLUT_WIDTH 80
#define FOG_CHAR_CLUT_HEIGHT 8
#define FOG_CHAR_CLUT_PIXELS (FOG_CHAR_CLUT_WIDTH * FOG_CHAR_CLUT_HEIGHT)
#define FOG_CHAR_STEPS 15          /* Blended character fog steps uploaded and linked per frame */
#define FOG_CHAR_MOVE_SLOTS 16     /* Per-bank stride in gCharFogMoves[2][16] */
#define FOG_CHAR_VRAM_LIMIT_X 640

/* Depth-to-OTZ mapping and ordering-table linking bounds. */
#define FOG_MAX_DEPTH 48000      /* Far depth clamp for fog span distribution */
#define FOG_FINAL_OT_BUCKET 4091 /* Tail OT bucket receiving the fully-fogged CLUT move */
#define FOG_FAR_OTZ_NONE 0xffff  /* Sentinel in gFogFarOtzBucket when no fog move is linked */

/* gCharFogFighterSlot when no fighter is fogged. */
#define FOG_CHAR_SLOT_NONE -1

/* Per-stage depth-fog toggle and final-step linking mode. */
typedef struct FogStageConfig {
    short enabled;        /* +0x00: Nonzero to link fog CLUT DR_MOVE packets on this stage */
    short directLastLink; /* +0x02: Nonzero to link the final step directly into OT[4091] */
} FogStageConfig;

void QueueStageFogClutMoves(int stageId, int startDepth);
void BuildFogClutTable(void);
void EnableCharFogSlot(int slot);
void DisableCharFogSlot(void);
int IsCharFogSlotActive(int slot);
int GetFogFarOtzBucket(void);

void LinkFogClutMoves(unsigned int depth, unsigned int depthSpan, int directLastLink);
void LinkCharFogClutMoves(unsigned int depth, unsigned int depthSpan, int directLastLink);
void LinkFinalFogClutDirect(void);
void BuildCharFogClutTable(void);

#endif
