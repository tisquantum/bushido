/* Combat weapon slash trails: each fighter's blade endpoints kept in an eight-edge history,
 * smoothed, and drawn as semi-transparent POLY_GT4 strips subdivided by camera distance. */

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <inline_o.h>

#include "weapon_trail.h"
#include "fighter.h"
#include "fighter_hit_sphere.h"
#include "collision_scratch.h"
#include "game_state.h"
#include "link.h"
#include "display_buffer.h"
#include "gpu_code.h"
#include "cloth.h"
#include "combat_event.h"
#include "main.h"
#include "rot_matrix.h"
#include "globals.h"
#include "combat_state.h"

static WeaponTrailState gWeaponTrails[2][WEAPON_TRAIL_MAX_PER_FIGHTER];
static POLY_GT4 gWeaponTrailPackets[2][2][2][WEAPON_TRAIL_MAX_QUADS]; /* [frame buffer][player][trail][quad] */

/* At the start of the scratchpad, over CollisionScratch.bodyPartPos; the weapon points this reads
 * at +0x210 stay intact. */
typedef struct WeaponTrailWork {
    MATRIX matrix;
    VECTOR origin;
    POLY_GT4 *nextPacket;
    WeaponTrailState *trail;
    WorldVector first;
    WorldVector last;
    unsigned int redTrail;
    SVECTOR vertices[4];
    WorldVector firstDelta;
    WorldVector secondDelta;
    WorldVector blend;
    unsigned int depth;
} WeaponTrailWork;

#define WEAPON_TRAIL_WORK ((WeaponTrailWork *)getScratchAddr(0))

#define PACK_TRAIL_COLOR(r, g, b) \
    (GPU_CODE_WORD(GPU_CODE_POLY_GT4 | GPU_CODE_SEMITRANS) | ((unsigned int)(b) << 16) | ((unsigned int)(g) << 8) | (r))

/* LOD 3 to 0: 8, 4, 2 and 1 slices. */
unsigned int gWeaponTrailLodDists[WEAPON_TRAIL_LOD_BANDS] = { 500, 1000, 2000, 4000 };

/* For each LOD 0..3, indices into WeaponTrailState.uvRamp ended by 0xff. */
unsigned char gWeaponTrailUvSteps[4][16] = {
    {
        0x00, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff,
    },
    {
        0x00, 0x08, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff,
    },
    {
        0x00, 0x04, 0x08, 0x0c, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff,
    },
    {
        0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff,
    },
};
/* 0 to 16 by ones: the next row of the pattern above, ended by zeros, not 0xff; nothing reads these 32 bytes. */
unsigned char gWeaponTrailUvStepsUnused[2][16] = {
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
        0x0f,
    },
    {
        0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00,
    },
};

static void InitWeaponTrailPackets(WeaponTrailState *trail)
{
    u_short average[2];
    int i, j;
    POLY_GT4 *packet;

    for (i = 0; i < WEAPON_TRAIL_UV_STEPS; ++i) {
        LoadAverageByte(trail->uvStart, trail->uvEnd,
                        ((WEAPON_TRAIL_UV_STEPS - 1) - i) << 8, i << 8, (u_char *)average);
        trail->uvRamp[i] = average[0];
    }

    trail->flags = WEAPON_TRAIL_FLAG_RESET;
    for (i = 0; i < 2; ++i) {
        packet = trail->packets[i];
        for (j = 0; j < WEAPON_TRAIL_MAX_QUADS; ++j, ++packet) {
            SetPolyGT4(packet);
            packet->tpage = trail->tpage;
            packet->clut = trail->clut;
        }
    }
}

