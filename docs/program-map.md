# Program map

Bushido Blade is one PlayStation executable that stays resident for the whole game: no code is loaded from the
disc later, and every other file is data ([formats.md](formats.md)). Routine and global names are the source's;
a unit is a file of `src/`.

## Systems

Each system has a document of its own under `systems/`, saying how it works and which routines and records to
read.

The game engine:

- [The frame](systems/frame.md): boot, the main loop, timing, the display buffers and ordering tables.
- [Rendering](systems/rendering.md): from a scene node to GPU packets; the drawers, subdivision, skinning, the
  special cases and the 2D layer.
- [Models and animation](systems/models.md): the model tables and the compacting arena, the mesh records, the
  skeleton and skinned body, the pose stream.
- [Cloth](systems/cloth.md): the mass-spring solver, fighters' cloth parts and their colliders, the wind.
- [Lighting and fog](systems/lighting.md): the light slots, stage lighting, the point light, fog by palette swap.
- [The camera](systems/camera.md): the battle camera, camera walls and see-through scenery, the first-person and
  scripted cameras.
- [Stages](systems/stages.md): loading, the layout and the cell grid, culling, detail and see-through cells, the
  floor, cuttable props, each stage's own code, moving between stages.
- [Audio](systems/audio.md): the sound banks and their archives, the 24-voice queue, positional sound, stage
  ambience, XA music.
- [Files and memory](systems/files-and-memory.md): the disc's file table and the read state machine, inside the
  archives, the memory map of each build, the memory card's save format.
- [The memory card](systems/memory-card.md): the card events and file calls, the probe, the save, load and format
  commands, and the states that drive them.
- [Numbers and units](systems/fixed-point.md): angles, Q12, positions, the lookup tables and square roots, depth
  to ordering-table entry.
- [Effects](systems/effects.md): the 3D effect pool and its meshes, sparks, dust, splashes, debris, blood and
  spray, billboards, footprints, slash and ribbon trails.
- [Weather](systems/weather.md): snow and rain, particles kept around the camera and blown by the stage wind.
- [Video memory](systems/vram.md): the display area, the palette rows and the fog steps, and where fighters, effects,
  stage extras and screens put their textures, as far as the code fixes it.

The game:

- [The fighter](systems/fighter.md): the `Fighter` record, the motion archive and its placements, how one frame
  of a fighter runs, movement over the stage.
- [Input](systems/input.md): the controller frame, from pad to motion trigger, queueing and buffering, rumble,
  and what stands in for a controller: the computer, the link cable, scripted cutscenes.
- [Combat](systems/combat.md): the 22 body spheres and the blade's sweep, what a hit does by body part, guard,
  lasting injury, clashes, the blade lock, bodies colliding, death.
- [Frame events and combat events](systems/combat-events.md): the animations' event byte code, floor surfaces
  and their sounds, the twenty combat events and what each plays and shows, their replay.
- [Weapons and sub-weapons](systems/weapons.md): what differs per weapon, the per-character speed and clash
  power tables, Katze's gun, thrown items and their flight.
- [The computer opponent](systems/ai.md): what it reads, difficulty and per-character tendencies, its decisions,
  attack scoring, input scripts, Katze.
- [Game flow](systems/game-flow.md): the 51 states in groups, boot and the attract loop, the main menu and the
  modes, character select, options, unlocks and the movie theatre, saving.
- [Story mode and the code of honour](systems/story.md): the ten bouts and their opponents and stages, the
  honour rules and the readiness window, the three endings, ring-outs, a challenger.
- [The menus](systems/menus.md): sprite lists and the menu backdrop, the main menu's rolling strip, the options
  screen in both layouts, Japan's analog controller menu.
- [Character select](systems/character-select.md): the eight select modes, the 3D scene of two fighters and a host
  run by pose scripts, the cursors, weapon cards and stamp, the stage lists and their tables.
- [The opening](systems/opening.md): the splash screen, the attract opening's file and script, its fighter and camera
  scenes, its banners and card overlays, an unused title menu.
- [Story presentation](systems/story-presentation.md): the cutscenes' files, cameras, poses and faces, the
  approach cutscene, the prologue slideshow, the dishonourable ending, subtitles, the credits.
