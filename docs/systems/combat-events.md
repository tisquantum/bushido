# Frame events and combat events

A fight's sounds and effects come from two sources, both in `src/combat_event.c`. The animations carry **frame
events**: a byte code in each clip's `AnimInfo` that fires footsteps, dust, voices and weapon sounds on given
frames. The collision and hit code raises **combat events**: twenty numbered outcomes (a wound, a clash, a parry)
that `EmitCombatEvent` turns into a sound, an effect and often a flash of light, and records for the replay.
Positional sound itself is in [audio.md](audio.md), the effects in [effects.md](effects.md).

## Frame events

`UpdateFighterFrame` (`src/fighter.c`) calls `ProcessAnimFrameEvents` once a frame for each fighter, unless
its animation frame stalled; the replay calls it again for the frames it plays back. The stream is
`MOTION_FILE->animInfo[animClipId - 1].frameEvents`, in the motion archive, and ends at a zero byte:

```
FF hi lo          the next event fires only for character classes whose bit is set in the 16-bit mask hi:lo
ev frame          fires when the clip plays forward and reaches frame
ev FF frame       fires when the clip plays backward and reaches frame
```

A placement plays forward when its flags hold `MOTION_FLAG_FORWARD`; playing backward, the frame compared is
`frameLimit - (frame - startFrame + 1)`. Several events may fire on one frame. The event byte is a sound number or,
from 0x80, a special event:

| Event | What happens |
| --- | --- |
| 1 | Ground impact: sound `gSurfaceGroundSound[variant]` (1, 1, 1, 4, 1, 1, 1, 2 for variants 0 to 7), or 0x15 on stage 4's surface 1 |
| 8 | Sound 8, or 7 when the fighter's `tractionWeight` is 821 or more |
| other below 0x16 | That sound, the same for both fighters |
| 0x16, 0x17 | Left and right footstep: a footprint and a sound (below) |
| 0x18 to 0x27 | Sound + 43 x player |
| 0x28 to 0x31 | A voice: sound + 43 x player on the fighter's own voice (0 or 1) |
| 0x32 to 0x7F | A weapon sound: sound + 43 x the fighter's weapon pair slot |
| 0x80, 0x81 | Dust at the left or right foot (sprite type 2), `statureScale / 32` above it (128 at stature 1.0), unless the surface splashes |
| 0x82 | On Dozaemon Moat, with the fighter's Y 1001 or more: the wading sound 0x0D from the upper torso |

The 43 is `SOUND_BANK_STRIDE`: the sound map has a block of 43 numbers per fighter and per weapon pair slot, so the
same event plays each side's own voice or weapon ([audio.md](audio.md)). All of these are played with
`PlaySpatialSound` from the fighter's position.

**Footsteps.** On a footstep the foot's position is taken and, if the fighter's floor is within 500 of the foot, a
footprint is laid (`SpawnFighterFootprint`) and, where the surface splashes, a puff of snow (on stage 4) or
a water splash at the foot. The sound is the footstep number plus twice the surface's sound variant, so nine
variants give the footstep sounds 0x16 to 0x27, then plus 43 x player.

## Surfaces

What a floor sounds like comes from `gStageSurfaceEffectTable[stage][surface]`, the surface being the low three
bits of the floor polygon's attribute under the fighter (`FLOOR_ATTR_SURFACE_MASK`, [stages.md](stages.md)).
Each byte packs a sound variant (bits 0 to 3), a debris field saying what a fighter can kick up and throw there
(bits 4 to 6, read by `GetStageSurfaceParticleType`; 0 none, 1 to 4 the sub-weapons 8 to 11 of
[weapons.md](weapons.md), whose bursts are the debris kinds 0 to 3 of [effects.md](effects.md)) and a splash
flag (bit 7), which also suppresses foot dust. Surfaces not listed are 0 (variant 0, nothing to kick up):

