/* Sub-weapons: the thrown-item slots (sixteen; eleven in Japan) of character sub-weapons and surface debris, their
 * flight, bounce, pickup and hits on fighters, their trails, and the per-slot history replays read. */

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <memory.h>
#include <inline_o.h>

#include "angle.h"
#include "combat_hit.h"
#include "display_buffer.h"
#include "gpu_code.h"
#include "fighter.h"
#include "world_vector.h"
#include "fighter_hit_sphere.h"
#include "collision_scratch.h"
#include "floor_collision.h"
#include "stage_id.h"
#include "effect.h"
#include "geometry.h"
#include "main.h"
#include "rot_matrix.h"
#include "scene_math.h"
#include "subweapon.h"
#include "subweapon_node.h"
#include "stage_moat.h"
#include "globals.h"
#include "combat_state.h"
#include "replay.h"

typedef struct SubWeaponTrailScratch {
    VECTOR cameraOrigin;
    SVECTOR verts[4];
    long screenXY[2];
    u_long otz;
} SubWeaponTrailScratch;

#define TRAIL_TPAGE_ADDITIVE          GET_TPAGE(0, 1, 0, 0)
#define SUB_WEAPON_BURST_FLOOR_OFFSET 800
#define SUB_WEAPON_LOADOUT_MAX        5
#define SUB_WEAPON_WALL_NORMAL_Y_MAX  2048 /* Q12 0.5: |normal.vy| < 0.5 reflects as a wall */
#define SUB_WEAPON_GUARD_PARRY_MASK   ((1 << 1) | (1 << 3)) /* sub-weapon IDs 1 and 3 parry on guard
                                                               instead of guard-breaking */

SubWeaponSlot gSubWeaponSlots[SUB_WEAPON_MAX];

/* Launch and trail parameters indexed by sub-weapon ID: 0 none, 1..5 character sub-weapons, 8..11 surface debris. */
SubWeaponParams gSubWeaponParams[12] = {
    {   0,   0,    0, 100, 0x000000 },
    { 512, -50, 2048, 107, 0x383818 },
    { 300, -60, 1152, 250, 0x183030 },
    { 480, -50, 1536, 190, 0x383830 },
    { 400, -50, 2048, 390, 0x303020 },
    { 460, -55, 2048, 280, 0x383830 },
    { 512, -50,    0, 100, 0x000000 },
    { 512, -50,    0, 100, 0x000000 },
    { 330, -90,    0, 100, 0x000000 },
    { 330, -90,    0, 100, 0x000000 },
    { 330, -90,    0, 100, 0x000000 },
    { 330, -90,    0, 100, 0x000000 },
};

short gSubWeaponHistoryIndex[SUB_WEAPON_MAX];
SubWeaponHistoryEntry gSubWeaponHistory[SUB_WEAPON_MAX][SUB_WEAPON_HISTORY_COUNT];
POLY_G4 gSubWeaponTrailG4[2][SUB_WEAPON_MAX];
POLY_F4 gSubWeaponTrailF4[2][SUB_WEAPON_MAX];
DR_TPAGE gSubWeaponTrailTPage[2][SUB_WEAPON_MAX];
unsigned char gSubWeaponModelOwner[16];