- [Movies](systems/movies.md): the stream player and its two builds, the movie theatre, the ending movies.
- [The rules of a match](systems/match-rules.md): a bout's start, its end and the post-kill count, the replay,
  victory and Continue, moving between the stages joined by portals.

Both:

- [Oddities of the original program](systems/oddities.md): what the 1997 code does other than its shape or data
  suggests, faithfully reproduced: checks that never pass, values that never last, states nothing enters.

## Where to find things

| To find | Look at |
| --- | --- |
| A fighter's stance | Not a field: the low two bits of the current animation's flags byte (`fighter->animInfo->flags & ANIM_FLAG_STANCE_MASK`, `include/fighter.h`), 0 high, 1 middle, 2 low (`STANCE_HIGH`, `STANCE_MID`, `STANCE_LOW`, `include/fighter_ai.h`). It changes when `SetFighterMotionPlacement` (`src/fighter.c`) moves the fighter to a placement whose animation has another stance; `UpdateFighterFrame` chooses that placement from the controller input (below, "The motion graph"). |
| What a button does in a fight | The current `MotionPlacement`'s triggers, in the motion archive on the disc; walked by `UpdateFighterFrame` (`src/fighter.c`) |
| Controller input | `PollControllers` and `gPadState` (`src/pad.c`); `BuildFighterInput` (`src/combat_state.c`) turns pads or the AI into a fighter's input |
| Hits, wounds, crippled limbs and kills | `ApplyFighterHitImpact` and the rest of `src/combat_hit.c`; the 22 hit spheres in `src/fighter_hit.c`; the swept tests in `src/collision.c` and `src/geometry.c` |
| How wounds slow a fighter | `UpdateFighterAnimSpeed` (`src/fighter_anim_speed.c`) |
| The locked-blades struggle | `gBladeLockState`; `UpdateBladeLockState` (`src/combat_hit.c`), entered from `UpdateCombatCollisions` (`src/collision.c`) |
| The computer opponent | `UpdateFighterAiInput` and the 30 states of `gAiStateHandlers` (`src/fighter_ai.c`) |
| Cameras | `UpdateBattleCamera`, `UpdatePovCamera` and the scene cameras (`src/camera.c`) |
| The game's flow from screen to screen | `gGameState` and `gGameStateHandlers` (`src/main.c`, indices in `include/game_state.h`) |
| Story, VS, Practice, Slash and Link modes | `gPlayMode` (`PLAY_MODE_*`, `include/game_state.h`); Slash mode's waves and the Katze unlock (`TryUnlockKatze`) in `src/story_state.c` |
| Stage loading, spawns and transitions | `LoadCombatStage` and `CheckStageTransition` (`src/combat_state.c`), `src/stage_tables.c`, `InitFighterForScene` (`src/fighter.c`) |
| Drawing | `BeginSceneFrame` and `EndSceneFrame` (`src/scene.c`), `SubmitSceneDisplayList` (`src/display_list.c`), the renderer in `src/render.s` |
| Reading a file from the disc | `ResolveCdFileHandle` and `StartCdFileRead` (`src/cd.c`); the loaders in `src/asset_load.c` and `src/model_arena.c` |
| Sound effects and voices | `QueueSoundEffect` and the voice queue (`src/sound.c`); positional sound and combat events in `src/combat_event.c` |
| Saving | `src/memcard.c` (the save data and its states), `src/memcard_io.c` (the card itself) |
| Options and unlocks | `ResetGameOptions` (`src/main.c`); `PackSaveFlags` and `UnpackSaveFlags` (`src/memcard.c`) |
| Source kept in an unusual form because the bytes need it | [kept-constructs.md](kept-constructs.md), by file |

## Layout in memory

