# What the program holds that play does not reach

A finding here is a fact about the three shipped executables (Japan `SLPM_860.20`, North America `SCUS_941.80`, Europe `SCES_008.99`) or their retail discs that play does not show or that contradicts what play suggests, proven directly from the decompiled source, its tables, and the retail disc images. Every finding holds for all three builds unless a build is named, and every finding stands on the proof given; some were known to players before this decompilation. Throughout, **no caller** means no call instruction, no address taken, and no table or callback reference in the linked executable, counted over the game's own compiled and assembled routines (`984` in North America, `0x80012654`–`0x80064BE0`, and Europe, `0x80012654`–`0x80064FF0`; `1,006` in Japan, `0x800120D4`–`0x800663E8`, listed in `config/bushidoblade/reports/*.json` and ordered by `config/bushidoblade/SCUS.LNK`).

## Code nothing reaches

### Seventy-one own routines in North America and Europe (and ninety-six in Japan) have no caller

Of the game's 984 own routines in North America (334,012 bytes) and Europe (335,052 bytes), 71 routines (7,852 bytes, 2.35% in North America and 2.34% in Europe) across 31 units have no caller in the linked executable, and 6 more routines (1,136 bytes: `InitWhiteFullscreenBackdrop`, `SubmitWhiteFullscreenBackdrop`, `LoadDefaultFighters`, `ReadTimTextureInfo`, `SpawnUvAnimEffect`, and `UpdateUvAnimEffect`) are called only by those 71. In Japan, 96 of 1,006 own routines (14,392 bytes, 4.21% of 341,568 bytes) across 33 units have no caller—the 69 uncalled routines shared by all three builds plus 27 Japan-only routines absent from the later builds—and 8 more (1,516 bytes, adding `GetDepthScaleShift` and `ScaleModelMeshVertices`) are called only by them. One routine, `StartSubtitleScriptPtr`, exists only outside Japan and has no caller there; `GetDepthScaleShift` is called in Japan only by uncalled `DrawBillboardQuadNode` and has no caller outside Japan.

Proof: `config/bushidoblade/reports/SCUS_941.80.json`, `config/bushidoblade/reports/SLPM_860.20.json`, `config/bushidoblade/reports/SCES_008.99.json`; `config/bushidoblade/SCUS.LNK`; `src/billboard.c`, `src/call_thunk.s`, `src/cd.c`, `src/cloth.c`, `src/combat_event.c`, `src/decal_driver.c`, `src/display_list.c`, `src/effect.c`, `src/fighter_ai.c`, `src/fighter_cloth.c`, `src/fighter_cloth_setup.c`, `src/fighter_pose.c`, `src/font_hud.c`, `src/gte_ir.c`, `src/light_init.c`, `src/main.c`, `src/model_arena.c`, `src/model_table.c`, `src/opening_demo.c`, `src/otz.c`, `src/render.s`, `src/rot_matrix.c`, `src/scene.c`, `src/scene_math.c`, `src/select_scene.c`, `src/shadow_proj.c`, `src/sprite2d.c`, `src/stage_cull.c`, `src/stage_light.c`, `src/subtitle.c`, `src/tim.c`, `src/timer.c`, `src/tpage_reloc.c`, `src/vram_backup.c`.

### A seven-entry title menu in `src/opening_demo.c` has no caller

`InitOpeningTitleMenu` (64 bytes) and `UpdateAndDrawOpeningTitleMenu` (616 bytes) implement a standalone seven-entry scrolling menu (`gOpeningTitleMenuSprites[7]`) drawn over a title logo (`gOpeningTitleLogoSprites`), flashing up/down arrows (`gOpeningTitleArrowSprites`), and a full-screen white backdrop (`InitWhiteFullscreenBackdrop`, `SubmitWhiteFullscreenBackdrop`, 284 bytes combined). Each entry scrolls at 3 lines per frame (`OPENING_TITLE_SCROLL_STEP`) across an 18-line strip. Nothing in any build calls `InitOpeningTitleMenu` or `UpdateAndDrawOpeningTitleMenu`.

Proof: `src/opening_demo.c` (`InitOpeningTitleMenu`, `UpdateAndDrawOpeningTitleMenu`, `InitWhiteFullscreenBackdrop`, `SubmitWhiteFullscreenBackdrop`, `gOpeningTitleMenuSprites`).

### A two-player Continue HUD has no caller

`DrawTwoPlayerContinueHud` (268 bytes in North America and Europe; 324 bytes in Japan) draws two-digit `1P` and `2P` win counts at `(56, 32)` and `(464, 32)` in dark blue (`0x101060`) above a double-scale `"CONTINUE? %1x"` countdown at `(157, 104)` in cyan (`0x808000`). No routine in any build calls it; the shipped Continue screen uses its own single-player banner instead.

Proof: `src/font_hud.c` (`DrawTwoPlayerContinueHud`).

### Two entire units have zero callers

`src/vram_backup.c` (`BackupVramStrip` and `RestoreVramStrip`, 120 bytes total) saves and restores a 256×5 strip of video memory at `(320, 480)` via `StoreImage` and `LoadImage`, and `src/call_thunk.s` (`CallWithPreservedReturn`, 44 bytes) invokes a function pointer `fn(arg)` while returning a preserved value `a0`. No routine in any build calls any function in either unit.

Proof: `src/vram_backup.c` (`BackupVramStrip`, `RestoreVramStrip`, `gVramStripRect`), `src/call_thunk.s` (`CallWithPreservedReturn`).

### The program's only `srand` call exists in Japan alone and has no caller

`SeedRandomFromLoadArea` (112 bytes, `src/main.c`, compiled only under `VERSION_JP`) sums and XORs `0xB400` four-word blocks of `DYNAMIC_LOAD_BUFFER` and passes the result to `srand`. Nothing calls it in Japan, and no routine calls `srand` at all in North America or Europe. As read from the code, `LIBC`'s random generator always starts from its default seed `1` at boot, diverging across runs only through the `rand()` calls spun inside the main loop's frame-pacing wait (`GetRCnt(RCntCNT1) < FRAME_PACING_HBLANKS`) and on menu frames.

Proof: `src/main.c` (`SeedRandomFromLoadArea`, `main` frame-pacing loop), `src/menu_state.c` (`UpdateMainMenuState`, `UpdateCharacterSelect`).

### Debug traps, host polling, and console `printf` messages remain linked in retail executables

`TriggerDebugTrap` (`src/render.s`) halts by storing `0` to `0x1F800400` (the word immediately past the 1 KB scratchpad) and is called from 26 error-check call sites across 10 C units (plus a jump in `src/render.s`)—including `TriggerDebugTrap("Multipul Model")` in `InitClothPartFromMesh`—and fills unused slots of `gSceneNodeRenderDispatch`. `LoadCombatSoundBanks` checks `headerSize > SOUND_VAB_HEADER_WORK_SIZE`; if exceeded, it prints `"SE HEADER SIZR:%08x\n"` and `"SE HEADER SIZE OVER FLOW\n"` via `printf` and enters an infinite `pollhost()` loop. `RunBootVersionState` prints each build's version banner via `printf` at boot, and `WaitLinkPacketRead` prints `"\nREAD ERROR\n"` and `"\nREAD TIME OUT:%d\n"` on link-cable read failures.

Proof: `src/render.s` (`TriggerDebugTrap`, `gSceneNodeRenderDispatch`), `src/fighter_cloth.c` (`InitClothPartFromMesh`, `"Multipul Model"` call), `src/combat_state.c` (`LoadCombatSoundBanks`, `headerSize > SOUND_VAB_HEADER_WORK_SIZE` block), `src/main.c` (`RunBootVersionState`), `src/link.c` (`WaitLinkPacketRead`).

### Four game states are never entered, and four more are empty stubs

Of the 51 handlers in `gGameStateHandlers`, states 11, 12, 15, and 16 (`GameState11NoOp`, `GameState12NoOp`, `GameState15NoOp`, `GameState16NoOp`) are empty routines, and states 45 and 46 (`InitAnalogControllerMenuState`, `UpdateAnalogControllerMenuState`) are empty outside Japan. Four further states have no path into them: `GAME_STATE_WAIT_CANCEL_TO_CREDITS` (30), `GAME_STATE_WAIT_CROSS_TO_MENU` (42), and `GAME_STATE_INIT_WAIT_CANCEL_TO_MENU` (43) are never assigned to `gGameState`, while `GAME_STATE_UPDATE_WAIT_CANCEL_TO_MENU` (44) is assigned only by state 43. State 30 waits for Cancel and moves to the credits; state 42 waits for Cross and moves to the main menu; state 43 transitions directly to state 44, which waits for Cancel and returns to the main menu.

Proof: `include/game_state.h` (`enum GameState`), `src/main.c` (`gGameStateHandlers`), `src/menu_state.c` (`GameState11NoOp`, `GameState12NoOp`), `src/story_state.c` (`GameState15NoOp`, `GameState16NoOp`, `WaitCancelToCreditsState`, `WaitCrossToMenuState`, `InitWaitCancelToMenuState`, `UpdateWaitCancelToMenuState`, `InitAnalogControllerMenuState`, `UpdateAnalogControllerMenuState`).

