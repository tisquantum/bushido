/*
 * Combat sound and effects: positional sound by distance and listener side, the animation
 * frame-event bytecode (footsteps, dust, voices), and the 20 numbered combat events.
 */

#include <sys/types.h>
#include <libgte.h>
#include <inline_o.h>
#include "stage_id.h"
#include "match_setup.h"
#include "link.h"

#include "collision_scratch.h"
#include "combat_event.h"
#include "effect_slot.h"
#include "fighter_hit_sphere.h"
#include "world_vector.h"
#include "fighter.h"
#include "game_state.h"
#include "motion_file.h"
#include "camera.h"
#include "voice_state.h"
#include <rand.h>
#include "angle.h"
#include "cloth.h"
#include "decal_driver.h"
#include "billboard.h"
#include "effect.h"
#include "muzzle_flash.h"
#include "ocean.h"
#include "point_light.h"
#include "replay.h"
#include "scene_math.h"
#include "sound.h"
#include "stage_moat.h"
#include "globals.h"

#define EVENT_EFFECT_POS            (&COLLISION_SCRATCH->work.workPoints[0][0])

#define SURFACE_SOUND_VARIANT_MASK  0x0f
#define SURFACE_PARTICLE_SHIFT      4
#define SURFACE_PARTICLE_MASK       0x07
#define SURFACE_SPLASH_FLAG         0x80
#define SURFACE_EFFECT(sound, debris, splash) \
    ((sound) | ((debris) << SURFACE_PARTICLE_SHIFT) | ((splash) ? SURFACE_SPLASH_FLAG : 0))


#define MOAT_WADE_MIN_Y             1001
#define FOOTPRINT_GROUND_TOLERANCE  500
#define HEAVY_GEAR_WEIGHT_THRESHOLD 821
#define REMOTE_EFFECT_SUPPRESS_DIST 20000
#define JUMP_MAX_HORIZONTAL_SPEED   300
#define JUMP_MIN_HORIZONTAL_SPEED   150

#define ANIM_EVENT_GROUND_IMPACT    1
#define ANIM_EVENT_HEAVY_GEAR       8
#define ANIM_EVENT_LEFT_FOOTSTEP    0x16
#define ANIM_EVENT_RIGHT_FOOTSTEP   0x17
#define ANIM_EVENT_VOICE_START      0x28
#define ANIM_EVENT_WEAPON_START     0x32
#define ANIM_EVENT_SPECIAL_BASE     0x80
#define ANIM_EVENT_LEFT_FOOT_DUST   0x80
#define ANIM_EVENT_RIGHT_FOOT_DUST  0x81
#define ANIM_EVENT_WATER_WADE_SOUND 0x82
#define ANIM_EVENT_CLASS_MASK       0xff
#define ANIM_EVENT_REVERSE_MARKER   0xff

#define SOUND_BANK_STRIDE           43
#define SOUND_ID_MASK               0xff
#define SOUND_VOICE_SHIFT           8
#define SOUND_PRIORITY_BASE         0x00100
#define SOUND_EXPLICIT_VOICE_BASE   0x10000
#define SOUND_VOLUME_ONLY_BASE      0x20000
#define SOUND_PAN_DENOMINATOR       6144

/* Common and per-bank combat sound IDs (weapon sounds add weaponPairSlot * SOUND_BANK_STRIDE). */
#define SOUND_COMBAT_SPARK              5
#define SOUND_HEAVY_GEAR_ALT            7
#define SOUND_COMBAT_BODY_BUMP          0x0b
#define SOUND_COMBAT_WATER_ENTRY        0x0c
#define SOUND_WATER_WADE                0x0d
#define SOUND_COMBAT_WATER_SPLASH       0x0e
#define SOUND_COMBAT_SURFACE_THROW_OTHER 0x0f
#define SOUND_COMBAT_SUB_WEAPON_THROW   0x10
#define SOUND_COMBAT_SURFACE_THROW_KIND0 0x14
#define SOUND_SNOW_GROUND_IMPACT        0x15
#define SOUND_VOICE_STORY_READY         0x2a
#define SOUND_WEAPON_GUNSHOT            0x32
#define SOUND_WEAPON_WOUND              0x36
#define SOUND_WEAPON_FATAL_HIT          0x37
#define SOUND_WEAPON_GUARD_PARRY        0x38
#define SOUND_WEAPON_GUARD_BREAK        0x39
#define SOUND_WEAPON_CLASH              0x3a
#define SOUND_WEAPON_GUN_IMPACT         0x3e
#define SOUND_WEAPON_PROP_CUT           0x40