static void DrawSubWeaponTrail(int slotIndex, SubWeaponSlot *slot, int trailRgb)
{
    SubWeaponTrailScratch *scratch;
    POLY_G4 *trailQuad;
    POLY_F4 *tipQuad;
    DR_TPAGE *tpage;
    int dx, dz;
    int trailWidth;
    int perpAngle;
    int offsetX, offsetZ;
    int bufferIndex;
    int tipAngle;
    int weaponYaw;
    u_long halfDepth;
    u_long depthShift;

    dx = slot->motion.prevPosition.x - slot->motion.position.x;
    dz = slot->motion.prevPosition.z - slot->motion.position.z;
    trailWidth = gSubWeaponParams[slot->id].trailWidth;
    scratch = (SubWeaponTrailScratch *)getScratchAddr(0);
    if (dx * dx + dz * dz < trailWidth * trailWidth / 4) {
        return;
    }
    perpAngle = ratan2(dx, dz) + ANGLE_QUARTER;
    offsetX = gSinLutQ12[perpAngle & ANGLE_MASK] * trailWidth >> 12;
    offsetZ = gSinLutQ12[(perpAngle + ANGLE_QUARTER) & ANGLE_MASK] * trailWidth >> 12;
    scratch->cameraOrigin.vx = gCameraMatrix->t[0];
    scratch->cameraOrigin.vy = gCameraMatrix->t[1];
    scratch->cameraOrigin.vz = gCameraMatrix->t[2];
    scratch->verts[0].vx = slot->motion.position.x + offsetX - offsetZ - scratch->cameraOrigin.vx;
    scratch->verts[0].vy = slot->motion.position.y - scratch->cameraOrigin.vy;
    scratch->verts[0].vz = slot->motion.position.z + offsetZ + offsetX - scratch->cameraOrigin.vz;
    scratch->verts[1].vx = slot->motion.position.x - offsetX - offsetZ - scratch->cameraOrigin.vx;
    scratch->verts[1].vy = slot->motion.position.y - scratch->cameraOrigin.vy;
    scratch->verts[1].vz = slot->motion.position.z - offsetZ + offsetX - scratch->cameraOrigin.vz;
    scratch->verts[2].vx = slot->motion.prevPosition.x + offsetX + dx - scratch->cameraOrigin.vx;
    scratch->verts[2].vy = slot->motion.prevPosition.y - scratch->cameraOrigin.vy;
    scratch->verts[2].vz = slot->motion.prevPosition.z + offsetZ + dz - scratch->cameraOrigin.vz;
    bufferIndex = gFrameCounter & 1;
    trailQuad = &gSubWeaponTrailG4[bufferIndex][slotIndex];
    tipQuad = &gSubWeaponTrailF4[bufferIndex][slotIndex];
    tpage = &gSubWeaponTrailTPage[bufferIndex][slotIndex];
    if (gOptionMonochrome != 0) {
        trailRgb = (((trailRgb >> 16) & 0xff) + ((trailRgb >> 8) & 0xff) + (trailRgb & 0xff)) / 3;
        trailRgb = trailRgb | (trailRgb << 16 | trailRgb << 8);
    }
    SetRotMatrix(gCameraMatrix);
    SetTransMatrix(&gZeroTransMatrix);
    gte_ldv3(&scratch->verts[0], &scratch->verts[1], &scratch->verts[2]);
    gte_rtpt();
    SetPolyG4(trailQuad);
    scratch->verts[3].vx = slot->motion.prevPosition.x - offsetX + dx - scratch->cameraOrigin.vx;
    scratch->verts[3].vy = slot->motion.prevPosition.y - scratch->cameraOrigin.vy;
    scratch->verts[3].vz = slot->motion.prevPosition.z - offsetZ + dz - scratch->cameraOrigin.vz;
    gte_stsxy3(&scratch->screenXY[0], &scratch->screenXY[1], &trailQuad->x2);
    *(long *)&trailQuad->x0 = scratch->screenXY[0];
    *(long *)&trailQuad->x1 = scratch->screenXY[1];
    gte_ldv0(&scratch->verts[3]);
    gte_rtps();
    trailRgb |= GPU_CODE_WORD(GPU_CODE_POLY_G4 | GPU_CODE_SEMITRANS);
    *(long *)&trailQuad->r0 = trailRgb;
    *(long *)&trailQuad->r1 = trailRgb;
    gte_stsxy(&trailQuad->x3);
    gte_avsz4();
    *(long *)&trailQuad->r2 = 0;
    *(long *)&trailQuad->r3 = 0;
    gte_stotz(&scratch->otz);
    /* Non-linear OTZ compression into 4096 ordering-table buckets. */
    halfDepth = scratch->otz >> 1;
    depthShift = scratch->otz >> 12;
    scratch->otz = (halfDepth & 0x7ff | 0xfffff000) >> depthShift & 0xfff;
    if (scratch->otz >= DISPLAY_OT_LEN) {
        return;
    }
    AddPrim(gCurrentOt + scratch->otz, trailQuad);

    tipAngle = slot->motion.angleY;
    weaponYaw = tipAngle;
    tipAngle = (perpAngle - weaponYaw) & ANGLE_MASK;
    if (tipAngle >= ANGLE_HALF) {
        tipAngle = ANGLE_FULL - tipAngle;
    }
    if (tipAngle < ANGLE_QUARTER) {
        tipAngle = weaponYaw;
    } else {
        tipAngle = weaponYaw + ANGLE_HALF;
    }
    offsetX = gSinLutQ12[tipAngle & ANGLE_MASK] * trailWidth >> 12;
    offsetZ = gSinLutQ12[(tipAngle + ANGLE_QUARTER) & ANGLE_MASK] * trailWidth >> 12;
    scratch->verts[2].vx = slot->motion.position.x + offsetX - scratch->cameraOrigin.vx;
    scratch->verts[2].vy = slot->motion.position.y - scratch->cameraOrigin.vy;
    scratch->verts[2].vz = slot->motion.position.z + offsetZ - scratch->cameraOrigin.vz;
    scratch->verts[3].vx = slot->motion.position.x - offsetX - scratch->cameraOrigin.vx;
    scratch->verts[3].vy = slot->motion.position.y - scratch->cameraOrigin.vy;
    scratch->verts[3].vz = slot->motion.position.z - offsetZ - scratch->cameraOrigin.vz;
    gte_ldv3(&scratch->verts[2], &scratch->verts[3], &scratch->verts[3]);
    gte_rtpt();
    SetPolyF4(tipQuad);
    *(long *)&tipQuad->x0 = scratch->screenXY[0];
    *(long *)&tipQuad->x1 = scratch->screenXY[1];
    *(long *)&tipQuad->r0 = (trailRgb & GPU_RGB_MASK) | GPU_CODE_WORD(GPU_CODE_POLY_F4 | GPU_CODE_SEMITRANS);
    gte_stsxy3(&tipQuad->x2, &tipQuad->x3, &scratch->screenXY[0]);
    AddPrim(gCurrentOt + scratch->otz, tipQuad);
    SetDrawTPage(tpage, 0, 1, TRAIL_TPAGE_ADDITIVE);
    AddPrim(gCurrentOt + scratch->otz, tpage);
}

