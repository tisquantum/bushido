/* The stage ground decals' driver: picks each stage's decal atlas, draws the decal pool after
 * the stage is culled, and lays a footprint where a fighter's foot comes down. */
#include <sys/types.h>
#include <libgte.h>
#include <libetc.h>
#include <libgpu.h>

#include "gpu_code.h"
#include "fighter_skeleton.h"
#include "scene_node.h"
#include "display_buffer.h"
#include "packet_build.h"
#include "render_scratch.h"
#include "fighter_pose.h"
#include "decal_driver.h"
#include "angle.h"
#include "scene_math.h"
#include "decal.h"
#include "main.h"
#include "model_arena.h"
#include "otz.h"
#include "globals.h"

#define FOOT_CONTACT_Y_THRESHOLD (-200)

/* [fighter][foot]: set when a footprint is laid as the foot rises above height -200, cleared when it comes
 * back down. Only UpdateFootDecalContact, which nothing calls, reads it; the stage start clears rows 0 and 1. */
static int gFootprintLaid[18][2];

void InitStageGroundDecals(void)
{
    EffectTextureEntry texture;

    switch (GetLoadedStageId()) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 12:
    case 22:
        texture.tpage = GET_TPAGE(0, 2, 640, 256);
        texture.clut = GET_CLUT(304, 480);
        texture.u = 0;
        texture.v = 0x10;
        break;
    }
    /* every stage whose setup calls this is listed above */
    InitGroundDecalAtlas(&texture);
    gFootprintLaid[0][0] = 0;
    gFootprintLaid[0][1] = 0;
    gFootprintLaid[1][0] = 0;
    gFootprintLaid[1][1] = 0;
}

void UpdateAndDrawStageGroundDecals(void)
{
    MATRIX matrix;
    GroundDecalScratch *scratch = (GroundDecalScratch *)getScratchAddr(8);
    int i, count, bucket;
    POLY_FT4 **packets;
    long *depths;

    BuildNegatedNodeRotation(&gCameraNode);
    matrix = gCameraNode.worldMatrix;
    /* Halve Y row for 640x240 non-square pixels. */
    matrix.m[1][0] >>= 1;
    matrix.m[1][1] >>= 1;
    matrix.m[1][2] >>= 1;
    scratch->matrix = matrix;
    scratch->translation[0] = matrix.t[0];
    scratch->translation[1] = matrix.t[1];
    scratch->translation[2] = matrix.t[2];
    scratch->packets = gPacketCursor;
    count = UpdateAndBuildGroundDecals(scratch);
    packets = (POLY_FT4 **)scratch->packets;
    gPacketCursor = packets;
    depths = (long *)(packets + count);
    for (i = 0; i < count; i++) {
        bucket = CompressDepthToOtz(depths[i] * 4, 0);
        if (bucket > STAGE_TILE_NEAR_CLIP_OTZ && bucket < OTZ_BUCKET_COUNT) {
            AddPrim(&gCurrentOt[bucket], packets[i]);
        }
    }
}

void SpawnFighterFootprint(int side, int footprintIndex, int *position)
{
    struct SceneNode *foot;
    VECTOR direction;
    SVECTOR forward;

    foot = GetFighterRootNode(side);
    if ((footprintIndex & 1) == 0) {
        foot += FIGHTER_NODE_LEFT_FOOT;
    } else {
        foot += FIGHTER_NODE_RIGHT_FOOT;
    }
    forward.vx = 0;
    forward.vy = 0;
    forward.vz = ONE;
    ApplyMatrix(&foot->worldMatrix, &forward, &direction);
    SpawnGroundDecal(footprintIndex, position,
                     (ratan2(direction.vx, direction.vz) + ANGLE_HALF) & ANGLE_MASK);
}

/* Nothing calls it. */
void UpdateFootDecalContact(int fighterIndex, int footIndex, struct SceneNode *foot)
{
    VECTOR direction;
    SVECTOR forward;
    int angle;

    forward.vx = 0;
    forward.vy = 0;
    forward.vz = ONE;
    ApplyMatrix(&foot->worldMatrix, &forward, &direction);
    angle = ratan2(direction.vx, direction.vz);
    if (gFootprintLaid[fighterIndex][footIndex] == 0) {
        if (foot->worldMatrix.t[1] < FOOT_CONTACT_Y_THRESHOLD) {
            direction.vx = foot->worldMatrix.t[0];
            direction.vy = 0;
            direction.vz = foot->worldMatrix.t[2];
            SpawnGroundDecal(footIndex % 2, (int *)&direction.vx, angle);
            gFootprintLaid[fighterIndex][footIndex] = 1;
        }
    } else if (foot->worldMatrix.t[1] >= FOOT_CONTACT_Y_THRESHOLD) {
        gFootprintLaid[fighterIndex][footIndex] = 0;
    }
}