static int TrailSubdivisionLevel(WeaponTrailWork *work, WorldVector *first, WorldVector *second, VECTOR *origin)
{
    unsigned int squared, distance, shift;
    int leading;
    int i;

    work->firstDelta.x = (first->x + second->x) / 2 - origin->vx;
    work->firstDelta.y = (first->y + second->y) / 2 - origin->vy;
    work->firstDelta.z = (first->z + second->z) / 2 - origin->vz;
    if ((unsigned int)(work->firstDelta.x + 8192) > 16384 ||
        (unsigned int)(work->firstDelta.y + 8192) > 16384 ||
        (unsigned int)(work->firstDelta.z + 8192) > 16384) {
        return 0;
    }
    gte_ldlvl(&work->firstDelta);
    gte_sqr0();
    gte_stlvnl(&work->firstDelta);
    squared = work->firstDelta.x + work->firstDelta.y + work->firstDelta.z;
    if (squared < 1024) {
        distance = gSqrtLutQ3[squared] >> 3;
    } else {
        gte_ldlzc(squared);
        gte_nop();
        gte_nop();
        gte_stlzc(&leading);
        shift = leading;
        shift = 22 - (shift & ~1);
        squared = gSqrtLutQ3[squared >> shift];
        distance = (squared << 16) >> (19 - (shift >> 1));
    }
    for (i = 0; i < WEAPON_TRAIL_LOD_BANDS; ++i) {
        if (distance < gWeaponTrailLodDists[i]) {
            return (WEAPON_TRAIL_LOD_BANDS - 1) - i;
        }
    }
    return 0;
}

static void SmoothTrailPoint(WeaponTrailWork *work,
                             WorldVector *first, WorldVector *middle, WorldVector *last, WorldVector *out)
{
    unsigned int squared;
    int firstLeading, secondLeading, blendLeading;
    int firstLength, secondLength, blendLength;
    int cosine, firstWeight, secondWeight;
    int evenLeading;

    work->firstDelta.x = middle->x - first->x;
    work->firstDelta.y = middle->y - first->y;
    work->firstDelta.z = middle->z - first->z;
    gte_ldlvl(&work->firstDelta);
    gte_sqr0();
    work->secondDelta.x = last->x - middle->x;
    work->secondDelta.y = last->y - middle->y;
    work->secondDelta.z = last->z - middle->z;
    gte_stlvnl(&work->blend);
    squared = work->blend.x + work->blend.y + work->blend.z;
    if (squared < 1024) {
        firstLength = gSqrtLutQ3[squared] >> 3;
    } else {
        unsigned int shift;

        gte_ldlzc(squared);
        gte_nop();
        gte_nop();
        gte_stlzc(&firstLeading);
        evenLeading = firstLeading & ~1;
        shift = 22 - evenLeading;
        firstLength = ((unsigned int)gSqrtLutQ3[squared >> shift] << 16) >> (19 - (shift >> 1));
    }

    gte_ldlvl(&work->secondDelta);
    gte_sqr0();
    gte_stlvnl(&work->blend);
    squared = work->blend.x + work->blend.y + work->blend.z;
    if (squared < 1024) {
        secondLength = gSqrtLutQ3[squared] >> 3;
    } else {
        unsigned int shift;

        gte_ldlzc(squared);
        gte_nop();
        gte_nop();
        gte_stlzc(&secondLeading);
        evenLeading = secondLeading & ~1;
        shift = 22 - evenLeading;
        secondLength = ((unsigned int)gSqrtLutQ3[squared >> shift] << 16) >> (19 - (shift >> 1));
    }

    if (firstLength == 0 || secondLength == 0) {
        cosine = ONE;
    } else {
        cosine = work->firstDelta.x * work->secondDelta.x +
                 work->firstDelta.y * work->secondDelta.y +
                 work->firstDelta.z * work->secondDelta.z;
        cosine = (cosine << 6) / firstLength;
        cosine = (cosine << 6) / secondLength;
    }
    firstWeight = (ONE - cosine) / 2;
    secondWeight = ONE - firstWeight;
    gte_lddp(firstWeight);
    gte_ldlvl(&work->firstDelta);
    gte_gpf12();
    gte_lddp(secondWeight);
    gte_ldlvl(&work->secondDelta);
    gte_gpl12();
    gte_stlvl(&work->blend);
    gte_ldlvl(&work->blend);
    gte_sqr0();
    gte_stlvnl(&work->firstDelta);
    squared = work->firstDelta.x + work->firstDelta.y + work->firstDelta.z;
    if (squared < 1024) {
        blendLength = gSqrtLutQ3[squared] >> 3;
    } else {
        unsigned int shift;

        gte_ldlzc(squared);
        gte_nop();
        gte_nop();
        gte_stlzc(&blendLeading);
        evenLeading = blendLeading & ~1;
        shift = 22 - evenLeading;
        blendLength = ((unsigned int)gSqrtLutQ3[squared >> shift] << 16) >> (19 - (shift >> 1));
    }
    if (blendLength == 0) {
        blendLength = 1;
    }
    out->x = middle->x + (work->blend.x * secondLength / 2) / blendLength;
    out->y = middle->y + (work->blend.y * secondLength / 2) / blendLength;
    out->z = middle->z + (work->blend.z * secondLength / 2) / blendLength;
}