| Stage | Surfaces: variant, debris field |
| ---: | --- |
| 0 | 0: 3; 4: 4 |
| 1 | 0: 7, 3; 2: 7, 3; 3: 3, 4 |
| 2 | 0: 7, 3; 1: 3 |
| 3 | 0, 1, 3: 7, 3; 2: 3 |
| 4 | 0: 7, 3; 1: 8, 1, splashes; 3: 4 |
| 5 | 0, 3: 4; 1, 4: 7, 3; 2: 3 |
| 6 | 0, 1: 7, 3; 2: 3 |
| 7 | 0, 2: 7, 3; 1: 7, 2; 3: 3 |
| 8 | 0, 3: 7, 3; 1: 3; 2: 7, 2 |
| 9 | 0: 7, 3; 1: 1; 2: 4; 3: 3 |
| 10 | 0: 7, 3; 3: 5; 4: 4; 5, 7: 3, 4 |
| 11 | 0: 1 |
| 12 (Executioner's Cove) | 0: 6, 2, splashes; 2: 3, 2 |
| 13 | 0: 3; 1: 7 |
| 14 (Bamboo Thicket), 17 (Yagura Point), 24 (Yahiro Road) | every listed surface 3, 2 (0 and 1; 0 to 2 on 17 and 24) |
| 15 (Meikyokan) | 1: 2 |
| 16 (Cherry Blossom Grove) | 0: 4; 1, 2: 3, 2 |
| 19 (Dozaemon Moat) | 0: 6; 1: 3 |
| 21 (Warrior's Dwelling) | 1, 2: 3, 2; 3: 4 |
| 22 | 0, 5: 3, 4; 2, 3: 7, 3 |
| 25 (Underground Grotto) | 0, 1: 6, 4, splashes; 2: 4, 2 |
| 18, 20, 23 | none |

On Executioner's Cove the surface is not read from the floor: it is 0 (splashing) on the low-X side of the moving
wave edge and 2 on the other (`SampleOceanWaveEdgeX`).

## Combat events

`EmitCombatEvent(player, event, position)` dispatches the event (`DispatchCombatEventEffect`) and records it for
the replay. The player argument chooses whose sound bank is used: for most events the fighter it names, but the
wounds and the fatal hit name the fighter struck and play the striker's weapon sound, and a thrown item's bounce
and moat splash name the side of the camera the item is on (`GetSoundListenerSide`, 0 or 1), so the bounce plays
the weapon sound of whichever fighter has that index ([oddities](oddities.md)). The numbers are `CombatEvent` in
`include/combat_event.h`:

| No. | Name | Raised by | Sound | Effect and light |
| ---: | --- | --- | --- | --- |
| 1 | `COMBAT_EVENT_TORSO_WOUND` | a non-fatal hit on spheres 0 to 5 (`ApplyFighterHitImpact`, `src/combat_hit.c`) | striker's weapon 0x36 | billboard 7, flash |
| 2 | `COMBAT_EVENT_WEAPON_CLASH` | blades meeting, for each fighter mid-attack (`ResolveWeaponVsBodyHits`, `src/collision.c`); both fighters on entering the blade lock (`UpdateCombatCollisions`) | own weapon 0x3A | billboard 5, flash |
| 3 | `COMBAT_EVENT_GUARD_PARRY` | a guard that holds (`ApplyFighterHitImpact`, `ResolveBladeClashReaction`) | own weapon 0x38 | sprite 0, flash |
| 4 | `COMBAT_EVENT_FATAL_HIT` | a killing hit, or any hit on a fighter already dead, dying or defeated | striker's weapon 0x37 | billboards 4 and 6, flash, blood spray |
| 5 | `COMBAT_EVENT_WEAPON_ENV_HIT` | a blade striking a stage prop or a wall mid-attack (`CheckWeaponClashAndEnvHit`, `src/collision.c`) | own weapon 0x39 | sprite 1, flash |
| 6 | `COMBAT_EVENT_SUB_WEAPON_BOUNCE` | a thrown item bouncing (`EmitSubWeaponBounceEffect`, `src/subweapon.c`) | as 5, see below | as 5 |
| 7 | `COMBAT_EVENT_GUNSHOT` | Katze firing (`FireGunshotRayAndCheckHit`), at the muzzle | own weapon 0x32 | muzzle flash |
| 8 | `COMBAT_EVENT_LIMB_WOUND` | a non-fatal hit on spheres 6 to 21 | striker's weapon 0x36 | billboard 2, flash |
| 9 | `COMBAT_EVENT_BODY_BUMP` | a running fighter striking a standing one from behind (`ResolveFighterBodyCollision`) | 0x0B | none |
| 10 | `COMBAT_EVENT_GUARD_BREAK` | a guard that breaks | own weapon 0x39 | sprite 0, flash |
| 11 | `COMBAT_EVENT_BLADE_LOCK_SPARK` | the blade lock, every eighth frame from frame 16, alternating sides, 200 above the fighters' middle (`UpdateBladeLockState`) | 0x05, one time in four | sprite 3, flash |
| 12 | `COMBAT_EVENT_STORY_READY_VOICE` | the Story opponent's honour window closing (`UpdateStoryHonorWindow`, `src/match_setup.c`) | 0x2A + 43, on voice 1 | none |
| 13 | `COMBAT_EVENT_GUN_IMPACT` | a shot whose ray meets the floor or a wall, unless it struck a body nearer | own weapon 0x3E and 0x32 | sprite 2 |
| 14 | `COMBAT_EVENT_PROP_CUT` | a blade cutting a prop (`CheckWeaponStagePropHit`, `src/stage_prop_hit.c`) | own weapon 0x40 | billboard 15 |
| 15 | `COMBAT_EVENT_SUB_WEAPON_THROW` | throwing a carried item (`SpawnSubWeapon`) | 0x10 | none |
| 16 | `COMBAT_EVENT_SPARK_SOUND` | nothing | 0x05 | none |
| 17 | `COMBAT_EVENT_WATER_ENTRY` | a fighter's Y passing 700 downward on Dozaemon Moat (`UpdateFighterFrame`) | 0x0C | sprite 8 |
| 18 | `COMBAT_EVENT_SURFACE_THROW_KIND0` | throwing debris of kind 0 (sub-weapon 8) | 0x14 | (the burst itself) |
| 19 | `COMBAT_EVENT_SURFACE_THROW_OTHER` | throwing debris of kinds 1 to 3 (sub-weapons 9 to 11) | 0x0F | (the burst itself) |
| 20 | `COMBAT_EVENT_WATER_SPLASH` | a weapon point (`CheckWeaponStagePropHit`, `src/stage_prop_hit.c`) or a thrown item (`UpdateAndDrawSubWeapons`) passing down through the moat's surface at 1200 | 0x0E | sprite 9 at the surface |

"Own weapon" and "striker's weapon" are sound numbers offset by 43 times that fighter's weapon pair slot. The light
is the point light's flash ([lighting.md](lighting.md)): `TriggerPointLightFlash` with the billboard kind for
events with a billboard, `TriggerAltPointLightFlash` with the sprite type for the sparks and the blade lock. The
gunshot's own effect is `SpawnGunMuzzleFlash`; Katze's shot is raised for the shooter, at its weapon's second point
100 higher, even in the Slash clear cutscene, where nothing else of the shot is tested.

## Suppression in the first-person view

`ShouldSuppressRemoteEffects(player)` drops both kinds of event for the far fighter in the first-person view: the
opponent (in a Link bout the fighter the other console plays) when the fighters stand more than 20,000 apart. A
suppressed event plays nothing and is not recorded.

## The replay

`RecordReplayCombatEvent` (`src/replay.c`) keeps at most one event per fighter per frame in that frame's
`ReplayFrame`, with its position stored relative to the fighter in one of 64 marks (`gReplayEffectMarks`, taken in
turn; an event is dropped when the next mark is still in use). A fatal hit also keeps the blood direction, shifted
down by 5. Playing back, `UpdateBattleReplay` re-dispatches each recorded event through `DispatchCombatEventEffect`
and runs the frame events again from the recorded surface. A cut prop is dispatched directly and never recorded, so
the replay shows no prop cut effect or sound.

## Other routines here

`SolveBallisticJumpVelocity` gives the horizontal velocity that carries a jump from one point to another under a
given upward speed and gravity, limiting the speed to 300 and lowering a speed under 150 by half its shortfall from
150; `UpdateFighterFrame` uses it, with gravity 42, to aim a homing jump at the opponent's torso.
`GetSoundListenerSide` returns which side of the camera a point lies on (0 or 1).
