# Constructs the original's bytes need

The original's code has constructs that plain C would not write. Each one below is in the source
because the original's bytes need it: written in its plain form, it compiles to different code. The
first list says what each is and why; the second keeps the few where the cause, or whether the
authors wrote it so, is not established.

49 constructs in 40 routines: 40 whose cause is established and 9 whose cause is not.

## What the original has, and why

The original's frame keeps room for a local that no instruction touches: the source declares the local and does not use it. (21)

- `src/cloth.c`, `SolveFighterClothLinks`: `unsigned char unused[8];`
  An 8-byte local is declared and unused to hold the original's 40-byte frame (without it the frame shrinks to 32 bytes and 8 stack offsets shift).
- `src/cloth.c`, `StepFighterClothModel`: `unsigned char unused[8];`
  An 8-byte local is declared and unused to hold the original's 120-byte frame (without it the frame shrinks to 112 bytes and 22 stack offsets shift).
- `src/cloth.c`, `StepStageClothModel`: `unsigned char unused[8];`
  An 8-byte local is declared and unused to hold the original's 112-byte frame (without it the frame shrinks to 104 bytes and 20 stack offsets shift).
- `src/effect.c`, `UpdateStage4LightGlow`: `unsigned char unused[16];`
  A 16-byte local, declared and unused.
- `src/fighter.c`, `CanClimbRecordedLedge`: `unsigned char unused[16];`
  A 16-byte aggregate local (unsigned char unused[16]) is declared and unused; without it, GCC 2.7.2 at -O2 shrinks the frame from 72 to 56 bytes and shifts all 20 stack offsets across SCUS, SLPM and SCES.
- `src/main_menu_ui.c`, `UpdateAndDrawMainMenu`: `DRAWENV env;`; `RECT window;`
  RECT window and DRAWENV env are declared and unused to hold the original's 152-byte frame, and the guard on analog and MAIN_MENU_ITEM_ANALOG compiles to nothing outside Japan where analog is always false.
- `src/model_table.c`, `DrawBillboardQuadNode`: `SVECTOR unused;`
  SVECTOR unused is declared and unused to hold Japan's 88-byte frame (without it the frame shrinks to 80 bytes and 20 stack offsets shift).
- `src/movie.c`, `CleanupMoviePlayback`: `RECT screen;`
  RECT screen is used by ClearImage in the USA and Europe builds and declared unused in Japan, where the original keeps a 32-byte frame with 8 untouched stack bytes; guarding the declaration under #ifndef VERSION_JP shrinks Japan's frame to 24 bytes and differs in 4 of 21 rows because GCC 2.7.2 at -O2 still allocates a stack slot for an unused aggregate local.
- `src/movie.c`, `RunMoviePlaybackLoop`: `unsigned char unused[8];`
  An 8-byte local (unsigned char unused[8]) is declared and unused in Japan's RunMoviePlaybackLoop; removing it shrinks the original 64-byte frame to 56 bytes and shifts stack offsets across SLPM because GCC 2.7.2 at -O2 reserves a stack slot for an unused aggregate local (the other 8 untouched bytes belong to the compiler's folded division by 2 in DecDCTout).
- `src/opening_demo.c`, `OpeningDemoScriptNoOp`: `unsigned char unused[8];`
  An 8-byte local, declared and unused.
- `src/ribbon_trail.c`, `InterpolateRibbonMidpoint`: `unsigned char unused[24];`
  A 24-byte local, declared and unused.
- `src/sound.c`, `RequestSoundBankLoad`: `unsigned char unused[16];`
  A 16-byte local is declared and unused to hold the original's 40-byte frame, and the weapon-pair lookup jumps to its load handler with goto foundPair because no structured form can place that block between SOUND_LOAD_LINK_LOBBY and SOUND_LOAD_SLASH_MODE while keeping the original's unfilled dialogue-branch delay slot.
- `src/splash.c`, `PlayBootSplashScreen`: `unsigned char unused[24];`
  A 24-byte local, declared and unused.
- `src/story_cutscene.c`, `ApplyCutsceneFighterPose`: `FighterPose pose;`
  FighterPose pose, a 108-byte local, declared and unused.
- `src/story_cutscene.c`, `InitStoryCutscene`: `unsigned char unused[48];`
  A 48-byte local, declared and unused.
- `src/story_slideshow.c`, `InitStorySlideshow`: `unsigned char unused[8];`
  An 8-byte local, declared and unused.
- `src/tim.c`, `LoadTimGroup`: `TextureInfo info;`
  TextureInfo info (32 bytes) is declared and unused to hold the original's 72-byte frame (without it the frame shrinks to 40 bytes and 10 stack offsets shift).
