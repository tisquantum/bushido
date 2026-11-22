# Weapons and sub-weapons

A weapon is an id from 0 to 9 (`Fighter.weaponId`), named in `include/weapon_id.h` as the weapon select
screen's cards name them: 0 katana, 1 long sword, 2 broadsword, 3 saber, 4 naginata, 5 rapier, 6 nodachi,
7 sledgehammer. The six playable characters choose among these eight; 8 is Katze's gun; 9, the default weapon of
the boss of class 10, has no card (`DATA3/O02.DAT` holds cards for 0 to 8 only) and no name. Thrown sub-weapons
are ids 1 to 5, and 8 to 11 are bursts thrown up from the ground ([effects.md](effects.md)).

## What differs per weapon

From `src/fighter.c` (grip, points, trails, traction) and `src/match_setup.c`:

| Id | Weapon | Grip | Edge points | Body segments | Trail strips | Traction weight |
| ---: | --- | --- | ---: | --- | ---: | ---: |
| 0 | katana | two hands | 2 | 0 to 1 | 1 | 15 |
| 1 | long sword | one hand | 2 | 0 to 1 | 1 | 14 |
| 2 | broadsword | two hands | 2 | 0 to 1 | 1 | 41 |
| 3 | saber | one hand | 2 | 0 to 1 | 1 | 12 |
| 4 | naginata | two hands | 4 | 0 to 1; 2 to 0, never vital | 1 | 22 |
| 5 | rapier | one hand | 2 | 0 to 1 | 1 | 9 |
| 6 | nodachi | two hands | 3 | 0 to 1 | 2 | 19 |
| 7 | sledgehammer | two hands | 4 | 0 to 1; 2 to 3 | 1 | 45 |
| 8 | gun | one hand | 2 | a ray | 0 | 7 |
| 9 | (none) | two hands | 4 | 2 to 1 | 2 | 22 |

- **Edge points** (`gWeaponEdgeOffsets`, counts in `gWeaponEdgePointCounts`) are offsets from the right-hand
  weapon joint; along the weapon's length they run, for instance, from 72 to 996 for the katana, from 325 to
  1051 for the broadsword, and from -1201 behind the hand to 1319 ahead of it for the naginata. The class 7
  boss uses its own pair (`gFinalBossAltWeaponEdgeOffsets`). The **body segments** are the point pairs whose sweep is
  tested against the opponent's spheres (`ResolveWeaponVsBodyHits`, `src/collision.c`); blade against blade
  uses one pair per weapon (`CheckWeaponClashAndEnvHit`: 2 to 1 for the naginata and weapon 9, 2 to 3 for the
  sledgehammer, 0 to 1 otherwise). The naginata's second segment, from behind the hand to the first point,
  never makes a vital contact, so it wounds where the first kills ([combat.md](combat.md)).
- **Traction weight** adds to the character's own (`tractionWeight`), which lowers `velocityRetention`, the share
  of velocity kept, and so stops the fighter sooner ([fighter.md](fighter.md)).
- **Cutting props**: the sledgehammer never cuts a cuttable prop, the naginata cuts only with part of its sweep
  (`CheckWeaponStagePropHit`, `src/stage_prop_hit.c`).
- **Motion**: `gWeaponPairMotionFiles` picks one of 44 motion archives from the two fighters' weapons; weapon 9
  uses the naginata's motions and sounds, and no archive exists for gun against gun. All moves, stances and guards
  belong to the archive, so a weapon's repertoire is data, not code.
- The slash trails (`gWeaponTrailCount`, `gWeaponTrailEndpoints`) are drawn by `src/weapon_trail.c`
  ([rendering.md](rendering.md)); clips flagged `ANIM_FLAG_RED_TRAIL` draw them red.

## Character and weapon together

`InitFighterForScene` (`src/fighter.c`) reads two Q12 tables by class and weapon: `gFighterAnimSpeed`, the
fighter's base animation speed, and `gFighterClashPower`, which decides parries, guard breaks, body pushes and
the blade-lock meter ([combat.md](combat.md)). 4096 is 1.0:

| Clash power | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Kannuki | 4341 | 4218 | 4833 | 4096 | 4587 | 3522 | 4587 | 4915 |
| Mikado | 4014 | 3932 | 4096 | 3768 | 4587 | 3276 | 4341 | 4751 |
| Black Lotus | 4341 | 4218 | 4710 | 3850 | 4096 | 3522 | 4587 | 4751 |
| Tatsumi | 3932 | 3686 | 4218 | 3522 | 4014 | 3276 | 4218 | 4505 |
| Red Shadow | 4014 | 4218 | 4218 | 4096 | 4014 | 3276 | 4341 | 4587 |
| Utsusemi | 4341 | 4096 | 4710 | 3768 | 4505 | 3276 | 4587 | 4751 |

