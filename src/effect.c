/* Visual effects: the pool of effect slots, the meshes they draw, their spawners and per-frame
 * updates, and DrawEffectMesh. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <rand.h>

#include "effect_slot.h"
#include "packet_build.h"
#include "scene_node.h"
#include "render_scratch.h"
#include "screen_fade.h"
#include "display_buffer.h"
#include "otz.h"
#include "angle.h"
#include "gpu_code.h"
#include "effect.h"
#include "fighter_pose.h"
#include "main.h"
#include "rot_matrix.h"
#include "scene_math.h"
#include "stage_callbacks.h"
#include "stage_moat.h"
#include "globals.h"
#include "render.h"
#include "scene.h"
#include "tim.h"

static EffectSlot gEffectPool[EFFECT_POOL_SIZE];

void ClearEffectPool(void)
{
    int i;

    for (i = 0; i < EFFECT_POOL_SIZE; i++) {
        gEffectPool[i].active = EFFECT_SLOT_FREE;
    }
}

EffectSlot *AllocEffectSlot(void)
{
    int slotIndex;

    for (slotIndex = 0; slotIndex < EFFECT_POOL_SIZE; slotIndex++) {
        if (gEffectPool[slotIndex].active == EFFECT_SLOT_FREE) {
            return &gEffectPool[slotIndex];
        }
    }
    return 0;
}

void UpdateAndDrawEffects(void)
{
    int i;

    i = 0;
    do {
        if (gEffectPool[i].active != EFFECT_SLOT_FREE) {
            gEffectPool[i].update(&gEffectPool[i]);
            if (gEffectPool[i].active == EFFECT_SLOT_ACTIVE) {
                DrawEffectMesh(gEffectPool[i].mesh);
            }
        }
        i++;
    } while (i < EFFECT_POOL_SIZE);
}

EffectMesh4x6 gParrySparkMesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -450, -450, 0, -1 },
        {  450, -450, 0, -1 },
        { -450,  450, 0, -1 },
        {  450,  450, 0, -1 },
    },
    6, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 0, GET_CLUT(896, 510), 47, 0,
            GET_TPAGE(0, 1, 896, 256), 0, 23, 47, 23, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 24, GET_CLUT(896, 510), 47, 24,
            GET_TPAGE(0, 1, 896, 256), 0, 47, 47, 47, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 48, GET_CLUT(896, 510), 47, 48,
            GET_TPAGE(0, 1, 896, 256), 0, 71, 47, 71, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 72, GET_CLUT(896, 510), 47, 72,
            GET_TPAGE(0, 1, 896, 256), 0, 95, 47, 95, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 96, GET_CLUT(896, 510), 47, 96,
            GET_TPAGE(0, 1, 896, 256), 0, 119, 47, 119, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 120, GET_CLUT(896, 510), 47, 120,
            GET_TPAGE(0, 1, 896, 256), 0, 143, 47, 143, { 0, 1, 2, 3 } },
    },
};

EffectMesh4x6 gWallHitSparkMesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -450, -450, 0, -1 },
        {  450, -450, 0, -1 },
        { -450,  450, 0, -1 },
        {  450,  450, 0, -1 },
    },
    5, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 0, GET_CLUT(896, 475), 191, 0,
            GET_TPAGE(0, 1, 960, 256), 144, 23, 191, 23, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 24, GET_CLUT(896, 475), 191, 24,
            GET_TPAGE(0, 1, 960, 256), 144, 47, 191, 47, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 48, GET_CLUT(896, 475), 191, 48,
            GET_TPAGE(0, 1, 960, 256), 144, 71, 191, 71, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 72, GET_CLUT(896, 475), 191, 72,
            GET_TPAGE(0, 1, 960, 256), 144, 95, 191, 95, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 96, GET_CLUT(896, 475), 191, 96,
            GET_TPAGE(0, 1, 960, 256), 144, 119, 191, 119, { 0, 1, 2, 3 } },
    },
};

EffectMesh4x10 gDustPuffMesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -150, -150, 0, -1 },
        {  150, -150, 0, -1 },
        { -150,  150, 0, -1 },
        {  150,  150, 0, -1 },
    },
    10, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 0, GET_CLUT(896, 493), 239, 0,
            GET_TPAGE(0, 1, 960, 256), 192, 23, 239, 23, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 24, GET_CLUT(896, 492), 239, 24,
            GET_TPAGE(0, 1, 960, 256), 192, 47, 239, 47, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 48, GET_CLUT(896, 491), 239, 48,
            GET_TPAGE(0, 1, 960, 256), 192, 71, 239, 71, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 72, GET_CLUT(896, 490), 239, 72,
            GET_TPAGE(0, 1, 960, 256), 192, 95, 239, 95, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 96, GET_CLUT(896, 489), 239, 96,
            GET_TPAGE(0, 1, 960, 256), 192, 119, 239, 119, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 120, GET_CLUT(896, 488), 239, 120,
            GET_TPAGE(0, 1, 960, 256), 192, 143, 239, 143, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 144, GET_CLUT(896, 487), 239, 144,
            GET_TPAGE(0, 1, 960, 256), 192, 167, 239, 167, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 168, GET_CLUT(896, 486), 239, 168,
            GET_TPAGE(0, 1, 960, 256), 192, 191, 239, 191, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 192, GET_CLUT(896, 485), 239, 192,
            GET_TPAGE(0, 1, 960, 256), 192, 215, 239, 215, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 216, GET_CLUT(896, 484), 239, 216,
            GET_TPAGE(0, 1, 960, 256), 192, 239, 239, 239, { 0, 1, 2, 3 } },
    },
};

/* Nothing spawns effect type 4. */
EffectMesh4x8 gUnusedEffect4Mesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -200, -200, 0, -1 },
        {  200, -200, 0, -1 },
        { -200,  200, 0, -1 },
        {  200,  200, 0, -1 },
    },
    8, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 0, GET_CLUT(896, 501), 179, 0,
            GET_TPAGE(0, 0, 896, 256), 144, 15, 179, 15, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 16, GET_CLUT(896, 500), 179, 16,
            GET_TPAGE(0, 0, 896, 256), 144, 31, 179, 31, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 32, GET_CLUT(896, 499), 179, 32,
            GET_TPAGE(0, 0, 896, 256), 144, 47, 179, 47, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 48, GET_CLUT(896, 498), 179, 48,
            GET_TPAGE(0, 0, 896, 256), 144, 63, 179, 63, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 64, GET_CLUT(896, 497), 179, 64,
            GET_TPAGE(0, 0, 896, 256), 144, 79, 179, 79, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 80, GET_CLUT(896, 496), 179, 80,
            GET_TPAGE(0, 0, 896, 256), 144, 95, 179, 95, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 96, GET_CLUT(896, 495), 179, 96,
            GET_TPAGE(0, 0, 896, 256), 144, 111, 179, 111, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 112, GET_CLUT(896, 494), 179, 112,
            GET_TPAGE(0, 0, 896, 256), 144, 127, 179, 127, { 0, 1, 2, 3 } },
    },
};

EffectMesh4x8 gGunMuzzleFlashP1Mesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -100, -300, 0, -1 },
        {  100, -300, 0, -1 },
        { -100,    0, 0, -1 },
        {  100,    0, 0, -1 },
    },
    6, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 0, GET_CLUT(816, 1), 23, 0,
            GET_TPAGE(0, 0, 768, 0), 0, 39, 23, 39, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 24, 0, GET_CLUT(816, 1), 47, 0,
            GET_TPAGE(0, 0, 768, 0), 24, 39, 47, 39, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 48, 0, GET_CLUT(816, 1), 71, 0,
            GET_TPAGE(0, 0, 768, 0), 48, 39, 71, 39, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 72, 0, GET_CLUT(816, 1), 95, 0,
            GET_TPAGE(0, 0, 768, 0), 72, 39, 95, 39, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 96, 0, GET_CLUT(816, 1), 119, 0,
            GET_TPAGE(0, 0, 768, 0), 96, 39, 119, 39, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 120, 0, GET_CLUT(816, 1), 143, 0,
            GET_TPAGE(0, 0, 768, 0), 120, 39, 143, 39, { 0, 1, 2, 3 } },
    },
};