static void DrawWeaponTrail(WeaponTrailWork *work)
{
    WeaponTrailState *trail = work->trail;
    WeaponTrailEdge *older, *middle, *newest;
    unsigned int trailFlags = trail->flags;
    int age, slice;
    int lod, scale, parts, newestIndex;
    POLY_GT4 *packet;
    unsigned int startColor, endColor;

    if (trailFlags & WEAPON_TRAIL_FLAG_RESET) {
        trail->flags = trailFlags & ~WEAPON_TRAIL_FLAG_RESET;
        for (age = 0; age < WEAPON_TRAIL_HISTORY_LEN; ++age) {
            trail->edges[age].first = work->first;
            trail->edges[age].last = work->last;
        }
        trail->cursor = 0;
    } else if (!(trailFlags & WEAPON_TRAIL_FLAG_FREEZE)) {
        int cursor = trail->cursor;
        int next;
        age = (cursor - 1) & WEAPON_TRAIL_HISTORY_MASK;
        next = (cursor + 1) & WEAPON_TRAIL_HISTORY_MASK;
        trail->edges[next].first = work->first;
        trail->edges[next].last = work->last;
        older = &trail->edges[(cursor - 3) & WEAPON_TRAIL_HISTORY_MASK];
        middle = &trail->edges[age];
        newest = &trail->edges[next];
        SmoothTrailPoint(work, &older->first, &middle->first, &newest->first,
                         &trail->edges[trail->cursor].first);
        SmoothTrailPoint(work, &older->last, &middle->last, &newest->last,
                         &trail->edges[trail->cursor].last);
        trail->cursor = (trail->cursor + 2) & WEAPON_TRAIL_HISTORY_MASK;
    }

    SetRotMatrix(&work->matrix);
    SetTransMatrix(&gZeroTransMatrix);
    lod = TrailSubdivisionLevel(work,
                                &trail->edges[(trail->cursor - 1) & WEAPON_TRAIL_HISTORY_MASK].first,
                                &trail->edges[trail->cursor].last,
                                &work->origin);
    parts = 1 << lod;
    scale = 12 - lod;
    newestIndex = trail->cursor - 1;
    packet = work->nextPacket;
    for (age = 0; age < WEAPON_TRAIL_VISIBLE_SEGMENTS; ++age) {
        if (gOptionMonochrome == 0) {
            if (work->redTrail != 0) {
                startColor = (WEAPON_TRAIL_VISIBLE_SEGMENTS - age) * 85 / 2;
                startColor = PACK_TRAIL_COLOR(startColor, startColor >> 1, startColor >> 1);
                endColor = (WEAPON_TRAIL_VISIBLE_SEGMENTS - 1 - age) * 85 / 2;
                endColor = PACK_TRAIL_COLOR(endColor, endColor >> 1, endColor >> 1);
            } else {
                startColor = (WEAPON_TRAIL_VISIBLE_SEGMENTS - age) * 128 / WEAPON_TRAIL_VISIBLE_SEGMENTS;
                startColor = PACK_TRAIL_COLOR(startColor, startColor, startColor);
                endColor = (WEAPON_TRAIL_VISIBLE_SEGMENTS - 1 - age) * 128 / WEAPON_TRAIL_VISIBLE_SEGMENTS;
                endColor = PACK_TRAIL_COLOR(endColor, endColor, endColor);
            }
        } else {
            if (work->redTrail != 0) {
                startColor = (WEAPON_TRAIL_VISIBLE_SEGMENTS - age) * 85 / 2;
                endColor = (WEAPON_TRAIL_VISIBLE_SEGMENTS - 1 - age) * 85 / 2;
            } else {
                startColor = (WEAPON_TRAIL_VISIBLE_SEGMENTS - age) * 128 / WEAPON_TRAIL_VISIBLE_SEGMENTS;
                endColor = (WEAPON_TRAIL_VISIBLE_SEGMENTS - 1 - age) * 128 / WEAPON_TRAIL_VISIBLE_SEGMENTS;
            }
            startColor = PACK_TRAIL_COLOR(startColor, startColor, startColor);
            endColor = PACK_TRAIL_COLOR(endColor, endColor, endColor);
        }

        for (slice = 0; slice < parts; ++slice) {
            int edge = (newestIndex - age) & WEAPON_TRAIL_HISTORY_MASK;
            unsigned int left = (unsigned int)(parts - slice) << scale;
            unsigned int right;
            unsigned int u0, u1;
            unsigned int high, low;
            SVECTOR *vertex = work->vertices;
            if (left == 0) {
                u0 = gWeaponTrailUvSteps[lod][slice];
                vertex->vx = (trail->edges + edge)->last.x - work->origin.vx;
                vertex->vy = (trail->edges + edge)->last.y - work->origin.vy;
                left = (unsigned int)-1 << scale;
                vertex->vz = (trail->edges + edge)->last.z - work->origin.vz;
            } else if ((right = (unsigned int)(slice) << scale) == 0) {
                u0 = gWeaponTrailUvSteps[lod][slice];
                vertex->vx = (trail->edges + edge)->first.x - work->origin.vx;
                vertex->vy = (trail->edges + edge)->first.y - work->origin.vy;
                left = (unsigned int)(parts - (slice + 1)) << scale;
                vertex->vz = (trail->edges + edge)->first.z - work->origin.vz;
            } else {
                gte_lddp(left);
                gte_ldlvl(&trail->edges[edge].first);
                gte_gpf12();
                left = (unsigned int)(parts - (slice + 1)) << scale;
                gte_lddp(right);
                gte_ldlvl(&trail->edges[edge].last);
                gte_gpl12();
                u0 = gWeaponTrailUvSteps[lod][slice];
                gte_stlvl(&work->firstDelta);
                vertex->vx = work->firstDelta.x - work->origin.vx;
                vertex->vy = work->firstDelta.y - work->origin.vy;
                vertex->vz = work->firstDelta.z - work->origin.vz;
            }

            ++vertex;
            if (left == 0) {
                u1 = gWeaponTrailUvSteps[lod][slice + 1];
                vertex->vx = (trail->edges + edge)->last.x - work->origin.vx;
                vertex->vy = (trail->edges + edge)->last.y - work->origin.vy;
                left = 1U << scale;
                vertex->vz = (trail->edges + edge)->last.z - work->origin.vz;
            } else if ((right = (unsigned int)(slice + 1) << scale) == 0) {
                u1 = gWeaponTrailUvSteps[lod][slice + 1];
                vertex->vx = (trail->edges + edge)->first.x - work->origin.vx;
                vertex->vy = (trail->edges + edge)->first.y - work->origin.vy;
                left = (unsigned int)(parts - slice) << scale;
                vertex->vz = (trail->edges + edge)->first.z - work->origin.vz;
            } else {
                gte_lddp(left);
                gte_ldlvl(&trail->edges[edge].first);
                gte_gpf12();
                left = (unsigned int)(parts - slice) << scale;
                gte_lddp(right);
                gte_ldlvl(&trail->edges[edge].last);
                gte_gpl12();
                u1 = gWeaponTrailUvSteps[lod][slice + 1];
                gte_stlvl(&work->firstDelta);
                vertex->vx = work->firstDelta.x - work->origin.vx;
                vertex->vy = work->firstDelta.y - work->origin.vy;
                vertex->vz = work->firstDelta.z - work->origin.vz;
            }

            edge = (newestIndex - age - 1) & WEAPON_TRAIL_HISTORY_MASK;
            ++vertex;
            if (left == 0) {
                *(u_long *)&packet->r0 = startColor;
                vertex->vx = (trail->edges + edge)->last.x - work->origin.vx;
                vertex->vy = (trail->edges + edge)->last.y - work->origin.vy;
                left = (unsigned int)-1 << scale;
                vertex->vz = (trail->edges + edge)->last.z - work->origin.vz;
            } else if ((right = (unsigned int)(slice) << scale) == 0) {
                *(u_long *)&packet->r0 = startColor;
                vertex->vx = (trail->edges + edge)->first.x - work->origin.vx;
                vertex->vy = (trail->edges + edge)->first.y - work->origin.vy;
                left = (unsigned int)(parts - (slice + 1)) << scale;
                vertex->vz = (trail->edges + edge)->first.z - work->origin.vz;
            } else {
                gte_lddp(left);
                gte_ldlvl(&trail->edges[edge].first);
                gte_gpf12();
                left = (unsigned int)(parts - (slice + 1)) << scale;
                gte_lddp(right);
                gte_ldlvl(&trail->edges[edge].last);
                gte_gpl12();
                *(u_long *)&packet->r0 = startColor;
                gte_stlvl(&work->firstDelta);
                vertex->vx = work->firstDelta.x - work->origin.vx;
                vertex->vy = work->firstDelta.y - work->origin.vy;
                vertex->vz = work->firstDelta.z - work->origin.vz;
            }

            ++vertex;
            if (left == 0) {
                *(u_long *)&packet->r1 = startColor;
                *(u_long *)&packet->r2 = endColor;
                vertex->vx = (trail->edges + edge)->last.x - work->origin.vx;
                vertex->vy = (trail->edges + edge)->last.y - work->origin.vy;
                vertex->vz = (trail->edges + edge)->last.z - work->origin.vz;
            } else if ((right = (unsigned int)(slice + 1) << scale) == 0) {
                *(u_long *)&packet->r1 = startColor;
                *(u_long *)&packet->r2 = endColor;
                vertex->vx = (trail->edges + edge)->first.x - work->origin.vx;
                vertex->vy = (trail->edges + edge)->first.y - work->origin.vy;
                vertex->vz = (trail->edges + edge)->first.z - work->origin.vz;
            } else {
                gte_lddp(left);
                gte_ldlvl(&trail->edges[edge].first);
                gte_gpf12();
                *(u_long *)&packet->r1 = startColor;
                gte_lddp(right);
                gte_ldlvl(&trail->edges[edge].last);
                gte_gpl12();
                *(u_long *)&packet->r2 = endColor;
                gte_stlvl(&work->firstDelta);
                vertex->vx = work->firstDelta.x - work->origin.vx;
                vertex->vy = work->firstDelta.y - work->origin.vy;
                vertex->vz = work->firstDelta.z - work->origin.vz;
            }

            gte_ldv3(&work->vertices[0], &work->vertices[1], &work->vertices[2]);
            gte_rtpt();
            *(u_short *)&packet->u0 = trail->uvRamp[u0];
            *(u_short *)&packet->u1 = trail->uvRamp[u1];
            gte_stsxy3(&packet->x0, &packet->x1, &packet->x2);
            gte_ldv0(&work->vertices[3]);
            gte_rtps();
            *(u_short *)&packet->u2 = trail->uvRamp[u0];
            *(u_short *)&packet->u3 = trail->uvRamp[u1];
            gte_stsxy(&packet->x3);
            gte_avsz4();
            *(u_long *)&packet->r3 = endColor;
            gte_stotz(&work->depth);
            high = work->depth;
            low = high >> 1;
            high >>= 12;
            low &= 0x7ff;
            work->depth = ((low | 0xfffff000U) >> high) & 0xfff;
            if (work->depth < DISPLAY_OT_LEN) {
                AddPrim(&gCurrentOt[work->depth], packet);
            }
            ++packet;
        }
    }
    work->nextPacket = packet;
}