static void DrawSubWeapons(void)
{
    SubWeaponSlot *slot;
    int slotIndex;
    int subWeaponId;
    int trailRgb;

    slot = gSubWeaponSlots;
    for (slotIndex = 0; slotIndex < SUB_WEAPON_MAX; ++slotIndex, ++slot) {
        subWeaponId = slot->id;
        if (subWeaponId != SUB_WEAPON_NONE &&
            slot->motion.stageId == gCurrentStageId &&
            subWeaponId < SUB_WEAPON_SURFACE_BASE) {
            SubmitSubWeaponNode(gSubWeaponModelOwner[subWeaponId], &slot->motion.position, slot->motion.angleY);
            trailRgb = gSubWeaponParams[slot->id].trailRgb;
            if (trailRgb > 0) {
                DrawSubWeaponTrail(slotIndex, slot, trailRgb);
            }
        }
    }
}

static void RecordSubWeaponHistory(int slotIndex, int historyMode)
{
    int historyIndex;
    SubWeaponHistoryEntry *entry;
    SubWeaponSlot *slot;
    int frame;

    historyIndex = gSubWeaponHistoryIndex[slotIndex]++ & SUB_WEAPON_HISTORY_MASK;
    entry = &gSubWeaponHistory[slotIndex][historyIndex];
    frame = gCombatFrameCounter;
    slot = &gSubWeaponSlots[slotIndex];

    entry->frame = frame;
    entry->x = slot->motion.position.x;
    entry->y = slot->motion.position.y;
    entry->z = slot->motion.position.z;
    entry->velocity[0] = slot->motion.velocityX;
    entry->velocity[1] = slot->motion.velocityY;
    entry->velocity[2] = slot->motion.velocityZ;
    entry->flagsAndAngle = (slot->motion.isMoving << 15) | historyMode |
        (slot->motion.angleY & SUB_WEAPON_HISTORY_ANGLE_MASK);
    entry->spinSpeed = slot->motion.spinY;
    entry->id = slot->id;
}

static void RestoreSubWeaponFromHistory(int slotIndex, SubWeaponHistoryEntry *entry)
{
    SubWeaponSlot *slot;
    WorldVector *pos;
    int direction[3];

    slot = &gSubWeaponSlots[slotIndex];
    slot->motion.position.x = entry->x;
    slot->motion.position.y = entry->y;
    slot->motion.position.z = entry->z;
    slot->motion.prevPosition = slot->motion.position;

    slot->motion.velocityX = entry->velocity[0];
    slot->motion.velocityY = entry->velocity[1];
    slot->motion.velocityZ = entry->velocity[2];
    slot->motion.isMoving = (unsigned int)entry->flagsAndAngle >> 15;
    slot->motion.angleY = entry->flagsAndAngle;
    slot->motion.spinY = entry->spinSpeed;
    slot->id = entry->id;

    pos = &slot->motion.position;
    if ((slot->hitState = entry->flagsAndAngle & SUB_WEAPON_HISTORY_MODE_MASK) == SUB_WEAPON_HISTORY_CLEARED) {
        slot->id = SUB_WEAPON_NONE;
    }
    if (slot->hitState != SUB_WEAPON_HISTORY_THROWN || slot->id < SUB_WEAPON_SURFACE_BASE) {
        return;
    }

    pos->y += SUB_WEAPON_BURST_FLOOR_OFFSET;
    direction[0] = slot->motion.velocityX;
    direction[1] = pos->y;
    direction[2] = slot->motion.velocityZ;
    SpawnParticleBurst(slot->id - SUB_WEAPON_SURFACE_BASE, (VECTOR *)pos, direction);
    pos->y -= SUB_WEAPON_BURST_FLOOR_OFFSET;
}

static void ReflectSubWeaponVelocity(SubWeaponSlot *slot, SVECTOR *normal)
{
    int angle;
    int cosAngle, sinAngle;
    int x, z;
    int reflectedZ, rotatedX;

    angle = ratan2(normal->vx, normal->vz);
    cosAngle = gSinLutQ12[(angle + ANGLE_QUARTER) & ANGLE_MASK];
    x = slot->motion.velocityX;
    sinAngle = gSinLutQ12[angle & ANGLE_MASK];
    z = slot->motion.velocityZ;
    rotatedX = (x * cosAngle + z * sinAngle) >> 12;
    reflectedZ = -((-x * sinAngle + z * cosAngle) >> 12);
    slot->motion.velocityX = (rotatedX * cosAngle - reflectedZ * sinAngle) >> 15;
    slot->motion.velocityZ = (rotatedX * sinAngle + reflectedZ * cosAngle) >> 15;
    slot->motion.velocityY /= 4;
}

