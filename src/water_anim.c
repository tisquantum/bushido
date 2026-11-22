/* Water animation: mipmapped VRAM texture scrolling for streams and waterfalls, three-frame
 * VRAM texture swaps queued as DR_MOVE packets, and Stage 1's waterfall splash columns. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "display_buffer.h"
#include "effect.h"
#include "effect_slot.h"
#include "fighter_pose.h"
#include "globals.h"
#include "main.h"
#include "scene_node.h"
#include "water_anim.h"
#include "scene.h"

#define WATER_SCROLL_DESC_COUNT 2
#define VRAM_MIP_LEVELS 3
#define VRAM_TPAGE_WIDTH 64
#define VRAM_TPAGE_HEIGHT 256

#define STAGE1_WATERFALL_COLUMN_COUNT 2
#define WATERFALL_POOL_Y 2000
#define WATERFALL_COLUMN_HALF_WIDTH 200
#define WATERFALL_SPRAY_UP_Y (-ONE)

#define VRAM_ANIM_DESC_COUNT 4
#define VRAM_ANIM_FRAME_COUNT 3
#define VRAM_ANIM_SLOT_COUNT 2
#define VRAM_ANIM_OT_BUCKET 4095 /* Back-most OT bucket so VRAM swaps run before scene polygons */

/* 14-byte mipmapped VRAM vertical scroll descriptor (3 halved mip levels per 64x256 tpage). */
typedef struct VramScrollDesc {
    short sourceX, sourceY;
    short width, height;
    short destX, destY;
    short step;
} VramScrollDesc;

typedef struct VramCoord {
    short x, y;
} VramCoord;

/* 20-byte 3-frame mipmapped VRAM texture swap descriptor in gVramAnimDescs[4][3]. */
typedef struct VramAnimDesc {
    short destX, destY;
    short width, height;
    VramCoord source[VRAM_ANIM_FRAME_COUNT];
} VramAnimDesc;

int gWaterScrollOffset = 0;

/* Mipmapped VRAM scroll descriptors: [0] Stage 25 stream, [1] Stage 1 waterfall. */
VramScrollDesc gWaterScrollDescs[WATER_SCROLL_DESC_COUNT] = {
    { 736, 256, 32, 128, 672, 256, 1 },
    { 784, 384, 16, 64, 768, 384, 4 },
};

/* World positions of Stage 1's two waterfall splash columns (paired with fighter 0 and 1). */
VECTOR gStage1WaterfallColumns[STAGE1_WATERFALL_COLUMN_COUNT] = {
    { -12000, WATERFALL_POOL_Y, 8150, 0 },
    { -12000, WATERFALL_POOL_Y, 7850, 0 },
};

/* 3-level mipmap 3-frame VRAM animation tables: [0..1] Stage 1, [2] Stage 10, [3] Stage 22. */
VramAnimDesc gVramAnimDescs[VRAM_ANIM_DESC_COUNT][VRAM_MIP_LEVELS] = {
    {
        { 736, 256, 16, 64, { { 800, 256 }, { 800, 320 }, { 800, 384 } } },
        { 720, 256, 8, 32, { { 784, 256 }, { 784, 288 }, { 784, 320 } } },
        { 712, 256, 4, 16, { { 776, 256 }, { 776, 272 }, { 776, 288 } } },
    },
    {
        { 752, 256, 16, 64, { { 816, 256 }, { 816, 320 }, { 816, 384 } } },
        { 728, 256, 8, 32, { { 792, 256 }, { 792, 288 }, { 792, 320 } } },
        { 716, 256, 4, 16, { { 780, 256 }, { 780, 272 }, { 780, 288 } } },
    },
    {
        { 736, 448, 16, 64, { { 880, 256 }, { 880, 320 }, { 880, 384 } } },
        { 720, 352, 8, 32, { { 856, 256 }, { 856, 288 }, { 856, 320 } } },
        { 712, 304, 4, 16, { { 844, 256 }, { 844, 272 }, { 844, 288 } } },
    },
    {
        { 752, 256, 16, 64, { { 816, 256 }, { 816, 320 }, { 800, 256 } } },
        { 728, 256, 8, 32, { { 792, 256 }, { 792, 288 }, { 784, 256 } } },
        { 716, 256, 4, 16, { { 780, 256 }, { 780, 272 }, { 776, 256 } } },
    },
};

/* Per frame buffer and slot, one DR_MOVE per mip level. */
static DR_MOVE gVramAnimMovePackets[DISPLAY_BUFFER_COUNT][VRAM_ANIM_SLOT_COUNT][VRAM_MIP_LEVELS];

void ResetWaterScrollOffset(void)
{
    gWaterScrollOffset = 0;
}

