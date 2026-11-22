/* Camera sightlines: walks the stage grid cell by cell from the camera to a target, hiding the
 * near cells that block it and making the further ones semi-transparent. */
#include <sys/types.h>
#include <libgte.h>
#include "scene_node.h"
#include "sightline.h"
#include "stage_cull.h"

#define SIGHTLINE_GRID_WORDS         32
#define SIGHTLINE_SLOPE_SHIFT        12
#define SIGHTLINE_SEMITRANS_MIN_STEP 3

static unsigned int gSightlineBitmask[SIGHTLINE_GRID_WORDS]; /* cleared each frame; nothing reads it */

void ClearSightlineGrid(void)
{
    int remainingWords;
    unsigned int *wordPtr;

    remainingWords = SIGHTLINE_GRID_WORDS - 1;
    wordPtr = &gSightlineBitmask[SIGHTLINE_GRID_WORDS - 1];
    do {
        *wordPtr = 0;
        remainingWords -= 1;
        wordPtr -= 1;
    } while (remainingWords >= 0);
}

static void TraceSightlineGrid(int startX, int startZ, int endX, int endZ)
{
    int step, directionX;
    int x = startX + STAGE_GRID_ORIGIN_OFFSET;
    int z = startZ + STAGE_GRID_ORIGIN_OFFSET;
    int targetX = endX + STAGE_GRID_ORIGIN_OFFSET;
    int targetZ = endZ + STAGE_GRID_ORIGIN_OFFSET;
    int dx = targetX - x;
    int dz = targetZ - z;
    int cellX = x / STAGE_CELL_SIZE;
    int cellZ = z / STAGE_CELL_SIZE;
    int endCellX = targetX / STAGE_CELL_SIZE;
    int endCellZ = targetZ / STAGE_CELL_SIZE;
    int centerX, centerZ;
    int directionZ;
    int alongZ, slope, remaining;
    int visited;
    SVECTOR single, cell, next;

    if (dx == 0 && dz == 0) {
        if (cellX >= 0 && cellZ >= 0 && cellX < STAGE_GRID_DIM && cellZ < STAGE_GRID_DIM) {
            single.vx = cellX - STAGE_GRID_HALF_DIM;
            single.vz = cellZ - STAGE_GRID_HALF_DIM;
            MarkCellOccludedIfOpaque(&single);
        }
        return;
    }

    centerX = cellX * STAGE_CELL_SIZE + STAGE_CELL_HALF_SIZE;
    centerZ = cellZ * STAGE_CELL_SIZE + STAGE_CELL_HALF_SIZE;
    x -= centerX;
    z -= centerZ;
    targetX -= centerX;
    targetZ -= centerZ;
    if (dx < 0) {
        directionX = -1;
        dx = -dx;
        x = -x;
        targetX = -targetX;
    } else {
        directionX = 1;
    }
    if (dz < 0) {
        directionZ = -1;
        dz = -dz;
        z = -z;
        targetZ = -targetZ;
    } else {
        directionZ = 1;
    }
    if (dx < dz) {
        int swapCoord = dx;
        dx = dz;
        dz = swapCoord;
        swapCoord = x;
        x = z;
        z = swapCoord;
        targetX = targetZ;
        alongZ = 1;
    } else {
        alongZ = 0;
    }

    slope = (dz << SIGHTLINE_SLOPE_SHIFT) / dx;
    targetX += STAGE_CELL_HALF_SIZE;
    x += STAGE_CELL_HALF_SIZE;
    remaining = targetX / STAGE_CELL_SIZE - x / STAGE_CELL_SIZE;
    z += STAGE_CELL_HALF_SIZE;
    z -= (x * slope) >> SIGHTLINE_SLOPE_SHIFT;
    step = (slope * STAGE_CELL_SIZE) >> SIGHTLINE_SLOPE_SHIFT;
    visited = 0;
    while (remaining != -1) {
        if (cellX >= 0 && cellZ >= 0 && cellX < STAGE_GRID_DIM && cellZ < STAGE_GRID_DIM) {
            cell.vx = cellX - STAGE_GRID_HALF_DIM;
            cell.vz = cellZ - STAGE_GRID_HALF_DIM;
            MarkCellOccludedIfOpaque(&cell);
            if (visited >= SIGHTLINE_SEMITRANS_MIN_STEP) {
                MarkCellSemiTrans(&cell);
            }
        }
        z %= STAGE_CELL_SIZE;
        z += step;
        if (remaining == 0) {
            break;
        }
        if (z > STAGE_CELL_SIZE) {
            if (alongZ) {
                if (directionX < 0) --cellX;
                else ++cellX;
            } else {
                if (directionZ < 0) --cellZ;
                else ++cellZ;
            }
            if (cellX >= 0 && cellZ >= 0 && cellX < STAGE_GRID_DIM && cellZ < STAGE_GRID_DIM) {
                next.vx = cellX - STAGE_GRID_HALF_DIM;
                next.vz = cellZ - STAGE_GRID_HALF_DIM;
                MarkCellOccludedIfOpaque(&next);
                if (visited >= SIGHTLINE_SEMITRANS_MIN_STEP) {
                    MarkCellSemiTrans(&next);
                }
            }
        }
        if (alongZ) {
            if (directionZ < 0) --cellZ;
            else ++cellZ;
        } else {
            if (directionX < 0) --cellX;
            else ++cellX;
        }
        --remaining;
        ++visited;
    }
    if (endCellX >= 0 && endCellZ >= 0 && endCellX < STAGE_GRID_DIM && endCellZ < STAGE_GRID_DIM) {
        next.vx = endCellX - STAGE_GRID_HALF_DIM;
        next.vz = endCellZ - STAGE_GRID_HALF_DIM;
        MarkCellOccludedIfOpaque(&next);
        MarkCellSemiTrans(&next);
    }
}

void TraceCameraSightlineTo(int targetX, int targetZ)
{
    TraceSightlineGrid(gCameraNode.localMatrix.t[0], gCameraNode.localMatrix.t[2], targetX, targetZ);
}

void EndSceneFrameNoOp(void)
{
}
