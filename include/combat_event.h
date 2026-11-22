#ifndef COMBAT_EVENT_H
#define COMBAT_EVENT_H

#include <sys/types.h>
#include <libgte.h>

#include "world_vector.h"

/* Combat audiovisual event IDs passed to EmitCombatEvent / DispatchCombatEventEffect (1..20). */
typedef enum CombatEvent {
    COMBAT_EVENT_TORSO_WOUND = 1,         /* non-lethal torso hit (ApplyFighterHitImpact) */
    COMBAT_EVENT_WEAPON_CLASH = 2,        /* blade-vs-blade clash / blade lock entry */
    COMBAT_EVENT_GUARD_PARRY = 3,         /* successful guard block/parry */
    COMBAT_EVENT_FATAL_HIT = 4,           /* lethal hit with blood spray along gBloodSprayDir */
    COMBAT_EVENT_WEAPON_ENV_HIT = 5,      /* weapon hits wall or solid stage prop */
    COMBAT_EVENT_SUB_WEAPON_BOUNCE = 6,   /* thrown sub-weapon bounces off wall */
    COMBAT_EVENT_GUNSHOT = 7,             /* Katze firearm shot and muzzle flash */
    COMBAT_EVENT_LIMB_WOUND = 8,          /* non-lethal limb wound / cripple */
    COMBAT_EVENT_BODY_BUMP = 9,           /* rear running tackle body collision */
    COMBAT_EVENT_GUARD_BREAK = 10,        /* guard break recoil */
    COMBAT_EVENT_BLADE_LOCK_SPARK = 11,   /* periodic blade-lock struggle sparks */
    COMBAT_EVENT_STORY_READY_VOICE = 12,  /* Story opponent ready voice line */
    COMBAT_EVENT_GUN_IMPACT = 13,         /* Katze bullet wall/floor impact */
    COMBAT_EVENT_PROP_CUT = 14,           /* cuttable stage prop severed */
    COMBAT_EVENT_SUB_WEAPON_THROW = 15,   /* thrown sub-weapon launch sound */
    COMBAT_EVENT_SPARK_SOUND = 16,          /* nothing raises it */
    COMBAT_EVENT_WATER_ENTRY = 17,        /* fighter enters Stage 19 water (y >= 700) */
    COMBAT_EVENT_SURFACE_THROW_KIND0 = 18,/* surface debris burst kind 0 (id == 8) */
    COMBAT_EVENT_SURFACE_THROW_OTHER = 19,/* surface debris burst kinds 1..3 (id 9..11) */
    COMBAT_EVENT_WATER_SPLASH = 20        /* weapon or sub-weapon breaks Stage 19 water surface */
} CombatEvent;

struct MotionPlacement;

int GetSoundListenerSide(VECTOR *position);
void PlaySpatialSound(int sound, VECTOR *position);
int ShouldSuppressRemoteEffects(int player);
int ProcessAnimFrameEvents(int player, struct MotionPlacement *placement, int frame, VECTOR *position,
                                int surfaceIndex, int groundY);
void DispatchCombatEventEffect(int player, int event, VECTOR *position);
void EmitCombatEvent(int player, int event, VECTOR *position);
void SolveBallisticJumpVelocity(VECTOR *start, VECTOR *target, int vy, int gravity, VECTOR *outVel);
int GetStageSurfaceParticleType(int surfaceIndex);

#endif