| Animation speed | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Kannuki | 3891 | 3686 | 4096 | 3686 | 3481 | 3481 | 4096 | 4096 |
| Mikado | 3686 | 3891 | 3481 | 4096 | 4096 | 3686 | 4096 | 3891 |
| Black Lotus | 4096 | 4096 | 4096 | 3481 | 3891 | 4096 | 3891 | 3686 |
| Tatsumi | 3481 | 3481 | 3686 | 4096 | 3481 | 3686 | 3686 | 3686 |
| Red Shadow | 3686 | 4096 | 3481 | 4096 | 3686 | 3891 | 3686 | 3481 |
| Utsusemi | 4096 | 3686 | 3891 | 3481 | 3686 | 3481 | 4096 | 3686 |

The six other classes have their own values, whatever the weapon: clash power 5120, 4915, 3891, 4874, 4751 and
4505 for classes 6 to 11, animation speed 4096. Their weapons are fixed (`gBossDefaultWeapons`,
`src/combat_state.c`, by fighter id from 12): class 6 uses the nodachi, class 7 the long sword, Katze the gun,
class 9 the katana, class 10 weapon 9, class 11 the rapier; the Slash mode enemies (fighter ids 19 to 22) the
katana.

## Katze's gun

Katze starts with 10 rounds and 9 magazines. `FireGunshotRayAndCheckHit` (`src/combat_hit.c`) fires once, on the
first active frame of an attack clip, while rounds remain: the ray runs level from the gun's two points, 128
times their length, and is tested against the 22 spheres with outer and vital radii like a blade (except spheres
8 to 11 once the left arm is crippled, [oddities.md](oddities.md)). It then marches
32 steps of the barrel's length over the floor; if a wall comes before the target, the hit is cancelled and a
bullet impact appears on the wall. A reload clip refills the gun to 10 on its first active frame if a magazine
is left. The gun cannot be guarded, never clashes and never strikes walls; it is the only weapon whose trail
count is 0.

## Sub-weapons

Each character starts a bout with its own items (`gCharacterSubWeapons`, `src/subweapon.c`): Kannuki one of id
2, Mikado one of id 5, Black Lotus one of id 4, Red Shadow five of id 1, Utsusemi two of id 3; Tatsumi and the
other classes carry none. A fighter can hold 16 (11 in Japan).

**Throwing.** A throw clip releases on its first active frame, if the left arm is intact and the fighter is one
of the twelve playable ids (`ThrowFighterSubWeapon`). A throw placement flagged `MOTION_FLAG_SURFACE_SUBWEAPON`
throws the ground instead: the stage's surface under the fighter gives a burst kind (`GetStageSurfaceParticleType`,
`src/combat_event.c`) and the item is id kind + 7; a surface with no kind throws nothing. Otherwise the first
carried item goes and the rest move up. `SpawnSubWeapon` takes a free slot of `gSubWeaponSlots` (in the
Japanese build its search masks with 10, so only slots 0, 2, 8 and 10 are ever taken) and launches the item
along the fighter's heading with the speeds of `gSubWeaponParams`:

| Id | Speed | Upward | Spin | Trail |
| ---: | ---: | ---: | ---: | --- |
| 1 | 512 | -50 | 2048 | width 107 |
| 2 | 300 | -60 | 1152 | width 250 |
| 3 | 480 | -50 | 1536 | width 190 |
| 4 | 400 | -50 | 2048 | width 390 |
| 5 | 460 | -55 | 2048 | width 280 |
| 8 to 11 | 330 | -90 | 0 | none |

**Flight** (`UpdateAndDrawSubWeapons`): gravity adds 8 a frame (7 for debris, whose horizontal speed also falls
by a sixteenth); under the water of Dozaemon Moat (stage 19) the item slows to three quarters each frame and
its fall gains 4 a frame, up to 32; spin loses an eighth each frame. Where there is no floor (a wall) the item
rebounds at an eighth of its speed; a steep surface reflects it (`ReflectSubWeaponVelocity`); a floor bounces it
back up at a quarter of its fall. Any contact makes it harmless, and debris vanishes. Once nearly still it
settles and can be picked up.

**Hitting** (`CheckSubWeaponFighterHits`): a flying, still harmful item's path this frame is tested against the
other fighter's outer spheres, in index order. The first touched takes `ApplyFighterHitImpact` as a sub-weapon or
debris hit, its angle measured against the item's flight. Items 1 and 3 are parried by a guard, 2, 4 and 5 break
it; none makes a vital contact. The item then deflects off the body (`DeflectThrownSubWeapon`), losing three
quarters or seven eighths of its speed; debris instead pushes the target along. An item never hits its thrower.

**Picking up** (`PickupNearbySubWeapons`): on a pickup clip's first active frame, and every frame while crouching
or downed, every settled item within 1000 across and 100 in height of the fighter joins its list, the opponent's
included.

Each slot keeps a ring of its last 16 events (thrown, deflected, settled, cleared) so the replay can fly the items
again (`RecordSubWeaponHistory`, `RewindSubWeaponsForReplay`).
