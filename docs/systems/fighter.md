# The fighter

A combatant is a `Fighter` record (`include/fighter.h`), one per side in `gFighters[2]`; side 0 is player one.
Everything a fighter does is chosen from the bout's motion archive and advanced once a frame (30 frames a
second; 25 in Europe) by `UpdateFighterFrame` (`src/fighter.c`). Angles are 4096 to a turn, scales are Q12
(4096 is 1.0) and world Y points down ([fixed-point.md](fixed-point.md)).

## The record

The fields fall into groups, in this order in the record:

- **The computer's state** (`aiOscAngle` through `aiAttackDelay`): used only when `isCpu` is set
  ([ai.md](ai.md)).
- **Identity and per-bout constants.** `opponent`, `side`, `isCpu`; `fighterId` (one of 24) and the
  `characterClass` it maps to through `gFighterIdToClass` (ids 0 to 5 and 6 to 11 are the six playable
  characters in two costume sets, 12 to 17 the bosses and Katze, 18 to 23 reuse playable classes); `weaponId`
  and `weaponGripType`; `statureScale` (`gCharHeightScales` by fighter id, in `src/fighter_skeleton.c`);
  `baseAnimSpeed` and `baseClashPower` from the per-class, per-weapon tables ([weapons.md](weapons.md));
  `tractionWeight`, the sum of `gCharTractionWeights[class]` and `gWeaponTractionWeights[weapon]`; jump speeds
  in `jumpLaunchParams`; `weaponPairSlot`, which half of the motion archive is this fighter's
  (`AssignFighterPairSlots` in `src/match_setup.c` gives slot 0 to the lower weapon id, counting weapon 9 as the
  naginata).