void UpdateWaterTextureScroll(int index)
{
    VramScrollDesc *desc;
    RECT rect;
    int offset, mipLevel;
    int sourcePageX, sourcePageY, sourceOffsetX, sourceOffsetY;
    int destPageX, destPageY, destOffsetX, destOffsetY;
    int width, height, destX, destY, sourceY, firstHeight;
    short secondHeight;

    desc = &gWaterScrollDescs[index];
    offset = gWaterScrollOffset;
    sourcePageX = (short)(desc->sourceX & ~(VRAM_TPAGE_WIDTH - 1));
    sourcePageY = (short)(desc->sourceY & ~(VRAM_TPAGE_HEIGHT - 1));
    sourceOffsetX = (short)(desc->sourceX % VRAM_TPAGE_WIDTH);
    sourceOffsetY = (short)(desc->sourceY % VRAM_TPAGE_HEIGHT);
    width = desc->width;
    height = desc->height;
    destPageX = (short)(desc->destX & ~(VRAM_TPAGE_WIDTH - 1));
    destPageY = (short)(desc->destY & ~(VRAM_TPAGE_HEIGHT - 1));
    destOffsetX = (short)(desc->destX % VRAM_TPAGE_WIDTH);
    destOffsetY = (short)(desc->destY % VRAM_TPAGE_HEIGHT);
    for (mipLevel = 0; mipLevel < VRAM_MIP_LEVELS; mipLevel++) {
        destX = destPageX + destOffsetX;
        destY = destPageY + destOffsetY;
        rect.x = sourcePageX + sourceOffsetX;
        sourceY = sourcePageY + sourceOffsetY;
        rect.y = sourceY;
        firstHeight = height - offset;
        secondHeight = offset;
        rect.w = width;
        rect.h = firstHeight;
        MoveImage(&rect, destX, destY + offset);
        rect.y = sourceY + firstHeight;
        rect.h = secondHeight;
        MoveImage(&rect, destX, destY);
        destOffsetX >>= 1;
        destOffsetY >>= 1;
        offset >>= 1;
        sourceOffsetX >>= 1;
        sourceOffsetY >>= 1;
        width >>= 1;
        height >>= 1;
    }
    gWaterScrollOffset += desc->step;
    if (gWaterScrollOffset >= desc->height) {
        gWaterScrollOffset %= desc->height;
    }
}

void UpdateStage1WaterfallSplashes(void)
{
    int side, joint, highestY;
    int colX, colZ, dx, dz, jointY;
    VECTOR *columnPos;
    MATRIX **joints;
    VECTOR sprayDir;

    if ((gSceneFrameCounter & 3) != 0)
        return;

    sprayDir.vx = 0;
    sprayDir.vy = WATERFALL_SPRAY_UP_Y; /* Q12 upward spray axis (-Y is up) */
    sprayDir.vz = 0;
    for (side = 0; side < STAGE1_WATERFALL_COLUMN_COUNT; side++) {
        highestY = WATERFALL_POOL_Y;
        columnPos = &gStage1WaterfallColumns[side];
        joints = GetFighterWorldMatrixList(side);
        colX = columnPos->vx;
        colZ = columnPos->vz;
        for (joint = 0; joint < FIGHTER_JOINT_COUNT; joint++) {
            dx = joints[joint]->t[0] - colX;
            dz = joints[joint]->t[2] - colZ;
            if ((dx < 0 ? -dx : dx) < WATERFALL_COLUMN_HALF_WIDTH) {
                if ((dz < 0 ? -dz : dz) < WATERFALL_COLUMN_HALF_WIDTH) {
                    jointY = joints[joint]->t[1];
                    if (jointY < highestY)
                        highestY = jointY;
                }
            }
        }

        columnPos->vy = highestY;
        if (highestY < WATERFALL_POOL_Y)
            SpawnDirectionalSprayUnit(SPRAY_KIND_WATERFALL, columnPos, &sprayDir);
    }
}

void QueueAnimatedVramTextures(int animIndex, int slot)
{
    VramAnimDesc *desc;
    DR_MOVE *packet;
    RECT rect;
    int frame;
    int mipLevel;

    desc = gVramAnimDescs[animIndex];
    frame = (gSceneFrameCounter >> 2) % VRAM_ANIM_FRAME_COUNT;
    packet = gVramAnimMovePackets[gFrameCounter & 1][slot];
    for (mipLevel = 0; mipLevel < VRAM_MIP_LEVELS; mipLevel++, desc++, packet++) {
        rect.x = desc->source[frame].x;
        rect.y = desc->source[frame].y;
        rect.w = desc->width;
        rect.h = desc->height;
        SetDrawMove(packet, &rect, desc->destX, desc->destY);
        addPrim(&gCurrentOt[VRAM_ANIM_OT_BUCKET], packet);
    }
}