### The all-modes unlock check on the main menu can never run

`InitSystem` initializes `gAllMenuModesEnabled` to `1`, and no routine ever assigns a negative value to it. In `InitMainMenuState`, the button check for `L1`, `R1`, and `R2` held on both controllers (`PADL1 | PADR1 | PADR2`) sits inside `if (gAllMenuModesEnabled < 0)`. As read from the code, that test never runs, and every entry of `gMainMenuEntries` whose `alwaysUnlocked` flag is `0` (including Link mode) is unlocked from boot.

Proof: `src/main.c` (`InitSystem`, `gAllMenuModesEnabled = 1`), `src/menu_state.c` (`InitMainMenuState`, `gAllMenuModesEnabled < 0` check; `gMainMenuEntries`).

### `IsWeaponSwingPlaneVertical` can never return true, so the computer always guards

`IsWeaponSwingPlaneVertical` computes the swing plane's normal Y component into an `unsigned int` and tests whether it lies between `-2896` and `2896`. In unsigned 32-bit arithmetic `-2896` converts to `0xFFFFF4B0`, and no unsigned value is simultaneously `>= 0xFFFFF4B0` and `<= 2896`, so the routine always returns `0` in all three builds. As read from the code, `fighter->opponent->isVerticalSwing` stays `0`, so `RunAiDefendOrEvade` always selects `AI_EVADE_GUARD` (0) rather than entering its standing or downed evasion choices.

Proof: `src/geometry.c` (`IsWeaponSwingPlaneVertical`), `src/fighter_ai.c` (`RunAiDefendOrEvade`, `opponent->isVerticalSwing == 0` branch).

### `RunAiDefendOrEvade` has an empty unassigned arm (`case 4`), leaving `gAiScriptBackstep` and `gAiEvadeScripts[6]` unreachable

In `RunAiDefendOrEvade`, the standing branch assigns `evadeChoice` from `rand() & 3` (`0..3`) and the downed branch assigns `(rand() & 1) + 5` (`5..6`), never assigning `4`. The `switch (fighter->aiScratch.evadeChoice)` includes an empty `case 4: break;`, while the active evasion cases index `gAiEvadeScripts[7]` by `evadeChoice - 1`: choices `AI_EVADE_STAND_NONE` (1) and `AI_EVADE_DOWNED_NONE` (5) index null entries `[0]` and `[4]`, `gAiEvadeScripts[3]` (`gAiScriptBackstep`, referenced nowhere else) requires the unassigned value `4`, and `gAiEvadeScripts[6]` (`gAiScriptSidestepUp`) requires `7`, above the maximum assigned value `6`.

Proof: `src/fighter_ai.c` (`gAiScriptBackstep`, `gAiEvadeScripts`, `enum AiEvadeChoice`, `RunAiDefendOrEvade`, `switch (fighter->aiScratch.evadeChoice)`).

### Three of the thirty AI states are unreachable, and two AI helper routines have no caller

In `gAiStateHandlers[30]`, nothing in the program assigns `AI_STATE_WALK_BACKWARD` (12) or `AI_STATE_SIDESTEP_DOWN` (29) to `aiState`, and `AI_STATE_WALK_FORWARD` (11) is assigned only inside `EnterAiWalkOrDash`, which has no caller. Two more AI helpers, `EnterAiRollAway` and `RollAiUnusedChance` (the sole reader of `AiCharParams.unreachedChance`), also have no caller, and `UpdateFighterAiInputNoOp` is empty.

Proof: `include/fighter_ai.h` (`enum AiState`, `AiCharParams`), `src/fighter_ai.c` (`gAiStateHandlers`, `EnterAiWalkOrDash`, `EnterAiRollAway`, `RollAiUnusedChance`, `UpdateFighterAiInputNoOp`).

### Nine of the fifteen billboard kinds and all fourteen `Spawn*Billboard` wrappers are never called

All 14 per-kind wrapper functions in `src/billboard.c` (`SpawnBillboard1`, `SpawnLimbWoundBillboard`, `SpawnOrangeSparkBillboard`, `SpawnFatalHitPrimaryBillboard`, `SpawnWeaponClashBillboard`, `SpawnFatalHitSecondaryBillboard`, `SpawnTorsoWoundBillboard`, and `SpawnBillboard8`–`SpawnBillboard14`) have no caller; `DispatchCombatEventEffect` calls `SpawnBillboardEffect` directly for kinds 2, 4, 5, 6, 7, and 15 only. Consequently, billboard kinds 1, 3 (`BILLBOARD_EFFECT_ORANGE_SPARK`), and 8 to 14 are never spawned on any build (kinds 1 and 8 to 14 also have no textures in `gBillboardTextures`, though their update handlers remain in `gBillboardEffectHandlers`).

Proof: `include/billboard.h` (`enum BillboardEffectKind`), `src/billboard.c` (`SpawnBillboard1` through `SpawnBillboard14`, `gBillboardEffectHandlers`, `gBillboardUvBuffers`), `src/combat_event.c` (`DispatchCombatEventEffect`, `SpawnBillboardEffect` calls).

### Unreachable switch arms remain in combat events, spray particles, the link lobby, opening scripts, and `RenderSceneDisplayList`

Five state or command switches retain arms nothing reaches: `COMBAT_EVENT_SPARK_SOUND` (16) in `DispatchCombatEventEffect` is never emitted; `SPRAY_KIND_NONE` (0) and `SPRAY_KIND_WATER_SURFACE` (3) in `UpdateSprayParticle` are never spawned; `LINK_LOBBY_UNUSED_8` (8) and `LINK_LOBBY_UNUSED_9` (9) in `UpdateLinkLobbyState` are never assigned; `OPENING_OP_LOAD_TIM` (3) in `StepOpeningDemoScript` appears in none of the three scripts of `gOpeningDemoScripts`; and the sole test of `SceneNode.flags` in `RenderSceneDisplayList` branches to the same target on both outcomes.

Proof: `include/combat_event.h` (`COMBAT_EVENT_SPARK_SOUND`), `src/combat_event.c` (`DispatchCombatEventEffect`), `include/effect_slot.h` (`enum SprayParticleKind`), `src/effect.c` (`UpdateSprayParticle`), `src/link.c` (`UpdateLinkLobbyState`), `src/opening_demo.c` (`StepOpeningDemoScript`, `gOpeningDemoScripts`), `src/render.s` (`RenderSceneDisplayList`).

### Formatting a memory card always reports success, leaving `MEMORY_CARD_STATUS_FORMAT_ERROR` unreachable

In `StepMemoryCardCommand`, the format arm calls `FormatMemoryCard`, ignores its return value, and unconditionally assigns `MEMORY_CARD_STATUS_FORMAT_OK`. No code anywhere assigns `MEMORY_CARD_STATUS_FORMAT_ERROR`, even though both `UpdateOptionsMenu` and `UpdateAutoSaveState` include arms to handle it.

Proof: `src/memcard.c` (`StepMemoryCardCommand`, format branch; `UpdateAutoSaveState`), `src/menu_state.c` (`UpdateOptionsMenu`).

### `LoadFighterBaseTextures`'s special case for Black Lotus applies no overlays

`LoadFighterBaseTextures` calls `ApplyFighterTextureOverlays` when `characterId` is in `6..11` (the story alternates) or equals `CHAR_ARCHIVE_BLACK_LOTUS` (2). However, `gFighterOverlayMasks` stores `0` for both base and alternate modes of characters `0..5` (including Black Lotus), so `available & bit` is always `0` for `CHAR_ARCHIVE_BLACK_LOTUS` and the loop uploads no overlay textures.

Proof: `src/fighter_texture.c` (`LoadFighterBaseTextures`, `gFighterOverlayMasks`, `ApplyFighterTextureOverlays`).

### Ordering-table depth ceiling checks against 8,192 and 4,096 always pass