static void StepReplaySubWeapon(int slotIndex)
{
    SubWeaponSlot *slot;
    int next[3];
    int floorHeight;
    SVECTOR *surfaceNormal;
    int movedFreely;

    slot = &gSubWeaponSlots[slotIndex];
    slot->motion.prevPosition = slot->motion.position;
    switch (slot->hitState) {
    case SUB_WEAPON_HISTORY_CLEARED:
    case SUB_WEAPON_HISTORY_SETTLED:
        break;
    case SUB_WEAPON_HISTORY_THROWN:
    case SUB_WEAPON_HISTORY_DEFLECTED:
        if (slot->motion.isMoving == 0) {
            break;
        }
        slot->motion.angleY += slot->motion.spinY;

        if (slot->id >= SUB_WEAPON_SURFACE_BASE) {
            slot->motion.velocityX = slot->motion.velocityX * 15 / 16;
            slot->motion.velocityZ = slot->motion.velocityZ * 15 / 16;
            slot->motion.velocityY += 7;
        } else if (gCurrentStageId == STAGE_ID_DOZAEMON_MOAT && slot->motion.position.y > MOAT_WATER_SURFACE_Y) {
            slot->motion.velocityX = slot->motion.velocityX * 3 / 4;
            slot->motion.velocityZ = slot->motion.velocityZ * 3 / 4;
            slot->motion.velocityY += 4;
            if (slot->motion.velocityY > 32) {
                slot->motion.velocityY = 32;
            }
        } else {
            slot->motion.velocityY += 8;
        }

        slot->motion.spinY = slot->motion.spinY * 7 / 8;
        next[0] = slot->motion.position.x + slot->motion.velocityX;
        next[1] = slot->motion.position.y + slot->motion.velocityY;
        next[2] = slot->motion.position.z + slot->motion.velocityZ;
        floorHeight = QueryFloorHeight(next[0], next[1], next[2]);
        surfaceNormal = GetLastFloorNormal();
        if ((GetLastFloorAttr() & FLOOR_ATTR_NO_IMPACT) != 0) {
            floorHeight = 20000;
        }

        movedFreely = 0;
        if (floorHeight == FLOOR_QUERY_MISS) {
            slot->motion.velocityX = -slot->motion.velocityX / 8;
            slot->motion.velocityZ = -slot->motion.velocityZ / 8;
            slot->motion.velocityY = slot->motion.velocityY / 4;
            slot->motion.position.x += slot->motion.velocityX;
            slot->motion.position.z += slot->motion.velocityZ;
            if (slot->id >= SUB_WEAPON_SURFACE_BASE) {
                slot->hitState = SUB_WEAPON_HISTORY_CLEARED;
            }
        } else if (floorHeight < next[1]) {
            if (surfaceNormal->vy > -SUB_WEAPON_WALL_NORMAL_Y_MAX && surfaceNormal->vy < SUB_WEAPON_WALL_NORMAL_Y_MAX) {
                ReflectSubWeaponVelocity(slot, surfaceNormal);
                if (slot->id >= SUB_WEAPON_SURFACE_BASE) {
                    slot->hitState = SUB_WEAPON_HISTORY_CLEARED;
                }
            } else {
                slot->motion.velocityY = -slot->motion.velocityY / 4;
                slot->motion.velocityX = slot->motion.velocityX * 3 / 4;
                slot->motion.velocityZ = slot->motion.velocityZ * 3 / 4;
                slot->motion.position.y += slot->motion.velocityY;
                if (slot->id >= SUB_WEAPON_SURFACE_BASE) {
                    slot->hitState = SUB_WEAPON_HISTORY_CLEARED;
                }
            }
        } else {
            memcpy(&slot->motion.position, next, sizeof(next));
            movedFreely = 1;
        }

        if (slot->motion.velocityY > -10 && slot->motion.velocityY < 10 &&
            slot->motion.velocityX > -4 && slot->motion.velocityX < 4 &&
            slot->motion.velocityZ > -4 && slot->motion.velocityZ < 4 &&
            !movedFreely) {
            slot->motion.isMoving = 0;
        }
        break;
    }
}

void UpdateReplaySubWeapons(void)
{
    int slotIndex;
    SubWeaponHistoryEntry *entry;

    for (slotIndex = 0; slotIndex < SUB_WEAPON_MAX; ++slotIndex) {
        entry = &gSubWeaponHistory[slotIndex][gSubWeaponHistoryIndex[slotIndex]];
        if (gReplayPlaybackFrame == entry->frame) {
            RestoreSubWeaponFromHistory(slotIndex, entry);
            gSubWeaponHistoryIndex[slotIndex] = (gSubWeaponHistoryIndex[slotIndex] + 1) & SUB_WEAPON_HISTORY_MASK;
        } else {
            StepReplaySubWeapon(slotIndex);
        }
    }
    DrawSubWeapons();
}