- `src/tim.c`, `LoadTimGroupWithOffset`: `TextureInfo info;`
  TextureInfo info (32 bytes) is declared and unused to hold the original's 80-byte frame (without it the frame shrinks to 48 bytes and 14 stack offsets shift).
- `src/tim.c`, `LoadTimGroupWithSeparateOffsets`: `TextureInfo info;`
  TextureInfo info (32 bytes) is declared and unused to hold the original's 88-byte frame (without it the frame shrinks to 56 bytes and 20 stack offsets shift).
- `src/weather.c`, `UpdateAndDrawSnow`: `unsigned char unused[384];`
  A 384-byte local, declared and unused.

Both arms of a test are the same statements: the compiler keeps one copy of them and the test before it. (1)

- `src/pad.c`, `PollControllers`: `if (analogMode) input.type[port] = PAD_TYPE_DIGITAL; else input.type[port] = PAD_TYPE_DIGITAL;`
  In the USA and Europe builds the original tests analogMode where both arms set input.type[port] to PAD_TYPE_DIGITAL, and the zero it gave analogMode is still loaded into a register that nothing reads; what told the arms apart is not known.

One source serves every build: a construct that compiles to nothing in one build does work in another. (1)

- `src/main_menu_ui.c`, `UpdateAndDrawMainMenu`: `if (!analog && gMainMenuIndex == MAIN_MENU_ITEM_ANALOG) {`
  RECT window and DRAWENV env are declared and unused to hold the original's 152-byte frame, and the guard on analog and MAIN_MENU_ITEM_ANALOG compiles to nothing outside Japan where analog is always false.

A loop that runs once: its body alone compiles otherwise, so the original's source had the loop around it. (2)

- `src/model_table.c`, `SetMeshTexturedPrimsSemiTrans`: `do {`
  A single-pass loop wraps the mesh walk because only a loop statement emits the loop-boundary marker that both sets the prologue schedule and raises the register-allocation weight of mesh above the constant minus one.
- `src/stage_moat.c`, `ConfigureSemiTransQuadPrims`: `do {`
  A single-pass loop wraps the mesh walk because only a loop statement emits the loop-boundary marker that both sets the prologue schedule and raises the register-allocation weight of the primitive-size table address above meshIndex.

A jump the original takes where every loop and conditional tried compiles differently. (13)

- `src/combat_event.c`, `SolveBallisticJumpVelocity`: `goto output;`
  The non-negative discriminant branch jumps over the trailing speed clamp with goto output so that three copies of the clamp stay separate through global register allocation, lowering the allocation weight of dx and dz below gravity before post-reload cross-jumping collapses them into the single clamp in the binary.
- `src/combat_hit.c`, `UpdateBladeLockInput`: `goto sidestep_left;`
  The goto sidestep_left is the original's own branch: it tests PADLup on the held buttons it already has and jumps straight into the sidestep, while the path that falls through loads the held buttons again and tests PADLup a second time.
- `src/lib/c2/sprintf.c`, `sprintf`: `goto do_dec;`; `goto do_hex;`; `goto done;`; `goto next_mod;` (2 places); `goto reswitch;`
  The six gotos (next_mod twice, reswitch, do_dec, do_hex, and done) are this formatter's jumps within and out of the conversion switch, and every structured form tried compiles differently.
- `src/movie.c`, `UploadMovieFrameSlices`: `goto next_slice;`
  The slice upload loop repeats with goto next_slice because the loop optimizer hoists the image buffer index shift out of any structured for, while or do-while loop, whereas the original computes that shift on every iteration.
- `src/point_light.c`, `UpdatePointLight`: `goto disable;`
  A target within the delta range but farther than POINT_LIGHT_MAX_RANGE_SQ jumps to the disable step with goto disable, where every structured form tried compiles differently on all three builds.
- `src/pose_stream.c`, `DecodePoseFrame`: `goto decode;`
  The keyframe seek path jumps over the cache-resume block into the delta loop with goto decode because no structured conditional places the seek block before a single shared resume block without either repeating a cache-slot test or duplicating copy loops that post-reload cross-jumping cannot merge.
- `src/sound.c`, `RequestSoundBankLoad`: `goto foundPair;`
  A 16-byte local is declared and unused to hold the original's 40-byte frame, and the weapon-pair lookup jumps to its load handler with goto foundPair because no structured form can place that block between SOUND_LOAD_LINK_LOBBY and SOUND_LOAD_SLASH_MODE while keeping the original's unfilled dialogue-branch delay slot.