EffectMesh4x8 gGunMuzzleFlashP2Mesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -100, -300, 0, -1 },
        {  100, -300, 0, -1 },
        { -100,    0, 0, -1 },
        {  100,    0, 0, -1 },
    },
    6, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 0, GET_CLUT(1008, 1), 23, 0,
            GET_TPAGE(0, 1, 960, 0), 0, 39, 23, 39, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 24, 0, GET_CLUT(1008, 1), 47, 0,
            GET_TPAGE(0, 1, 960, 0), 24, 39, 47, 39, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 48, 0, GET_CLUT(1008, 1), 71, 0,
            GET_TPAGE(0, 1, 960, 0), 48, 39, 71, 39, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 72, 0, GET_CLUT(1008, 1), 95, 0,
            GET_TPAGE(0, 1, 960, 0), 72, 39, 95, 39, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 96, 0, GET_CLUT(1008, 1), 119, 0,
            GET_TPAGE(0, 1, 960, 0), 96, 39, 119, 39, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 120, 0, GET_CLUT(1008, 1), 143, 0,
            GET_TPAGE(0, 1, 960, 0), 120, 39, 143, 39, { 0, 1, 2, 3 } },
    },
};

/* Nothing spawns effect type 7. */
EffectMesh4x6 gUnusedEffect7Mesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -200, -400, 0, -1 },
        {  200, -400, 0, -1 },
        { -200,    0, 0, -1 },
        {  200,    0, 0, -1 },
    },
    6, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 240, GET_CLUT(272, 480), 15, 240,
            GET_TPAGE(0, 0, 768, 256), 0, 255, 15, 255, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 16, 240, GET_CLUT(272, 480), 31, 240,
            GET_TPAGE(0, 0, 768, 256), 16, 255, 31, 255, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 32, 240, GET_CLUT(272, 480), 47, 240,
            GET_TPAGE(0, 0, 768, 256), 32, 255, 47, 255, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 48, 240, GET_CLUT(272, 480), 63, 240,
            GET_TPAGE(0, 0, 768, 256), 48, 255, 63, 255, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 64, 240, GET_CLUT(272, 480), 79, 240,
            GET_TPAGE(0, 0, 768, 256), 64, 255, 79, 255, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 80, 240, GET_CLUT(272, 480), 95, 240,
            GET_TPAGE(0, 0, 768, 256), 80, 255, 95, 255, { 0, 1, 2, 3 } },
    },
};

EffectMesh4x5 gWaterSurfaceSplashMesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -300, -600, 0, -1 },
        {  300, -600, 0, -1 },
        { -300,    0, 0, -1 },
        {  300,    0, 0, -1 },
    },
    5, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 16, 128, GET_CLUT(880, 508), 47, 128,
            GET_TPAGE(0, 1, 832, 256), 16, 159, 47, 159, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 48, 128, GET_CLUT(880, 508), 79, 128,
            GET_TPAGE(0, 1, 832, 256), 48, 159, 79, 159, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 80, 128, GET_CLUT(880, 508), 111, 128,
            GET_TPAGE(0, 1, 832, 256), 80, 159, 111, 159, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 112, 128, GET_CLUT(880, 508), 143, 128,
            GET_TPAGE(0, 1, 832, 256), 112, 159, 143, 159, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 128, GET_CLUT(880, 508), 175, 128,
            GET_TPAGE(0, 1, 832, 256), 144, 159, 175, 159, { 0, 1, 2, 3 } },
    },
};

EffectMesh4x5 gWaterFootstepSplashMesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -200, -500, 0, -1 },
        {  200, -500, 0, -1 },
        { -200,    0, 0, -1 },
        {  200,    0, 0, -1 },
    },
    5, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 96, 224, GET_CLUT(288, 480), 127, 224,
            GET_TPAGE(0, 1, 832, 256), 96, 255, 127, 255, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 128, 224, GET_CLUT(288, 480), 159, 224,
            GET_TPAGE(0, 1, 832, 256), 128, 255, 159, 255, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 160, 224, GET_CLUT(288, 480), 191, 224,
            GET_TPAGE(0, 1, 832, 256), 160, 255, 191, 255, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 224, GET_CLUT(288, 480), 223, 224,
            GET_TPAGE(0, 1, 832, 256), 192, 255, 223, 255, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 224, 224, GET_CLUT(288, 480), 255, 224,
            GET_TPAGE(0, 1, 832, 256), 224, 255, 255, 255, { 0, 1, 2, 3 } },
    },
};

EffectMesh4x7 gSprayParticleMesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -300, -300, 0, -1 },
        {  300, -300, 0, -1 },
        { -300,  300, 0, -1 },
        {  300,  300, 0, -1 },
    },
    7, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 96, 208, GET_CLUT(896, 471), 143, 208,
            GET_TPAGE(0, 1, 896, 256), 96, 231, 143, 231, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 128, GET_CLUT(896, 471), 191, 128,
            GET_TPAGE(0, 1, 896, 256), 144, 151, 191, 151, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 152, GET_CLUT(896, 471), 191, 152,
            GET_TPAGE(0, 1, 896, 256), 144, 175, 191, 175, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 176, GET_CLUT(896, 471), 191, 176,
            GET_TPAGE(0, 1, 896, 256), 144, 199, 191, 199, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 200, GET_CLUT(896, 471), 191, 200,
            GET_TPAGE(0, 1, 896, 256), 144, 223, 191, 223, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 224, GET_CLUT(896, 471), 191, 224,
            GET_TPAGE(0, 1, 896, 256), 144, 247, 191, 247, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 96, 232, GET_CLUT(896, 471), 143, 232,
            GET_TPAGE(0, 1, 896, 256), 96, 255, 143, 255, { 0, 1, 2, 3 } },
    },
};

EffectMesh4x5 gSnowFootstepPuffMesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -200, -500, 0, -1 },
        {  200, -500, 0, -1 },
        { -200,    0, 0, -1 },
        {  200,    0, 0, -1 },
    },
    5, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 128, 0, GET_CLUT(864, 289), 159, 0,
            GET_TPAGE(0, 0, 832, 256), 128, 31, 159, 31, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 160, 0, GET_CLUT(864, 289), 191, 0,
            GET_TPAGE(0, 0, 832, 256), 160, 31, 191, 31, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 192, 0, GET_CLUT(864, 289), 223, 0,
            GET_TPAGE(0, 0, 832, 256), 192, 31, 223, 31, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 224, 0, GET_CLUT(864, 289), 255, 0,
            GET_TPAGE(0, 0, 832, 256), 224, 31, 255, 31, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 224, 32, GET_CLUT(864, 289), 255, 32,
            GET_TPAGE(0, 0, 832, 256), 224, 63, 255, 63, { 0, 1, 2, 3 } },
    },
};

EffectType gEffectTypeTable[EFFECT_TYPE_COUNT] = {
    { 6, 1, (void *)&gParrySparkMesh },
    { 5, 1, (void *)&gWallHitSparkMesh },
    { 10, 1, (void *)&gDustPuffMesh },
    { 0, 0, 0 },
    { 8, 1, (void *)&gUnusedEffect4Mesh },
    { 6, 1, (void *)&gGunMuzzleFlashP1Mesh },
    { 6, 1, (void *)&gGunMuzzleFlashP2Mesh },
    { 6, 1, (void *)&gUnusedEffect7Mesh },
    { 0, 0, 0 },
    { 5, 3, (void *)&gWaterSurfaceSplashMesh },
    { 0, 0, 0 },
    { 5, 2, (void *)&gWaterFootstepSplashMesh },
    { 7, 1, (void *)&gSprayParticleMesh },
    { 5, 2, (void *)&gSnowFootstepPuffMesh },
};