/*
 * Per-stage (0..25), per-floor-material (0..7) surface effect bitfield:
 *   bits 0..3: surface sound variant (indexes gSurfaceGroundSound[] and offsets footsteps by variant * 2)
 *   bits 4..6: throwable surface debris type returned by GetStageSurfaceParticleType (0 = none)
 *   bit  7:    footstep splash/snow puff flag (also suppresses dust puff events 0x80/0x81)
 */
unsigned char gStageSurfaceEffectTable[STAGE_COUNT][8] = {
    { SURFACE_EFFECT(3, 0, 0), 0, 0, 0, SURFACE_EFFECT(4, 0, 0), 0, 0, 0 },
    { SURFACE_EFFECT(7, 3, 0), 0, SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(3, 4, 0), 0, 0, 0, 0 },
    { SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(3, 0, 0), 0, 0, 0, 0, 0, 0 },
    { SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(3, 0, 0), SURFACE_EFFECT(7, 3, 0), 0, 0, 0, 0 },
    { SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(8, 1, 1), 0, SURFACE_EFFECT(4, 0, 0), 0, 0, 0, 0 },
    { SURFACE_EFFECT(4, 0, 0), SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(3, 0, 0), SURFACE_EFFECT(4, 0, 0),
      SURFACE_EFFECT(7, 3, 0), 0, 0, 0 },
    { SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(3, 0, 0), 0, 0, 0, 0, 0 },
    { SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(7, 2, 0), SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(3, 0, 0), 0, 0, 0, 0 },
    { SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(3, 0, 0), SURFACE_EFFECT(7, 2, 0), SURFACE_EFFECT(7, 3, 0), 0, 0, 0, 0 },
    { SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(1, 0, 0), SURFACE_EFFECT(4, 0, 0), SURFACE_EFFECT(3, 0, 0), 0, 0, 0, 0 },
    { SURFACE_EFFECT(7, 3, 0), 0, 0, SURFACE_EFFECT(5, 0, 0),
      SURFACE_EFFECT(4, 0, 0), SURFACE_EFFECT(3, 4, 0), 0, SURFACE_EFFECT(3, 4, 0) },
    { SURFACE_EFFECT(1, 0, 0), 0, 0, 0, 0, 0, 0, 0 },
    { SURFACE_EFFECT(6, 2, 1), 0, SURFACE_EFFECT(3, 2, 0), 0, 0, 0, 0, 0 },
    { SURFACE_EFFECT(3, 0, 0), SURFACE_EFFECT(7, 0, 0), 0, 0, 0, 0, 0, 0 },
    { SURFACE_EFFECT(3, 2, 0), SURFACE_EFFECT(3, 2, 0), 0, 0, 0, 0, 0, 0 },
    { 0, SURFACE_EFFECT(2, 0, 0), 0, 0, 0, 0, 0, 0 },
    { SURFACE_EFFECT(4, 0, 0), SURFACE_EFFECT(3, 2, 0), SURFACE_EFFECT(3, 2, 0), 0, 0, 0, 0, 0 },
    { SURFACE_EFFECT(3, 2, 0), SURFACE_EFFECT(3, 2, 0), SURFACE_EFFECT(3, 2, 0), 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { SURFACE_EFFECT(6, 0, 0), SURFACE_EFFECT(3, 0, 0), 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, SURFACE_EFFECT(3, 2, 0), SURFACE_EFFECT(3, 2, 0), SURFACE_EFFECT(4, 0, 0), 0, 0, 0, 0 },
    { SURFACE_EFFECT(3, 4, 0), 0, SURFACE_EFFECT(7, 3, 0), SURFACE_EFFECT(7, 3, 0), 0, SURFACE_EFFECT(3, 4, 0), 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { SURFACE_EFFECT(3, 2, 0), SURFACE_EFFECT(3, 2, 0), SURFACE_EFFECT(3, 2, 0), 0, 0, 0, 0, 0 },
    { SURFACE_EFFECT(6, 4, 1), SURFACE_EFFECT(6, 4, 1), SURFACE_EFFECT(4, 2, 0), 0, 0, 0, 0, 0 },
};

#define SOUND_MIN_DISTANCE 5000
#define SOUND_MAX_DISTANCE 50000
#define SOUND_FALLOFF_SPAN (SOUND_MAX_DISTANCE - SOUND_MIN_DISTANCE)

/* Nothing calls it. */
static int CalcSoundDistanceVolume(int distance)
{
    if (distance <= SOUND_MIN_DISTANCE)
        return SOUND_MAX_VOLUME;
    if (distance >= SOUND_MAX_DISTANCE)
        return 0;
    return (SOUND_MAX_DISTANCE - distance) * SOUND_MAX_VOLUME / SOUND_FALLOFF_SPAN;
}

int GetSoundListenerSide(VECTOR *position)
{
    int listenerX, listenerZ;
    int bearing;

    listenerX = gCameraState.target.vx +
        (gCameraState.distance * gSinLutQ12[gCameraState.angles.vy & ANGLE_MASK] >> 12);
    listenerZ = gCameraState.target.vz -
        (gCameraState.distance * gSinLutQ12[(gCameraState.angles.vy + ANGLE_QUARTER) & ANGLE_MASK] >> 12);
    bearing = (gCameraState.angles.vy + ratan2(listenerX - position->vx, listenerZ - position->vz)) & ANGLE_MASK;
    return bearing >= ANGLE_HALF;
}

/*
 * Spatializes and dispatches a 3D sound by listener distance and bearing.
 * Encoded sound ID ranges:
 *   0x00000..0x000ff: dynamic SFX pool (QueueSoundEffect)
 *   0x00100..0x0ffff: priority voice ((sound >> 8) == 1 ? voice 0 : voice 1)
 *   0x10000..0x1ffff: stop and queue sound & 0xff on explicit voice (sound >> 8) & 0xff
 *   >= 0x20000:       update volume only on explicit voice (sound >> 8) & 0xff
 */
void PlaySpatialSound(int sound, VECTOR *position)
{
    int listenerX, listenerZ;
    int dx, dz;
    unsigned int distSq;
    int distance;
    int leadingZeros;
    int rightVol, leftVol;
    int bearing;
    int panAngle;

    listenerX = gCameraState.target.vx +
        (gCameraState.distance * gSinLutQ12[gCameraState.angles.vy & ANGLE_MASK] >> 12);
    listenerZ = gCameraState.target.vz -
        (gCameraState.distance * gSinLutQ12[(gCameraState.angles.vy + ANGLE_QUARTER) & ANGLE_MASK] >> 12);
    dx = listenerX - position->vx;
    dz = listenerZ - position->vz;
    dx /= 2;
    dz /= 2;
    if ((unsigned int)(dx + SOUND_MAX_DISTANCE / 2) > SOUND_MAX_DISTANCE ||
        (unsigned int)(dz + SOUND_MAX_DISTANCE / 2) > SOUND_MAX_DISTANCE) {
        return;
    }
    distSq = dx * dx + dz * dz;
    if ((unsigned int)distSq < 1024) {
        distance = gSqrtLutQ3[distSq] >> 3;
    } else {
        unsigned int shift;
        gte_ldlzc(distSq);
        gte_nop();
        gte_nop();
        gte_stlzc(&leadingZeros);
        shift = leadingZeros;
        shift = 22 - (shift & ~1);
        distSq = gSqrtLutQ3[distSq >> shift];
        distance = (distSq << 16) >> (19 - (shift >> 1));
    }
    distance *= 2;
    if (distance <= SOUND_MIN_DISTANCE) {
        rightVol = SOUND_MAX_VOLUME;
    } else if (distance >= SOUND_MAX_DISTANCE) {
        rightVol = 0;
    } else {
        rightVol = (SOUND_MAX_DISTANCE - distance) * SOUND_MAX_VOLUME / SOUND_FALLOFF_SPAN;
    }
    leftVol = rightVol * rightVol / 128;
    bearing = (gCameraState.angles.vy + ratan2(listenerX - position->vx, listenerZ - position->vz)) & ANGLE_MASK;
    if (bearing < ANGLE_HALF) {
        int swap = rightVol;
        rightVol = leftVol;
        leftVol = swap;
    }
    if (sound >= SOUND_EXPLICIT_VOICE_BASE) {
        int volume = rightVol;

        panAngle = (bearing + ANGLE_QUARTER) & ANGLE_MASK;
        if (panAngle >= ANGLE_HALF) {
            panAngle = ANGLE_FULL - panAngle;
        }
        rightVol = rightVol * (SOUND_PAN_DENOMINATOR - panAngle) / SOUND_PAN_DENOMINATOR;
        panAngle = (bearing - ANGLE_QUARTER) & ANGLE_MASK;
        if (panAngle >= ANGLE_HALF) {
            panAngle = ANGLE_FULL - panAngle;
        }
        leftVol = volume * (SOUND_PAN_DENOMINATOR - panAngle) / SOUND_PAN_DENOMINATOR;
    }
    if (sound >= SOUND_VOLUME_ONLY_BASE) {
        SetSoundVoiceVolume(rightVol, leftVol, (sound >> SOUND_VOICE_SHIFT) & SOUND_ID_MASK);
    } else if (sound >= SOUND_EXPLICIT_VOICE_BASE) {
        bearing = (sound >> SOUND_VOICE_SHIFT) & SOUND_ID_MASK;
        StopSoundVoice(bearing);
        QueueSoundOnVoice(sound & SOUND_ID_MASK, rightVol, leftVol, bearing);
    } else if (sound >= SOUND_PRIORITY_BASE) {
        bearing = sound >> SOUND_VOICE_SHIFT;
        if (bearing == 1) {
            StopPriorityVoice0();
            QueuePriorityVoice0(sound & SOUND_ID_MASK, rightVol, leftVol);
        } else {
            StopPriorityVoice1();
            QueuePriorityVoice1(sound & SOUND_ID_MASK, rightVol, leftVol);
        }
    } else {
        QueueSoundEffect(sound, rightVol, leftVol);
    }
}

static void HandleAnimSpecialEvent(int player, int event, int suppressDust)
{
    WorldVector *effectPos = EVENT_EFFECT_POS;
    Fighter *fighter = &gFighters[player];
    int x, y, z, footOffsetY;

    switch (event) {
    case ANIM_EVENT_LEFT_FOOT_DUST:
        if (suppressDust == 0) {
            x = fighter->leftFootPos.x;
            effectPos->x = x;
            footOffsetY = fighter->statureScale / 32;
            y = fighter->leftFootPos.y;
            y -= footOffsetY;
            effectPos->y = y;
            z = fighter->leftFootPos.z;
            effectPos->z = z;
            SpawnEffect(EFFECT_DUST_PUFF, x, y, z);
        }
        break;
    case ANIM_EVENT_RIGHT_FOOT_DUST:
        if (suppressDust == 0) {
            x = fighter->rightFootPos.x;
            effectPos->x = x;
            footOffsetY = fighter->statureScale / 32;
            y = fighter->rightFootPos.y;
            y -= footOffsetY;
            effectPos->y = y;
            z = fighter->rightFootPos.z;
            effectPos->z = z;
            SpawnEffect(EFFECT_DUST_PUFF, x, y, z);
        }
        break;
    case ANIM_EVENT_WATER_WADE_SOUND:
        if (gCurrentStageId == STAGE_ID_DOZAEMON_MOAT && fighter->position.vy >= MOAT_WADE_MIN_Y) {
            PlaySpatialSound(SOUND_WATER_WADE, (VECTOR *)&COLLISION_SCRATCH->bodyPartPos[player][FIGHTER_SPHERE_TORSO]);
        }
        break;
    }
}

int GetStageSurfaceParticleType(int surfaceIndex)
{
    return (gStageSurfaceEffectTable[gCurrentStageId][surfaceIndex] >> SURFACE_PARTICLE_SHIFT) & SURFACE_PARTICLE_MASK;
}

/* Ground-impact sound by gStageSurfaceEffectTable's low nibble (0..7; the one 8, stage 4's surface 1, is taken by
 * the case before the lookup). */
unsigned char gSurfaceGroundSound[8] = { 1, 1, 1, 4, 1, 1, 1, 2 };

int ShouldSuppressRemoteEffects(int player)
{
    int povEnabled;
    int localSide;
    int isOpponent;

    povEnabled = gPovModeEnabled;
    if (povEnabled != 1)
        return 0;
    isOpponent = 0;
    if (gPlayMode == PLAY_MODE_LINK) {
        localSide = gLinkRole;
        if ((localSide == povEnabled && player == 0) || (localSide == 0 && player == povEnabled))
            isOpponent = 1;
    } else {
        isOpponent = player == 1;
    }
    if (!isOpponent || gFighterDistanceXZ <= REMOTE_EFFECT_SUPPRESS_DIST)
        return 0;
    return 1;
}

/*
 * Walks the null-terminated animation frame-event bytecode stream in AnimInfo.frameEvents:
 *   0xFF <hi> <lo>: optional 16-bit characterClass eligibility mask prefixing the next event
 *   <event> <frame>: forward-playback event (when placement->flags & MOTION_FLAG_FORWARD)
 *   <event> 0xFF <frame>: reverse-playback event
 * Events >= 0x80 dispatch to HandleAnimSpecialEvent; < 0x80 emit footprints/effects and spatial SFX.
 */
int ProcessAnimFrameEvents(int player, MotionPlacement *placement, int frame, VECTOR *position,
                           int surfaceIndex, int groundY)
{
    int handled;
    WorldVector *footPos;
    int waveEdgeX;
    unsigned char surfaceEntry;
    int placementFlags;
    int surfaceSoundKind;
    int hasFootstepSplash;
    int frameOffset;
    int lastFrame;
    unsigned char *event;
    unsigned char opcode;
    int eligible;
    unsigned char *eventFrame;
    int soundId;
    int floorY;
    int footprintRow;
    int footprintIndex;
    int voiceBank;
    int targetFrame;
    int forward;

    handled = 0;
    footPos = EVENT_EFFECT_POS;
    if (ShouldSuppressRemoteEffects(player) != 0)
        return 0;
    if (gCurrentStageId == STAGE_ID_EXECUTIONERS_COVE) {
        waveEdgeX = SampleOceanWaveEdgeX(gFighters[player].position.vz);
        surfaceIndex = (gFighters[player].position.vx >= waveEdgeX) * 2;
    }
    surfaceEntry = gStageSurfaceEffectTable[gCurrentStageId][surfaceIndex];
    placementFlags = placement->flags;
    surfaceSoundKind = surfaceEntry & SURFACE_SOUND_VARIANT_MASK;
    hasFootstepSplash = surfaceEntry & SURFACE_SPLASH_FLAG;
    forward = placementFlags & MOTION_FLAG_FORWARD;
    if (forward != 0) {
        targetFrame = frame;
    } else {
        frameOffset = frame - placement->startFrame + 1;
        lastFrame = placement->frameLimit;
        targetFrame = lastFrame - frameOffset;
    }
    event = MOTION_FILE->animInfo[placement->animClipId - 1].frameEvents;
    while ((opcode = *event) != 0) {
        if (opcode == ANIM_EVENT_CLASS_MASK) {
            eligible = ((event[1] << 8) | event[2]) & (1 << gFighters[player].characterClass);
            event += 3;
            if (eligible == 0) {
                event = event[1] == ANIM_EVENT_REVERSE_MARKER ? event + 3 : event + 2;
                continue;
            }
        }
        if (forward != 0) {
            if (event[1] == ANIM_EVENT_REVERSE_MARKER) {
                event += 3;
                continue;
            }
            eventFrame = event + 1;
        } else {
            if (event[1] != ANIM_EVENT_REVERSE_MARKER) {
                event += 2;
                continue;
            }
            eventFrame = event + 2;
        }
        if (targetFrame == *eventFrame) {
            soundId = *event;
            if (soundId >= ANIM_EVENT_SPECIAL_BASE) {
                HandleAnimSpecialEvent(player, soundId, hasFootstepSplash);
            } else {
                floorY = gFighters[player].groundY;
                footprintRow = gFighters[player].fighterId != 0;
                footPos->y = floorY;
                if (soundId == ANIM_EVENT_LEFT_FOOTSTEP) {
                    if (floorY < gFighters[player].leftFootPos.y + FOOTPRINT_GROUND_TOLERANCE) {
                        footprintIndex = footprintRow << 1;
                        footPos->x = gFighters[player].leftFootPos.x;
                        footPos->z = gFighters[player].leftFootPos.z;
                        SpawnFighterFootprint(player, footprintIndex, &footPos->x);
                        if (hasFootstepSplash != 0) {
                            SpawnEffect(gCurrentStageId == 4 ? EFFECT_SNOW_FOOTSTEP_PUFF : EFFECT_WATER_FOOTSTEP_SPLASH,
                                        footPos->x, footPos->y, footPos->z);
                        }
                    }
                } else if (soundId == ANIM_EVENT_RIGHT_FOOTSTEP) {
                    if (floorY < gFighters[player].rightFootPos.y + FOOTPRINT_GROUND_TOLERANCE) {
                        footprintIndex = (footprintRow << 1) | 1;
                        footPos->x = gFighters[player].rightFootPos.x;
                        footPos->z = gFighters[player].rightFootPos.z;
                        SpawnFighterFootprint(player, footprintIndex, &footPos->x);
                        if (hasFootstepSplash != 0) {
                            SpawnEffect(gCurrentStageId == 4 ? EFFECT_SNOW_FOOTSTEP_PUFF : EFFECT_WATER_FOOTSTEP_SPLASH,
                                        footPos->x, footPos->y, footPos->z);
                        }
                    }
                } else if (soundId == ANIM_EVENT_HEAVY_GEAR) {
                    if (gFighters[player].tractionWeight >= HEAVY_GEAR_WEIGHT_THRESHOLD)
                        soundId = SOUND_HEAVY_GEAR_ALT;
                } else if (soundId == ANIM_EVENT_GROUND_IMPACT) {
                    if (gCurrentStageId == 4 && surfaceIndex == 1)
                        soundId = SOUND_SNOW_GROUND_IMPACT;
                    else
                        soundId = gSurfaceGroundSound[surfaceSoundKind];
                }
                voiceBank = 0;
                if (soundId >= ANIM_EVENT_LEFT_FOOTSTEP) {
                    if (soundId >= ANIM_EVENT_WEAPON_START) {
                        soundId += gFighters[player].weaponPairSlot * SOUND_BANK_STRIDE;
                    } else {
                        if (soundId < ANIM_EVENT_VOICE_START) {
                            if (soundId == ANIM_EVENT_LEFT_FOOTSTEP || soundId == ANIM_EVENT_RIGHT_FOOTSTEP) {
                                soundId += surfaceSoundKind << 1;
                            }
                        } else {
                            voiceBank = player + 1;
                        }
                        soundId += player * SOUND_BANK_STRIDE;
                    }
                }
                PlaySpatialSound(voiceBank << SOUND_VOICE_SHIFT | soundId, position);
            }
            handled = 1;
        }
        event = eventFrame + 1;
    }
    return handled;
}

void DispatchCombatEventEffect(int player, int event, VECTOR *position)
{
    int playerSoundBase;
    int weaponPairSlot;
    int opponentWeaponSlot;
    int opponentSoundBase;
    int weaponSoundBase;
    int soundId;
    int eventSoundId;

    playerSoundBase = player * SOUND_BANK_STRIDE;
    weaponPairSlot = gFighters[player].weaponPairSlot;
    weaponSoundBase = weaponPairSlot * SOUND_BANK_STRIDE;
    opponentWeaponSlot = gFighters[!player].weaponPairSlot;
    opponentSoundBase = opponentWeaponSlot * SOUND_BANK_STRIDE;
    switch (event) {
    case COMBAT_EVENT_TORSO_WOUND:
        PlaySpatialSound(opponentSoundBase + SOUND_WEAPON_WOUND, position);
        SpawnBillboardEffect(BILLBOARD_EFFECT_TORSO_WOUND, position);
        TriggerPointLightFlash(BILLBOARD_EFFECT_TORSO_WOUND, position->vx, position->vy, position->vz);
        return;
    case COMBAT_EVENT_LIMB_WOUND:
        PlaySpatialSound(opponentSoundBase + SOUND_WEAPON_WOUND, position);
        SpawnBillboardEffect(BILLBOARD_EFFECT_LIMB_WOUND, position);
        TriggerPointLightFlash(BILLBOARD_EFFECT_LIMB_WOUND, position->vx, position->vy, position->vz);
        return;
    case COMBAT_EVENT_WEAPON_CLASH:
        PlaySpatialSound(weaponSoundBase + SOUND_WEAPON_CLASH, position);
        SpawnBillboardEffect(BILLBOARD_EFFECT_WEAPON_CLASH, position);
        TriggerPointLightFlash(BILLBOARD_EFFECT_WEAPON_CLASH, position->vx, position->vy, position->vz);
        return;
    case COMBAT_EVENT_GUARD_PARRY:
        soundId = weaponSoundBase + SOUND_WEAPON_GUARD_PARRY;
        PlaySpatialSound(soundId, position);
        SpawnEffect(EFFECT_PARRY_SPARK, position->vx, position->vy, position->vz);
        TriggerAltPointLightFlash(EFFECT_PARRY_SPARK, position->vx, position->vy, position->vz);
        return;
    case COMBAT_EVENT_GUARD_BREAK:
        soundId = weaponSoundBase + SOUND_WEAPON_GUARD_BREAK;
        PlaySpatialSound(soundId, position);
        SpawnEffect(EFFECT_PARRY_SPARK, position->vx, position->vy, position->vz);
        TriggerAltPointLightFlash(EFFECT_PARRY_SPARK, position->vx, position->vy, position->vz);
        return;
    case COMBAT_EVENT_FATAL_HIT:
        PlaySpatialSound(opponentSoundBase + SOUND_WEAPON_FATAL_HIT, position);
        SpawnBillboardEffect(BILLBOARD_EFFECT_FATAL_HIT_PRIMARY, position);
        SpawnBillboardEffect(BILLBOARD_EFFECT_FATAL_HIT_SECONDARY, position);
        TriggerPointLightFlash(BILLBOARD_EFFECT_FATAL_HIT_SECONDARY, position->vx, position->vy, position->vz);
        SpawnBloodSpray(position, &gBloodSprayDir);
        return;
    case COMBAT_EVENT_WEAPON_ENV_HIT:
    case COMBAT_EVENT_SUB_WEAPON_BOUNCE:
        PlaySpatialSound(weaponSoundBase + SOUND_WEAPON_GUARD_BREAK, position);
        SpawnEffect(EFFECT_WALL_HIT_SPARK, position->vx, position->vy, position->vz);
        TriggerAltPointLightFlash(EFFECT_WALL_HIT_SPARK, position->vx, position->vy, position->vz);
        return;
    case COMBAT_EVENT_GUNSHOT:
        PlaySpatialSound(weaponSoundBase + SOUND_WEAPON_GUNSHOT, position);
        SpawnGunMuzzleFlash(player);
        return;
    case COMBAT_EVENT_GUN_IMPACT:
        PlaySpatialSound(weaponSoundBase + SOUND_WEAPON_GUN_IMPACT, position);
        PlaySpatialSound(weaponSoundBase + SOUND_WEAPON_GUNSHOT, position);
        SpawnEffect(EFFECT_DUST_PUFF, position->vx, position->vy, position->vz);
        return;
    case COMBAT_EVENT_BODY_BUMP:
        eventSoundId = SOUND_COMBAT_BODY_BUMP;
        PlaySpatialSound(eventSoundId, position);
        return;
    case COMBAT_EVENT_BLADE_LOCK_SPARK:
        if ((rand() & 3) == 0) {
            PlaySpatialSound(SOUND_COMBAT_SPARK, position);
        }
        SpawnEffect(EFFECT_BLADE_LOCK_FLASH, position->vx, position->vy, position->vz);
        TriggerAltPointLightFlash(EFFECT_BLADE_LOCK_FLASH, position->vx, position->vy, position->vz);
        return;
    case COMBAT_EVENT_STORY_READY_VOICE:
        eventSoundId = ((player + 1) << SOUND_VOICE_SHIFT) + playerSoundBase + SOUND_VOICE_STORY_READY;
        PlaySpatialSound(eventSoundId, position);
        return;
    case COMBAT_EVENT_PROP_CUT:
        PlaySpatialSound(weaponSoundBase + SOUND_WEAPON_PROP_CUT, position);
        SpawnBillboardEffect(BILLBOARD_EFFECT_DELAYED_AMBIENT, position);
        return;
    case COMBAT_EVENT_SUB_WEAPON_THROW:
        eventSoundId = SOUND_COMBAT_SUB_WEAPON_THROW;
        PlaySpatialSound(eventSoundId, position);
        return;
    case COMBAT_EVENT_SPARK_SOUND:
        eventSoundId = SOUND_COMBAT_SPARK;
        PlaySpatialSound(eventSoundId, position);
        return;
    case COMBAT_EVENT_WATER_ENTRY:
        PlaySpatialSound(SOUND_COMBAT_WATER_ENTRY, position);
        SpawnEffect(EFFECT_WATER_SPLASH, position->vx, position->vy, position->vz);
        return;
    case COMBAT_EVENT_SURFACE_THROW_KIND0:
        eventSoundId = SOUND_COMBAT_SURFACE_THROW_KIND0;
        PlaySpatialSound(eventSoundId, position);
        return;
    case COMBAT_EVENT_SURFACE_THROW_OTHER:
        eventSoundId = SOUND_COMBAT_SURFACE_THROW_OTHER;
        PlaySpatialSound(eventSoundId, position);
        return;
    case COMBAT_EVENT_WATER_SPLASH:
        SpawnEffect(EFFECT_WATER_SURFACE_SPLASH, position->vx, MOAT_WATER_SURFACE_Y, position->vz);
        eventSoundId = SOUND_COMBAT_WATER_SPLASH;
        PlaySpatialSound(eventSoundId, position);
    default:
        return;
    }
}

void EmitCombatEvent(int player, int event, VECTOR *position)
{
    if (ShouldSuppressRemoteEffects(player) == 0) {
        DispatchCombatEventEffect(player, event, position);
        RecordReplayCombatEvent(player, event, position);
    }
}

void SolveBallisticJumpVelocity(VECTOR *start, VECTOR *target, int vy, int gravity, VECTOR *outVel)
{
    int dx;
    int dz;
    int horizontalDistance;
    int verticalTerm;
    int twiceHeightDelta;
    int discriminant;
    int horizontalSpeed;

    dx = target->vx - start->vx;
    dz = target->vz - start->vz;
    horizontalDistance = SquareRoot0(dx * dx + dz * dz);
    if (horizontalDistance == 0) {
        outVel->vz = 0;
        outVel->vx = 0;
        return;
    }

    verticalTerm = target->vy - start->vy;
    if (verticalTerm == 0) {
        outVel->vx = (-gravity * dx) / (vy * 2);
        outVel->vz = (-gravity * dz) / (vy * 2);
        return;
    }

    twiceHeightDelta = verticalTerm * 2;
    discriminant = vy * vy + gravity * twiceHeightDelta;
    if (discriminant < 0) {
        horizontalSpeed = JUMP_MAX_HORIZONTAL_SPEED;
    } else {
        discriminant = SquareRoot0(discriminant << 10);
        horizontalSpeed = (((vy << 5) + discriminant) * horizontalDistance / twiceHeightDelta) / 32;
        if (horizontalSpeed < 0) {
            verticalTerm = (vy << 5) - discriminant;
            horizontalSpeed = (verticalTerm * horizontalDistance / twiceHeightDelta) / 32;
            if (horizontalSpeed > JUMP_MAX_HORIZONTAL_SPEED) {
                horizontalSpeed = JUMP_MAX_HORIZONTAL_SPEED;
            }
            if (horizontalSpeed < JUMP_MIN_HORIZONTAL_SPEED) {
                horizontalSpeed -= (JUMP_MIN_HORIZONTAL_SPEED - horizontalSpeed) / 2;
            }
        } else {
            if (horizontalSpeed > JUMP_MAX_HORIZONTAL_SPEED) {
                horizontalSpeed = JUMP_MAX_HORIZONTAL_SPEED;
            }
            if (horizontalSpeed < JUMP_MIN_HORIZONTAL_SPEED) {
                horizontalSpeed -= (JUMP_MIN_HORIZONTAL_SPEED - horizontalSpeed) / 2;
            }
        }
        goto output;
    }
    /* the original clamps the maximum speed again when the discriminant is negative */
    if (horizontalSpeed > JUMP_MAX_HORIZONTAL_SPEED) {
        horizontalSpeed = JUMP_MAX_HORIZONTAL_SPEED;
    }
    if (horizontalSpeed < JUMP_MIN_HORIZONTAL_SPEED) {
        horizontalSpeed -= (JUMP_MIN_HORIZONTAL_SPEED - horizontalSpeed) / 2;
    }
output:
    outVel->vx = (horizontalSpeed * dx) / horizontalDistance;
    outVel->vz = (horizontalSpeed * dz) / horizontalDistance;
}
