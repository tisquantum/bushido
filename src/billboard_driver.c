/* Billboard effects driver: builds each kind's texture entry from VRAM coordinates and adds the
 * frame's billboard quads to the ordering table. */
#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>

#include "display_buffer.h"
#include "packet_build.h"
#include "scene_node.h"
#include "billboard_driver.h"
#include "scene_math.h"
#include "billboard.h"
#include "main.h"
#include "model_arena.h"
#include "stage_cull.h"
#include "otz.h"
#include "globals.h"

#define BILLBOARD_TEX_SKIP (-2)
#define BILLBOARD_TEX_END  (-1)

/* Kinds 1 to 14, each { vramX, vramY, clutX, clutY, semi-transparency rate } or
 * BILLBOARD_TEX_SKIP; ended by BILLBOARD_TEX_END. */
short gEffectTextureSources[40] = {
    BILLBOARD_TEX_SKIP,                     /* 1 */
    0x398, 0x100, 0x380, 0x1f8, 1,          /* 2:  LIMB_WOUND */
    0x380, 0x100, 0x380, 0x1ff, 1,          /* 3:  ORANGE_SPARK */
    0x38c, 0x100, 0x380, 0x1fa, 1,          /* 4:  FATAL_HIT_PRIMARY */
    0x3e4, 0x178, 0x380, 0x1f9, 1,          /* 5:  WEAPON_CLASH */
    0x3cc, 0x100, 0x380, 0x1fc, 1,          /* 6:  FATAL_HIT_SECONDARY */
    0x3d8, 0x100, 0x380, 0x1fb, 1,          /* 7:  TORSO_WOUND */
    BILLBOARD_TEX_SKIP, BILLBOARD_TEX_SKIP, /* 8, 9 */
    BILLBOARD_TEX_SKIP, BILLBOARD_TEX_SKIP, /* 10, 11 */
    BILLBOARD_TEX_SKIP, BILLBOARD_TEX_SKIP, /* 12, 13 */
    BILLBOARD_TEX_SKIP,                     /* 14 */
    BILLBOARD_TEX_END, 0,
};

void UpdateAndDrawBillboardEffects(void)
{
    MATRIX matrix;
    BillboardEffectWork *work = (BillboardEffectWork *)getScratchAddr(8);
    POLY_FT4 **prims;
    long *depths;
    int i, count, bucket;

    BuildNegatedNodeRotation(&gCameraNode);
    matrix = gCameraNode.worldMatrix;
    /* Y halved: the 640x240 screen's pixels are twice as tall as wide. */
    matrix.m[1][0] >>= 1;
    matrix.m[1][1] >>= 1;
    matrix.m[1][2] >>= 1;

    work->cameraAngle = (0x800 - GetCameraHeadingAngle()) & 0xfff;
    work->matrix = matrix;
    work->eye.vx = matrix.t[0];
    work->eye.vy = matrix.t[1];
    work->eye.vz = matrix.t[2];
    work->packets = gPacketCursor;

    count = UpdateAndBuildBillboardEffects(work);
    prims = (POLY_FT4 **)work->packets;
    gPacketCursor = prims;
    depths = (long *)(prims + count);

    for (i = 0; i < count; i++) {
        bucket = CompressDepthToOtz(depths[i] * 4 - 200, 0);
        if (bucket < OTZ_BUCKET_COUNT)
            AddPrim(&gCurrentOt[bucket], prims[i]);
    }
}

void InitBillboardEffects(void)
{
    EffectTextureEntry textures[BILLBOARD_EFFECT_KIND_COUNT];
    EffectTextureEntry *entry;
    short *src;
    short x, y;

    LoadCommonEffectTextures();
    entry = textures;
    src = gEffectTextureSources;

    while ((x = *src++) != BILLBOARD_TEX_END) {
        if (x == BILLBOARD_TEX_SKIP) {
            entry++;
            continue;
        }
        y = *src++;
        entry->u = (x & 63) * 4;
        entry->v = y;
        entry->tpage = GetTPage(0, src[2], x, y);
        entry->clut = GetClut(src[0], src[1]);
        src += 3;
        entry++;
    }

    BuildEffectTextureUVs(textures);
}
