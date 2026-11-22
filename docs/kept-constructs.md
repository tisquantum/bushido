# Constructs the original's bytes need

The original's code has constructs that plain C would not write. Each one below is in the source
because the original's bytes need it: written in its plain form, it compiles to different code. The
first list says what each is and why; the second keeps the few where the cause, or whether the
authors wrote it so, is not established.

53 constructs in 43 routines: 46 whose cause is established and 7 whose cause is not.

## What the original has, and why

The original's frame keeps room for a local that no instruction touches: the source declares the local and does not use it. (20)

- `src/cloth.c`, `SolveFighterClothLinks`: `unsigned char unused[8];`
  An 8-byte local, declared and unused.
- `src/cloth.c`, `StepFighterClothModel`: `unsigned char unused[8];`
  An 8-byte local, declared and unused.
- `src/cloth.c`, `StepStageClothModel`: `unsigned char unused[8];`
  An 8-byte local, declared and unused.
- `src/effect.c`, `UpdateStage4LightGlow`: `unsigned char unused[16];`
  A 16-byte local, declared and unused.
- `src/fighter.c`, `CanClimbRecordedLedge`: `unsigned char unused[16];`
  A 16-byte local, declared and unused.
- `src/main_menu_ui.c`, `UpdateAndDrawMainMenu`: `DRAWENV env;`; `RECT window;`
  RECT window and DRAWENV env, declared and unused.
- `src/model_table.c`, `DrawBillboardQuadNode`: `SVECTOR unused;`
  SVECTOR unused, declared and unused.
- `src/movie.c`, `CleanupMoviePlayback`: `RECT screen;`
  RECT screen, used by the USA and Europe ClearImage, is declared in Japan too and unused there.
- `src/movie.c`, `RunMoviePlaybackLoop`: `unsigned char unused[8];`
  An 8-byte local, declared and unused.
- `src/opening_demo.c`, `OpeningDemoScriptNoOp`: `unsigned char unused[8];`
  An 8-byte local, declared and unused.
- `src/ribbon_trail.c`, `InterpolateRibbonMidpoint`: `unsigned char unused[24];`
  A 24-byte local, declared and unused.
- `src/sound.c`, `RequestSoundBankLoad`: `unsigned char unused[16];`
  A 16-byte local, declared and unused.
- `src/splash.c`, `PlayBootSplashScreen`: `unsigned char unused[24];`
  A 24-byte local, declared and unused.
- `src/story_cutscene.c`, `ApplyCutsceneFighterPose`: `FighterPose pose;`
  FighterPose pose, declared and unused.
- `src/story_cutscene.c`, `InitStoryCutscene`: `unsigned char unused[48];`
  A 48-byte local, declared and unused.
- `src/story_slideshow.c`, `InitStorySlideshow`: `unsigned char unused[8];`
  An 8-byte local, declared and unused.
- `src/tim.c`, `LoadTimGroupWithOffset`: `TextureInfo info;`
  TextureInfo info, declared and unused.
- `src/tim.c`, `LoadTimGroupWithSeparateOffsets`: `TextureInfo info;`
  TextureInfo info, declared and unused.
- `src/weather.c`, `UpdateAndDrawSnow`: `unsigned char unused[384];`
  A 384-byte local, declared and unused.

Both arms of a test are the same statements: the compiler keeps one copy of them and the test before it. (2)

- `src/camera.c`, `UpdateBattleCamera`: `if (mode == 0) { minPitch = 0x80; requiredPitch = 0x80; } else { minPitch = 0x80; requiredPitch = 0x80; }`
  Both arms of the test on mode set minPitch and requiredPitch to 0x80 in the original, and its reads and writes of gCameraState are reproduced only by the three static inline helpers ApproachCameraTarget, UpdateCameraDistance and ApproachCameraAngles; that the authors wrote helpers is not established.
- `src/pad.c`, `PollControllers`: `if (analogMode) input.type[port] = PAD_TYPE_DIGITAL; else input.type[port] = PAD_TYPE_DIGITAL;`
  In the USA and Europe builds the original tests analogMode where both arms set input.type[port] to PAD_TYPE_DIGITAL, and the zero it gave analogMode is still loaded into a register that nothing reads; what told the arms apart is not known.

One source serves every build: a construct that compiles to nothing in one build does work in another. (2)

- `src/main_menu_ui.c`, `UpdateAndDrawMainMenu`: `if (!analog && gMainMenuIndex == MAIN_MENU_ITEM_ANALOG) {`
- `src/model_arena.c`, `LoadStageArchive`: `RegisterModelTable(modelTable, modelSlot);`

A loop that runs once: its body alone compiles otherwise, so the original's source had the loop around it. (2)

- `src/model_table.c`, `SetMeshTexturedPrimsSemiTrans`: `do {`
- `src/stage_moat.c`, `ConfigureSemiTransQuadPrims`: `do {`

A jump the original takes where every loop and conditional tried compiles differently. (13)

- `src/combat_event.c`, `SolveBallisticJumpVelocity`: `goto output;`
- `src/combat_hit.c`, `UpdateBladeLockInput`: `goto sidestep_left;`
  The goto sidestep_left is the original's own branch: it tests PADLup on the held buttons it already has and jumps straight into the sidestep, while the path that falls through loads the held buttons again and tests PADLup a second time.
