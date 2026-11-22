# The frame

Bushido Blade runs one loop for its whole life: read the pads, run the current game state, wait for the
vertical blank, show what was drawn. What a game state does is [game-flow.md](game-flow.md); how the 3D
scene becomes GPU packets is [rendering.md](rendering.md).

## Boot

`main` (`src/main.c`) begins differently per build: the Japanese build sets the stack to 0x801FFF00 and calls
`SetMem(2)`, the European build calls `SetVideoMode(MODE_PAL)`. `InitSystem` then brings the machine up in
this order:

1. `ResetCallback`; the controllers (`InitPAD` on two 8-byte receive buffers, an empty rumble packet through
   `SendPAD`, `StartPAD`).
2. The GPU (`ResetGraph(0)`, display masked), the CD (`InitCdSubsystem`) and the GTE (`InitGeom`), with the
   projection centre at (320, 120) and the projection distance for a 45 degree field of view
   (`CalcGeomScreenFromFov`, `src/opening_overlay.c`: 320 times the cotangent of half the angle, 772).
3. The two display buffers (below), a `ClearImage` of the whole 640 by 480 frame buffer.
4. The memory card, the controller state, a black background clear, the 3D engine (`Init3dEngine`,
   `src/scene.c`), the frame timer (`InitHBlankRootCounter`, `src/timer.c`), the shared work buffer at
   0x80148000, the stage portal table and the default options.

Outside Japan the publisher's splash screen plays next (`PlayBootSplashScreen`, `src/splash.c`). `main` then
installs `RumbleVSyncCallback`, which runs `UpdateControllerRumble` at every vertical blank, unmasks the
display and enters the loop in state 0 (`RunBootVersionState`).

`Init3dEngine` resets the engine's pieces in order: the model arena, the rotation builders
(`InitRotMatrixDispatchTable`), the cached GPU command codes (`InitGpuPrimCodeCache`), the 20 model table
slots, the two camera nodes and eight light slots (`InitSceneCameras`, `InitLightSlots`, `src/light_init.c`),
the cloth system and the attached-part pool. It also clears the peak-usage counter of the packet buffer and
turns monochrome rendering off.

## Display buffers and ordering tables

The frame is 640 by 240, drawn and shown non-interlaced. The two buffers are `db[2]` (`DisplayBuffer`,
`include/display_buffer.h`), each a `DRAWENV`, a `DISPENV` and an ordering table of 4,101 entries
(`DISPLAY_OT_LEN`, 0x1005): 0x4084 bytes a buffer.

| Buffer | Draws into VRAM rows | Shows VRAM rows |
| --- | --- | --- |
| `db[0]` | 0 to 239 | 240 to 479 |
| `db[1]` | 240 to 479 | 0 to 239 |

A buffer's draw and display areas are the two halves of VRAM, so the picture on screen is always the one the
other buffer drew. In Europe both display areas start 28 lines down the taller PAL screen
(`DISPLAY_SCREEN_Y`).

The ordering tables are reverse tables: `ClearOTagR` links entry 4100 down to entry 0, and `DrawOTag` starts
at entry 4100, so a higher index is drawn earlier and lies further back. Every 3D primitive is linked at an
index computed from its depth ([rendering.md](rendering.md), [fixed-point.md](fixed-point.md)); 2D layers that
must be in front go in at entry 0. `gCurrentOt` points at the table of the frame being built.

## The loop

Each pass is one frame:

```
PollControllers                      pads (or the link cable) -> gPadState
db[gFrameCounter & 1]                this frame's buffer; ClearOTagR its table -> gCurrentOt
UpdateCdSubsystem                    one step of the CD read or music stream state machine
TickSoundVoiceCooldowns              age the 24 voice slots
gGameStateHandlers[gGameState]()     the game: simulate, build packets, link them into gCurrentOt
spin rand() until 0x180 hblanks      frame pacing (below)
VSync(1); DrawSync(0); VSync(0)      wait for the previous frame's drawing, then the vertical blank
ResetRCnt(RCntCNT1)
if gDisplayPresentEnabled:           PutDispEnv, PutDrawEnv, DrawOTag(&ot[4100]); gFrameCounter++
```