/* Draws a chain of EFFECT_MESH records into gCurrentOt; returns the word after the last. */
int *DrawEffectMesh(int *meshData)
{
    long *screenXy;
    short *screenZ;
    POLY_FT4 *poly;
    int mode;
    int depthShift;
    int endFlag;
    int count;
    int remainingQuads;
    int quadStart;
    int vertexIndices;
    int v0Index;
    int depth;
    long interp;
    long gteFlag;

    screenXy = MESH_SCRATCH->screenXy;
    screenZ = MESH_SCRATCH->screenZ;
    poly = gPacketCursor;

    do {
        mode = meshData[0] & EFFECT_MESH_MODE_MASK;
        endFlag = meshData[0] & EFFECT_MESH_LAST;
        depthShift = SetupEffectTransform(mode, meshData[1], meshData[2], meshData[3]);
        meshData += 4;

        count = *meshData++;
        while (count--) {
            *screenZ++ = RotTransPers((SVECTOR *)meshData, screenXy++, &interp, &gteFlag) << 2;
            meshData += 2;
        }

        screenXy = MESH_SCRATCH->screenXy;
        screenZ = MESH_SCRATCH->screenZ;

        remainingQuads = *meshData++;
        quadStart = *meshData++;
        count = *meshData++;
        remainingQuads -= quadStart + count;
        meshData += quadStart * 5;

        while (count--) {
            setlen(poly, 9);
            *(int *)&poly->r0 = meshData[0];
            *(int *)&poly->u0 = meshData[1];
            *(int *)&poly->u1 = meshData[2];
            *(short *)&poly->u2 = meshData[3];
            *(short *)&poly->u3 = meshData[3] >> 16;
            meshData += 4;
            vertexIndices = *meshData++;
            v0Index = vertexIndices & 0xff;
            *(long *)&poly->x0 = screenXy[v0Index];
            *(long *)&poly->x1 = screenXy[(vertexIndices >> 8) & 0xff];
            *(long *)&poly->x2 = screenXy[(vertexIndices >> 16) & 0xff];
            *(long *)&poly->x3 = screenXy[(vertexIndices >> 24) & 0xff];
            if (mode != EFFECT_MESH_BILLBOARD) {
                depth = screenZ[v0Index];
            } else {
                depth = screenZ[v0Index] - 500;
            }
            if (depth > 0 && depth < 0x10000) {
                AddPrim(&gCurrentOt[CompressDepthToOtz(depth, depthShift)], poly);
            }
            poly++;
        }

        meshData += remainingQuads * 5;
    } while (endFlag == 0);

    gPacketCursor = poly;
    return meshData;
}

int SetupEffectTransform(int mode, int x, int y, int z)
{
    MATRIX result;
    SVECTOR offset;
    MATRIX *view;
    int dx, dy, dz;
    int absX, extent, shift;
    int scale, yscale;

    view = GetAspectCorrectedCameraMatrix();
    dx = x - view->t[0];
    dy = y - view->t[1];
    dz = z - view->t[2];

    absX = dx >= 0 ? dx : -dx;
    if (dy >= 0)
        extent = absX | dy;
    else
        extent = absX | -dy;
    if (dz >= 0)
        extent |= dz;
    else
        extent |= -dz;

    /* Scale down by 2^shift so rotated SVECTOR offsets stay within signed 16-bit GTE range. */
    shift = 0;
    if (extent > 0xb500)
        shift = 2;
    else if (extent > 0x5a00)
        shift = 1;

    offset.vx = dx >> shift;
    offset.vy = dy >> shift;
    offset.vz = dz >> shift;
    ApplyMatrix(view, &offset, (VECTOR *)result.t);

    if (mode == EFFECT_MESH_BILLBOARD) {
        /* 640x240 display has 2:1 pixel aspect, so billboard Y scale is half X/Z scale. */
        scale = ONE >> shift;
        yscale = ONE >> (shift + 1);
        result.m[0][0] = scale;
        result.m[0][1] = 0;
        result.m[0][2] = 0;
        result.m[1][0] = 0;
        result.m[1][1] = yscale;
        result.m[1][2] = 0;
        result.m[2][0] = 0;
        result.m[2][1] = 0;
        result.m[2][2] = scale;
        SetGteRotAndTransMatrix(&result);
    } else {
        SetRotMatrix(view);
        SetTransMatrix(&result);
    }

    return shift;
}

void SpawnEffect(int type, long x, long y, long z)
{
    EffectSlot *effect;
    void *mesh;
    short ticksPerFrame;

    switch (type) {
    case EFFECT_BLADE_LOCK_FLASH:
        SpawnBladeLockFlashEffect(x, y, z);
        break;
    case EFFECT_WATER_SPLASH:
        SpawnWaterSplashEffect(x, MOAT_WATER_SURFACE_Y, z);
        break;
    case EFFECT_WATER_RIPPLE:
        SpawnWaterRippleEffect(x, MOAT_WATER_SURFACE_Y, z);
        break;
    default:
        effect = AllocEffectSlot();
        if (effect != 0) {
            effect->active = EFFECT_SLOT_ACTIVE;
            effect->update = UpdateSpriteEffect;
            mesh = gEffectTypeTable[type].mesh;
            effect->pos.vx = x;
            effect->pos.vy = y;
            effect->pos.vz = z;
            effect->mesh = mesh;
            effect->params.sprite.frameCount = gEffectTypeTable[type].frameCount;
            ticksPerFrame = gEffectTypeTable[type].ticksPerFrame;
            effect->frameOrTick = 0;
            effect->subTick = 0;
            effect->params.sprite.ticksPerFrame = ticksPerFrame;
        }
        break;
    }
}

void UpdateSpriteEffect(EffectSlot *effect)
{
    EffectMesh4xN *mesh;

    mesh = (EffectMesh4xN *)effect->mesh;
    mesh->pos[0] = effect->pos.vx;
    mesh->pos[1] = effect->pos.vy;
    mesh->pos[2] = effect->pos.vz;
    mesh->quadStart = effect->frameOrTick;
    if (++effect->subTick >= effect->params.sprite.ticksPerFrame) {
        if (++effect->frameOrTick >= effect->params.sprite.frameCount) {
            effect->active = EFFECT_SLOT_FREE;
        }
        effect->subTick = 0;
    }
}