| | North America | Japan | Europe |
| --- | --- | --- | --- |
| Loaded at | 0x80010000 | 0x80010000 | 0x80010000 |
| Code (`.text`) | 0x80012654 to 0x800789C4 | 0x800120D4 to 0x8007A3FC | 0x80012654 to 0x80078DD4 |
| Sony's library code begins | 0x80064BE0 | 0x800663E8 | 0x80064FF0 |
| Entry point (LIBSN's startup) | 0x8006F004 | 0x80070A1C | 0x8006F414 |
| Global pointer (`$gp`) | 0x800A0A48 | 0x800A2A84 | 0x800A0E34 |
| Small data ends | 0x800A0BCC | 0x800A2BD4 | 0x800A0FB8 |
| Uninitialized data ends | 0x801466D0 | 0x80147A28 | 0x80146AC8 |

Read-only data sits below the code, initialized data above it up to small data. The startup clears the
uninitialized data and calls `main`. Above it the game keeps two fixed work areas: a 16 KB shared buffer at
0x80148000 (`gSharedWorkBuffer`: the replay, memory card transfers, the select screen, subtitles) and the
dynamic load buffer at 0x8014C000 (`DYNAMIC_LOAD_BUFFER`, `include/load_area.h`), into which each scene reads
its files: a bout's motion archive and sound banks, the opening, the select scene, a cutscene, a slideshow, a
splash image; the movie player keeps its workspace there. Collision and rendering use the 1 KB scratchpad at
0x1F800000 (`include/collision_scratch.h`, `include/render_scratch.h`).

The units are linked in the order `config/bushidoblade/SCUS.LNK` lists them, which is the original's: the effects
and 3D engine first, then the presentation, sound, the main loop, the game states, fighters and combat, and last
the link cable, memory card and movie player.

## The source by subsystem

### Boot, main loop and game states

| Unit | Holds |
| --- | --- |
| `main.c` | `main`, `InitSystem`, default options, the rumble VSync hook, and `gGameStateHandlers` |
| `globals.c` | Definitions of uninitialized globals that no other unit defines; no code |
| `timer.c` | Root counter 1, counting horizontal blanks for frame pacing |
| `menu_state.c` | The front end's states: main menu, character select, options, the attract-mode demo battle |
| `opening_state.c` | The attract opening: the three opening variants with their music |
| `combat_state.c` | `InitCombatState` and `UpdateCombatState`, mode sound banks, ambient sound and music, scoring, fighter input |
| `story_state.c` | Victory, story cutscenes and the approach cutscene, Continue, endings, prologues, credits, the Slash clear, the movie theatre |
| `replay.c` | The instant replay: a 256-frame ring of fighter snapshots and its playback |
| `match_setup.c` | Character and weapon pair slots, loading the motion archive and the stage, fighter distance, the honour window, stage events |

### Fighters

| Unit | Holds |
| --- | --- |
| `fighter.c` | The fighter's frame: spawning, the motion graph, movement, floors, walls, ledges, jumps and falls, look angles, rumble |
| `fighter_turn.c`, `fighter_anim_speed.c` | Turning toward a heading; animation speed slowed by wounds |
| `pose_stream.c` | Decoding the compressed skeletal animation |
| `fighter_pose.c`, `fighter_skeleton.c` | The 18-joint skeleton: posing it, its meshes, shadows and weapons, height scaling |
| `fighter_hit.c` | The 22 hit spheres and weapon edge points carried to world space each frame |
| `fighter_ai.c` | The computer opponent: per-character tendencies, input scripts, the 30-state behaviour machine |
| `fighter_face.c`, `fighter_texture.c` | Facial expressions; textures, alternates and injury overlays |
| `fighter_cloth.c`, `fighter_cloth_setup.c` | Garment cloth and the limb capsules it collides with |
| `muzzle_flash.c`, `attached_part.c` | Katze's muzzle flash; the pool of parts attached to fighters |

### Combat

| Unit | Holds |
| --- | --- |
| `collision.c` | Each frame's collisions: weapon against walls and floor, blade against blade, blade against body |
| `geometry.c` | The intersection tests and orientation queries they use, on the scratchpad |
| `combat_hit.c` | What a hit does: wounds, crippled limbs, kills, parries and clashes, Katze's shot, the blade lock, body separation |
| `combat_event.c` | Positional sound, the animation frame events (footsteps, dust, voices) and the 20 combat events |
| `subweapon.c`, `subweapon_node.c` | Thrown items (sub-weapons and kicked-up surface debris): flight, pickup, hits and their models |
| `weapon_trail.c` | The blades' slash trails |
| `stage_prop_hit.c`, `cuttable_prop.c` | Cutting stage props (on stage 3 and Bamboo Thicket) and their falling tops |

### Cameras

| Unit | Holds |
| --- | --- |
| `camera.c` | The battle camera, the first-person camera, and the victory, Continue and Slash-clear cameras |
| `camera_track.c` | Decoding cutscene camera tracks |
| `sightline.c` | Walking the stage grid from the camera to a fighter to hide what stands between |

### The 3D engine

| Unit | Holds |
| --- | --- |
| `scene.c` | `Init3dEngine`, `BeginSceneFrame` and `EndSceneFrame`, and the render queue `gRenderQueue` |
| `display_list.c` | Composing the view matrices and handing the frame to the renderer |
| `render.s` | The hand-written renderer: the scene-node walk, transforms on the scratchpad, every primitive drawer |
| `otz.c` | The primitive dispatch tables and depth-to-ordering-table compression |
| `scene_math.c`, `rot_matrix.c`, `shadow_proj.c`, `gte_ir.c` | Lookup tables, world matrices, rotation builders, planar shadows |
| `model_table.c`, `model_arena.c`, `resource_cache.c` | The 20 model slots, the compacting arena that holds the loaded archives, the resource cache |
| `tpage_reloc.c`, `tim.c` | Moving a mesh to another texture page; uploading textures |
| `light_init.c`, `stage_light.c`, `point_light.c` | Cameras and light slots at start, the stages' day and night lighting, flashes of light |
| `fog.c`, `screen_fade.c`, `sprite2d.c`, `gradient_bg.c`, `tile_clip.c` | Depth fog by palette swap, full-screen fades, 2D sprite lists, the menu backdrop, near clipping of floor tiles |
| `call_thunk.s` | A call that keeps a value across it |

### Stages and effects

| Unit | Holds |
| --- | --- |
| `stage_env.c`, `stage_cull.c` | Setting a stage up; its grid, culling and per-stage callbacks |
| `stage_tables.c` | The 27 portals joining stages and the 26 stages' spawn points |
| `stage_callbacks.c` | Each stage's own routines (snow, rain, lights, the shoreline, foliage, water) |
| `floor_collision.c` | The floor height and surface under a point |
| `weather.c`, `water_anim.c`, `ocean.c`, `foliage.c`, `stage_moat.c` | Snow and rain, animated water, the sea of Executioner's Cove (stage 12), the foliage and moat of Dozaemon Moat (stage 19) |
| `cloth.c`, `cloth_strip.c` | The mass-spring cloth solver; stage 9's cloth strips |
| `decal.c`, `decal_driver.c` | Footprints on the ground |
| `billboard.c`, `billboard_driver.c`, `effect.c` | Camera-facing sprite effects; the 24-slot pool of 3D effects |

### Presentation

| Unit | Holds |
| --- | --- |
| `opening_demo.c`, `opening_overlay.c`, `ribbon_trail.c`, `ribbon_driver.c` | The attract-mode opening, its banners and slash-cut overlays, its ribbon trails |
| `select_scene.c`, `select_ui.c` | Character, weapon and stage select: the 3D showcase and the 2D interface |
| `main_menu_ui.c`, `options_ui.c`, `analog_menu.c` | The main menu, the options menu, Japan's analog controller menu |
| `font_hud.c` | The bitmap font, the in-game overlays (Continue, wins, Slash mode, pause, loading), the link lobby screen |
| `story_cutscene.c`, `story_slideshow.c`, `subtitle.c` | Story mode's 3D cutscenes, its picture slideshows, subtitles |
| `theater_ui.c`, `credits.c`, `splash.c` | The movie theatre screen, the staff credits, the boot splash |
| `pov_overlay.c`, `pov_tex.s`, `pov_flat.s`, `cuttable_fade.s` | The first-person view's mannequin and its drawers; the fading drawers of cut props |
| `vram_backup.c` | Saving and restoring a strip of video memory |

### Hardware and input/output

| Unit | Holds |
| --- | --- |
| `pad.c` | Reading both controllers, edge detection, rumble |
| `cd.c` | The CD driver: file reads, XA and CD audio streaming, the disc's file table |
| `asset_load.c` | Loading a file by id |
| `sound.c` | The sound banks, the 24-voice queue, reverb |
| `memcard_io.c`, `memcard.c` | The memory card and the save data |
| `movie.c` | The movie player |
| `link.c` | Two consoles over the link cable: the lobby and the packet each frame exchanges |

Sony's Runtime Library is under `src/lib/`, one directory per library ([building.md](building.md)).

## How a frame runs

### Boot

`main` (`src/main.c`) sets the PAL video mode in Europe, then `InitSystem` starts the controllers, the graphics
and geometry processors, the display buffers, the CD, the memory card, the 3D engine (`Init3dEngine`), the
frame timer and the default options. Outside Japan the publisher's splash screen plays (`PlayBootSplashScreen`).
`main` installs `RumbleVSyncCallback`, turns the display on and enters its loop at state 0.

### The main loop

Each pass of the loop is one frame, paced to 30 frames a second on NTSC and 25 on PAL:

1. `PollControllers` reads both pads (through the link cable when two consoles are linked) into `gPadState`.
2. The frame's display buffer is chosen (`db[gFrameCounter & 1]`) and its ordering table cleared into
   `gCurrentOt`.
3. `UpdateCdSubsystem` advances a pending file read or the music stream; `TickSoundVoiceCooldowns` ages the voices.
4. A link cable failure forces the link abort state; then `gGameStateHandlers[gGameState]()` runs the current
   state.
5. The loop calls `rand` until the frame timer reaches its mark, waits for the vertical blank and the GPU, and,
   unless a state has turned presentation off (`gDisplayPresentEnabled`), swaps the display and draw
   environments and draws the ordering table.

### One frame of a fight

`UpdateCombatState` (`src/combat_state.c`) runs a bout:

1. Pause and soft reset are checked first; while paused, only the pause overlay is drawn.
2. The camera moves: `UpdateBattleCamera`, or `UpdatePovCamera` in the first-person view.
3. `BeginSceneFrame` (`src/scene.c`) places the camera, updates the lights and opens the frame's GPU packet
   buffer; `UpdateCombatDistanceAndKoTimer` measures the fighters' distance.
4. For each fighter, `BuildFighterInput` takes the pad or asks `UpdateFighterAiInput`, and `UpdateFighterFrame`
   (`src/fighter.c`) advances the motion graph, decodes the pose (`DecodePoseFramePair`), moves the fighter over
   the floor, poses and queues the skeleton (`UpdateAndEnqueueFighterPose`), places the hit spheres
   (`UpdateFighterHitGeometry`), records the replay frame and fires the animation's frame events.
5. `UpdateCombatCollisions` (`src/collision.c`) runs the blade lock if one is under way; otherwise body
   separation, weapon against walls, props and the other blade (`CheckWeaponClashAndEnvHit`), and weapon against
   body (`ResolveWeaponVsBodyHits`), each landing through `src/combat_hit.c`. Thrown items are checked and the
   slash trails drawn.
6. Thrown items fly (`UpdateAndDrawSubWeapons`), the story's honour window and the stage's events are updated.
7. `EndSceneFrame` culls the stage and queues its fog (`CullStageAndQueueFog`), steps the cloth, the sprite and 3D
   effects and the lights, and hands the frame to the renderer (`SubmitSceneDisplayList`).
8. Ambient sound, subtitles and the mode's overlays follow, then the checks that end the bout: a kill, a fall, a
   move to another stage (`CheckStageTransition`).

## The game states

`gGameState` indexes `gGameStateHandlers` (`src/main.c`); most states are an initialising handler followed by
the handler that runs each frame. The indices are named in `include/game_state.h`.

| States | Handlers | What |
| --- | --- | --- |
| 0 | `RunBootVersionState` | Prints the version banner, then the boot auto-load |
| 1, 2 | `InitCombatState`, `UpdateCombatState` | A bout |
| 3, 4 | `InitOpeningState`, `UpdateOpeningState` | The attract-mode opening |
| 5, 6 | `InitDemoBattle`, `UpdateDemoBattle` | The attract-mode demo battle |
| 7, 8 | `InitMainMenuState`, `UpdateMainMenuState` | The main menu |
| 9, 10 | `InitCharacterSelect`, `UpdateCharacterSelect` | Character, weapon and stage select |
| 11, 12, 15, 16 | `GameState11NoOp` and the like | Empty |
| 13, 14 | `InitSlashClearCutsceneState`, `UpdateSlashClearCutsceneState` | The end of Slash mode |
| 17, 18 | `InitBattleReplay`, `UpdateBattleReplay` | The instant replay |
| 19, 20 | `InitVictorySceneState`, `UpdateVictorySceneState` | The victory scene |
| 21, 22 | `InitLinkLobbyState`, `UpdateLinkLobbyState` | The link cable lobby |
| 23, 24 | `InitOptionsMenu`, `UpdateOptionsMenu` | Options and the memory card |
| 25, 26 | `InitStoryCutsceneState`, `UpdateStoryCutsceneState` | Story mode's cutscenes between bouts |
| 27, 28 | `InitContinueScreenState`, `UpdateContinueScreenState` | Continue |
| 29, 30 | `PlayStoryEndingMovieState`, `WaitCancelToCreditsState` | An ending movie |
| 31, 32 | `InitAutoSaveState`, `UpdateAutoSaveState` | Automatic saving |
| 33, 34 | `InitBootAutoLoadState`, `UpdateBootAutoLoadState` | Loading the save at boot |
| 35, 36 | `InitBadEndingSlideState`, `UpdateBadEndingSlideState` | The dishonourable ending |
| 37, 38 | `InitStoryPrologueState`, `UpdateStoryPrologueState` | A character's prologue |
| 39, 40 | `InitCreditsState`, `UpdateCreditsState` | The staff credits |
| 41 | `HandleLinkAbortState` | Leaving after a link cable failure |
| 42 to 44 | `WaitCrossToMenuState`, `InitWaitCancelToMenuState`, `UpdateWaitCancelToMenuState` | Never entered: 42 and 44 wait for a button, then the main menu ([oddities](systems/oddities.md)) |
| 45, 46 | `InitAnalogControllerMenuState`, `UpdateAnalogControllerMenuState` | Japan's analog controller menu (empty elsewhere) |
| 47, 48 | `InitApproachCutsceneState`, `UpdateApproachCutsceneState` | Story mode's approach cutscene |
| 49, 50 | `InitMovieTheaterState`, `UpdateMovieTheaterState` | The movie theatre |

The play mode (`gPlayMode`) is separate: Story, VS, Practice, Slash (a hundred opponents in ten levels) and Link.

## Main records

| Record | Declared in | What it is |
| --- | --- | --- |
| `Fighter` | `include/fighter.h` | A combatant at run time, `gFighters[2]`: 808 bytes (792 in Japan) |
| `MotionPlacement`, `MotionTrigger` | `include/fighter.h` | A node of the motion graph and its input triggers |
| `AnimInfo` | `include/fighter.h` | An animation clip's action type, active frames, flags (with the stance) and frame events |
| `MotionFileHeader` | `include/motion_file.h` | The motion archive at 0x8014C000 |
| `FighterHitSphere` | `include/fighter_hit_sphere.h` | One of the 22 body spheres: its bone, offset, outer and vital radii |
| `PadInputFrame` | `include/pad.h` | A fighter's input for one frame (`currentInput`), from a pad or the AI |
| `SceneNode` | `include/scene_node.h` | The 132-byte node of the 3D scene: joints, shadows, props, cloth, effects and cameras alike |
| `CameraState` | `include/camera.h` | The camera's target, angles and distance (`gCameraState`) |
| `DisplayBuffer` | `include/display_buffer.h` | One of the two display buffers `db`: draw and display environments and the ordering table |
| `LightSlot` | `include/light.h` | One of the eight GTE light setups (`gLightSlots`) |
| `SubWeaponSlot` | `include/subweapon.h` | One of the 16 thrown items, 11 in Japan (`gSubWeaponSlots`) |
| `EffectSlot` | `include/effect_slot.h` | One of the 24 3D effects |
| `ClothModel` | `include/cloth.h` | A cloth mesh of the solver |
| `CdStream` | `include/cd.h` | The CD driver's state, in `gCdDrive`, whose storage the movie player shares (`MovieCdSlot`) |
| `LinkPacket` | `include/link.h` | The 8-byte packet two linked consoles exchange each frame |
| `MemoryCardSaveHeader` | `include/memcard.h` | The header a save file starts with: title, icon palette and frames |
| `MovieDecodeEnv`, `MovieState` | `include/movie.h` | The movie decoder's state: Japan's, and the other builds' with tiles and ordering tables |
| `StageTransition` | `include/stage_tables.h` | A portal between two stages |

### The fighter

The fields of `Fighter`, in their order in the record:

- **The computer's state** (`aiState` and its timers, scripts and attack tables), when the fighter is not a player.
- **Identity:** `opponent`, `isCpu`, `characterClass` (the archetype, below), `fighterId` (model, skin and voice),
  `weaponId`, `weaponPairSlot` (which half of the loaded motion archive is this fighter's), stature and weapon
  weight.
- **Motion:** `currentInput`, `placement` and `queuedPlacement`, `animInfo`, `action` (the clip's action type),
  frame and `animSpeed`, `hitActive`, `pendingAction` (a forced reaction).
- **Place:** `position`, `angles`, velocities, floor and wall contact, falling, climbing and jumping.
- **Injury:** `leftArmIntact`, `legsIntact`, `torsoWoundCount`, `thighWoundCount`, `rightArmWoundCount`.
- **The rest:** `bladeLockMeter`, `subWeaponCount` and the carried items, the saved pose for blending, rumble,
  Katze's `gunAmmo`, `faceExpression`.

### The motion graph

A fighter is always in one `MotionPlacement`: a span of frames of one animation, the placement that follows when
the span ends, and a list of `MotionTrigger`s. Each trigger is a controller button mask, the edge it fires on
(pressed, released, held or not held) and optionally the character classes it applies to, and the placement it
leads to. Each frame `UpdateFighterFrame` tests the triggers against the fighter's input and calls
`SetFighterMotionPlacement` with the first that fires (or queues it while the placement allows queueing). Attacks,
guards, steps and stance changes are all such transitions, read from the motion archive of the weapon pair in the
bout. The animation's `AnimInfo` gives the action type that the rest of the game tests (`action`), the frames in
which an attack can hit, and the flags that hold the stance.

## Game terms

- **Characters** (`characterClass`, `include/fighter.h`): 0 Kannuki, 1 Mikado, 2 Black Lotus, 3 Tatsumi, 4 Red
  Shadow, 5 Utsusemi, the six playable; then the story's bosses and Katze (8), the gunman Slash mode unlocks.
- **Stage:** one of the 26 places (`DATA2/S00.DAT` to `S25.DAT`; stage 23 only the opening demo loads), many
  joined by portals a fighter can cross mid-bout (`src/stage_tables.c`). `include/stage_id.h` names the
  nine the stage select shows by a plate, Slash mode's stage 20 and stage 23.
- **Wounds:** a hit that does not kill can cripple the left arm or the legs or count a wound
  that slows its animations. A hit on a vital sphere kills.
- **Blade lock:** two blades caught together; the players mash buttons to win the struggle.
- **Honour:** in Story mode, making the computer opponent react before it has readied itself (in its intro clip, or
  from behind while its short readiness window is held open) or while it climbs loses honour
  (`gBushidoHonorIntact`), and the story then ends in the dishonourable ending ([systems/story.md](systems/story.md)).
- **First-person view (POV):** play from the fighter's own eyes (`gPovModeEnabled`), with a mannequin overlay
  showing the fighter's body.
- **Sub-weapons:** items a fighter carries and throws, and the surface debris it kicks up.

## Open questions

- **Empty and unreferenced routines.** States 11, 12, 15 and 16 do nothing. A few routines are never called
  ([oddities](systems/oddities.md)), and the empty routine called first by `EndSceneFrame`
  (`EndSceneFrameNoOp`) compiles to an immediate return.
- **Unread fields.** Some record fields are written and never read, or read only by the hand-written renderer;
  their meaning is stated where the source knows it.
