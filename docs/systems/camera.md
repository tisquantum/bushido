# The camera

Every 3D view is set from one record, a focus point, three angles and a distance. A bout's camera follows the
two fighters and climbs over the stage's camera walls; the first-person camera rides a fighter's head; the
end-of-bout scenes move the record by formula; the opening and the story cutscenes play recorded camera
tracks. All of it is in `src/camera.c` and `src/camera_track.c` unless said otherwise.

## The record and how it places the eye

`CameraState` (`include/camera.h`, 0x1C bytes) holds `target` (a world point), `angles` (pitch, yaw, roll,
4096 to the turn) and `distance`. The game's one instance is `gCameraState`.

`BeginSceneFrame` (`src/scene.c`) turns the record into the camera node: the eye is at
`target + R(-angles) * (0, 0, -distance)` and the node's rotation is the negated angles, built in the fast
order. With a distance of 0 the eye is the target itself, which the first-person and aimed cameras use.

The projection is set separately: `SetGeomScreen` with 320 times the cotangent of half the field of view
(`CalcGeomScreenFromFov`, `src/opening_overlay.c`). A bout uses 45 degrees, or 80 in the first-person view
(`DEFAULT_SCENE_FOV`, `include/camera.h`; `COMBAT_FOV_POV`, `src/combat_state.c`).

## The battle camera

`UpdateBattleCamera` runs once a frame in a bout and in the attract demo; the instant replay runs it in its
second mode. Unless `gBattleCameraLocked` is set, it eases the record toward a goal:

- **Focus.** The midpoint of the two fighters' camera targets. The target moves a quarter of the way there
  each frame.
- **Distance.** The fighters' 3D separation times the cotangent of 165 angle units (about 14.5 degrees),
  halved, plus the absolute height difference. While either fighter runs and the result is under 16,000, 40
  percent of the gap to 16,000 is added. The distance moves a twelfth of the way each frame and is held between
  5,000 (3,000 during a blade lock and in the replay) and 35,000.
- **Yaw.** Square to the line between the fighters: a quarter turn less `ratan2(dx, dz)`. During a blade lock
  the yaw swings by the difference of the two fighters' lock meters divided by 32, at most 896 either way.
- **Pitch.** At least 0x80 (11.25 degrees); 0x40 on Meikyokan (stage 15) and on the lower levels of stages
  4 and 10, 0x20 in Slash mode. Walls can raise it (below). It may not exceed
  `0x200 - distance * 384 / 40000` (0x80 beyond a distance of 40,000, and 0x60 beyond 18,000 on those lower
  levels), so a far camera looks down less.

Angles move an eighth of the way each frame (`DivideSignedAngle` takes the short way round). In the replay
mode the focus slides between the winner and the loser as the replay frame advances, and the yaw sweeps half a
turn over 195 frames ([match-rules.md](match-rules.md) has the replay itself).

### Camera walls

A stage archive carries a list of camera walls (`StageCameraWall`, `include/camera.h`: an XZ segment and a top
height; [stages.md](stages.md)). Each frame the camera tests the segment from each fighter to the current eye
against every wall with `IntersectAxisAlignedWall`. Walls run along X or Z, and the order of their two ends says
which side blocks, so a wall hides the fighter from one side only. When the line is cut, the pitch at which the
eye would see over the wall's top from the fighter is computed (`ratan2` of the height difference over the
horizontal distance, its square root taken through `gSqrtLutQ3`), and when that is less than a quarter turn the
camera's pitch is raised to it plus 64, within the limit above. On the lower levels of stages 4 and 10, walls no
higher than 4,000 are ignored. The whole computation uses the scratchpad (`BattleCameraScratch`).

### Seeing through the stage

A wall that raises the camera also leaves an occlusion marker: the grid cell where the line crossed it, nudged
to the near side (`gOcclusionMarkers`). The next frame `ApplyCameraOcclusionMarkers` marks those cells
semi-transparent in the stage's detail grid.