- `src/lib/c2/sprintf.c`, `sprintf`: `goto do_dec;`; `goto do_hex;`; `goto done;`; `goto next_mod;`; `goto reswitch;`
- `src/movie.c`, `UploadMovieFrameSlices`: `goto next_slice;`
- `src/point_light.c`, `UpdatePointLight`: `goto disable;`
  A target within the delta range but farther than POINT_LIGHT_MAX_RANGE_SQ jumps to the disable step (goto disable) in place of an empty then-arm before the lighting, which compiles alike on all three builds; a goto to a shared exit is ordinary C and carries no line.
- `src/pose_stream.c`, `DecodePoseFrame`: `goto decode;`
  The cache-resume tests are nested (if the newer slot is not the previous frame: the older slot, else seek from the group's start), which compiles alike on all three builds; one goto remains, from the seek past the resume block into the decode loop.
- `src/sound.c`, `RequestSoundBankLoad`: `goto foundPair;`
- `src/stage_prop_hit.c`, `CheckWeaponStagePropHit`: `goto cutProp;`; `goto done;`

A statement or declaration that leaves no instruction of its own but decides which registers hold the routine's values or the order of its instructions. (2)

- `src/fighter.c`, `UpdateFighterFrame`: `fighter->jumpVel.x = fighter->jumpVel.x - fighter->position.vx + fighter->position.vx;`; `fighter->jumpVel.z = fighter->jumpVel.z - fighter->position.vz + fighter->position.vz;`
  After SolveBallisticJumpVelocity the original writes jumpVel's x and z back unchanged, and only a statement that subtracts and adds back the fighter's position gives its frame the size the original has; what the authors wrote there is not known.

A test or loop the compiler removes whose presence still shapes the code around it. (3)

- `src/combat_state.c`, `InitCombatState`: `if (gBattleCameraLocked != 0 && stageChanged) gBattleCameraLocked = 0;`
- `src/lib/c2/sprintf.c`, `sprintf`: `if (spec.f_long) {`
  Each conversion takes its argument as a long or as an int by spec's f_long flag, as portable C does: the two arms compile to the same instructions because long and int are one size, but the test is removed only after registers are allocated, so without it registers change elsewhere in the routine.
- `src/lib/snd/tempo.c`, `_SsSndTempo`: `if (score->tempo < score->tempo_target) { score->tempo = score->tempo_target; }`
  Both clamps of score->tempo to score->tempo_target are live code: the original makes the first clamp's comparison and then jumps into the second clamp's branch and store, which the two share, so the first clamp has no store of its own.

A declaration or form the original's code needs, found by a reader who tried the plainer ones. (2)

- `src/fighter_cloth.c`, at file scope: `short gClothWeightsDemoRedShadow[88] = {`
  The empty weight list every character without cloth shares is the tail of gClothWeightsDemoRedShadow, because as its own object the list is 4 bytes, which the compiler places in small data, moving the data after it.
- `src/lib/cd/sys.c`, at file scope: `extern u_char CD_status;`
  The CD library's interrupt module defines CD_status as an int, and this module's object reads it as a byte, so the original declares CD_status here as a byte.

## Kept with the cause not established

Each construct below is in the source because the original's bytes need it: every plainer form tried compiles differently, and why the compiler needs this one is not established. It stays as written until its cause is found. (7)

- `src/camera.c`, `ApproachCameraAngles` (inlined): `static inline void ApproachCameraAngles(SVECTOR *targetAngles, int divisor)`
  Written in place, the address of gCameraState stays in a register across UpdateBattleCamera and the routine compiles differently; that the authors wrote helpers is not established.
- `src/camera.c`, `ApproachCameraTarget` (inlined): `static inline void ApproachCameraTarget(VECTOR *focus)`
  Written in place, the address of gCameraState stays in a register across UpdateBattleCamera and the routine compiles differently; that the authors wrote helpers is not established.
- `src/camera.c`, `UpdateCameraDistance` (inlined): `static inline void UpdateCameraDistance(int targetDist, int mode, int bladeLockState)`
  Written in place, the address of gCameraState stays in a register across UpdateBattleCamera and the routine compiles differently; that the authors wrote helpers is not established.
- `src/lib/cd/bios.c`, `set_intr` (inlined): `static inline void set_intr(int i, u_char intr)`
  Written in place, the two stores of a status chosen by a test into Intr keep Intr's address in a register, where the original stores to Intr by its address directly; only the static inline helper set_intr gives that, and that the authors wrote a helper is not established.
- `src/lib/comb/comb.c`, `_comb_control`: `CombWaitFunc *cb = &CombWaitCallback;`
  The wait callback is read through a local pointer; read directly, the routine compiles differently.
- `src/lib/spu/s_srmp.c`, `CopyBytes` (inlined): `static __inline__ void CopyBytes(const void *src, void *dst, long n)`
  The original copies each reverb preset byte by byte in three places; the loop written in place compiles differently.
- `src/model_arena.c`, `LoadCharacterArchive`: `if (byteSize < 0) {`
  The division of byteSize by 4 is written out with its rounding; written as / 4, the size and the quotient share one register.