- `src/stage_prop_hit.c`, `CheckWeaponStagePropHit`: `goto done;`
  The naginata miss path exits with blocked = 1 and goto done so that its jump targets the trailing return label rather than the epilogue, preventing post-reload cross-jumping from merging it with the sledgehammer early return.

A test or loop the compiler removes whose presence still shapes the code around it. (2)

- `src/combat_state.c`, `InitCombatState`: `if (gBattleCameraLocked != 0 && stageChanged) gBattleCameraLocked = 0;`
  The guard on gBattleCameraLocked and stageChanged can never pass and emits no instructions of its own, yet removing it (including storing the fighter midpoints directly into gCameraState.target or five other reorderings) differs by 11 to 15 rows (44 B) on SCUS, SLPM and SCES because its branches end the first CSE pass extended block before the heading address is formed; what the original tested here is not known.
- `src/lib/snd/tempo.c`, `_SsSndTempo`: `if (score->tempo < score->tempo_target) { score->tempo = score->tempo_target; }`
  Both clamps of score->tempo to score->tempo_target are live code: the second jump pass cross-jumps the first clamp's branch and store into the second clamp, and delay-slot scheduling deletes the redundant reload of score->tempo_target and moves the first clamp's live comparison into the cross-jump's delay slot after the next line marker.

## Kept with the cause not established

Each construct below is in the source because the original's bytes need it: every plainer form tried compiles differently, and why the compiler needs this one is not established. It stays as written until its cause is found. (9)

- `src/camera.c`, `ApproachCameraAngles` (inlined): `static inline void ApproachCameraAngles(SVECTOR *targetAngles, int divisor)`
  Written in place, the record's base stays in a register; that the authors wrote helpers is not established.
- `src/camera.c`, `ApproachCameraTarget` (inlined): `static inline void ApproachCameraTarget(VECTOR *focus)`
  Written in place, the record's base stays in a register; that the authors wrote helpers is not established.
- `src/camera.c`, `UpdateBattleCamera`: `if (mode == 0) { minPitch = 0x80; requiredPitch = 0x80; } else { minPitch = 0x80; requiredPitch = 0x80; }`
  Both arms of the test on mode set minPitch and requiredPitch to 0x80 because the reload pass inserts a stack load of spilled mode for the branch before post-reload scheduling hoists it and post-reload cross-jumping collapses the identical arms without removing the now-dead load, while the routine's accesses to gCameraState are reproduced only by the three static inline helpers ApproachCameraTarget, UpdateCameraDistance and ApproachCameraAngles whose use by the authors is not established.
- `src/camera.c`, `UpdateCameraDistance` (inlined): `static inline void UpdateCameraDistance(int targetDist, int mode, int bladeLockState)`
  Written in place, the record's base stays in a register; that the authors wrote helpers is not established.
- `src/fighter.c`, `UpdateFighterFrame`: `fighter->jumpVel.x = fighter->jumpVel.x - fighter->position.vx + fighter->position.vx;`; `fighter->jumpVel.z = fighter->jumpVel.z - fighter->position.vz + fighter->position.vz;`
  After SolveBallisticJumpVelocity the original reloads and stores jumpVel.x and jumpVel.z unchanged while keeping two untouched 8-byte compiler spill slots after the spilled input and opponent parameters; the combine pass produces both the unchanged store-back and those two slots when an expression over a memory load cancels back to the original field, though which memory operand the authors wrote is not established from the bytes.
- `src/lib/cd/bios.c`, `set_intr` (inlined): `static inline void set_intr(int i, u_char intr)`
  Direct constant-indexed writes to volatile Intr keep its address in a register across RTL generation, CSE, and combine, whereas the variable index folds in CSE to direct symbolic stores and the u_char parameter keeps CdlDataReady's constant separate in delay-slot filling; whether the authors wrote set_intr is not established.
- `src/lib/comb/comb.c`, `_comb_control`: `CombWaitFunc *cb = &CombWaitCallback;`
  Taking the address of CombWaitCallback into a local pointer keeps its symbol in one pseudo-register across the load and store so combine cannot fold either access back into a two-word symbolic macro and delay-slot filling places the single-word store in the jump's delay slot; whether the authors used a local pointer or another register-indirect form is not established.
- `src/model_arena.c`, `LoadCharacterArchive`: `if (byteSize < 0) {`
  The division of byteSize by 4 is written out with its rounding because no plain signed divide can both emit both buffer stores before the sign branch and keep byteSize separate from the shifted quotient across that branch; whether the authors wrote the shift-and-fixup explicitly is not established.
