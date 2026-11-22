#ifndef FLOOR_COLLISION_H
#define FLOOR_COLLISION_H

#include <sys/types.h>
#include <libgte.h>

#define FLOOR_QUERY_MISS        0x7fffffff         /* QueryFloorHeight: no floor under the point */
#define FLOOR_HEIGHT_NONE       ((int)0x80000000)  /* a height kept after a miss: no floor */
#define FLOOR_BAND_HEIGHT       2000
#define FLOOR_WALKABLE_NORMAL_Y (-3072) /* Q12 -0.75 (~41.4 deg max walkable slope) */
#define FLOOR_ATTR_SURFACE_MASK 0x07
#define FLOOR_SURFACE_SNOW      1
#define FLOOR_ATTR_NO_IMPACT    0x40 /* no wall-bump stumble, no blade wall strike; thrown sub-weapons pass */

/* What the stage loader, the fighters and the AI ask of the floor collision grid. */
void SetFloorCollisionData(short *grid);
void RelocateFloorCollisionData(int delta);
int QueryFloorHeight(int x, int y, int z);
SVECTOR *GetLastFloorNormal(void);
int GetLastFloorAttr(void);

#endif