void RewindSubWeaponsForReplay(void)
{
    int slotIndex;
    int back;
    int historyIndex;
    int delta;
    int adjustedStamp;
    SubWeaponHistoryEntry *entry;
    SubWeaponSlot *slot;

    slot = gSubWeaponSlots;
    for (slotIndex = 0; slotIndex < SUB_WEAPON_MAX; ++slotIndex, ++slot) {
        back = 1;
        historyIndex = (gSubWeaponHistoryIndex[slotIndex] - back) & SUB_WEAPON_HISTORY_MASK;
        entry = &gSubWeaponHistory[slotIndex][historyIndex];
        while (entry->frame != SUB_WEAPON_HISTORY_EMPTY) {
            adjustedStamp = entry->frame + REPLAY_DURATION_FRAMES;
            delta = adjustedStamp - gCombatFrameCounter;
            if (delta <= 0) {
                break;
            }
            ++back;
            historyIndex = (gSubWeaponHistoryIndex[slotIndex] - back) & SUB_WEAPON_HISTORY_MASK;
            entry = &gSubWeaponHistory[slotIndex][historyIndex];
        }
        RestoreSubWeaponFromHistory(slotIndex, entry);
        gSubWeaponHistoryIndex[slotIndex] = (historyIndex + 1) & SUB_WEAPON_HISTORY_MASK;
        if (entry->frame != SUB_WEAPON_HISTORY_EMPTY &&
            slot->hitState != SUB_WEAPON_HISTORY_CLEARED &&
            slot->hitState != SUB_WEAPON_HISTORY_SETTLED) {
            while (delta < 0) {
                StepReplaySubWeapon(slotIndex);
                ++delta;
            }
        }
    }
}

void ClearSubWeaponSlots(void)
{
    int slotIndex;

    for (slotIndex = SUB_WEAPON_MAX - 1; slotIndex >= 0; slotIndex--) {
        gSubWeaponSlots[slotIndex].id = SUB_WEAPON_NONE;
    }
}

void ResetSubWeaponHistory(void)
{
    int slotIndex, historyIndex;

    for (slotIndex = 0; slotIndex < SUB_WEAPON_MAX; ++slotIndex) {
        gSubWeaponHistoryIndex[slotIndex] = 0;
        for (historyIndex = 0; historyIndex < SUB_WEAPON_HISTORY_COUNT; ++historyIndex) {
            gSubWeaponHistory[slotIndex][historyIndex].frame = SUB_WEAPON_HISTORY_EMPTY;
            gSubWeaponHistory[slotIndex][historyIndex].flagsAndAngle = 0;
        }
    }
}

/* Starting carried sub-weapon IDs indexed by Fighter.characterClass (0..5 playable, 6..11 bosses). */
unsigned char gCharacterSubWeapons[FIGHTER_CLASS_COUNT][SUB_WEAPON_LOADOUT_MAX] = {
    { 2, 0, 0, 0, 0 }, /* Kannuki */
    { 5, 0, 0, 0, 0 }, /* Mikado */
    { 4, 0, 0, 0, 0 }, /* Black Lotus */
    { 0, 0, 0, 0, 0 }, /* Tatsumi */
    { 1, 1, 1, 1, 1 }, /* Red Shadow */
    { 3, 3, 0, 0, 0 }, /* Utsusemi */
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
};

void InitFighterSubWeapons(Fighter *fighter)
{
    int i;
    int subWeaponId;

    fighter->subWeaponCount = 0;
    for (i = 0; i < SUB_WEAPON_LOADOUT_MAX; ++i) {
        subWeaponId = gCharacterSubWeapons[fighter->characterClass][i];
        fighter->subWeaponIds[i] = subWeaponId;
        if (subWeaponId != SUB_WEAPON_NONE) {
            ++fighter->subWeaponCount;
        }
    }
    gSubWeaponModelOwner[gCharacterSubWeapons[fighter->characterClass][0]] = fighter->side;
}

short gSubWeaponNextSlot;