A link cable error (`gLinkErrorFlag`) forces `GAME_STATE_LINK_ABORT` before the state runs. The state
handlers are listed in [program-map.md](../program-map.md#the-game-states).

The GPU draws a frame's table while the processor builds the next one: `DrawOTag` returns at once, and the
`DrawSync(0)` of the following pass is where the processor waits for it. Because the frame counter's parity
picks the buffer, the packet buffer and the static meshes' packet copies ([rendering.md](rendering.md)), the
GPU never reads a packet the processor is writing.

When `gDisplayPresentEnabled` is clear the loop neither swaps nor draws: the last picture stays on screen and
the frame counter stops. The bout (`src/combat_state.c`) clears it when the game is paused, on the frame that
resets both fighters for another round or loads the next Slash-mode opponent, and on the frame that moves the
bout to another stage; the link lobby clears it while the consoles synchronise (`src/link.c`), and the menu,
opening and story states clear it on the frames they load.

## Timing

Root counter 1 counts horizontal blanks, free running (`InitHBlankRootCounter`, `SetRCnt(RCntCNT1, -1,
0x2000)`); the loop resets it right after each vertical blank. Before waiting for the next blank, the loop
spins until the counter reaches `FRAME_PACING_HBLANKS`, 0x180 (384) lines. A field is about 263 lines on
NTSC and about 313 on PAL, so 384 lines always lie past the next blank and `VSync(0)` then waits for the one
after: each frame lasts two fields, 30 frames a second on NTSC and 25 on PAL. A frame whose work runs past
two fields takes three. The loop itself makes no other allowance for PAL.

The spin calls `rand` on every pass, so the random sequence advances by however many passes the frame left
over. The Japanese build also has a routine that seeds `rand` from a checksum of the 0xB4000 bytes above
0x8014C000, the dynamic load area (`SeedRandomFromLoadArea`, `src/main.c`); nothing calls it.

## One frame of a bout

A bout's state, `UpdateCombatState` (`src/combat_state.c`), is the busiest frame. Its order matters for the
engine because each step leaves state the next one reads:

1. **Camera.** `UpdateBattleCamera`, or `UpdatePovCamera` in the first-person view, moves `gCameraState`
   ([camera.md](camera.md)).
2. **Scene begins.** `UpdateSceneCameraAndWind` calls `BeginSceneFrame` (`src/scene.c`) with the camera's
   target, angles and distance. It places the camera node at `target + R(-angles) * (0, 0, -distance)`, builds
   the view matrices, stamps the stage's detail grid around the camera, clears the sightline grid, rebuilds
   the light direction matrices of the two fighters' slots and the stage slot, takes this frame's packet
   buffer (`gPacketCursor`), runs the stage's frame callback, and resets the point-light targets and the cloth
   wind. The battle camera's wall markers are then applied and the stage's wind set.
3. **Fighters.** For each side, input then `UpdateFighterFrame` ([fighter.md](fighter.md)); posing the
   skeleton queues its scene nodes in `gRenderQueue` (`UpdateAndEnqueueFighterPose`, [models.md](models.md)).
4. **Collisions and items** ([combat.md](combat.md)), which queue trails, sparks and thrown items.
5. **Sightlines.** In the third-person view, `TraceCameraSightlineTo` walks the stage grid from the camera to
   each fighter ([stages.md](stages.md)).
6. **Scene ends.** `EndSceneFrame` culls the stage into the render queue and links its fog
   (`CullStageAndQueueFog`), rebuilds and steps the fighters' cloth, updates and draws the billboard and 3D
   effects and the attached parts, applies the point light, and hands the frame to the renderer
   (`SubmitSceneDisplayList`, `src/display_list.c`), which walks the queue and links every primitive into
   `gCurrentOt`.
7. **Sound.** Ambient sounds, then `FlushQueuedSounds` keys on every voice queued this frame
   ([audio.md](audio.md)). Subtitles and the 2D overlays follow.

Scripted scenes (the opening demo, story cutscenes) do not call `BeginSceneFrame`: they write the camera node
from a camera track and run the same stamping and light rebuilding themselves ([camera.md](camera.md)).

## Budgets the frame checks

- **GPU packets.** Each frame's dynamic packets go into one of two buffers of 0x15800 (88,064) bytes
  (`GetGpuPacketBuffer`, `src/model_arena.c`). `EndSceneFrame` records the bytes used and the peak, and calls
  `TriggerDebugTrap` when a frame used more than the buffer holds.
- **Scene nodes.** `gRenderQueue` holds 512 node pointers (`src/scene.c`). It is not bounds-checked.
- **The canary.** The last word of the effect textures (`gAuxRegion.effectTextures`, `src/model_arena.c`),
  which is also the last word of packet buffer 1, holds 0x12345678 (`MODEL_ARENA_CANARY`).
  `LoadArenaGroup` checks it before and after every load, and `CheckModelArenaCanary` when the opening demo
  asks; a changed word calls `TriggerDebugTrap` ([files-and-memory.md](files-and-memory.md)).

`TriggerDebugTrap` (`src/render.s`) stores zero to 0x1F800400, the first word past the scratchpad; on a retail
console nothing happens. What it signalled on the authors' hardware is not established.

## Where to look

| What | Where |
| --- | --- |
| The loop, `InitSystem`, the display buffers | `src/main.c`, `include/display_buffer.h` |
| The frame timer | `src/timer.c` |
| Beginning and ending the 3D frame | `BeginSceneFrame`, `EndSceneFrame` (`src/scene.c`) |
| Handing the frame to the renderer | `SubmitSceneDisplayList` (`src/display_list.c`) |
| The bout's frame | `UpdateCombatState`, `UpdateSceneCameraAndWind` (`src/combat_state.c`) |