After the fighters move, `TraceCameraSightlineTo` (`src/sightline.c`) walks the grid cell by cell from the eye
to each fighter, a line through the 3,200-unit cells. Every cell it crosses is marked occluded unless it is
already semi-transparent; from the fourth cell on, and at the fighter's own cell, it is also marked
semi-transparent. When the stage is culled ([stages.md](stages.md)), tiles flagged as hideable vanish in a cell
marked occluded only, and are drawn semi-transparent in a cell marked both; ordinary tiles are drawn
semi-transparent only in a cell marked semi-transparent and not occluded, which only the wall markers produce.
So scenery near the camera that stands between it and a fighter disappears, and further scenery turns
see-through. The first-person view does not trace sightlines.

## The first-person camera

`UpdatePovCamera` puts the eye at the fighter's head with distance 0 (`headPos`, `Fighter`). Its height eases a
quarter of the way each frame toward the head, raised by 950 units times the fighter's stature when the head is
less than 913 units above the feet (Y points down). The yaw follows the fighter's heading at once, half a turn
round while climbing, and eases an eighth of the way when the motion faces backwards. The pitch eases an eighth
of the way toward the angle to the opponent, weighted by how directly the fighter faces it, and toward zero when
the fighter stands upright and the opponent stands still 8,000 units or more away. The mannequin drawn over this
view is in [rendering.md](rendering.md).

## Scene cameras

Each is called once a frame with a Q12 progress (`src/story_state.c`): the victory and Continue cameras' count up from 0
to 4096, the Slash-clear camera's counts down from 4096 to 0, and the approach cutscene's camera ignores it.

| Routine | Scene | Movement |
| --- | --- | --- |
| `SetCameraTransitionFromFighter` | Victory | Distance from 6,000 in to 1,500, pitch from 0x200 down by 768 (544 if the legs are crippled), yaw turning an eighth of a turn |
| `UpdateContinueOrbitCamera` | Continue | Focus between the head and `position`, pitch 0x200, the yaw orbiting a thirty-second of the way per frame toward its goal; the distance pulls out from 1,500 to 7,500 as the countdown runs (back toward 1,500 once Continue is chosen), a thirty-second of the gap each frame |
| `SetSlashClearCamera` | Slash mode cleared | A full turn of yaw, the distance 4,000 less 3,000 times the cosine of the progress, the focus rising 1,200 units as it goes |
| `AimSceneCameraAtFighter` | Story mode's approach cutscene | The eye stays (on stage 9 it rises 64 a frame); pitch and yaw ease a quarter of the way toward the fighter, distance 0 |

These clear the occlusion markers, so nothing is made see-through.

## Recorded camera tracks

The opening demo and the story cutscenes move the camera from tracks in their archives (`DATA3/O01.DAT`,
`DATA7/V*.DAT`). `src/camera_track.c` keeps two readers. `InitCameraTrackReader` points one at a stream;
`DecodeCameraTrackFrame` reads one frame: for each of six values (position x, y, z and angles x, y, z) a
4-bit width (0 meaning 16) and a signed delta of that width, most significant bit first, added to the running
value.

The caller writes the result straight into the camera node, bypassing `gCameraState`: the position with X and Z
negated (offset from an anchor point in a cutscene, `src/story_cutscene.c`), the angles with a constant added
to the yaw (`OPENING_CAMERA_YAW_OFFSET`, `CUTSCENE_CAMERA_YAW_OFFSET`). It then does by hand what
`BeginSceneFrame` would: `BuildNegatedNodeRotation`, `UpdateAspectCorrectedCameraMatrix`, the detail grid stamp,
and the light directions of slots 0, 1 and 2.

## The camera as a listener

Positional sound is heard from the battle camera's eye, computed from `gCameraState` (`PlaySpatialSound`,
`src/combat_event.c`): see [audio.md](audio.md).