static void SpawnSubWeapon(Fighter *fighter, int subWeaponId)
{
    SubWeaponSlot *slot;
    int probe, slotIndex, firstProbe;
    SubWeaponParams *params;
    VECTOR *position;
    int direction[3];

    firstProbe = gSubWeaponNextSlot++;
    /* On Japan the mask is 10: the probe reaches slots 0, 2, 8 and 10 only. */
    for (probe = 0; probe < SUB_WEAPON_MAX; probe++) {
        slotIndex = (firstProbe + probe) & (SUB_WEAPON_MAX - 1);
        slot = &gSubWeaponSlots[slotIndex];
        if (slot->id == SUB_WEAPON_NONE)
            break;
    }

    slot->id = subWeaponId;
    slot->hitState = SUB_WEAPON_HIT_ACTIVE;
    slot->owner = fighter->side;
    slot->motion.position.x = fighter->position.vx;
    slot->motion.position.y = fighter->position.vy - fighter->statureScale / 32;
    slot->motion.position.z = fighter->position.vz;

    params = &gSubWeaponParams[subWeaponId];
    slot->motion.velocityX = (gSinLutQ12[fighter->angles.heading & ANGLE_MASK] * params->speed) >> 12;
    slot->motion.velocityY = params->verticalSpeed;
    slot->motion.velocityZ = (gSinLutQ12[(fighter->angles.heading + ANGLE_QUARTER) & ANGLE_MASK] * params->speed) >> 12;
    slot->motion.position.x += slot->motion.velocityX;
    slot->motion.position.y += slot->motion.velocityY;
    slot->motion.position.z += slot->motion.velocityZ;
    slot->motion.position.x += slot->motion.velocityX / 2;
    slot->motion.position.y += slot->motion.velocityY / 2;
    slot->motion.position.z += slot->motion.velocityZ / 2;
    slot->motion.prevPosition = slot->motion.position;

    slot->motion.angleX = 0;
    slot->motion.angleY = 0;
    slot->motion.angleZ = 0;
    slot->motion.spinX = 0;
    slot->motion.stageId = gCurrentStageId;
    slot->motion.spinY = params->spin;
    slot->motion.isMoving = 1;
    slot->motion.spinZ = 0;

    position = (VECTOR *)&slot->motion.position;
    if (slot->id >= SUB_WEAPON_SURFACE_BASE) {
        slot->motion.position.y += SUB_WEAPON_BURST_FLOOR_OFFSET;
        direction[0] = slot->motion.velocityX;
        direction[1] = slot->motion.position.y;
        direction[2] = slot->motion.velocityZ;
        SpawnParticleBurst(slot->id - SUB_WEAPON_SURFACE_BASE, position, direction);
        slot->motion.position.y -= SUB_WEAPON_BURST_FLOOR_OFFSET;
        if (slot->id == SUB_WEAPON_SURFACE_BASE)
            EmitCombatEvent(slot->owner, COMBAT_EVENT_SURFACE_THROW_KIND0, position);
        else
            EmitCombatEvent(slot->owner, COMBAT_EVENT_SURFACE_THROW_OTHER, position);
    } else {
        EmitCombatEvent(slot->owner, COMBAT_EVENT_SUB_WEAPON_THROW, position);
    }
    RecordSubWeaponHistory(slotIndex, SUB_WEAPON_HISTORY_THROWN);
}

int ThrowFighterSubWeapon(Fighter *fighter, int surfaceType)
{
    int subWeaponId;
    int i;

    if (surfaceType != 0) {
        subWeaponId = surfaceType + (SUB_WEAPON_SURFACE_BASE - 1);
        SpawnSubWeapon(fighter, subWeaponId);
    } else {
        if (fighter->subWeaponCount == 0)
            return SUB_WEAPON_NONE;

        subWeaponId = fighter->subWeaponIds[0];
        SpawnSubWeapon(fighter, subWeaponId);

        for (i = 0; i < fighter->subWeaponCount - 1; i++)
            fighter->subWeaponIds[i] = fighter->subWeaponIds[i + 1];

        --fighter->subWeaponCount;
    }

    return subWeaponId;
}

static int AddFighterSubWeapon(Fighter *fighter, int subWeaponId)
{
    if (fighter->subWeaponCount == SUB_WEAPON_MAX) {
        return 0;
    } else {
        fighter->subWeaponIds[fighter->subWeaponCount++] = subWeaponId;
        return 1;
    }
}

int PickupNearbySubWeapons(Fighter *fighter)
{
    SubWeaponSlot *slot;
    int slotIndex;
    short subWeaponId;

    slot = gSubWeaponSlots;
    slotIndex = 0;
    do {
        subWeaponId = slot->id;
        if (subWeaponId != SUB_WEAPON_NONE && slot->motion.isMoving == 0) {
            if (slot->motion.position.y > fighter->anchor.vy - 100 &&
                slot->motion.position.y < fighter->anchor.vy + 100) {
                if (slot->motion.position.x > fighter->position.vx - 1000 &&
                    slot->motion.position.x < fighter->position.vx + 1000) {
                    if (slot->motion.position.z > fighter->position.vz - 1000 &&
                        slot->motion.position.z < fighter->position.vz + 1000) {
                        if (AddFighterSubWeapon(fighter, subWeaponId) == 0)
                            break;
                        slot->id = SUB_WEAPON_NONE;
                        RecordSubWeaponHistory(slotIndex, SUB_WEAPON_HISTORY_CLEARED);
                    }
                }
            }
        }
        ++slotIndex;
        ++slot;
    } while (slotIndex < SUB_WEAPON_MAX);

    return 0;
}

static void EmitSubWeaponBounceEffect(SubWeaponSlot *slot)
{
    VECTOR *position;
    int side;

    if (slot->id < SUB_WEAPON_SURFACE_BASE) {
        position = (VECTOR *)&slot->motion.position;
        side = GetSoundListenerSide(position);
        EmitCombatEvent(side, COMBAT_EVENT_SUB_WEAPON_BOUNCE, position);
    }
}

