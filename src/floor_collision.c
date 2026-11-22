/* Floor collision: the stage's 32x32 grid of floor polygons and planes, queried for the floor
 * height under a point (world Y points down; normals are Q12 with -ONE straight up), recording
 * the surface normal and attribute of the floor it found. */
#include <sys/types.h>
#include <libgte.h>

#include "floor_collision.h"

/* Grid geometry: 32x32 cells of 3200x3200 world units centered at the origin (-51200..+51200).
 * The grid begins with 32*32 halfwords, each a cell's byte offset from the base (-1 = empty); each cell holds a
 * polygon count followed by packed floor records (flags, optional 2D vertices, height or plane). */
#define FLOOR_GRID_DIM               32
#define FLOOR_CELL_SIZE              3200
#define FLOOR_GRID_HALF_EXTENT       ((FLOOR_GRID_DIM / 2) * FLOOR_CELL_SIZE)
#define FLOOR_CELL_EMPTY             (-1)

/* A floor record's flags word: vertex count, surface attribute, and how its height is found. */
#define FLOOR_VERTEX_COUNT(flags)    ((flags) & 0x7f)
#define FLOOR_ATTR(flags)            (((flags) >> 7) & 0x7f)
#define FLOOR_LEVEL                  0x4000 /* level: one height word, no plane */
#define FLOOR_WHOLE_CELL             0x8000 /* covers the cell: no edge test, no vertices */
#define FLOOR_CELL_LEVEL             (FLOOR_LEVEL | FLOOR_WHOLE_CELL)

/* Vertical wall plane (ny == 0) result: impassable height and steep unwalkable Q12 normal. */
#define FLOOR_VERTICAL_WALL_HEIGHT   (-30000)
#define FLOOR_VERTICAL_WALL_NORMAL_Y (-355)

typedef struct FloorVertex2D {
    short x;
    short z;
} FloorVertex2D;

static short *gFloorCollisionGrid;
static SVECTOR gLastFloorNormal;
static short gLastFloorAttr;

void SetFloorCollisionData(short *grid)
{
    gFloorCollisionGrid = grid;
}

void RelocateFloorCollisionData(int delta)
{
    gFloorCollisionGrid += delta / 2;
}

int QueryFloorHeight(int x, int y, int z)
{
    int cellX;
    int cellZ;
    int localX;
    int localZ;
    int cellOffset;
    short *cursor;
    int polyCount;
    int flags;
    int vertexCount;
    int i;
    int next;
    FloorVertex2D *verts;
    int nx;
    int ny;
    int nz;
    int planeD;
    int height;
    int bestHeight;
    int fallbackHeight;
    short hitVerticalWall;
    int fallbackAttr;
    SVECTOR fallbackNormal;

    hitVerticalWall = 0;
    bestHeight = FLOOR_QUERY_MISS;
    fallbackHeight = FLOOR_HEIGHT_NONE;
    x += FLOOR_GRID_HALF_EXTENT;
    z += FLOOR_GRID_HALF_EXTENT;
    cellX = x / FLOOR_CELL_SIZE;
    cellZ = z / FLOOR_CELL_SIZE;
    if ((u_int)cellX >= FLOOR_GRID_DIM || (u_int)cellZ >= FLOOR_GRID_DIM)
        return FLOOR_QUERY_MISS;
    localX = x % FLOOR_CELL_SIZE;
    localZ = z % FLOOR_CELL_SIZE;
    cellOffset = gFloorCollisionGrid[cellZ * FLOOR_GRID_DIM + cellX];
    if (cellOffset == FLOOR_CELL_EMPTY)
        return FLOOR_QUERY_MISS;
    cursor = (short *)((char *)gFloorCollisionGrid + cellOffset);
    polyCount = *cursor++;
    while (polyCount-- != 0) {
        flags = *cursor++;
        if ((flags & FLOOR_CELL_LEVEL) == FLOOR_CELL_LEVEL) {
            bestHeight = *cursor;
            gLastFloorNormal.vy = -ONE;
            gLastFloorNormal.vx = 0;
            gLastFloorNormal.vz = 0;
            gLastFloorAttr = FLOOR_ATTR(flags);
            return bestHeight;
        }
        vertexCount = FLOOR_VERTEX_COUNT(flags);
        if (flags & FLOOR_WHOLE_CELL) {
            i = vertexCount;
        } else {
            verts = (FloorVertex2D *)cursor;
            for (i = 0; i < vertexCount; i++) {
                next = i + 1;
                if (next >= vertexCount)
                    next = 0;
                if ((localX - verts[i].x) * (verts[next].z - verts[i].z) -
                    (localZ - verts[i].z) * (verts[next].x - verts[i].x) > 0)
                    break;
            }
            cursor += vertexCount * 2;
        }
        if (i >= vertexCount) {
            if (flags & FLOOR_LEVEL) {
                nx = 0;
                ny = -ONE;
                nz = 0;
                height = *cursor++;
            } else {
                nx = cursor[0];
                ny = cursor[1];
                nz = cursor[2];
                planeD = cursor[3];
                cursor += 4;
                if (ny == 0) {
                    hitVerticalWall = 1;
                    break;
                }
                height = (nx * localX + nz * localZ + (planeD << 12)) / -ny;
            }
            if (height < bestHeight) {
                if (height >= y) {
                    gLastFloorNormal.vx = nx;
                    gLastFloorNormal.vy = ny;
                    gLastFloorNormal.vz = nz;
                    gLastFloorAttr = FLOOR_ATTR(flags);
                    bestHeight = height;
                } else if (fallbackHeight < height) {
                    fallbackHeight = height;
                    fallbackAttr = FLOOR_ATTR(flags);
                    fallbackNormal.vx = nx;
                    fallbackNormal.vy = ny;
                    fallbackNormal.vz = nz;
                }
            }
        } else if (flags & FLOOR_LEVEL) {
            cursor += 1;
        } else {
            cursor += 4;
        }
    }
    if (hitVerticalWall) {
        gLastFloorNormal.vx = nx;
        gLastFloorNormal.vy = FLOOR_VERTICAL_WALL_NORMAL_Y;
        gLastFloorNormal.vz = nz;
        gLastFloorAttr = FLOOR_ATTR(flags);
        bestHeight = FLOOR_VERTICAL_WALL_HEIGHT;
    } else if (bestHeight == FLOOR_QUERY_MISS && fallbackHeight != FLOOR_HEIGHT_NONE) {
        bestHeight = fallbackHeight;
        gLastFloorAttr = fallbackAttr;
        gLastFloorNormal.vx = fallbackNormal.vx;
        gLastFloorNormal.vy = fallbackNormal.vy;
        gLastFloorNormal.vz = fallbackNormal.vz;
    }
    return bestHeight;
}

SVECTOR *GetLastFloorNormal(void)
{
    return &gLastFloorNormal;
}

int GetLastFloorAttr(void)
{
    return gLastFloorAttr;
}