void ResetFighterWeaponTrails(int player)
{
    int count = gWeaponTrailCount[gFighters[player].weaponId];
    int i;

    for (i = 0; i < count; i++) {
        gWeaponTrails[player][i].flags |= WEAPON_TRAIL_FLAG_RESET;
    }
}

void DrawAllWeaponTrails(void)
{
    WeaponTrailWork *work = WEAPON_TRAIL_WORK;
    CollisionScratch *scratch;
    int player, i;
    Fighter *fighter;
    POLY_GT4 *nextPacket;
    int count;
    WeaponTrailSpan *span;

    scratch = COLLISION_SCRATCH;
    if (gGameState == GAME_STATE_UPDATE_COMBAT && gPovModeEnabled == 1) {
        return;
    }
    for (player = 0; player < 2; ++player) {
        fighter = &gFighters[player];
        if (gGameState != GAME_STATE_UPDATE_BATTLE_REPLAY) {
            if (fighter->action == FIGHTER_ACTION_DEAD || fighter->action == FIGHTER_ACTION_DEFEATED) {
                ResetFighterWeaponTrails(player);
                continue;
            }
            if (ShouldSuppressRemoteEffects(player) != 0) {
                continue;
            }
        }
        work->redTrail = fighter->animInfo->flags & ANIM_FLAG_RED_TRAIL;
        nextPacket = gWeaponTrailPackets[gFrameCounter & 1][player][0];
        count = gWeaponTrailCount[fighter->weaponId];
        for (i = 0; i < count; ++i) {
            span = &gWeaponTrailEndpoints[fighter->weaponId][i];
            if (span->first != span->last) {
                work->trail = &gWeaponTrails[player][i];
                work->first = scratch->weaponPoints[player][span->first];
                work->last = scratch->weaponPoints[player][span->last];
                work->matrix = *gCameraMatrix;
                work->origin.vx = gCameraMatrix->t[0];
                work->origin.vy = gCameraMatrix->t[1];
                work->origin.vz = gCameraMatrix->t[2];
                work->nextPacket = nextPacket;
                DrawWeaponTrail(work);
                nextPacket = work->nextPacket;
            }
        }
    }
}