`InsertDeferredPrimsIntoOt` rejects a bucket when `otz >= 8192`, and the billboard and ground-decal drivers reject when `otz >= 4096`. `CompressDepthToOtz` clamps and compresses every depth into `0..4095`, so none of those upper-bound rejects can ever trigger (the decal driver's lower-bound check can still reject).

Proof: `src/otz.c` (`CompressDepthToOtz`), `src/stage_callbacks.c` (`InsertDeferredPrimsIntoOt`), `src/billboard_driver.c`, `src/decal_driver.c`.

### `DrawPolyFt4Unlit`'s top and left off-screen rejection test never fires

Fifteen of the sixteen quad drawers in `src/render.s` reject quads wholly above or left of the screen by ANDing all four screen-coordinate words (`t0`, `t1`, `t2`, `t7`). `DrawPolyFt4Unlit` instead ANDs `t0`, `t1`, `t2`, and `t3`, where `t3` holds the first vertex's index multiplied by 4—a small non-negative integer with neither bit 31 nor bit 15 set. As read from the code, the sign/bit-15 test never succeeds, so every unlit textured quad proceeds to the back-face check and off-screen quads are submitted for the GPU to clip.

Proof: `src/render.s` (`DrawPolyFt4Unlit`).

---

## Table rows and disc content nothing references

### Five complete archives on every disc sit outside `gCdFileTable`

Apart from `SYSTEM.CNF`, the executable, and the zero-padding file (`DUMMY.DAT`, 63,072,000 bytes in Japan; `DUMMY2.DAT`, 126,144,000 bytes in North America and Europe), five valid data archives totalling 279,268 bytes appear identically on all three discs with no entry pointing to them in `gCdFileTable`:
- `DATA0/M26.DAT` (74,040 bytes): a unique four-section character archive (5 meshes, 4 bone overrides, 15 shadow meshes, 21 TIMs) whose mesh and texture sections match no other character archive.
- `DATA6/A04.DAT`, `DATA6/A05.DAT`, and `DATA6/A06.DAT` (52,812 bytes each; 158,436 bytes combined): three byte-identical copies of a unique four-section character archive (7 meshes, 6 bone overrides, 15 shadow meshes, 6 TIMs) matching no other archive; `gCdFileTable` slots 23 to 25 (and `gCdFileNames[23..25]`) point to `DATA6/A00.DAT` instead.
- `SE0/SFX.` (46,792 bytes): a standalone one-VAB sound archive (1 program, 8 tones, 8 VAG samples).

Proof: `src/cd.c` (`gCdFileTable`), `src/asset_load.c` (`gCdFileNames[19..25]`), [formats.md](formats.md), disc files `DATA0/M26.DAT`, `DATA6/A04.DAT`, `DATA6/A05.DAT`, `DATA6/A06.DAT`, and `SE0/SFX.`.

### Stage 18 (`DATA2/S18.DAT`) is a complete 442-mesh stage archive that no mode or portal reaches

`DATA2/S18.DAT` (177,488 bytes, identical on all three discs) is a six-section stage archive containing 442 meshes that matches no other stage file. Stage 18 has an entry in `gCdFileTable`, four spawn pairs in `gStageSpawnPositions[18]`, and a wind entry `{ 4096, 0 }` in `gStageWindTable[18]`, but `gStageCallbacks[18]` holds three null pointers and `gStageAmbientSoundFile[18]` is `SOUND_AMBIENT_FILE_NONE` (`-1`). Stage 18 does not appear in any stage-select table (`gVsStageSelectTable`, `gPovStageSelectTable`, `gPracticeStageSelectTable`, `gLinkStageSelectTable`), `gDemoStageOrder`, `gStoryBoutStageIds`, any of the 27 portals in `gStageTransitions`, Slash mode, or the opening demo, so no reachable path loads it.

Proof: `src/cd.c` (`gCdFileTable`), `src/stage_tables.c` (`gStageTransitions`, `gStageSpawnPositions`), `src/stage_cull.c` (`gStageCallbacks`), `src/sound.c` (`gStageAmbientSoundFile`), `src/combat_state.c` (`gStoryBoutStageIds`, `gStageWindTable`), `src/menu_state.c` (`gVsStageSelectTable`, `gPovStageSelectTable`, `gPracticeStageSelectTable`, `gLinkStageSelectTable`, `gDemoStageOrder`), disc file `DATA2/S18.DAT`.

### Four individual Slash-enemy sound archives in `gCdFileTable` are never loaded

Sound category indices 64 to 67 of `gCdFileTable` point to four archives identical across all three discs: `SE2/NIN1.` (74,432 bytes), `SE2/BAKA.` (77,072 bytes), `SE2/JYOTYU.` (74,080 bytes), and `SE2/NIN2.` (74,032 bytes; 299,616 bytes combined), corresponding to `SOUND_FILE_FIGHTER_BASE` (45) plus Slash enemy IDs 19 to 22. `LoadCombatSoundBanks` clamps both fighters' sound IDs to `0` whenever `id >= SOUND_FIGHTER_BANK_COUNT` (18, covering indices 45 to 62), and `LoadSlashModeSoundBanks` loads the combined Slash archive `SE2/CHANBARA.` (index 146, 207,736 bytes) via `SOUND_LOAD_SLASH_MODE` instead of individual enemy banks, so none of the four archives is ever loaded.

Proof: `src/sound.c` (`SOUND_FILE_FIGHTER_BASE`, `RequestSoundBankLoad`), `src/combat_state.c` (`LoadCombatSoundBanks`, `LoadSlashModeSoundBanks`), disc files `SE2/NIN1.`, `SE2/BAKA.`, `SE2/JYOTYU.`, and `SE2/NIN2.`.

### Boss texture archive `DATA4/D18.DAT` is skipped by `LoadFighterBaseTextures`

`LoadFighterBaseTextures` loads base fighter texture archives `DATA4/D06.DAT` through `DATA4/D23.DAT` by switching on `characterId` (`0..17`). The `switch (characterId)` lists 17 cases and omits `CHAR_ARCHIVE_FINAL_BOSS` (12), so `DATA4/D18.DAT` (19,320 bytes, byte-identical to `DATA4/D12.DAT`, `CHAR_ARCHIVE_KANNUKI_ALT`) is never loaded on any build.

Proof: `src/fighter_texture.c` (`LoadFighterBaseTextures`, `switch (characterId)`), disc files `DATA4/D12.DAT` and `DATA4/D18.DAT`.

### `gCdFileNames` embeds 137 disc paths that no routine reads

`src/asset_load.c` compiles a table of 137 file-path strings (`gCdFileNames`, 136 in Japan, omitting `DATA3\O20.dat`) covering every data-category slot (`DATA0\M00.DAT` through `DATA7\V08.DAT`). Every file read in the game resolves by numeric handle through `gCdFileTable` (`src/cd.c`) without touching the ISO 9660 file system, and no routine reads `gCdFileNames`.

Proof: `src/asset_load.c` (`gCdFileNames`), `src/cd.c` (`gCdFileTable`, `ResolveCdFileHandle`).

### Main-menu entries 9 to 11 and `gAiMoveCycleMode` cannot be selected

`gMainMenuEntries` defines 12 entries: entries 9 and 10 are two-player first-person VS and two-player Practice, and entry 11 is a Story mode that sets `gAiMoveCycleMode = 1` (causing the computer opponent to mirror the player's stance and step through its stance's attack list in order). `UpdateAndDrawMainMenu` wraps the menu cursor after 8 entries (9 in Japan when an analog pad is attached), so entries 9 to 11 are never selectable and `gAiMoveCycleMode` stays `0`.

Proof: `src/menu_state.c` (`gMainMenuEntries`, `UpdateMainMenuState`), `src/main_menu_ui.c` (`UpdateAndDrawMainMenu`), `src/fighter_ai.c` (`UpdateFighterAiInput`, `gAiMoveCycleMode`).

### Stage 7 is launched under Dozaemon Moat's name plate in first-person VS and Link modes

The stage-select screen draws fixed name plates from `gStageSelectLayouts` (`src/select_ui.c`) while `UpdateCharacterSelect` (`src/menu_state.c`) launches the stage ID from the active mode's table. In VS mode, row 5 shows Dozaemon Moat's plate and launches `STAGE_ID_DOZAEMON_MOAT` (19). First-person VS uses the same layout but places stage `7` in row 5 of `gPovStageSelectTable`, and Link mode shows Dozaemon Moat's plate in row 4 while placing stage `7` in row 4 of `gLinkStageSelectTable`, so Dozaemon Moat cannot be selected in either mode.

Proof: `src/select_ui.c` (`gStageSelectLayouts`), `src/menu_state.c` (`gVsStageSelectTable`, `gPovStageSelectTable`, `gLinkStageSelectTable`, `UpdateCharacterSelect`).

### Stage 2 to Stage 3 is a one-way portal

Of the 27 stage transitions in `gStageTransitions`, 26 form 13 bidirectional pairs. The single unpaired entry leads from stage 2 to stage 3 with no return transition from stage 3 to stage 2, so a bout that crosses there cannot return by the same route.

Proof: `src/stage_tables.c` (`gStageTransitions`).

### Stage 9 sets up fifteen prop nodes that are never queued for drawing

`InitStage9PropsAndCloth` initialises 15 scene nodes in `gStagePropNodes` for stage 9 alongside its three cloth strips. Stage 9's per-frame callback, `UpdateStage9ClothStrips`, updates and submits only the cloth strips and never queues those 15 prop nodes.

Proof: `src/stage_callbacks.c` (`InitStage9PropsAndCloth`, `UpdateStage9ClothStrips`), `src/stage_cull.c` (`gStageCallbacks[9]`).

### Two standalone AI input scripts and the tenth Slash attack script are never read

`gAiSlashAttackScripts` holds 10 fixed attack scripts for Slash mode, indexed by `gSlashLevelIndex`, but `SelectAiAttackScript` uses the table only for levels 0 to 8 and scores the stance's attack list on level 9, leaving entry 9 unused. Two standalone scripts in `src/fighter_ai.c` (`gAiScriptLeapForwardQuick` and `gAiScriptSingleTriangle`) are referenced by no table and no routine.

Proof: `src/fighter_ai.c` (`gAiSlashAttackScripts`, `SelectAiAttackScript`, `gAiScriptLeapForwardQuick`, `gAiScriptSingleTriangle`).

### Two compiled 3D effect meshes, one opening overlay card, and one memory-card message sprite are never drawn

In `src/effect.c`, `gEffectTypeTable[14]` includes `gEffect4Mesh` (index 4, `EFFECT_UNUSED_4`) and `gEffect7Mesh` (index 7, `EFFECT_UNUSED_7`), neither of which is ever passed to `SpawnEffect`. In `src/opening_overlay.c`, card 14 of `gOpeningOverlayDescs[17]` appears in none of the cue groups of `gOpeningOverlayGroups`. In `src/options_ui.c`, message 15 (`gOptionsMsgCheckingCardSprites`, whose texture in `DATA3/O04.DAT` reads "Checking memory card, Just one moment." outside Japan) is mapped from no status in `gMemoryCardStatusToMsg` and is never displayed.

Proof: `include/effect_slot.h` (`EFFECT_UNUSED_4`, `EFFECT_UNUSED_7`), `src/effect.c` (`gEffect4Mesh`, `gEffect7Mesh`, `gEffectTypeTable`, `SpawnEffect`), `src/opening_overlay.c` (`gOpeningOverlayDescs`, `gOpeningOverlayGroups`), `src/options_ui.c` (`gOptionsMsgCheckingCardSprites`, `gMemoryCardStatusToMsg`), disc file `DATA3/O04.DAT`.

### An unread 64-byte stage table in `src/select_scene.c` disagrees with the active stage-select tables

`gStageSelectStagesByMode[8][8]` (64 bytes in `src/select_scene.c`) lists stage IDs per `SelectGameMode` (`-1` for modes without stage select), but no routine reads it. Where the active tables in `src/menu_state.c` put `STAGE_ID_YAGURA_POINT` (17) or `STAGE_ID_WARRIORS_DWELLING` (21) in column 0, `gStageSelectStagesByMode` lists stage `8`.

Proof: `src/select_scene.c` (`gStageSelectStagesByMode`), `src/menu_state.c` (`gVsStageSelectTable`, `gPovStageSelectTable`, `gLinkStageSelectTable`).

### Unread table rows sit in the resource cache, visibility masks, approach cutscene, and ground path

Several resident tables hold rows no code reaches: `gResourceGroupIds` defines 7 character-preload groups, of which groups 0 and 1 are never passed to `LoadResourceGroup`; `gStageVisibilityMask` has 65 rows, while `LoadVisibilityMaskPreset` overwrites only rows 0 to 63 from `DATA3/O16.DAT` or `O17.DAT`, leaving row 64 (a copy of row 0 in the executable) unselectable by any 6-bit heading; `gApproachCutsceneStartPart` stores `255` for stages bout 3 cannot end on (which would index far past `gApproachCutsceneCameras` if ever read); and `gUnusedGroundPath` (`src/stage_callbacks.c`, 184 bytes of 15 3D coordinates) is read by nothing.

Proof: `src/resource_cache.c` (`gResourceGroupIds`, `LoadResourceGroup`), `src/stage_cull.c` (`gStageVisibilityMask`, `LoadVisibilityMaskPreset`), `src/story_state.c` (`gApproachCutsceneStartPart`, `gApproachCutsceneCameras`), `src/stage_callbacks.c` (`gUnusedGroundPath`).

### The naginata's fourth edge point is read from the rapier's offset table

`gWeaponEdgePointCounts` gives `WEAPON_ID_NAGINATA` (4) four control points, whereas `gNaginataEdgeOffsets` holds only three entries. `UpdateFighterHitGeometry` reads the fourth point from the memory immediately following `gNaginataEdgeOffsets`, which is the first entry of `gRapierEdgeOffsets`. As read from the code, the naginata's clash, hit, and trail tests read only points 0 to 2, so the fourth point has no effect in play.

Proof: `src/fighter.c` (`gWeaponEdgePointCounts`, `gNaginataEdgeOffsets`, `gRapierEdgeOffsets`), `src/fighter_hit.c` (`UpdateFighterHitGeometry`).

---

## Rare and conditional behaviour

### A save file stores only one 32-bit word, replicated 32 times with a 17-of-32 majority vote

Everything Bushido Blade saves—8 option bits, `gKatzeUnlocked` (bit 8), and the 12-bit `gUnlockedMoviesMask` (bits 16 to 27)—fits into a single 32-bit word built by `PackSaveFlags`. `EncodeSavePayload` writes 32 random key bytes (`rand()`) followed by 32 copies of that word, rotating copy `i` right by `(key[i] + i) & 31` bits and inverting its bits whenever `(key[i] & i) != 0`. On load, `DecodeSavePayload` reverses each copy and accepts the first value matched by at least 17 of the 32 copies (`count >= 16` after the first occurrence), tolerating up to 15 corrupted copies before reporting `MEMORY_CARD_STATUS_CORRUPT_DATA`.

Proof: `src/memcard.c` (`PackSaveFlags`, `UnpackSaveFlags`, `EncodeSavePayload`, `DecodeSavePayload`).

### Katze's Slash unlock and the true ending enforce exact hidden conditions

`TryUnlockKatze` unlocks Katze (`gKatzeUnlocked = 1`) after clearing Slash mode only when all three hold: `gContinueCount == 0`, `gOptionDifficulty != 0` (hard), and `gOptionWallBump == 0` (wall bump off). In Story mode, `IsTrueEndingEligible` grants bout 9 and the true ending after bout 8 only when `gBushidoHonorIntact` is intact, `gStoryNoDamageFlag != 0` (player one never struck by a blade, gunshot, or sub-weapon), `gContinueCount == 0`, and `gReachedStage9Flag != 0` (stage 9 entered during bouts 0 to 3). The Dishonourable ending (`InitBadEndingSlideState`) picks one of five slides uniformly via `rand() % BAD_ENDING_SLIDE_COUNT` (`rand() % 5`).

Proof: `src/story_state.c` (`TryUnlockKatze`, `IsTrueEndingEligible`, `InitBadEndingSlideState`), `src/combat_hit.c` (`ApplyFighterHitImpact`, `gStoryNoDamageFlag = 0`), `src/subweapon.c` (`CheckSubWeaponFighterHits`, `gStoryNoDamageFlag = 0`), `src/combat_state.c` (`InitCombatState`, `gReachedStage9Flag`).

### Stage ambient sounds use integer-truncated per-frame odds, and Yahiro Road runs its own daytime state machine

For every `STAGE_AMBIENT_RANDOM` and `STAGE_AMBIENT_RANDOM_PAIR` entry after `minDelaySec * 30` frames, `UpdateAmbientSounds` computes `triggerThreshold = 0x1000 / (chanceDivisor * 30)` (or `0x1000 / 0xffff == 0` when `chanceDivisor == 0`) and fires when `(rand() & 0xfff) < triggerThreshold`, reducing the right channel by `rand() & 0x3f` out of `0x100`. On Yahiro Road (`STAGE_ID_YAHIRO_ROAD`, stage 24) in daytime, a separate state machine triggers sound slot 2 when `(rand() & 0xff) == 0` (1 in 256 per frame), then after `YAHIRO_ROAD_AMBIENT_MIN_FIRST_FRAMES` switches to sound slot 3 when `(rand() & 0xfff) < 27` (27 in 4,096 per frame, ~0.66%) or when `YAHIRO_ROAD_AMBIENT_MAX_FIRST_FRAMES` elapses.

Proof: `src/combat_state.c` (`UpdateAmbientSounds`, `gStageAmbientSoundLists`).

### A crippled left arm leaves its upper half hittable and makes the right upper arm unhittable

Once a fighter's left arm is crippled (`leftArmIntact == 0`), the blade, gunshot, and sub-weapon hit tests skip hit spheres 8 to 11 (`CRIPPLED_LEFT_ARM_SKIP_FIRST` = 8 through 11). In the 22-sphere body layout (`enum FighterHitSphereId`), the left arm is spheres 6 to 9 (which is the range `ApplyFighterHitImpact` and `MarkFighterInjuredRegion` treat as the left arm), while spheres 10 and 11 are the right upper arm. As read from the code, after the left arm is crippled its upper half (spheres 6 and 7) can still be struck, whereas the right upper arm (spheres 10 and 11) cannot be hit.

Proof: `include/collision_scratch.h` (`CRIPPLED_LEFT_ARM_SKIP_FIRST`), `include/fighter_hit_sphere.h` (`enum FighterHitSphereId`), `src/collision.c` (`TestTriangleVsBodySpheres`), `src/combat_hit.c` (`FireGunshotRayAndCheckHit`, `ApplyFighterHitImpact`, `MarkFighterInjuredRegion`), `src/subweapon.c` (`CheckSubWeaponFighterHits`).

### Equal `clashPower` parries on a body strike and breaks guard on a blade clash

When a guarding fighter is struck by a blade in `ApplyFighterHitImpact`, the guard parries if the defender's `clashPower` is greater than or equal to (`>=`) the attacker's. When two weapons clash in `ResolveBladeClashReaction`, a guarding fighter parries only if its `clashPower` is strictly greater (`>`). As read from the code, at equal `clashPower` the same guard parries in the first path and breaks in the second.

Proof: `src/combat_hit.c` (`ApplyFighterHitImpact`, `ResolveBladeClashReaction`).

### Slash and first-person VS opponents lose their mode-scaled `clashPower` on frame one

`InitFighterForScene` writes a scaled `clashPower` for Slash opponents (`CalcSlashAiClashPower`, by level) and for the first-person VS opponent (`CalcPovAiClashPowerScale`, by player one's wins). At the start of every frame, `UpdateFighterFrame` overwrites `fighter->clashPower` from `fighter->baseClashPower`, so as read from the code both opponents fight with their class and weapon's unmodified table value. The animation-speed scaling for the same two modes writes to `baseAnimSpeed` and persists.

Proof: `src/fighter.c` (`InitFighterForScene`, `CalcSlashAiClashPower`, `CalcPovAiClashPowerScale`, `UpdateFighterFrame`).

### Hit-sphere squared radii scale linearly with fighter stature rather than quadratically

`ScaleFighterHitSpheres` multiplies each sphere's `outerRadius` and `vitalRadius`, along with their precomputed squares `outerRadiusSq` and `vitalRadiusSq`, by the same stature factor `s` from `gCharHeightScales` (Q12, where 4,096 is 1.0), yielding `r² * s` instead of `(r * s)²`. Kannuki's IDs have stature `4218` (~1.03, so `sqrt(4218/4096) / (4218/4096)` tests ~1.5% smaller than their scaled radius), Mikado's `3891` (~2.6% larger) and Tatsumi's `3932` (~2.1% larger) within the scaled radius's bounding test; every other fighter ID uses `4096`.

Proof: `src/fighter_hit.c` (`ScaleFighterHitSpheres`), `src/fighter_skeleton.c` (`gCharHeightScales`).

### A flat triangle's fallback frame loads `edgeB` through a halfword GTE macro

When `BuildTriangleFrame` finds a triangle's normal too short and falls back to rotating both edges by the fallback frame, `edgeA` is loaded into the GTE as three 32-bit words, whereas `edgeB` is loaded through `gte_ldv0`, which reads three 16-bit halfwords. As read from the code, the GTE rotates `(edgeB.x & 0xFFFF, edgeB.x >> 16, edgeB.y & 0xFFFF)` instead of `(edgeB.x, edgeB.y, edgeB.z)`.

Proof: `src/geometry.c` (`BuildTriangleFrame`).

### The computer's flanking check probes the same side twice

Before choosing `AI_STATE_FLANK_UP` or `AI_STATE_FLANK_DOWN`, `EnterAiFlankDirection` computes two probe points to test the floor on either side of the fighter, but adds the same `(sideX, sideZ)` offset to both `left` and `right` instead of subtracting it for one. The first test picks `AI_STATE_FLANK_UP` if that probe's floor lies more than one floor band above the fighter's; the second test checks the identical point and can never decide, so otherwise `rand() & 1` chooses randomly without ever inspecting the opposite side.

Proof: `src/fighter_ai.c` (`EnterAiFlankDirection`).

### Katze's idle coin-toss chain rolls `AI_STATE_SIDESTEP_UP` twice and never sidesteps down

When Katze has ammunition and fails its 60% attack roll in `RunAiIdleDecision`, it chains four `(rand() & 1) == 0` tests in sequence: `AI_STATE_BACKFLIP_RETREAT` (1/2), `AI_STATE_SIDESTEP_UP` (1/4), `AI_STATE_SIDESTEP_UP` a second time (1/8), and `AI_STATE_SPRINT_RETREAT` (1/16), doing nothing on the remaining 1/16. As read from the code, Katze sidesteps up 3 times in 8 on a failed attack roll and never enters `AI_STATE_SIDESTEP_DOWN`.

Proof: `src/fighter_ai.c` (`RunAiIdleDecision`, Katze `FIGHTER_CLASS_KATZE` branch).

### A ring-out credits player one regardless of which fighter fell

In `UpdateCombatState`, when player two's `pitFallTimer` leads player one's by more than `RING_OUT_P2_LEAD_FRAMES` (150) or player one's leads player two's by more than `RING_OUT_P1_LEAD_FRAMES` (210), both branches set `gMatchWinnerSide = 0`, advance the Story bout, and enter the victory scene. As read from the code, player one falling off stage 4 or stage 10 wins the bout (on stage 9, player one's fall is caught on its first frame by the stage-9 fall handler before any lead accumulates).

Proof: `src/combat_state.c` (`UpdateCombatState`, `pitFallTimer` lead checks).

### Bosses' wound textures follow a playable character class's defeat bit

`TransitionAfterMatchOrReplay` records a beaten opponent by setting bit `gFighters[1].characterClass` in `gStoryDefeatedMask`, whereas `InitCombatState` enables all wound textures on the next opponent when bit `gSelectedFighter[1] % PLAYABLE_CHARACTER_COUNT` (`id % 6`) is set (outside a challenger bout, Kannuki's bout 9 enables them unconditionally). Playable fighter IDs 0 to 11 have `characterClass == id % 6`, so both agree; bosses (IDs 12 to 17, classes 6 to 11) have bits 6 to 11 set and never read, so as read from the code their wound textures depend on whether the playable class matching `id % 6` was beaten.

Proof: `src/replay.c` (`TransitionAfterMatchOrReplay`, `gStoryDefeatedMask`), `src/combat_state.c` (`InitCombatState`, `gStoryDefeatedMask` test).

### Striking a defeated fighter marks every wound region on player one

When a fighter whose `action` is `FIGHTER_ACTION_DEFEATED` (21) is struck in `ApplyFighterHitImpact`, the code sets every injury region in `gFighterInjuredRegions[0]` (player one's array), regardless of which fighter was hit. Because Story mode copies `gFighterInjuredRegions[0]` into player one's wound textures at the start of subsequent bouts, as read from the code hitting a defeated opponent scars player one across every region for the rest of the run.

Proof: `src/combat_hit.c` (`ApplyFighterHitImpact`, `FIGHTER_ACTION_DEFEATED` branch).

### The prologue's fades write one entry past each card into the bad-ending slide

`UpdateStoryPrologue` clears and brightens each of the prologue's three picture cards through one entry beyond the card's sprite list. Past card 0 and card 1 sit the first entries of cards 1 and 2 (cleared again before display); past card 2 sits the first entry of the dishonourable ending's first slide (`gBadEndingSlideRects[0]` in Japan, `gBadEndingSlide0Sprites[0]` outside Japan). As read from the code, cancelling the prologue during the first 32 frames of card 2's fade leaves that strip dim until a later prologue fade finishes at `128`.

Proof: `src/story_slideshow.c` (`UpdateStoryPrologue`, `gBadEndingSlideRects`, `gBadEndingSlide0Sprites`).

### The prologue's picture grid starts from an uninitialized row offset

`BuildTiledPolyFt4Grid` cuts a picture into rows of `64 - (pageY & 63)` lines, where `pageY` is the previous row's offset inside its texture page. On the first row it reads `pageY` before any assignment initializes it, so the first row's height depends on an uninitialized stack local; every row is clamped to the remaining height, so the picture stays covered while the horizontal cut positions vary.

Proof: `src/story_slideshow.c` (`BuildTiledPolyFt4Grid`).

### The character-select buffer checks compare word pointer differences against byte limits

`InitCharacterSelectScene` checks whether loaded assets exceed `0xAF000` bytes in the main buffer or `0x32000` bytes in the auxiliary buffer, calling `TriggerDebugTrap()` if exceeded. Because both subtractions take the difference of two `int *` pointers (measuring in 4-byte words rather than bytes), a load would have to run four times past either limit (`0x2BC000` or `0xC8000` bytes) before the trap fired.

Proof: `src/select_scene.c` (`InitCharacterSelectScene`, buffer-limit checks).

### A point light illuminates a fighter at any distance once any axis offset reaches `0x7000`

`UpdatePointLight` disables flash lighting on a fighter more than 5,000 units from the light, but executes the distance calculation only when `abs(dx) < 0x7000 && abs(dy) < 0x7000 && abs(dz) < 0x7000` (28,672 units). When any coordinate offset is `0x7000` or more, the distance rejection is bypassed and the fighter receives the flash light at any range.

Proof: `src/point_light.c` (`UpdatePointLight`).

### Executioner's Cove samples the ocean edge and velocity from the next wave column

`SampleOceanWaveEdgeX` and `SampleOceanWaveVelocity` interpolate between adjacent columns of the wave grid with weight `(z % 3200) & 1` out of `4096`. Because that expression evaluates only to `0` or `1`, both routines return the next column's edge or speed (to within 1 part in 4,096) regardless of where inside the 3,200-unit column the point lies.

Proof: `src/ocean.c` (`SampleOceanWaveEdgeX`, `SampleOceanWaveVelocity`).

### `DrawSubdivPolyFt3Dynamic` reuses an uninitialized register `t3` in its off-screen screen test

`DrawSubdivPolyFt3Dynamic` (`src/render.s`) tests whether a triangle lies wholly above or left of the screen by ANDing its three screen coordinates with `t3` without loading `t3` first. `t3` is written only when a triangle is subdivided (holding the third vertex's or last midpoint's UV word) and otherwise retains the caller's value; the extra bitmask term can only suppress a rejection, sending some off-screen triangles to subdivision and the GPU anyway.

Proof: `src/render.s` (`DrawSubdivPolyFt3Dynamic`).

### `DrawPolyGt4DoubleLit` inverts only two and a half of a back-facing quad's four normals

When `DrawPolyGt4DoubleLit` (`src/render.s`) flips normals to light the back side of a Gouraud-shaded quad, it negates all three components of corners 0 and 1 and the X and Y components of corner 2, leaving corner 2's Z and all three components of corner 3 unnegated. As read from the code, the fourth corner is lit by its front-face normal and the third by a hybrid normal.

Proof: `src/render.s` (`DrawPolyGt4DoubleLit`).

### The first-person mannequin overlay mixes coordinates from three different joints

`UpdatePovOverlayModel` adds to the camera orbit offset `x` from joint 0's world matrix, `y` from joint 1's, and `z` from joint 2's, rather than taking `(x, y, z)` from a single joint.

Proof: `src/pov_overlay.c` (`UpdatePovOverlayModel`).

### `TintVramClut`'s luminance mode scales green and blue from the scaled red channel

In `CLUT_TINT_MODE_LUMINANCE`, `TintVramClut` computes luminance grey, multiplies it by `rScale`, and then computes green and blue by scaling that already-scaled red value rather than the original grey. For the opening demo's palettes (`BuildOpeningTintedCluts`: `0xE00` for red and green, `0xB00` for blue, out of `0x1000`), green comes out at `(0xE00/0x1000)² ≈ 0.77` of grey instead of `0.88`, and blue at `(0xE00/0x1000)*(0xB00/0x1000) ≈ 0.60` instead of `0.69`.

Proof: `src/tim.c` (`TintVramClut`, `CLUT_TINT_MODE_LUMINANCE` branch), `src/opening_demo.c` (`BuildOpeningTintedCluts`).

### A cut prop's delayed ambient sound receives a stack pointer as its variant

Billboard kind 15 (`BILLBOARD_EFFECT_DELAYED_AMBIENT`) runs through `gBillboardEffectHandlers` like the drawn billboard kinds, so `UpdateDelayedAmbientTrigger` receives the address of the caller's `packets` local (`&packets`, a stack address in `0x801Fxxxx`) in its `int variant` parameter. As a signed 32-bit integer a stack address is negative, so `variant > 0 ? variant + 5 : 0` always evaluates to `0`, and `TriggerPositionalAmbientSound` always plays sound `0x79`.

Proof: `src/billboard.c` (`UpdateDelayedAmbientTrigger`, `gBillboardEffectHandlers`, `UpdateAndDrawBillboardEffects`), `src/combat_state.c` (`TriggerPositionalAmbientSound`).

### A thrown item's bounce and moat splash pick the fighter by camera side

`EmitSubWeaponBounceEffect` raises `COMBAT_EVENT_SUB_WEAPON_BOUNCE` (and `UpdateAndDrawSubWeapons` raises `COMBAT_EVENT_WATER_SPLASH` for moat splashes) with fighter index `GetSoundListenerSide(position)` (`0` or `1` depending on which side of the camera the item lies on). As read from the code, the bounce plays that side's fighter's weapon sound `0x39` and records the event on that fighter's replay slot rather than the thrower's.

Proof: `src/subweapon.c` (`EmitSubWeaponBounceEffect`, `UpdateAndDrawSubWeapons`), `src/combat_event.c` (`GetSoundListenerSide`, `EmitCombatEvent`).

### Cut stage props are omitted from the instant replay

`CheckWeaponStagePropHit` invokes `DispatchCombatEventEffect` directly for `COMBAT_EVENT_PROP_CUT` rather than calling `EmitCombatEvent`—the only combat event dispatched without passing through the replay recorder. As read from the code, the instant replay plays neither the prop-cut sound nor its delayed ambient cue.

Proof: `src/stage_prop_hit.c` (`CheckWeaponStagePropHit`), `src/combat_event.c` (`EmitCombatEvent`, `DispatchCombatEventEffect`).

### Snow restarts on only six of the eleven snowy stages

`InitStageSnowIfEnabled` restarts the snow particle field on stages 0, 1, 3, 4, 5, and 10 when a bout, replay, victory scene, or story state resets. Five more stages (2, 6, 7, 8, and 22) also run snow callbacks in `gStageCallbacks`, and on those five the snow particles continue from their previous positions.

Proof: `src/stage_callbacks.c` (`InitStageSnowIfEnabled`), `src/stage_cull.c` (`gStageCallbacks`).

### Katze's muzzle flash uses average blending for player one and additive blending for player two

`gGunMuzzleFlashP1Mesh` specifies semi-transparent texture page `0x000C` (blend mode 0, average of background and sprite), whereas `gGunMuzzleFlashP2Mesh` specifies `0x002F` (blend mode 1, additive). As read from the code, the same gunshot flashes brighter when fired by player two than by player one.

Proof: `src/effect.c` (`gGunMuzzleFlashP1Mesh`, `gGunMuzzleFlashP2Mesh`).

---

## What differs between the three builds

### Three sequential builds share one source tree, with 200 routine differences from Japan to North America and 8 to Europe

As documented in [history.md](history.md), Japan (`SLPM_860.20`, banner `VER.1.1`, 1,006 own routines) came first, North America (`SCUS_941.80`, `VER.1.1c`, 984 own routines) localised it for English, and Europe (`SCES_008.99`, `VER.1.1d`, 984 own routines) adapted North America for PAL. Comparing routines after relocation, North America and Japan share 784 identical routines and 183 differing routines, with 17 routines present only outside Japan (mostly `src/subtitle.c`, `src/splash.c`, and HUD overlays) and 39 present only in Japan (12 analog-menu/pad routines plus 27 uncalled routines in `src/model_table.c`, `src/render.s`, `src/rot_matrix.c`, `src/sprite2d.c`, and `src/main.c`). Europe differs from North America in exactly 8 routines (`main`, `InitSystem`, `PlayBootSplashScreen`, `UpdateOpeningBannerPackets`, `SetupMovieWorkspace`, `StartMoviePlayback`, `RunMoviePlaybackLoop`, and `FetchNextMovieFrame`).

Proof: [history.md](history.md), `config/bushidoblade/reports/SCUS_941.80.json`, `config/bushidoblade/reports/SLPM_860.20.json`, `config/bushidoblade/reports/SCES_008.99.json`, `src/main.c`, `src/splash.c`, `src/opening_overlay.c`, `src/movie.c`.

### The Analog Controller menu's art (`DATA3/O13.DAT`) was translated into English and shipped outside Japan even though the menu was compiled out

On the North American and European discs, `DATA3/O13.DAT` (40,908 bytes) remains in `gCdFileTable`, with four of its 19 TIM images (`TIM[3]`, `TIM[7]`, `TIM[8]`, and `TIM[11]`, 5,241 differing bytes) replaced relative to the Japanese disc; decoding those four 4-bit TIM bitmaps shows English text (`"Vibration Function"`, `"Set Joystick Neutral Position"`, `"Remove hands from joystick / and press the \"select\" button."`, and `"Controller"`). Outside `VERSION_JP`, however, `src/analog_menu.c` compiles out its entire implementation (leaving `InitAnalogControllerMenuState` and `UpdateAnalogControllerMenuState` empty), `UpdateAndDrawMainMenu` caps the menu at 8 entries instead of 9, and `PollControllers` reports analog pads as digital (type 4), so neither analog steering (`UpdateFighterRunSteering`) nor rumble (`UpdateControllerRumble`, which always sends 0 outside Japan, though `gOptionRumbleEnabled` still occupies bit 7 of the save word) nor `DATA3/O13.DAT` is reachable in North America or Europe.

Proof: `src/analog_menu.c` (`#ifdef VERSION_JP`, `LoadAnalogControllerAssets`), `src/story_state.c` (`InitAnalogControllerMenuState`, `UpdateAnalogControllerMenuState`), `src/main_menu_ui.c` (`UpdateAndDrawMainMenu`), `src/pad.c` (`PollControllers`, `UpdateControllerRumble`), `src/fighter.c` (`UpdateFighterRunSteering`), `src/memcard.c` (`PackSaveFlags`, `UnpackSaveFlags`), disc file `DATA3/O13.DAT`.

### Blood changes from semi-transparent amber in Japan to opaque red outside Japan

In `src/effect.c`, `gDefaultBloodColor` (copied into `gBloodParticleColor` by `SpawnBloodSpray`) is `{ 0xA0, 0x80, 0x00, GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS }` (semi-transparent amber) under `VERSION_JP` and `{ 0xE0, 0x00, 0x00, GPU_CODE_POLY_FT4 }` (opaque red) in North America and Europe. In all three builds the `NamedColor` record carries an unread 12-byte ASCII tag `"BLOOD COLOR"`.

Proof: `src/effect.c` (`NamedColor`, `gDefaultBloodColor`, `SpawnBloodSpray`).

### Attract-mode demo battles are silent and unlabelled in Japan

In `InitDemoBattle` and `UpdateDemoBattle`, `LoadCombatSoundBanks()`, `FlushQueuedSounds()`, and `DrawDemoOverlayText()` are compiled only under `#ifndef VERSION_JP`. As read from the code, the Japanese attract-mode demo battle plays with no sound effects and no `"DEMO"` overlay; similarly, `ShowPauseOverlay` and `DrawReplayOverlayText` exist only outside Japan.

Proof: `src/menu_state.c` (`InitDemoBattle`, `UpdateDemoBattle`), `src/combat_state.c` (`UpdateCombatState`, `ShowPauseOverlay`), `src/replay.c` (`UpdateBattleReplay`, `DrawReplayOverlayText`), `src/font_hud.c` (`DrawDemoOverlayText`, `DrawReplayOverlayText`).

### Combat, input, and menu rules differ in six small ways between Japan and the later builds

Between Japan and North America/Europe, six gameplay and interface rules change: (1) a `Fighter` holds up to 11 thrown sub-weapons in Japan (792-byte record) versus 16 outside Japan (808-byte record); (2) `UpdateFighterFrame` adds `&& gBladeLockState == BLADE_LOCK_NONE` outside Japan so ledge climbing cannot start during a blade lock; (3) `PAUSE_QUIT_BUTTONS` requires `Start + Select` in Japan versus all six buttons `Start + Select + L1 + L2 + R1 + R2` outside Japan; (4) `ResetGameOptions` defaults `gOptionDaytime` to night (`0`) in Japan and day (`1`) outside Japan; (5) two-player VS requires both pads connected outside Japan, stage select accepts vertical input in addition to horizontal, and Confirm/Cancel swap between Circle and Cross (though `UpdateAutoSaveState` waits for Cross on every build); and (6) `main` calls `SetSp(0x801fff00)` and `SetMem(2)` only in Japan.

Proof: `include/fighter.h` (`Fighter`), `include/subweapon.h`, `include/pad.h` (`MENU_CONFIRM`, `MENU_CANCEL`), `src/fighter.c` (`UpdateFighterFrame`, `gBladeLockState` guard), `src/combat_state.c` (`PAUSE_QUIT_BUTTONS`), `src/main.c` (`ResetGameOptions`, `main`), `src/menu_state.c` (`UpdateMainMenuState`, `UpdateCharacterSelect`), `src/select_scene.c` (`UpdateCharacterSelectMenu`), `src/memcard.c` (`UpdateAutoSaveState`).

### The special light slot is black outside Japan

`gSpecialLightPreset` (`src/stage_light.c`) is declared as seven 32-bit `int` values `{ 0, 0, 0, 1683, 3384, 725, 3650 }`. `SetLightSlotFromHsvPreset` reads it as `int`s in Japan and as 16-bit `short`s outside Japan, where the first six halfwords are all `0`, leaving `LIGHT_SLOT_SPECIAL` (used on Dozaemon Moat, stage 19) with zero direct light and zero ambient light.

Proof: `include/scene_light.h` (`SceneLightValue`), `src/stage_light.c` (`gSpecialLightPreset`, `InitSpecialLightSlot`, `SetLightSlotFromHsvPreset`).

### The Japanese options screen dims unselected values in the red channel alone

In `DrawOptionsMenu` under `VERSION_JP`, setting the shade of each row's value sprite writes the `r` channel three times (`sprite->r = ...`) instead of `r`, `g`, and `b`, leaving green and blue at the texture's full intensity so unselected rows shift cyan rather than darkening uniformly. North America and Europe write all three channels.

Proof: `src/options_ui.c` (`DrawOptionsMenu`, `#ifdef VERSION_JP` branch).

### `DrawFormattedText` marks memory before the glyph buffer in Japan and drops the Slash kill-count tint outside Japan

In Japan, `DrawFormattedText` sets bit 31 of the entry before `gFontGlyphWritePtr` even when no glyph was appended; `DrawSlashModeHud`'s formatting-only call `"#X#Y#w#h#S#C"` therefore ORs bit 31 into the word 12 bytes before `gFontGlyphBuffer` and redraws the previous call's glyphs. Outside Japan, `DrawFormattedText` closes the list only when non-empty, but resets `gFontTintR/G/B` to `128` at the start of every call, so `DrawSlashModeHud`'s second call prints the kill count untinted while `"KILLS"` and `"LEVEL"` keep the `0x808000` tint. Outside Japan, `DrawSlashModeHud` also decrements `kills--` before display and leaves `gSlashKillsLabelSprite` unused in `.data`.

Proof: `src/font_hud.c` (`DrawFormattedText`, `DrawSlashModeHud`, `gSlashKillsLabelSprite`, `gFontGlyphBuffer`).

### `DATA2/S20.DAT` is the only stage archive that differs between Japan and North America/Europe

Across all 26 stage archives (`DATA2/S00.DAT`–`S25.DAT`), 25 are byte-identical across all three discs. Only `DATA2/S20.DAT` (91,292 bytes, the Slash-mode arena) differs between Japan and North America/Europe, and only in a single 4-bit 512×48 texture (`TIM[42]` in section 5, 6,622 differing bytes), where decoding the bitmap shows Japanese calligraphy replaced by English Slash-clear score text (`"100 / Your score is / / 100"`).

Proof: `src/font_hud.c` (`gHudSlashVictoryBannerSprites`, `gHudSlashClearBannerSprites`), disc file `DATA2/S20.DAT` across all three builds.

### Save files outside Japan still write all three animated icon frames but display only the first

`BuildMemoryCardHeader` copies all three 16×16 frames of `gMemoryCardIconFrames[3][64]` into every save file on all three builds. Japan sets `MEMORY_CARD_ICON_FORMAT` to `0x13` (three-frame animated icon), whereas North America and Europe set it to `0x11` (single static frame), so frames 2 and 3 are written into every non-Japanese save file yet never shown by the PlayStation BIOS.

Proof: `src/memcard.c` (`MEMORY_CARD_ICON_FORMAT`, `gMemoryCardIconFrames`, `BuildMemoryCardHeader`).

### Japan's 24-bit movie player passes an uninitialized result pointer to `CdSync` and `CdReady`

In `RunMoviePlaybackLoop` under `VERSION_JP`, `u_char *cdResult` is never initialized before being passed to `CdSync(1, cdResult)` and `CdReady(1, cdResult)` (whereas North America and Europe pass a local `u_char cdResult[8]`). As read from the code, whenever a CD command completes during movie playback in Japan, its 8 result bytes are written to whatever address that uninitialized stack pointer holds (skipped only when it happens to be `0`).

Proof: `src/movie.c` (`RunMoviePlaybackLoop`, `#ifdef VERSION_JP` branch).

### Outside Japan, `BuildMovieTileQuads` uses an uninitialized `frameRectIndex`, and `WaitMovieFrameReady` never exits on time-out

In the North American and European 16-bit tiled movie player, `BuildMovieTileQuads` computes every tile's texture page from `dec->frameRects[frameRectIndex]` using an uninitialized local `frameRectIndex`. In the same builds, `WaitMovieFrameReady` counts down `0x800000` passes waiting for `dec->frameReady`, and on reaching zero flips `dec->frameIndex` without setting `dec->frameReady = 1` (unlike Japan, which sets it), leaving the loop waiting with `timeout` negative.

Proof: `src/movie.c` (`BuildMovieTileQuads`, `WaitMovieFrameReady`).

### Ending 5 mutes its CD audio 30 frames early outside Japan

In `FetchNextMovieFrame` under `#ifndef VERSION_JP`, when playing movie 5 (`STR16/E_H.STR`, Red Shadow's normal ending) and `gMovieHeader->frameCount >= gMovieMaxFrames[5] - 30` (frame 2,030 of 2,060), the player calls `SetCdAudioMix(0, 0, 0, 0)`. Japan's `FetchNextMovieFrame` contains no early mute for movie 5 (`STR/E_HOT.STR`).

Proof: `src/movie.c` (`FetchNextMovieFrame`, `gMovieMaxFrames`).

### Europe adds a movie frame-jump abort, retimes disc subtitles and the opening for 50 Hz, and drops three USA credits strips

In Europe alone, `RunMoviePlaybackLoop` aborts movie playback with `MOVIE_PLAYBACK_ERROR` if `gMovieFrameNumber` moves backward or jumps forward by more than 20 frames (`gMovieLastFrameNumber > gMovieFrameNumber || gMovieLastFrameNumber + 20 < gMovieFrameNumber`), and `SetupMovieWorkspace` writes an unread pointer `gMovieSectorData = (u_long *)(gMovieHeader + 1)`. For 50 Hz PAL timing, Europe shortens the opening demo (`DATA3/O01.DAT` section 4 camera track shrinks from 5,544 to 3,400 bytes; delays drop from 166 to 102 and 300 to 210 frames) and scales timestamps by 5/6 in the disc subtitle banks (`DATA3/O05.DAT`, `O14.DAT`, `O20.DAT`), while the executable's hardcoded combat taunt subtitles in `src/subtitle.c` keep NTSC frame numbers unchanged. Europe also omits three strips from `gCreditsEntries` that appear only in North America, even though `DATA3/O18.DAT` (229,772 bytes) is byte-identical on both discs.

Proof: `src/movie.c` (`SetupMovieWorkspace`, `FetchNextMovieFrame`, `RunMoviePlaybackLoop`, `gMovieSectorData`), `src/opening_demo.c` (`OPENING_DEMO_CARD_DELAY`, `OPENING_DEMO_END_DELAY`), `src/opening_overlay.c` (`OPENING_CARD_OVERLAY_HOLD`, `TITLE_FADE_OUT`), `src/subtitle.c` (`gCombatSubtitleScripts`), `src/credits.c` (`gCreditsEntries`), disc files `DATA3/O01.DAT`, `DATA3/O05.DAT`, `DATA3/O14.DAT`, `DATA3/O18.DAT`, and `DATA3/O20.DAT`.

---

## How the game was built

### Mixed Sony Runtime Library releases, an unreleased `LIBCOMB 3.7a`, and an interim `LIBAPI`

As established in [building.md](building.md), the game's own C code was compiled with CC1PSX 2.7.2 at `-O2 -G8` and assembled with the older PsyQ 3.3 `ASPSX 2.21` (producing `ori` for 0..65535 constants and four-instruction indexed global accesses) and linked by `PSYLINK 2.27` with `/n` (to exceed its 256-module default for the 103 game objects and 196 library modules). Its 12 libraries mix four Sony releases: Japanese Runtime Library 3.6 (`LIBC`, `LIBCARD`, `LIBCD`, `LIBETC`, `LIBGTE`, `LIBPRESS`, `LIBSN`, `LIBSPU`, compiled with CC1PSX 2.6.0 at `-O2 -G0` and `ASPSX 2.34`), North American 3.6 `LIBSND` (`MIDIREAD`), Runtime Library 3.7 `LIBGPU` (CC1PSX 2.7.2 + `ASPSX 2.56`), an unshipped `LIBCOMB 3.7a` between 3.7 and 4.0 (one extra 32-byte routine `reset_graph` and no ID stamp), and a hybrid `LIBAPI` combining 3.6 `COUNTER`, 3.7 `SEND`/`PATCH`/`SENDPAD`, an interim `PAD`, and a `C114` whose ID stamp ends in `36 00` (between 3.6's `36 10` and 3.7's `37 00`).

Proof: [building.md](building.md), `config/bushidoblade/tools.tsv`, `config/bushidoblade/flags.tsv`, `config/bushidoblade/libraries.tsv`, `src/lib/comb/comb.c`, `src/lib/api/pad.c`, `src/lib/api/c114.s`.

### Unread build timestamps in `src/movie.c` disagree with the discs' ISO 9660 dates, and Europe keeps USA's volume date

Every executable embeds an unread pointer `gBuildTimestamp` (`src/movie.c`) recording when the binary was built: Japan `Thu Feb 13 17:40:18 1997`, North America `Thu May 29 16:48:29 1997`, and Europe `Thu Jun 19 02:22:48 1997`. Comparing those against each disc's ISO 9660 Primary Volume Descriptor (sector 16) and file directory records: Japan's PVD is `1997-02-13 12:00:00` JST while `SLPM_860.20`'s directory entry is dated ten days earlier (`1997-02-03 20:20:04` JST); North America's PVD is `1997-05-09 07:33:45` JST while `SCUS_941.80` is dated `1997-05-22 23:51:24` JST (a week before its embedded May 29 stamp); and Europe keeps North America's exact `1997-05-09 07:33:45` JST PVD while `SCES_008.99`'s directory entry is `1997-06-19 02:22:58` JST—ten seconds after its embedded build stamp.

Proof: `src/movie.c` (`gBuildTimestamp`), ISO 9660 Primary Volume Descriptors (sector 16) and directory timestamps of the three retail disc images.

### Twenty-four game globals carry the authors' own names proven by `PSYLINK 2.27`'s common hash order

Because `PSYLINK 2.27` places uninitialized globals ("commons") by walking a 512-bucket hash table (`(len + sum(bytes)) mod 512`), the executable's layout preserves a trace of the original identifiers. Checked against the 63 library commons whose names are known, 21 game commons (`result`, `imgbuf0`, `imgbuf1`, `vlcbuf0`, `vlcbuf1`, `fd`, `ev0`–`ev3`, `card_sts_tmp`, `dec`, `ev10`–`ev13`, `card_sts`, `Ring_Buff`, `event_time_out`, `slot_dir`, `seq_table`) are proven as the authors' own spellings descended from Sony's kit samples, and 3 more (`db`, `ot0`, `ot1`) are probable.

Proof: [names.md](names.md), `config/bushidoblade/commons.tsv`, `src/cd.c`, `src/memcard_io.c`, `src/movie.c`, `src/sound.c`, `src/globals.c`.

### The character-select host writes 72 bytes past `gFighterJointWorldMatrices` into Dozaemon Moat's billboard nodes

`UpdateCharacterSelectMenu` poses the select-screen host every frame by calling `UpdateAndEnqueueFighterPose` with `side = 2`. That routine writes 18 joint-matrix pointers (72 bytes) into `gFighterJointWorldMatrices[2]`, past the two-row array `gFighterJointWorldMatrices[2]`, overwriting the first entry of `gMoatBillboardNodes` (`src/stage_moat.c`), which follows it in all three builds. Because the select screen resets the model arena and Dozaemon Moat reinitialises its billboard nodes when loaded, the overwrite has no visible effect.

Proof: `src/select_scene.c` (`UpdateCharacterSelectMenu`), `src/fighter_pose.c` (`UpdateAndEnqueueFighterPose`, `gFighterJointWorldMatrices`), `src/stage_moat.c` (`gMoatBillboardNodes`).

### Harmless overwrites, duplicate uploads, and unread `"MdlTable"` markers run during normal play

Four benign artifacts run without visible effect across all three builds: `BuildMemoryCardHeader` copies each 64-byte title into the 64-byte `header->title` with `strcpy`, landing the NUL terminator in `header->unused[0]` just before a loop zeroes `unused`; `LoadCharacterArchive` uploads player one's texture group twice (player two's once); `ResetCharacterSelectState` parents both weapon nodes three times in a row before `UpdateCharacterSelectMenu` overrides them, and `UpdateStoryCutscene` / `UpdateOpeningDemoFighterScene` pass an unset 2-word root position (overwritten immediately afterward) where `UpdateAndEnqueueFighterPose` reads 3 words; and `LoadArenaGroup` writes the 8-byte ASCII marker `"MdlTable"` after every loaded arena group, which nothing reads.

Proof: `src/memcard.c` (`BuildMemoryCardHeader`), `src/model_arena.c` (`LoadCharacterArchive`, `LoadArenaGroup`), `src/select_scene.c` (`ResetCharacterSelectState`), `src/story_cutscene.c` (`UpdateStoryCutscene`), `src/opening_demo.c` (`UpdateOpeningDemoFighterScene`), `src/fighter_pose.c` (`UpdateAndEnqueueFighterPose`).

### Unread counters and buffers remain actively written at run time

Multiple runtime structures are maintained every frame or bout and read by nothing else: `gFighterInputHistory` (8 frames of held buttons per player, updated in `BuildFighterInput`), `gStoryFallCount` (incremented on ring-outs in `UpdateCombatState`), `gLinkRxButtonBitCount` and `gLinkTxButtonBitCount` (set-bit totals in `ExchangeLinkInputPacket`), `gStageLitColor` (computed in `ApplyLightSlotToGte`), `gAmbientTriggerCycle` (cycled 0 to 2 in `UpdateDelayedAmbientTrigger`), `gArenaUsedBytes` (`LoadArenaGroup`), `gCameraStageCell` (`src/stage_cull.c`), and `gSightlineBitmask` (zeroed every frame by `ClearSightlineGrid`). Alongside them sit completely unread static allocations, notably `gModelArenaUnused[1336]` (`src/model_arena.c`), `gPointLightUnusedBss[444]` (`src/point_light.c`), and `gStringFormat` (`"%s"`, `src/main.c`).

Proof: `src/combat_state.c` (`gFighterInputHistory`, `BuildFighterInput`, `gStoryFallCount`, `UpdateCombatState`), `src/link.c` (`gLinkRxButtonBitCount`, `gLinkTxButtonBitCount`, `ExchangeLinkInputPacket`), `src/stage_cull.c` (`gStageLitColor`, `ApplyLightSlotToGte`, `gCameraStageCell`), `src/billboard.c` (`gAmbientTriggerCycle`, `UpdateDelayedAmbientTrigger`), `src/model_arena.c` (`gArenaUsedBytes`, `gModelArenaUnused`), `src/sightline.c` (`gSightlineBitmask`, `ClearSightlineGrid`), `src/point_light.c` (`gPointLightUnusedBss`), `src/main.c` (`gStringFormat`, `gStringFormatPtr`).