EffectMesh4x1 gStage4LightGlowMesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -800, -800, 0, -1 },
        {  800, -800, 0, -1 },
        { -800,  800, 0, -1 },
        {  800,  800, 0, -1 },
    },
    1, 0, 1,
    {
        { { { 0x40, 0x40, 0x40, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 32, 0, GET_CLUT(864, 288), 55, 0,
            GET_TPAGE(0, 1, 768, 256), 32, 11, 55, 11, { 0, 1, 2, 3 } },
    },
};

void SpawnStage4LightGlows(void)
{
    EffectSlot *firstGlow;
    EffectSlot *secondGlow;

    firstGlow = AllocEffectSlot();
    if (firstGlow != 0) {
        firstGlow->active = EFFECT_SLOT_ACTIVE;
        firstGlow->variant = 0;
        firstGlow->mesh = &gStage4LightGlowMesh;
        firstGlow->update = UpdateStage4LightGlow;
        secondGlow = AllocEffectSlot();
        if (secondGlow != 0) {
            secondGlow->active = EFFECT_SLOT_ACTIVE;
            secondGlow->variant = 1;
            secondGlow->mesh = &gStage4LightGlowMesh;
            secondGlow->update = UpdateStage4LightGlow;
        }
    }
}

int gStage4LightGlowPositions[2][3] = {
    { -26100, -1920, 3800 },
    { -19400, -1920, 3800 },
};

void UpdateStage4LightGlow(EffectSlot *effect)
{
    unsigned char unused[16]; /* not used */
    EffectMesh4x1 *mesh;
    int dx, dy, dz;
    int distance;

    mesh = (EffectMesh4x1 *)effect->mesh;
    mesh->pos[0] = gStage4LightGlowPositions[effect->variant][0];
    mesh->pos[1] = gStage4LightGlowPositions[effect->variant][1];
    mesh->pos[2] = gStage4LightGlowPositions[effect->variant][2];
    dx = mesh->pos[0] - gCameraNode.localMatrix.t[0];
    dy = mesh->pos[1] - gCameraNode.localMatrix.t[1];
    dz = mesh->pos[2] - gCameraNode.localMatrix.t[2];

    if ((dx < 0 ? -dx : dx) > 0x7000 ||
        (dy < 0 ? -dy : dy) > 0x7000 ||
        (dz < 0 ? -dz : dz) > 0x7000) {
        distance = 0xffff;
    } else {
        distance = ComputeVectorLength3D(dx, dy, dz);
    }
    distance -= 6000;
    if (distance < 0) {
        effect->active = EFFECT_SLOT_UPDATE_ONLY;
    } else {
        effect->active = EFFECT_SLOT_ACTIVE;
        distance = (distance >> 1) > ONE ? ONE : (distance >> 1);
        distance = (((rand() & 15) + 64) * distance) >> 12;
        mesh->quads[0].tint.color.r = distance;
        mesh->quads[0].tint.color.g = distance;
        mesh->quads[0].tint.color.b = distance;
    }
}

EffectMesh9x4 gWaterRingEffectMesh = {
    EFFECT_MESH_WORLD | EFFECT_MESH_LAST, { 0, 0, 0 }, 9,
    {
        { 0, 0, 0, -1 },
        { 0, 0, 0, -1 },
        { 0, 0, 0, -1 },
        { 0, 0, 0, -1 },
        { 0, 0, 0, -1 },
        { 0, 0, 0, -1 },
        { 0, 0, 0, -1 },
        { 0, 0, 0, -1 },
        { 0, 0, 0, -1 },
    },
    4, 0, 4,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 128, 96, GET_CLUT(864, 508), 143, 96,
            GET_TPAGE(0, 1, 832, 256), 128, 111, 143, 111, { 0, 1, 3, 4 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 159, 96, GET_CLUT(864, 508), 144, 96,
            GET_TPAGE(0, 1, 832, 256), 159, 111, 144, 111, { 2, 1, 5, 4 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 143, 112, GET_CLUT(864, 508), 128, 112,
            GET_TPAGE(0, 1, 832, 256), 143, 127, 128, 127, { 4, 3, 7, 6 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 112, GET_CLUT(864, 508), 159, 112,
            GET_TPAGE(0, 1, 832, 256), 144, 127, 159, 127, { 4, 5, 7, 8 } },
    },
};
EffectMesh4x8 gWaterSplashBillboardMesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -1200, -800, 0, -1 },
        {  1200, -800, 0, -1 },
        { -1200,  800, 0, -1 },
        {  1200,  800, 0, -1 },
    },
    8, 0, 1,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 160, 32, GET_CLUT(832, 500), 199, 32,
            GET_TPAGE(0, 1, 832, 256), 160, 46, 199, 46, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 160, 48, GET_CLUT(848, 500), 199, 48,
            GET_TPAGE(0, 1, 832, 256), 160, 62, 199, 62, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 160, 64, GET_CLUT(864, 500), 199, 64,
            GET_TPAGE(0, 1, 832, 256), 160, 78, 199, 78, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 160, 80, GET_CLUT(880, 500), 199, 80,
            GET_TPAGE(0, 1, 832, 256), 160, 94, 199, 94, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 208, 32, GET_CLUT(832, 496), 247, 32,
            GET_TPAGE(0, 1, 832, 256), 208, 46, 247, 46, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 208, 48, GET_CLUT(848, 496), 247, 48,
            GET_TPAGE(0, 1, 832, 256), 208, 62, 247, 62, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 208, 64, GET_CLUT(864, 496), 247, 64,
            GET_TPAGE(0, 1, 832, 256), 208, 78, 247, 78, { 0, 1, 2, 3 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 208, 80, GET_CLUT(880, 496), 247, 80,
            GET_TPAGE(0, 1, 832, 256), 208, 94, 247, 94, { 0, 1, 2, 3 } },
    },
};

void SpawnWaterSplashEffect(int x, int y, int z)
{
    EffectSlot *ringEffect;
    EffectSlot *billboardEffect;

    ringEffect = AllocEffectSlot();
    if (ringEffect != 0) {
        ringEffect->active = EFFECT_SLOT_ACTIVE;
        billboardEffect = AllocEffectSlot();
        if (billboardEffect == 0) {
            ringEffect->active = EFFECT_SLOT_FREE;
        } else {
            ringEffect->mesh = &gWaterRingEffectMesh;
            ringEffect->update = UpdateWaterRingEffect;
            ringEffect->active = EFFECT_SLOT_ACTIVE;
            ringEffect->pos.vx = x;
            ringEffect->pos.vy = y;
            ringEffect->pos.vz = z;
            ringEffect->frameOrTick = 0;
            ringEffect->subTick = 0;

            billboardEffect->mesh = &gWaterSplashBillboardMesh;
            billboardEffect->update = UpdateWaterSplashBillboard;
            billboardEffect->active = EFFECT_SLOT_ACTIVE;
            billboardEffect->pos.vx = x;
            billboardEffect->pos.vy = y - 500;
            billboardEffect->pos.vz = z;
            billboardEffect->frameOrTick = 0;
        }
    }
}

void UpdateWaterRingEffect(EffectSlot *effect)
{
    EffectMesh9x4 *mesh;
    int radius;
    short negativeRadius;
    int shade;
    int intensity;

    mesh = (EffectMesh9x4 *)effect->mesh;
    radius = effect->frameOrTick;
    negativeRadius = -radius;
    mesh->pos[0] = effect->pos.vx;
    mesh->pos[1] = effect->pos.vy;
    mesh->pos[2] = effect->pos.vz;
    mesh->vertices[8].vx = mesh->vertices[5].vx =
        mesh->vertices[2].vz = mesh->vertices[2].vx =
        mesh->vertices[1].vz = mesh->vertices[0].vz = radius;
    mesh->vertices[0].vx = negativeRadius;
    mesh->vertices[1].vx = 0;
    mesh->vertices[3].vx = negativeRadius;
    mesh->vertices[3].vz = 0;
    mesh->vertices[4].vx = 0;
    mesh->vertices[4].vz = 0;
    mesh->vertices[5].vz = 0;
    mesh->vertices[6].vx = negativeRadius;
    mesh->vertices[6].vz = negativeRadius;
    mesh->vertices[7].vx = 0;
    mesh->vertices[7].vz = negativeRadius;
    mesh->vertices[8].vz = negativeRadius;
    effect->frameOrTick = radius + ((3000 - radius) >> 3);
    shade = 0x8000 - ++effect->subTick * 1092;
    if (shade < 0) {
        shade = 0;
    }
    intensity = (unsigned int)shade >> 8;
    mesh->quads[0].tint.color.b = intensity;
    mesh->quads[0].tint.color.g = intensity;
    mesh->quads[0].tint.color.r = intensity;
    mesh->quads[1].tint.color.b = intensity;
    mesh->quads[1].tint.color.g = intensity;
    mesh->quads[1].tint.color.r = intensity;
    mesh->quads[2].tint.color.b = intensity;
    mesh->quads[2].tint.color.g = intensity;
    mesh->quads[2].tint.color.r = intensity;
    mesh->quads[3].tint.color.b = intensity;
    mesh->quads[3].tint.color.g = intensity;
    mesh->quads[3].tint.color.r = intensity;
    if (effect->subTick >= 30) {
        effect->active = EFFECT_SLOT_FREE;
    }
}