- **Motion.** `currentInput`; `placement` and `queuedPlacement`; `animInfo` (the clip's record), `action`,
  `animFrame`, `animFrac` and `animSpeed`; `pendingAction` and `pendingFallAction`, forced reactions waiting
  for the next frame; `facingState`; `hitActive` and `attackActive`; `bufferedButtons`.
- **Place.** `position` is `anchor` plus `rootOffset` (the clip's own root motion); `velocity` and `velocityRetention`;
  headings and the turn still to make; ground and wall contact; the jump, fall and climb states.
- **Body points** carried each frame from the skeleton: head, torso centre, feet, the weapon's previous edge
  points and tip velocity.
- **Injury**: `leftArmIntact`, `legsIntact` and three wound counters ([combat.md](combat.md)).
- **The rest**: the blade-lock meter, carried sub-weapons, the saved pose for blending, head look angles, rumble,
  Katze's `gunAmmo` and `gunMagazines`, `faceExpression`.

Per class (`src/fighter.c`), except stature, which is read by fighter id (`src/fighter_skeleton.c`): the
column gives it for the playable character's ids, 0 to 5 and 6 to 11.

| Class | Character | Stature | Traction | Jump forward, up | Climb offset |
| ---: | --- | ---: | ---: | --- | ---: |
| 0 | Kannuki | 4218 | 900 | 3840, -510 | 2 |
| 1 | Mikado | 3891 | 590 | 4080, -520 | 2 |
| 2 | Black Lotus | 4096 | 810 | 4080, -520 | 1 |
| 3 | Tatsumi | 3932 | 650 | 3968, -520 | 0 |
| 4 | Red Shadow | 4096 | 690 | 3840, -530 | 0 |
| 5 | Utsusemi | 4096 | 850 | 4080, -520 | 1 |

Classes 6 to 11 (the bosses and Katze) jump 4096, -540, with traction 900, 860, 730, 840, 900 and 610. Every
other fighter id has stature 4096, ids 18 to 23 included, though they reuse playable classes.

## The motion archive as the code reads it

Each bout loads one archive for its pair of weapons into the dynamic load buffer at 0x8014C000; the header is
`MotionFileHeader` (`include/motion_file.h`), reached through `MOTION_FILE`. `gWeaponPairMotionFiles`
(`src/match_setup.c`) picks one of 44 archives from the two weapon ids. The header holds the start placement of
each pair slot, the clip table (`AnimInfo`, clip 1 first), each clip's first frame in the pose stream, the pose
stream, the story intro placements per class, the victory placements (a second set for a winner whose legs are
crippled) and the AI's attack tables ([../formats.md](../formats.md)).

A **placement** (`MotionPlacement`, `include/fighter.h`) plays frames `startFrame` up to `frameLimit` of
one clip and names the placement that follows (`nextPlacement`). Input is read while the frame is below
`triggerLimit`; `canQueue` says whether input is queued instead of acted on, `flags` hold `MOTION_FLAG_FORWARD`
(clear: the clip plays backwards), `MOTION_FLAG_FATAL`, `MOTION_FLAG_SURFACE_SUBWEAPON` and
`MOTION_FLAG_BLEND_ON_EXIT`, and `facingMode` says how the fighter turns. `actionTable` points to the placements
forced reactions lead to. A zero-terminated list of `MotionTrigger`s follows the 20-byte header: a 32-bit mask
(button bits 0 to 15, the edge in bits 16 and 17: pressed, released, held, not held; an optional class mask from
bit 18) and the placement it leads to.

A **clip record** (`AnimInfo`) is four bytes and a pointer: the action type, the first and last active frame,
flags, and the frame events. The type becomes `Fighter.action` (`FighterAction`: neutral, attack, guard, recoil,
stagger, fatal, crouch, heavy recoil, climb, fall, jump, downed, dead, special, defeated, pickup, throw, blade
lock, blend, reload; type 8 is tested but has no name). The flags (`AnimInfoFlag`) are the **stance** in the low
two bits (0 high, 1 middle, 2 low), a red slash trail, run, walk, homing jump, fixed speed and downed. A
fighter's stance is therefore not stored anywhere: it is whatever the playing clip says. The frame events, read
by `ProcessAnimFrameEvents` (`src/combat_event.c`, [combat-events.md](combat-events.md)), are pairs of
event and frame, optionally marked for
backward play or limited to some classes: below 0x80 footsteps and sounds, from 0x80 up `HandleAnimSpecialEvent`.

## Changing placement

`SetFighterMotionPlacement` (`src/fighter.c`) enters a placement: it follows `nextPlacement` past any of zero
length, looks up the clip, sets `animFrame` to `startFrame`, copies the clip type into `action`, re-arms
`hitActive` (unless the fighter is leaving a homing-jump attack), takes the placement's facing mode, and turns
the fighter half a turn if the placement it leaves had an inverted facing (modes 3 to 5). Within a frame,
placements change in this order:

1. A forced reaction: if `pendingAction` is set, the fighter enters `placement->actionTable[pendingAction]`.
   The indices are `ForcedAction` (guard parry 0 to 2 and guard break 3 to 5 by stance, staggers 6 and 7, leg
   cripple 8, right-arm wound 9, left-arm cripple 10, fatal front and back 11 and 12, blade-lock entry 13, clash
   recoil 14, climbs 15 to 17, head-on stumble 18, prone leg cripple 19, falls 21 and 22, thigh wound 23); during
   a blade lock they are `BladeLockAction`.
2. Otherwise, the end of the span: at `frameLimit` the fighter takes the queued placement if there is one, else
   `nextPlacement`; a queueable placement also hands over to a queued one as soon as `animFrame` reaches
   `triggerLimit`.
3. Input: while `animFrame` is below `triggerLimit` and nothing is queued, the triggers are tested in order and
   the first whose buttons show on its edge fires; a queueable placement stores it, any other enters it at once.
   Leaving a guard for a placement of the same clip continues it mirrored from the current frame.
4. A fall: `pendingFallAction` enters its placement after the input.

Climbing a ledge, the end of a pickup and Katze's reload are handled in the same pass. Most transitions start a
pose blend (`StartFighterPoseBlend`, `src/fighter_turn.c`): the last decoded pose is mixed out with weights 0xC00,
0x800 and 0x400 over three frames (step 0x400), over five when starting a climb (step 0x300).

## One frame

`UpdateFighterFrame(player, input)` runs, in order:

```
input      copy the frame's input; on the bout's first frame clear it and sample the floor;
           in the first-person view rotate the d-pad (up becomes toward the opponent)
clock      angleToOpponent; record a winner if this fighter is dead, fatal or defeated;
           UpdateFighterAnimSpeed; animFrac += animSpeed, whole frames go to animFrame
graph      forced reaction / end of span / triggers / fall, as above; climb start; gun reload; blends
turn       a placement with automatic facing turns toward the opponent: each frame the heading moves
           turnRemaining * animSpeed / 0x6000, a sixth of the remaining turn at full speed
pose       DecodePoseFramePair decodes this frame and the next (src/pose_stream.c); a crippled left
           arm is overwritten by ApplyDisabledLeftArmPose; the head (POSE_JOINT_HEAD) gets the look angles
root       the clip's root joint gives rootOffset, rotated to the heading and scaled by stature;
           its change since last frame moves the anchor
move       velocity is scaled by velocityRetention; jump velocity, run steering; separation clamp;
           floor, wall, ledge, jump, fall and climb resolution
body       BlendPoseJointRotations and UpdateAndEnqueueFighterPose; UpdateFighterHitGeometry
           places the 22 hit spheres and the weapon edge points (combat.md)
actions    throw or pick up sub-weapons; velocityRetention from foot contact; record the replay frame
flags      attackActive while an attack is inside its active frames; clashPower; weaponTipVel;
           isSupine; pit timer; animation frame events; rumble; face expression
```

What crosses to the engine is the pose: `UpdateAndEnqueueFighterPose` (`src/fighter_pose.c`) takes the joint
angles, position, heading and shadow height and queues the skeleton ([models.md](models.md),
[rendering.md](rendering.md)).

`UpdateFighterAnimSpeed` (`src/fighter_anim_speed.c`) sets `animSpeed`: 1.0 for fatal, recoil, stagger and
fixed-speed clips and for an attack inside its active frames; otherwise `baseAnimSpeed` reduced by wounds
([combat.md](combat.md)).

## Moving over the stage

Floors are read with `QueryFloorHeight` (`src/floor_collision.c`, [stages.md](stages.md)), which returns a
height and a surface normal;
heights come in bands of 2000 (`FLOOR_BAND_HEIGHT`) and a surface is walkable when its normal's Y is below
-3072 (about 41 degrees). `ResolveFighterFloorMovement` moves the fighter to its wanted point: up to 99 units up
or any distance down a walkable surface it follows the floor; down a steep one it starts a fall
(`StartFighterFallFromLedge`); otherwise it is blocked, records the wall, and slides along it with three
quarters of the move, up to three times. With the wall-bump option on, a fighter running faster than 3000 that
loses more than half its move against a wall stumbles (`TryStartWallBumpStumble`), except in Link play.

- **Running**: clips flagged run add `statureScale / 16` to `runSpeed` each frame, less the turn just made,
  up to `statureScale * 25 / 32`; the fighter steers toward the d-pad direction relative to the opponent
  (`UpdateFighterRunSteering`, `gDpadAngleTable`).
- **Climbing**: R1 with a direction, against a recorded wall, legs intact, standing (or just after a head-on
  stumble): `CanClimbRecordedLedge` probes beyond the wall at three points and needs all three exactly one band
  higher. The climb placement is action 15 plus the class's offset; `climbState` counts four steps.
- **Jumping**: a jump clip, or an attack flagged homing jump, launches the fighter. A plain jump goes along the
  heading at the class's speeds; a homing jump takes 70 percent of the upward speed and solves the horizontal
  speed to land on the opponent's torso (`SolveBallisticJumpVelocity`, `src/combat_event.c`). Gravity adds 62
  a frame; landing on a walkable surface ends the jump.
- **Falling**: `fallState` 1 is a fall forwards, 2 a fall backwards (and any fall of a downed fighter). A clip
  that reaches its last frame during a fall is held there while 500 is added to the downward velocity each
  frame; before that, clips other than the fall clip add an amount that grows by 52 each frame. Landing from a
  backward fall adds a torso wound, two if the drop was more than one band.
- **Separation**: in most standing actions (`CLAMP_SEPARATION_ACTIONS`) a fighter cannot move to more than 18316
  units from the opponent (`ClampFighterMaxSeparation`), except in the first-person view.
- **Friction**: 0x800 less 0x200 for each foot on the ground, scaled by `(1400 - tractionWeight) * 3 / 1400`
  and kept within 0x600 to 0xC00; it is the share of velocity kept each frame, so heavier fighters stop sooner.

The head follows the opponent while standing or crouching (`UpdateFighterLookAngles`): yaw up to 512 and pitch
from -64 to 640, closing a quarter of the gap each frame.