void UpdateAndDrawSubWeapons(void)
{
    SubWeaponSlot *slot;
    int slotIndex;
    WorldVector nextPos;
    int surfaceY;
    SVECTOR *surfaceNormal;
    int advanced;

    slot = gSubWeaponSlots;
    slotIndex = 0;
    do {
        if (slot->id != SUB_WEAPON_NONE) {
            slot->motion.prevPosition = slot->motion.position;
            if (slot->motion.isMoving != 0) {
                slot->motion.angleY += slot->motion.spinY;
                if (slot->id >= SUB_WEAPON_SURFACE_BASE) {
                    slot->motion.velocityX = slot->motion.velocityX * 15 / 16;
                    slot->motion.velocityZ = slot->motion.velocityZ * 15 / 16;
                    slot->motion.velocityY += 7;
                } else if (gCurrentStageId == STAGE_ID_DOZAEMON_MOAT &&
                           slot->motion.position.y > MOAT_WATER_SURFACE_Y) {
                    /* Underwater drag and terminal fall speed in the moat. */
                    slot->motion.velocityX = slot->motion.velocityX * 3 / 4;
                    slot->motion.velocityZ = slot->motion.velocityZ * 3 / 4;
                    slot->motion.velocityY += 4;
                    if (slot->motion.velocityY > 32) {
                        slot->motion.velocityY = 32;
                    }
                } else {
                    slot->motion.velocityY += 8;
                }
                slot->motion.spinY = slot->motion.spinY * 7 / 8;
                nextPos.x = slot->motion.position.x + slot->motion.velocityX;
                nextPos.y = slot->motion.position.y + slot->motion.velocityY;
                nextPos.z = slot->motion.position.z + slot->motion.velocityZ;
                surfaceY = QueryFloorHeight(nextPos.x, nextPos.y, nextPos.z);
                surfaceNormal = GetLastFloorNormal();
                if ((GetLastFloorAttr() & FLOOR_ATTR_NO_IMPACT) != 0) {
                    surfaceY = 20000;
                }
                advanced = 0;
                if (surfaceY == FLOOR_QUERY_MISS) {
                    slot->motion.velocityX = -slot->motion.velocityX / 8;
                    slot->motion.velocityZ = -slot->motion.velocityZ / 8;
                    slot->motion.velocityY = slot->motion.velocityY / 4;
                    slot->motion.position.x += slot->motion.velocityX;
                    slot->motion.position.z += slot->motion.velocityZ;
                    EmitSubWeaponBounceEffect(slot);
                    slot->hitState = SUB_WEAPON_HIT_INACTIVE;
                } else if (surfaceY < nextPos.y) {
                    if (surfaceNormal->vy > -SUB_WEAPON_WALL_NORMAL_Y_MAX &&
                        surfaceNormal->vy < SUB_WEAPON_WALL_NORMAL_Y_MAX) {
                        ReflectSubWeaponVelocity(slot, surfaceNormal);
                        EmitSubWeaponBounceEffect(slot);
                        slot->hitState = SUB_WEAPON_HIT_INACTIVE;
                    } else {
                        slot->motion.velocityY = -slot->motion.velocityY / 4;
                        slot->motion.velocityX = slot->motion.velocityX * 3 / 4;
                        slot->motion.velocityZ = slot->motion.velocityZ * 3 / 4;
                        slot->hitState = SUB_WEAPON_HIT_INACTIVE;
                        slot->motion.position.y += slot->motion.velocityY;
                    }
                } else {
                    slot->motion.position = nextPos;
                    advanced = 1;
                }
                if (slot->id >= SUB_WEAPON_SURFACE_BASE && slot->hitState == SUB_WEAPON_HIT_INACTIVE) {
                    slot->id = SUB_WEAPON_NONE;
                    RecordSubWeaponHistory(slotIndex, SUB_WEAPON_HISTORY_CLEARED);
                } else if (slot->motion.velocityY > -10 && slot->motion.velocityY < 10 &&
                           slot->motion.velocityX > -4 && slot->motion.velocityX < 4 &&
                           slot->motion.velocityZ > -4 && slot->motion.velocityZ < 4 &&
                           advanced == 0) {
                    slot->motion.isMoving = 0;
                    RecordSubWeaponHistory(slotIndex, SUB_WEAPON_HISTORY_SETTLED);
                }
            }
            if (gCurrentStageId == STAGE_ID_DOZAEMON_MOAT && slot->motion.prevPosition.y < MOAT_WATER_SURFACE_Y &&
                slot->motion.position.y >= MOAT_WATER_SURFACE_Y) {
                EmitCombatEvent(GetSoundListenerSide((VECTOR *)&slot->motion.position), COMBAT_EVENT_WATER_SPLASH,
                                (VECTOR *)&slot->motion.position);
            }
        }
        ++slotIndex;
        slot++;
    } while (slotIndex < SUB_WEAPON_MAX);
    DrawSubWeapons();
}

static void SetSubWeaponIdentityRotMatrix(MATRIX *matrix)
{
    matrix->m[0][0] = ONE;
    matrix->m[0][1] = 0;
    matrix->m[0][2] = 0;
    matrix->m[1][0] = 0;
    matrix->m[1][1] = ONE;
    matrix->m[1][2] = 0;
    matrix->m[2][0] = 0;
    matrix->m[2][1] = 0;
    matrix->m[2][2] = ONE;
}