void UpdateWaterSplashBillboard(EffectSlot *effect)
{
    EffectMesh4x8 *mesh;

    mesh = (EffectMesh4x8 *)effect->mesh;
    mesh->pos[0] = effect->pos.vx;
    mesh->pos[1] = effect->pos.vy;
    mesh->pos[2] = effect->pos.vz;
    mesh->quadStart = effect->frameOrTick;
    if (effect->frameOrTick >= 8) {
        effect->active = EFFECT_SLOT_FREE;
    }
    if ((gSceneFrameCounter & 1) == 0) {
        effect->frameOrTick += 1;
    }
}

EffectMesh9x16 gWaterRippleEffectMesh = {
    EFFECT_MESH_WORLD | EFFECT_MESH_LAST, { 0, 0, 0 }, 9,
    {
        { -400, 0, -400, -1 },
        {    0, 0, -400, -1 },
        {  400, 0, -400, -1 },
        { -400, 0,    0, -1 },
        {    0, 0,    0, -1 },
        {  400, 0,    0, -1 },
        { -400, 0,  400, -1 },
        {    0, 0,  400, -1 },
        {  400, 0,  400, -1 },
    },
    16, 0, 4,
    {
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 16, 160, GET_CLUT(832, 492), 47, 160,
            GET_TPAGE(0, 1, 832, 256), 16, 191, 47, 191, { 0, 1, 3, 4 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 79, 160, GET_CLUT(832, 492), 48, 160,
            GET_TPAGE(0, 1, 832, 256), 79, 191, 48, 191, { 2, 1, 5, 4 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 47, 192, GET_CLUT(832, 492), 16, 192,
            GET_TPAGE(0, 1, 832, 256), 47, 223, 16, 223, { 4, 3, 7, 6 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 48, 192, GET_CLUT(832, 492), 79, 192,
            GET_TPAGE(0, 1, 832, 256), 48, 223, 79, 223, { 4, 5, 7, 8 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 80, 160, GET_CLUT(832, 492), 111, 160,
            GET_TPAGE(0, 1, 832, 256), 80, 191, 111, 191, { 0, 1, 3, 4 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 143, 160, GET_CLUT(832, 492), 112, 160,
            GET_TPAGE(0, 1, 832, 256), 143, 191, 112, 191, { 2, 1, 5, 4 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 111, 192, GET_CLUT(832, 492), 80, 192,
            GET_TPAGE(0, 1, 832, 256), 111, 223, 80, 223, { 4, 3, 7, 6 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 112, 192, GET_CLUT(832, 492), 143, 192,
            GET_TPAGE(0, 1, 832, 256), 112, 223, 143, 223, { 4, 5, 7, 8 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 144, 160, GET_CLUT(832, 492), 175, 160,
            GET_TPAGE(0, 1, 832, 256), 144, 191, 175, 191, { 0, 1, 3, 4 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 207, 160, GET_CLUT(832, 492), 176, 160,
            GET_TPAGE(0, 1, 832, 256), 207, 191, 176, 191, { 2, 1, 5, 4 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 175, 192, GET_CLUT(832, 492), 144, 192,
            GET_TPAGE(0, 1, 832, 256), 175, 223, 144, 223, { 4, 3, 7, 6 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 176, 192, GET_CLUT(832, 492), 207, 192,
            GET_TPAGE(0, 1, 832, 256), 176, 223, 207, 223, { 4, 5, 7, 8 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 176, 96, GET_CLUT(832, 492), 207, 96,
            GET_TPAGE(0, 1, 832, 256), 176, 127, 207, 127, { 0, 1, 3, 4 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 239, 96, GET_CLUT(832, 492), 208, 96,
            GET_TPAGE(0, 1, 832, 256), 239, 127, 208, 127, { 2, 1, 5, 4 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 207, 128, GET_CLUT(832, 492), 176, 128,
            GET_TPAGE(0, 1, 832, 256), 207, 159, 176, 159, { 4, 3, 7, 6 } },
        { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 208, 128, GET_CLUT(832, 492), 239, 128,
            GET_TPAGE(0, 1, 832, 256), 208, 159, 239, 159, { 4, 5, 7, 8 } },
    },
};

void SpawnWaterRippleEffect(int x, int y, int z)
{
    EffectSlot *effect;

    effect = AllocEffectSlot();
    if (effect != 0) {
        effect->active = EFFECT_SLOT_ACTIVE;
        effect->mesh = &gWaterRippleEffectMesh;
        effect->update = UpdateWaterRippleEffect;
        effect->pos.vx = x;
        effect->pos.vy = y;
        effect->pos.vz = z;
        effect->frameOrTick = 0;
        effect->subTick = 0;
    }
}

void UpdateWaterRippleEffect(EffectSlot *effect)
{
    EffectMesh9x16 *mesh;
    int quadStart;

    mesh = (EffectMesh9x16 *)effect->mesh;
    mesh->pos[0] = effect->pos.vx;
    mesh->pos[1] = effect->pos.vy;
    mesh->pos[2] = effect->pos.vz;
    quadStart = (++effect->subTick >> 1) << 2;
    mesh->quadStart = quadStart;
    if (quadStart >= 16) {
        effect->active = EFFECT_SLOT_FREE;
    }
}

EffectMesh4x1 gUvAnimEffectMeshes[2] = {
    {
        EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
        {
            { -300, -200, 0, -1 },
            {  300, -200, 0, -1 },
            { -300,  100, 0, -1 },
            {  300,  100, 0, -1 },
        },
        1, 0, 1,
        {
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 48, 168, GET_CLUT(960, 508), 79, 168,
                GET_TPAGE(0, 0, 960, 256), 48, 176, 79, 176, { 0, 1, 2, 3 } },
        },
    },
    {
        EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
        {
            { -300, -300, 0, -1 },
            {  300, -300, 0, -1 },
            { -300,  300, 0, -1 },
            {  300,  300, 0, -1 },
        },
        1, 0, 1,
        {
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 192, 128, GET_CLUT(976, 508), 255, 128,
                GET_TPAGE(0, 0, 960, 256), 192, 136, 255, 136, { 0, 1, 2, 3 } },
        },
    },
};

/* Reached only from SpawnBodyJointUvEffects, which nothing calls. */
static void SpawnUvAnimEffect(int variant, int x, int y, int z)
{
    EffectSlot *effect;

    effect = AllocEffectSlot();
    if (effect != 0) {
        effect->active = EFFECT_SLOT_ACTIVE;
        effect->mesh = &gUvAnimEffectMeshes[variant];
        effect->update = UpdateUvAnimEffect;
        effect->variant = variant;
        effect->subTick = 0;
        effect->frameOrTick = 0;
        effect->pos.vx = x;
        effect->pos.vy = y;
        effect->pos.vz = z;
    }
}

unsigned char gUvAnimVariant0VCoords[6][2] = {
    { 0x90, 0x98 },
    { 0x98, 0xa0 },
    { 0xa0, 0xa8 },
    { 0xa8, 0xb0 },
    { 0xb0, 0xb8 },
    { 0x00, 0x00 },
};
unsigned char gUvAnimVariant1VCoords[8][2] = {
    { 0x90, 0x98 },
    { 0x98, 0xa0 },
    { 0xa0, 0xa8 },
    { 0xa8, 0xb0 },
    { 0xb0, 0xb8 },
    { 0xb8, 0xc0 },
    { 0xc0, 0xc8 },
    { 0xc8, 0xd0 },
};

short gUvEffectBodyJointIndices[13] = { 0, 1, 2, 4, 5, 8, 9, 11, 12, 13, 15, 16, -1 };

/* Nothing calls it. */
static void SpawnBodyJointUvEffects(int fighterSlot)
{
    short *nextJoint = gUvEffectBodyJointIndices;
    MATRIX **jointMatrices = GetFighterWorldMatrixList(fighterSlot);
    short jointIndex;

    while ((jointIndex = *nextJoint++) != -1) {
        MATRIX *joint = jointMatrices[jointIndex];
        SpawnUvAnimEffect(1, joint->t[0], joint->t[1], joint->t[2]);
    }
}

void UpdateUvAnimEffect(EffectSlot *effect)
{
    EffectMesh4x1 *mesh = (EffectMesh4x1 *)effect->mesh;
    mesh->pos[0] = effect->pos.vx;
    mesh->pos[1] = effect->pos.vy;
    mesh->pos[2] = effect->pos.vz;

    if (effect->variant == 0) {
        mesh->quads[0].v0 = mesh->quads[0].v1 =
            gUvAnimVariant0VCoords[effect->frameOrTick][0];
        mesh->quads[0].v2 = mesh->quads[0].v3 =
            gUvAnimVariant0VCoords[effect->frameOrTick][1];
    } else {
        mesh->quads[0].v0 = mesh->quads[0].v1 =
            gUvAnimVariant1VCoords[effect->frameOrTick][0];
        mesh->quads[0].v2 = mesh->quads[0].v3 =
            gUvAnimVariant1VCoords[effect->frameOrTick][1];
    }

    if (effect->variant == 0) {
        if (++effect->subTick == 2) {
            effect->subTick = 0;
            if (++effect->frameOrTick >= 5) {
                effect->active = EFFECT_SLOT_FREE;
            }
        }
    } else {
        if (++effect->subTick == 2) {
            effect->subTick = 0;
            if (++effect->frameOrTick >= 8) {
                effect->active = EFFECT_SLOT_FREE;
            }
        }
    }
}

EffectMesh4x3 gBladeLockFlashMesh[2] = {
    {
        EFFECT_MESH_BILLBOARD, { 0, 0, 0 }, 4,
        {
            { -300, -300, 0, -1 },
            {  300, -300, 0, -1 },
            { -300,  300, 0, -1 },
            {  300,  300, 0, -1 },
        },
        3, 0, 1,
        {
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 0, GET_CLUT(896, 511), 48, 0,
                GET_TPAGE(0, 1, 896, 256), 0, 24, 48, 24, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 24, GET_CLUT(896, 511), 48, 24,
                GET_TPAGE(0, 1, 896, 256), 0, 48, 48, 48, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 0, 48, GET_CLUT(896, 511), 48, 48,
                GET_TPAGE(0, 1, 896, 256), 0, 72, 48, 72, { 0, 1, 2, 3 } },
        },
    },
    {
        EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
        {
            { -300, -300, 0, -1 },
            {  300, -300, 0, -1 },
            { -300,  300, 0, -1 },
            {  300,  300, 0, -1 },
        },
        3, 0, 1,
        {
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 120, GET_CLUT(896, 505), 192, 120,
                GET_TPAGE(0, 1, 960, 256), 144, 144, 192, 144, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 144, GET_CLUT(896, 505), 192, 144,
                GET_TPAGE(0, 1, 960, 256), 144, 168, 192, 168, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 168, GET_CLUT(896, 505), 192, 168,
                GET_TPAGE(0, 1, 960, 256), 144, 192, 192, 192, { 0, 1, 2, 3 } },
        },
    },
};
/* Indexed by SubWeaponSlot.id - SUB_WEAPON_SURFACE_BASE. */
EffectMesh4x5 gSurfaceDebrisParticleMeshes[4] = {
    {
        EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
        {
            { -300, -600, 0, -1 },
            {  300, -600, 0, -1 },
            { -300,    0, 0, -1 },
            {  300,    0, 0, -1 },
        },
        5, 0, 1,
        {
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 0, GET_CLUT(896, 483), 47, 0,
                GET_TPAGE(0, 0, 960, 256), 0, 23, 47, 23, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 24, GET_CLUT(896, 483), 47, 24,
                GET_TPAGE(0, 0, 960, 256), 0, 47, 47, 47, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 48, GET_CLUT(896, 483), 47, 48,
                GET_TPAGE(0, 0, 960, 256), 0, 71, 47, 71, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 72, GET_CLUT(896, 483), 47, 72,
                GET_TPAGE(0, 0, 960, 256), 0, 95, 47, 95, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 96, GET_CLUT(896, 483), 47, 96,
                GET_TPAGE(0, 0, 960, 256), 0, 119, 47, 119, { 0, 1, 2, 3 } },
        },
    },
    {
        EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
        {
            { -300, -600, 0, -1 },
            {  300, -600, 0, -1 },
            { -300,    0, 0, -1 },
            {  300,    0, 0, -1 },
        },
        5, 0, 1,
        {
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 0, GET_CLUT(896, 482), 47, 0,
                GET_TPAGE(0, 0, 960, 256), 0, 23, 47, 23, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 24, GET_CLUT(896, 482), 47, 24,
                GET_TPAGE(0, 0, 960, 256), 0, 47, 47, 47, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 48, GET_CLUT(896, 482), 47, 48,
                GET_TPAGE(0, 0, 960, 256), 0, 71, 47, 71, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 72, GET_CLUT(896, 482), 47, 72,
                GET_TPAGE(0, 0, 960, 256), 0, 95, 47, 95, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 96, GET_CLUT(896, 482), 47, 96,
                GET_TPAGE(0, 0, 960, 256), 0, 119, 47, 119, { 0, 1, 2, 3 } },
        },
    },
    {
        EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
        {
            { -300, -600, 0, -1 },
            {  300, -600, 0, -1 },
            { -300,    0, 0, -1 },
            {  300,    0, 0, -1 },
        },
        5, 0, 1,
        {
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 0, GET_CLUT(896, 481), 47, 0,
                GET_TPAGE(0, 0, 960, 256), 0, 23, 47, 23, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 24, GET_CLUT(896, 481), 47, 24,
                GET_TPAGE(0, 0, 960, 256), 0, 47, 47, 47, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 48, GET_CLUT(896, 481), 47, 48,
                GET_TPAGE(0, 0, 960, 256), 0, 71, 47, 71, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 72, GET_CLUT(896, 481), 47, 72,
                GET_TPAGE(0, 0, 960, 256), 0, 95, 47, 95, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 96, GET_CLUT(896, 481), 47, 96,
                GET_TPAGE(0, 0, 960, 256), 0, 119, 47, 119, { 0, 1, 2, 3 } },
        },
    },
    {
        EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
        {
            { -300, -600, 0, -1 },
            {  300, -600, 0, -1 },
            { -300,    0, 0, -1 },
            {  300,    0, 0, -1 },
        },
        5, 0, 1,
        {
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 0, GET_CLUT(896, 476), 47, 0,
                GET_TPAGE(0, 0, 960, 256), 0, 23, 47, 23, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 24, GET_CLUT(896, 476), 47, 24,
                GET_TPAGE(0, 0, 960, 256), 0, 47, 47, 47, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 48, GET_CLUT(896, 476), 47, 48,
                GET_TPAGE(0, 0, 960, 256), 0, 71, 47, 71, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 72, GET_CLUT(896, 476), 47, 72,
                GET_TPAGE(0, 0, 960, 256), 0, 95, 47, 95, { 0, 1, 2, 3 } },
            { { { 0x80, 0x80, 0x80, GPU_CODE_POLY_FT4 } }, 0, 96, GET_CLUT(896, 476), 47, 96,
                GET_TPAGE(0, 0, 960, 256), 0, 119, 47, 119, { 0, 1, 2, 3 } },
        },
    },
};

void SpawnBladeLockFlashEffect(int x, int y, int z)
{
    EffectSlot *effect;

    effect = AllocEffectSlot();
    if (effect != 0) {
        effect->active = EFFECT_SLOT_ACTIVE;
        effect->update = UpdateBladeLockFlashEffect;
        effect->mesh = gBladeLockFlashMesh;
        effect->pos.vx = x;
        effect->pos.vy = y;
        effect->pos.vz = z;
        effect->frameOrTick = 0;
        effect->subTick = 0;
    }
}

void UpdateBladeLockFlashEffect(EffectSlot *effect)
{
    EffectMesh4x3 *meshes;

    meshes = (EffectMesh4x3 *)effect->mesh;
    meshes[1].pos[0] = meshes[0].pos[0] = effect->pos.vx;
    meshes[1].pos[1] = meshes[0].pos[1] = effect->pos.vy;
    meshes[1].pos[2] = meshes[0].pos[2] = effect->pos.vz;
    if ((effect->subTick & 1) != 0) {
        meshes[0].flags = EFFECT_MESH_LAST;
        meshes[1].quadCount = 0;
        meshes[0].quadCount = 0;
    } else {
        meshes[0].flags = EFFECT_MESH_BILLBOARD;
        meshes[1].quadCount = 1;
        meshes[0].quadCount = 1;
        meshes[1].quadStart = meshes[0].quadStart = effect->frameOrTick;
    }
    effect->frameOrTick = ++effect->subTick >> 1;
    if (effect->frameOrTick >= 3) {
        effect->active = EFFECT_SLOT_FREE;
    }
}

EffectMesh4x5 *gBallisticParticleMeshes[4] = {
    &gSurfaceDebrisParticleMeshes[0],
    &gSurfaceDebrisParticleMeshes[1],
    &gSurfaceDebrisParticleMeshes[2],
    &gSurfaceDebrisParticleMeshes[3],
};

EffectMesh4x5 gBloodDropParticleMesh = {
    EFFECT_MESH_BILLBOARD | EFFECT_MESH_LAST, { 0, 0, 0 }, 4,
    {
        { -450, -450, 0, -1 },
        {  450, -450, 0, -1 },
        { -450,  450, 0, -1 },
        {  450,  450, 0, -1 },
    },
    5, 0, 1,
    {
        { { { 0xa0, 0x80, 0x00, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 0, GET_CLUT(896, 472), 191, 0,
            GET_TPAGE(0, 1, 960, 256), 144, 23, 191, 23, { 0, 1, 2, 3 } },
        { { { 0xa0, 0x80, 0x00, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 24, GET_CLUT(896, 472), 191, 24,
            GET_TPAGE(0, 1, 960, 256), 144, 47, 191, 47, { 0, 1, 2, 3 } },
        { { { 0xa0, 0x80, 0x00, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 48, GET_CLUT(896, 472), 191, 48,
            GET_TPAGE(0, 1, 960, 256), 144, 71, 191, 71, { 0, 1, 2, 3 } },
        { { { 0xa0, 0x80, 0x00, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 72, GET_CLUT(896, 472), 191, 72,
            GET_TPAGE(0, 1, 960, 256), 144, 95, 191, 95, { 0, 1, 2, 3 } },
        { { { 0xa0, 0x80, 0x00, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS } }, 144, 96, GET_CLUT(896, 472), 191, 96,
            GET_TPAGE(0, 1, 960, 256), 144, 119, 191, 119, { 0, 1, 2, 3 } },
    },
};

static void SpawnBallisticParticle(int kind, int x, int y, int z, int vx, int vy, int vz, int floorY)
{
    EffectSlot *effect;

    effect = AllocEffectSlot();
    if (effect != 0) {
        effect->active = EFFECT_SLOT_ACTIVE;
        effect->update = UpdateBallisticParticle;
        effect->mesh = gBallisticParticleMeshes[kind];
        effect->pos.vx = x;
        effect->pos.vy = y;
        effect->pos.vz = z;
        effect->frameOrTick = 0;
        effect->subTick = 0;
        effect->params.vel.x = vx;
        effect->params.vel.y = vy;
        effect->params.vel.z = vz;
        effect->floorY = floorY;
    }
}

void UpdateBallisticParticle(EffectSlot *effect)
{
    EffectMesh4x5 *mesh;
    short frame;

    mesh = (EffectMesh4x5 *)effect->mesh;
    mesh->pos[0] = effect->pos.vx;
    mesh->pos[1] = effect->pos.vy;
    mesh->pos[2] = effect->pos.vz;
    effect->pos.vx += effect->params.vel.x >> 4;
    effect->pos.vy += effect->params.vel.y >> 4;
    effect->pos.vz += effect->params.vel.z >> 4;
    effect->params.vel.y += 180;
    if (effect->floorY < mesh->pos[1]) {
        effect->active = EFFECT_SLOT_FREE;
    } else {
        mesh->quadStart = effect->frameOrTick;
        frame = ++effect->subTick >> 2;
        effect->frameOrTick = frame;
        if (frame >= 5) {
            effect->frameOrTick = 4;
        }
    }
}

void SpawnParticleBurst(int kind, VECTOR *position, int *direction)
{
    int yaw = ratan2(direction[0], direction[2]);
    short *sinTable = gSinLutQ12;
    short *yawSin = &sinTable[yaw & ANGLE_MASK];
    int ring, firstPitch, radius;

    for (ring = 0, firstPitch = 320, radius = 800;
         ring < 4;
         firstPitch += 64, ++ring, radius += 720) {
        int i, pitch;

        for (i = 0, pitch = firstPitch; i < 2; ++i, pitch += 192) {
            int xProduct, yProduct, zVelocity;
            int cosineAngle;

            zVelocity = (sinTable[pitch & ANGLE_MASK] * radius) >> 12;
            cosineAngle = (pitch + ANGLE_QUARTER) & ANGLE_MASK;
            yProduct = sinTable[cosineAngle] * radius;
            cosineAngle = (yaw + ANGLE_QUARTER) & ANGLE_MASK;
            xProduct = zVelocity * *yawSin;
            zVelocity = (zVelocity * sinTable[cosineAngle]) >> 12;
            /* direction[1] is the floor height the particles stop at, not a velocity. */
            SpawnBallisticParticle(kind, position->vx, position->vy,
                          position->vz, xProduct >> 12, -(yProduct >> 12), zVelocity, direction[1]);
        }
    }
}

static void SpawnBloodDropParticle(int unusedKind, int x, int y, int z, int vx, int vy, int vz)
{
    EffectSlot *effect;

    effect = AllocEffectSlot();
    if (effect != 0) {
        effect->active = EFFECT_SLOT_ACTIVE;
        effect->update = UpdateBloodDropParticle;
        effect->mesh = &gBloodDropParticleMesh;
        effect->pos.vx = x;
        effect->pos.vy = y;
        effect->pos.vz = z;
        effect->frameOrTick = 0;
        effect->subTick = 0;
        effect->params.vel.x = vx;
        effect->params.vel.y = vy;
        effect->params.vel.z = vz;
    }
}

CVECTOR gBloodParticleColor = { 0xa0, 0x80, 0, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS };

void UpdateBloodDropParticle(EffectSlot *effect)
{
    EffectMesh4x5 *mesh = (EffectMesh4x5 *)effect->mesh;
    short frame;

    mesh->pos[0] = effect->pos.vx;
    mesh->pos[1] = effect->pos.vy;
    mesh->pos[2] = effect->pos.vz;
    effect->pos.vx += effect->params.vel.x >> 4;
    effect->pos.vy += effect->params.vel.y >> 4;
    effect->pos.vz += effect->params.vel.z >> 4;
    effect->params.vel.y += 140;
    mesh->quadStart = effect->frameOrTick;
    frame = ++effect->subTick >> 2;
    effect->frameOrTick = frame;
    if (frame >= 5) {
        effect->active = EFFECT_SLOT_FREE;
    }
    (mesh->quads + mesh->quadStart)->tint.color = gBloodParticleColor;
}

/* The default blood tint with its label. */
typedef struct {
    CVECTOR color;
    char label[12];
} NamedColor;

#ifdef VERSION_JP
NamedColor gDefaultBloodColor = { { 0xa0, 0x80, 0x00, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS }, "BLOOD COLOR" };
#else
NamedColor gDefaultBloodColor = { { 0xe0, 0x00, 0x00, GPU_CODE_POLY_FT4 }, "BLOOD COLOR" };
#endif

short gBloodSprayLocalVelocities[3][3] = { { 0, 499, 2510 }, { 432, -249, 2510 }, { -432, -249, 2510 } };
/* Local spray velocity tables: each sub-table is { count, vx0, vy0, vz0, ... } in Q4 units. */
short gSprayVelocitiesPrimary[18] = {
    3,    0,  499, 2510,  432, -249, 2510, -432, -249, 2510,
    1,    0, -249, 1255,
    1,    0,  131,  659,
};

short gSprayVelocitiesSecondary[14] = {
    3,    0,  109, 2237,   95,  -54, 2237,  -95,  -54, 2237,
    1,    0, -137, 1255,
};

static void UpdateBloodSprayEmitter(EffectSlot *effect)
{
    SVECTOR rotation;
    VECTOR velocity;
    MATRIX matrix;
    short *localVel;
    int index;

    if (++effect->subTick >= 3) {
        rotation.vx = effect->params.emitter.pitch;
        rotation.vy = effect->params.emitter.yaw;
        rotation.vz = 0;
        BuildRotMatrixYXZ(&rotation, &matrix);
        localVel = gBloodSprayLocalVelocities[0];
        index = 0;
        do {
            rotation.vx = *localVel++;
            rotation.vy = *localVel++;
            rotation.vz = *localVel++;
            ApplyMatrix(&matrix, &rotation, &velocity);
            SpawnBloodDropParticle(0, effect->pos.vx, effect->pos.vy, effect->pos.vz,
                          velocity.vx, velocity.vy, velocity.vz);
        } while (++index < 3);
        effect->subTick = 0;
        if (--effect->params.emitter.wavesLeft <= 0) {
            effect->active = EFFECT_SLOT_FREE;
        }
    }
}

void SpawnBloodSpray(VECTOR *position, VECTOR *direction)
{
    EffectSlot *effect = AllocEffectSlot();
    VECTOR normal;
    int yaw, inverseYaw, pitch;

    if (effect != 0) {
        effect->active = EFFECT_SLOT_UPDATE_ONLY;
        effect->update = UpdateBloodSprayEmitter;
        VectorNormal(direction, &normal);
        yaw = ratan2(normal.vx, normal.vz);
        inverseYaw = -yaw;
        pitch = ratan2(normal.vy,
                      (gSinLutQ12[(inverseYaw + ANGLE_QUARTER) & ANGLE_MASK] * normal.vz -
                       gSinLutQ12[inverseYaw & ANGLE_MASK] * normal.vx) >> 12);
        effect->subTick = 0;
        effect->params.emitter.wavesLeft = 2;
        effect->params.emitter.pitch = -pitch;
        effect->params.emitter.yaw = yaw;
        effect->pos = *position;
        if (gRendererMonochrome != 0) {
            gBloodParticleColor.cd = GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS;
            gBloodParticleColor.r = 0x74;
            gBloodParticleColor.g = 0x74;
            gBloodParticleColor.b = 0x74;
        } else {
            gBloodParticleColor = gDefaultBloodColor.color;
        }
    }
}

static void SpawnSprayParticle(int kind, int x, int y, int z, int vx, int vy, int vz)
{
    EffectSlot *effect;

    effect = AllocEffectSlot();
    if (effect != 0) {
        effect->active = EFFECT_SLOT_ACTIVE;
        effect->update = UpdateSprayParticle;
        effect->variant = kind;
        effect->mesh = &gSprayParticleMesh;
        effect->pos.vx = x;
        effect->pos.vy = y;
        effect->pos.vz = z;
        effect->frameOrTick = 0;
        effect->subTick = 0;
        effect->params.vel.x = vx;
        effect->params.vel.y = vy;
        effect->params.vel.z = vz;
    }
}

void UpdateSprayParticle(EffectSlot *effect)
{
    EffectMesh4x7 *mesh = (EffectMesh4x7 *)effect->mesh;
    int frame;

    mesh->pos[0] = effect->pos.vx;
    mesh->pos[1] = effect->pos.vy;
    mesh->pos[2] = effect->pos.vz;

    effect->pos.vx += effect->params.vel.x >> 4;
    effect->pos.vy += effect->params.vel.y >> 4;
    effect->pos.vz += effect->params.vel.z >> 4;

    switch ((unsigned int)effect->variant) {
    case SPRAY_KIND_NONE:
        break;

    case SPRAY_KIND_STREAM:
        frame = effect->frameOrTick >> 1;
        effect->params.vel.y += 200;
        if (frame >= 6) {
            effect->active = EFFECT_SLOT_FREE;
        } else {
            mesh->quads[frame].tint.rgbc = GPU_CODE_WORD(GPU_CODE_POLY_FT4) | GPU_RGB_NEUTRAL;
        }
        break;

    case SPRAY_KIND_WATERFALL:
        frame = effect->frameOrTick >> 1;
        effect->params.vel.y += 140;
        if (frame >= 7) {
            effect->active = EFFECT_SLOT_FREE;
        } else {
            mesh->quads[frame].tint.rgbc = GPU_CODE_WORD(GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS) | GPU_RGB_NEUTRAL;
        }
        break;

    case SPRAY_KIND_WATER_SURFACE:
        frame = effect->frameOrTick >> 2;
        effect->params.vel.y += 200;
        if (frame >= 7)
            frame = 6;
        if (effect->pos.vy > MOAT_WATER_SURFACE_Y) {
            effect->active = EFFECT_SLOT_FREE;
        } else {
            mesh->quads[frame].tint.rgbc = GPU_CODE_WORD(GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS) | GPU_RGB_NEUTRAL;
        }
        break;

    case SPRAY_KIND_SHORELINE:
        frame = effect->frameOrTick >> 1;
        effect->params.vel.y += 200;
        if (frame >= 7)
            frame = 6;
        if (effect->pos.vy > 400) {
            effect->active = EFFECT_SLOT_FREE;
        } else {
            (mesh->quads + frame)->tint.rgbc = GPU_CODE_WORD(GPU_CODE_POLY_FT4) | GPU_RGB_NEUTRAL;
        }
        break;
    }

    ++effect->frameOrTick;
    mesh->quadStart = frame;
}

short *gSprayVelocityTables[5] = {
    &gSprayVelocitiesPrimary[0],
    &gSprayVelocitiesPrimary[10],
    &gSprayVelocitiesPrimary[14],
    &gSprayVelocitiesSecondary[0],
    &gSprayVelocitiesSecondary[10],
};

void SpawnDirectionalSpray(int kind, VECTOR *position, VECTOR *direction, int scale)
{
    SVECTOR angles;
    VECTOR velocity;
    MATRIX matrix;
    int yaw;
    short *tableCursor;
    int count;
    int i;

    VectorNormal(direction, &velocity);
    yaw = ratan2(velocity.vx, velocity.vz);
    angles.vx = -ratan2(velocity.vy,
        (gSinLutQ12[(-yaw + ANGLE_QUARTER) & ANGLE_MASK] * velocity.vz -
         gSinLutQ12[-yaw & ANGLE_MASK] * velocity.vx) >> 12);
    angles.vy = yaw;
    angles.vz = 0;
    BuildRotMatrixYXZ(&angles, &matrix);

    tableCursor = gSprayVelocityTables[kind];
    count = *tableCursor++;
    for (i = 0; i < count; i++) {
        angles.vx = (*tableCursor++ * scale) >> 12;
        angles.vy = (*tableCursor++ * scale) >> 12;
        angles.vz = (*tableCursor++ * scale) >> 12;
        ApplyMatrix(&matrix, &angles, &velocity);
        SpawnSprayParticle(kind, position->vx, position->vy, position->vz,
                     velocity.vx, velocity.vy, velocity.vz);
    }
}

void SpawnDirectionalSprayUnit(int kind, VECTOR *position, VECTOR *direction)
{
    SpawnDirectionalSpray(kind, position, direction, ONE);
}