void InitFighterWeaponTrails(int player)
{
    int i, count;
    WeaponTrailState *trail;
    int usePovOpponentClut;

    count = gWeaponTrailCount[gFighters[player].weaponId];
    for (i = 0; i < count; ++i) {
        int start, end;
        trail = &gWeaponTrails[player][i];
        start = i * 63 / count;
        end = ((i + 1) * 63) / count;
        trail->uvStart[1] = 200;
        trail->uvEnd[1] = 200;
        trail->uvStart[0] = start;
        trail->uvEnd[0] = end;
        if (player == 0) {
            trail->tpage = GetTPage(0, 1, 0x300, 200);
        } else {
            trail->tpage = GetTPage(0, 1, 0x3c0, 200);
        }

        usePovOpponentClut = 0;
        if (gPovModeEnabled == 1) {
            if (gPlayMode == PLAY_MODE_LINK) {
                if ((gLinkRole == LINK_ROLE_CLIENT && player == 0) ||
                    (gLinkRole == LINK_ROLE_HOST && player == 1)) {
                    usePovOpponentClut = 1;
                }
            } else {
                usePovOpponentClut = player == 1;
            }
        }
        if (usePovOpponentClut) {
            trail->clut = GetClut(0x40, 0x1ff);
        } else {
            trail->clut = GetClut(player == 0 ? 0x330 : 0x3f0, 0xc9);
        }
        trail->packets[0] = gWeaponTrailPackets[0][player][i];
        trail->packets[1] = gWeaponTrailPackets[1][player][i];
        InitWeaponTrailPackets(trail);
    }
}