/* Deflection yaw angles indexed by FighterHitSphere. */
unsigned short gSubWeaponDeflectYaw[FIGHTER_HIT_SPHERE_COUNT] = {
    0x0780, 0xf880, 0xfa00, 0x0600, 0xfa80, 0x0580, 0xfb00, 0xfb80, 0xfe00, 0xfe80, 0x0500, 0x0480,
    0x0200, 0x0180, 0xfd00, 0xfd80, 0xff00, 0xff80, 0x0300, 0x0280, 0x0100, 0x0080,
};

static void DeflectThrownSubWeapon(CollisionWork *work, SubWeaponSlot *slot, int sphere)
{
    MATRIX *rotation = &work->rotation;
    int yaw = gSubWeaponDeflectYaw[sphere] & ANGLE_MASK;
    int pitch = (((slot->motion.position.x << 4) + slot->motion.position.y + (slot->motion.position.z << 3)) &
                 (ANGLE_HALF - 1)) - ANGLE_QUARTER;

    SetSubWeaponIdentityRotMatrix(rotation);
    RotMatrixY(yaw, rotation);
    RotMatrixX(pitch, rotation);
    gte_SetRotMatrix(rotation);
    gte_ldlv0(&slot->motion.velocityX);
    gte_rtv0();
    gte_stlvnl(&slot->motion.velocityX);
    if (yaw > ANGLE_QUARTER && yaw < ANGLE_QUARTER * 3) {
        slot->motion.velocityX /= 8;
        slot->motion.velocityY /= 8;
        slot->motion.velocityZ /= 8;
    } else {
        slot->motion.velocityX /= 4;
        slot->motion.velocityY /= 4;
        slot->motion.velocityZ /= 4;
    }
}

void CheckSubWeaponFighterHits(void)
{
    CollisionWork *work = &COLLISION_SCRATCH->work;
    WorldVector *points = COLLISION_SCRATCH->work.workPoints[0];
    SubWeaponSlot *slot = gSubWeaponSlots;
    int slotIndex = 0;
    int other;
    Fighter *target;
    FighterHitSphere *hitSphere;
    int sphere;
    int hit;
    int direction;
    int angle;
    int hitKind;

    do {
        if (slot->id != SUB_WEAPON_NONE && slot->hitState != SUB_WEAPON_HIT_INACTIVE && slot->motion.isMoving != 0) {
            points[0] = slot->motion.prevPosition;
            points[1] = slot->motion.position;
            work->primaryVerts[0] = points;
            work->primaryVerts[1] = points + 1;
            BuildSegmentFrame(work);
            hit = 0;
            sphere = 0;
            other = slot->owner == 0;
            target = &gFighters[other];
            hitSphere = gFighterHitSpheres[other];
            do {
                if (target->leftArmIntact != 0 || sphere < CRIPPLED_LEFT_ARM_SKIP_FIRST ||
                    sphere >= CRIPPLED_LEFT_ARM_SKIP_FIRST + CRIPPLED_LEFT_ARM_SKIP_COUNT) {
                    hit = TestSphereVsSegmentFrame(
                        work,
                        &COLLISION_SCRATCH->bodyPartPos[other][sphere],
                        hitSphere->outerRadius,
                        hitSphere->outerRadiusSq
                    );
                    if (hit)
                        break;
                }
                ++sphere;
                ++hitSphere;
            } while (sphere < FIGHTER_HIT_SPHERE_COUNT);
            if (hit) {
                direction = ratan2(slot->motion.velocityX, slot->motion.velocityZ);
                angle = (target->angles.heading - direction) & ANGLE_MASK;
                if (angle >= ANGLE_HALF)
                    angle -= ANGLE_FULL;
                if (angle < 0)
                    angle = -angle;
                hitKind = slot->id < SUB_WEAPON_SURFACE_BASE ? HIT_BY_SUB_WEAPON : HIT_BY_DEBRIS;
                ApplyFighterHitImpact(hitKind, target, sphere, angle, 0, (SUB_WEAPON_GUARD_PARRY_MASK >> slot->id) & 1);
                if (target->side == 0 && slot->id < SUB_WEAPON_SURFACE_BASE)
                    gStoryNoDamageFlag = 0;
                slot->hitState = SUB_WEAPON_HIT_INACTIVE;
                if (slot->id < SUB_WEAPON_SURFACE_BASE) {
                    DeflectThrownSubWeapon(work, slot, sphere);
                    RecordSubWeaponHistory(slotIndex, SUB_WEAPON_HISTORY_DEFLECTED);
                } else {
                    target->velocity.x += slot->motion.velocityX;
                    target->velocity.z += slot->motion.velocityZ;
                    RecordSubWeaponHistory(slotIndex, SUB_WEAPON_HISTORY_CLEARED);
                }
            }
        }
        ++slotIndex;
        ++slot;
    } while (slotIndex < SUB_WEAPON_MAX);
}
