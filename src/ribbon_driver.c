/* The opening demo's ribbon trails: ten trail slots, each fed a hilt and tip per frame (from a
 * bone's offsets or directly) and drawn through the ribbon strip builder. */
#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>

#include "packet_build.h"
#include "scene_node.h"
#include "ribbon_trail.h"
#include "stage_callbacks.h"
#include "ribbon_driver.h"
#include "scene_math.h"
#include "globals.h"

static RibbonTrail gRibbonTrails[10];

void InitRibbonTrail(int textureSet, int trailIndex)
{
    RibbonTrail *trail = &gRibbonTrails[trailIndex];

    if (textureSet == 0) {
        trail->tpage = GetTPage(0, 1, 0x300, 200);
        trail->clut = GetClut(0x330, 201);
    } else {
        trail->tpage = GetTPage(0, 1, 0x3c0, 200);
        trail->clut = GetClut(0x3f0, 201);
    }
    trail->u0 = 0;
    trail->v0 = 200;
    trail->u1 = 63;
    trail->v1 = 200;
    InitRibbonTrailUvs(trail);
}

void UpdateAndDrawRibbonTrail(int trailIndex, VECTOR *hilt, VECTOR *tip)
{
    MATRIX matrix;
    RibbonDraw *work;
    int primCount;

    BuildNegatedNodeRotation(&gCameraNode);
    matrix = gCameraNode.worldMatrix;
    work = (RibbonDraw *)getScratchAddr(8);

    work->ribbon = &gRibbonTrails[trailIndex];
    /* Halve Y row for 640x240 non-square pixels. */
    matrix.m[1][0] >>= 1;
    matrix.m[1][1] >>= 1;
    matrix.m[1][2] >>= 1;

    work->latest.first = *hilt;
    work->latest.second = *tip;
    work->matrix = matrix;
    work->origin.vx = matrix.t[0];
    work->origin.vy = matrix.t[1];
    work->origin.vz = matrix.t[2];
    work->packets = gPacketCursor;

    primCount = UpdateAndBuildRibbonPackets(work);
    gPacketCursor = work->packets;
    InsertDeferredPrimsIntoOt(primCount);
}

void UpdateRibbonTrailFromBone(int trailIndex, struct SceneNode *bone, SVECTOR *hiltOffset, SVECTOR *tipOffset)
{
    VECTOR hilt;
    VECTOR tip;

    ApplyMatrix(&bone->worldMatrix, hiltOffset, &hilt);
    hilt.vx += bone->worldMatrix.t[0];
    hilt.vy += bone->worldMatrix.t[1];
    hilt.vz += bone->worldMatrix.t[2];
    ApplyMatrix(&bone->worldMatrix, tipOffset, &tip);
    tip.vx += bone->worldMatrix.t[0];
    tip.vy += bone->worldMatrix.t[1];
    tip.vz += bone->worldMatrix.t[2];
    UpdateAndDrawRibbonTrail(trailIndex, &hilt, &tip);
}
