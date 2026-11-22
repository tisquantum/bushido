# Combat

There is no health. A hit's outcome is decided by which of the defender's 22 body spheres the blade passes
through, whether it reaches a sphere's inner vital radius, which way the two fighters face, and what the defender
is doing. All of it runs in `UpdateCombatCollisions` (`src/collision.c`) after both fighters' frames
([fighter.md](fighter.md)), and lands through `src/combat_hit.c`.

## The body and the blade

`gBaseHitSpheres` (`src/fighter.c`) lists 22 spheres, each on a joint of the 18-joint skeleton
(`FighterHitSphere`, `include/fighter_hit_sphere.h`; joints numbered as `FighterJointIndex` in
`include/fighter_skeleton.h`: head 2, right arm 3 to 6, left arm 7 to 10, pelvis 11, right leg 12 to 14, left leg
15 to 17):

| Spheres | Bone | Outer radius | Vital radius |
| --- | --- | --- | --- |
| 0 | head | 228 | 228 |
| 1, 2, 3 | torso | 210, 210, 205 | 210, 210, 205 |
| 4, 5 | hips | 174 | 164 |
| 6, 7 and 8, 9 | left arm, second and third bones | 90 and 65 | none |
| 10, 11 and 12, 13 | right arm, second and third bones | 90 and 65 | none |
| 14, 15 and 16, 17 | right thigh and lower leg | 130 | none |
| 18, 19 and 20, 21 | left thigh and lower leg | 130 | none |

`ScaleFighterHitSpheres` (`src/fighter_hit.c`) scales offsets and radii by the fighter's stature into
`gFighterHitSpheres`, and `UpdateFighterHitGeometry` carries them to world space every frame. The same routine
places the weapon's edge points: two to four offsets per weapon (`gWeaponEdgeOffsets`) from the right-hand
weapon joint ([weapons.md](weapons.md)). Everything lives on the 1 KB scratchpad (`CollisionScratch`,
`include/collision_scratch.h`).

The head and torso spheres have a vital radius equal to the outer one: any blade that touches them makes a vital
contact. Only the hips leave a ring 10 units deep where a blade wounds instead of killing (and the naginata's
second segment, which never tests vital radii).

## A frame of collisions

```
blade lock under way -> UpdateBladeLockState, UpdateBladeLockSeparation; nothing else is tested
otherwise            -> ResolveFighterBodyCollision        bodies push apart
                        CheckWeaponClashAndEnvHit          weapon against props, walls, the other blade
                        a qualifying clash                 -> start a blade lock
                        no lock                            -> ResolveWeaponVsBodyHits
then always          -> keep this frame's edge points; CheckSubWeaponFighterHits; DrawAllWeaponTrails
```

**Against the stage** (`CheckWeaponClashAndEnvHit`): during an attack's active frames, a weapon that meets a
solid prop (`CheckWeaponStagePropHit`, `src/stage_prop_hit.c`) or whose tip or base point goes into a wall
higher than the fighter's floor (`CheckWeaponWallStrike`) bounces: the fighter takes the clash recoil reaction
and the swing loses `hitActive`, so it can hit nothing more. Cuttable props are cut instead
(`TriggerCuttablePropFall`). The wall test alone is skipped in Slash mode, on stage 11 and during homing-jump
attacks; on Executioner's Cove a wall counts only when its normal is steep enough (`COVE_WALL_NORMAL_Y_MIN`).

**Blade against blade**: each weapon's sweep this frame is a quad of its current and previous edge points, cut
into two triangles; after a box test the triangles of the two sweeps are intersected
(`TestTriangleIntersectsAlignedTriangle`, `src/geometry.c`), coordinates halved to keep the products in range.
A clash records a contact point, three parts this frame's points to one part last frame's. The gun never
clashes.

**Blade against body** (`ResolveWeaponVsBodyHits`): the attacker's sweep is two triangles, four if the tip moved
more than 2500 units, and each is tested against the defender's spheres (`TestTriangleVsBodySpheres`): a box
test, then sphere against triangle, and for a vital sphere the same test with the vital radius. The part hit is
the touched sphere nearest the attacker's previous tip. If the blades also clashed this frame and the clash point
is nearer than every touched sphere, the swing counts as a clash, not a hit.

A touched sphere is only pushed, not hurt, unless the attacker is attacking with `hitActive` set and inside the
clip's active frames (`attackActive`). A live swing hurts once: `hitActive` is cleared on its first hit or
clash. The impact angle is the difference of the two headings folded into 0 to 2048; above 1024 the attacker
struck from in front of the defender.

## What a hit does

`ApplyFighterHitImpact(mode, fighter, sphere, angle, isVitalHit, guardDeflects)` takes a blade (the gunshot
too), a sub-weapon or a burst of debris. In order:

1. A fighter already dying only bleeds again.
2. **Guard.** A guarding fighter hit from the front by a blade or a sub-weapon is not hurt. Against a blade it
   parries (the reaction for the attacker's stance, actions 0 to 2) when its `clashPower` is at least the
   attacker's, else its guard breaks (3 to 5, by its own stance). Sub-weapons 1 and 3 are parried, the others
   break the guard. Katze's gunshot is never guarded.
3. **Debris** hitting a fighter from the front, unless it is down, shows the wound face and rumbles, and staggers it
   when the sphere is vital; any debris hit adds the debris's velocity to the fighter's. It does no harm.
4. A **defeated** fighter (action 21) dies from any hit.
5. A **downed** fighter: any vital sphere kills (whatever the vital test said); a limb staggers it and takes the
   limb's injury below.
6. A **standing** fighter:

| Spheres | Effect | Reaction |
| --- | --- | --- |
| 0 to 5, vital contact | killed | fatal, front or back by the impact angle (11, 12) |
| 0 to 5, no vital contact | `torsoWoundCount` + 1 | stagger, front or back (6, 7) |
| 6 to 9 | `leftArmIntact` = 0 | left-arm cripple (10) |
| 10 to 13 | `rightArmWoundCount` + 1 | right-arm wound (9) |
| 14, 15, 18, 19 | `thighWoundCount` + 1 | thigh wound (23) |
| 16, 17, 20, 21 | `legsIntact` = 0 | leg cripple (8) |

Sub-weapons pass no vital contact, so they wound and cripple but kill only a downed fighter. Any blade, gun or
sub-weapon hit on player one that gets past the guard clears `gStoryNoDamageFlag` ([game-flow.md](game-flow.md)).

Each outcome also sets the face expression, a rumble preset, and a combat event (`EmitCombatEvent`,
`src/combat_event.c`, 20 kinds in `include/combat_event.h`, listed in [combat-events.md](combat-events.md))
carrying a position, which the engine turns into the visible and audible consequence: blood sprayed against the
direction of the attacker's blade tip for a kill, wound and spark effects otherwise
([rendering.md](rendering.md), [audio.md](audio.md)); the same event is recorded for the replay.
`MarkFighterInjuredRegion` adds the region to `gFighterInjuredRegions`, which selects wound textures on the
model ([models.md](models.md)); in Story mode player one's regions carry over from bout to bout, while the wound
counters reset at every bout's start.

## Lasting injury

- **Left arm**: posed hanging every frame (`ApplyDisabledLeftArmPose`, `src/fighter.c`); no sub-weapon can be
  thrown; a two-handed weapon's attacks run slower. From then on the blade, gunshot and sub-weapon tests skip
  spheres 8 to 11, which are not the left arm's alone ([oddities.md](oddities.md)).
- **Legs**: the fighter can no longer climb, enters a blade lock never, and wins with the legs-lost victory
  motion; which motions remain is decided by the motion archive.
- **Wounds** slow animation outside an attack's active frames (`UpdateFighterAnimSpeed`,
  `src/fighter_anim_speed.c`, `gWoundSpeedScales`), counts capped at 4; active frames always run at full speed:

| Count | 1 | 2 | 3 | 4 | Applies |
| --- | ---: | ---: | ---: | ---: | --- |
| torso | 0.95 | 0.90 | 0.85 | 0.80 | all but fixed-speed actions |
| right arm | 0.90 | 0.80 | 0.70 | 0.60 | attacks |
| thigh | 0.95 | 0.90 | 0.80 | 0.70 | other actions, legs intact |
| left arm, two-handed weapon | 0.85 | | | | attacks |

A backward fall also wounds the torso ([fighter.md](fighter.md)).

## Clashes

`ResolveBladeClashReaction` decides what a clash does to the fighter whose blade was met:

- attacking: no reaction if the attack is flagged downed; otherwise, if both or neither have crippled legs, the
  fighter recoils unless the opponent's stance is the next one in the cycle high, middle, low, high (a high
  attack holds against a middle one, a middle against a low, a low against a high); if only one has crippled
  legs, that one recoils;
- guarding and met from the front: a parry if its `clashPower` is strictly greater, else a guard break;
- standing, crouching, climbing or falling: clash recoil (14); dead, downed or in a special action: rumble only.

`clashPower` is `baseClashPower` each frame, plus 0x51E (about 0.32) for an attack begun from the high stance's
neutral clip.

## Blade lock

When both swings are active, neither is a downed, homing or running attack, neither fighter is jumping or has a
reaction pending, both have their legs, both are in the **same stance** and their floors differ by less than
1000, a clash becomes a lock (`gBladeLockState`, `InitFighterBladeLock`). Each fighter's `bladeLockMeter` grows
by `clashPower * 2 / (frames since that button + 1)` for each press of triangle, circle or cross, and decays by
an eighth on a frame without a press (`UpdateBladeLockInput`); above 2048 the fighter pushes. A fighter whose
meter is below 512 while the opponent's is below 2048 can back out with left; up and down sidestep. The two are
held about 1100 apart, the fuller meter driving the other back (`UpdateBladeLockSeparation`). When one meter
leads by more than 8192 and its owner is not holding right, that fighter gains the advantage
(`UpdateBladeLockState`): when its motion ends the loser staggers, unless the loser presses left, up or down
first and slips away. A fall breaks the lock.

## Bodies

`ResolveFighterBodyCollision` gives each fighter two spheres, radius 350 at the chest and 420 between the
thighs, and pushes overlapping bodies apart (at most 128 per contact), the one with the lower `clashPower` further.
Two fighters running faster than 3000 into each other head-on both stumble; a runner hitting a standing fighter
from behind staggers it.

## Death

The first fighter to enter a dead, fatal or defeated action, or a placement flagged fatal, decides the winner
(`gMatchWinnerSide`, set in `UpdateFighterFrame`); what follows is in [match-rules.md](match-rules.md).
`IsFighterSupine` (`src/geometry.c`) reads face up or face down from the torso spheres, which chooses the
front or back reaction for a downed fighter.
