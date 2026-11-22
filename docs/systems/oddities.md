# Oddities of the original program

Every routine in this tree compiles or assembles to the bytes of the 1997 executables, so whatever the game
does, the source does too, including the places where the code does something other than what its shape or
its data suggests. Each entry says what the code does, what follows in play, and where it is. Every entry holds
for all three builds (USA `SCUS`, Japan `SLPM`, Europe `SCES`) unless it names a build.

## Combat

**A crippled left arm shields part of the right arm.** Once the left arm is crippled, the blade, gunshot and
sub-weapon tests skip hit spheres 8 to 11 (`CRIPPLED_LEFT_ARM_SKIP_FIRST`, `include/collision_scratch.h`;
`TestTriangleVsBodySpheres` in `src/collision.c`, `FireGunshotRayAndCheckHit` in `src/combat_hit.c`,
`CheckSubWeaponFighterHits` in `src/subweapon.c`). The left arm is spheres 6 to 9 (joints 8 and 9), and that is
the range `ApplyFighterHitImpact` and `MarkFighterInjuredRegion` treat as the left arm; spheres 10 and 11 are the
right upper arm. So after the left arm is crippled, its upper half can still be struck, and the right upper arm
can no longer be hit at all.

**Equal clash power: parry or guard break by the path.** A guarding fighter struck by a blade parries when its
`clashPower` is at least the attacker's (`ApplyFighterHitImpact`, `src/combat_hit.c`); when the two weapons
meet instead, a guarding fighter parries only when its `clashPower` is strictly greater
(`ResolveBladeClashReaction`). At equal power the same guard parries in the first case and breaks in the second.

**Slash and first-person VS opponents never get their clash power.** `InitFighterForScene` (`src/fighter.c`)
writes `clashPower` for a Slash opponent (`CalcSlashAiClashPower`, by level) and for the first-person VS
opponent (`CalcPovAiClashPowerScale`, by player one's wins), but `UpdateFighterFrame` sets `clashPower` from
`baseClashPower` at the start of every frame, so the opponent fights with its class and weapon's table value.
The animation speed scaling of the same two modes is written to `baseAnimSpeed` and does last.

**Hit spheres' squared radii scale linearly.** `ScaleFighterHitSpheres` (`src/fighter_hit.c`) multiplies each
sphere's radius and its precomputed squared radius by the fighter's stature alike, so the squared radius used
in the distance tests is r² × s rather than (r × s)². Stature is read by fighter id (`gCharHeightScales`,
`src/fighter_skeleton.c`): Kannuki's ids (stature 4218, about 1.03) test about 1.5% smaller in radius than their
scaled radius, Mikado's (3891) and Tatsumi's (3932) about 2.6% and 2.1% larger, within the scaled radius's
bounding test. Every other fighter id, the bosses' included, has stature 4096 (1.0).

**The naginata's fourth edge point is the rapier's first.** `gWeaponEdgePointCounts` gives the naginata
(`WEAPON_ID_NAGINATA`) four control points, but `gNaginataEdgeOffsets` holds three (`src/fighter.c`);
`UpdateFighterHitGeometry` (`src/fighter_hit.c`) reads the fourth from the table that follows, the rapier's
first offset. Nothing reads that point for the naginata (its clash, hit and trail tests use points 0 to 2), so it
has no effect.

**A flat triangle's second edge is loaded from the wrong words.** When `BuildTriangleFrame` (`src/geometry.c`)
finds a triangle's normal too short, it rotates the frame's two edges by the fallback frame. `edgeA` goes to the
GTE as three words; `edgeB` goes through `gte_ldv0`, which loads three halfwords, so the GTE rotates `edgeB.x`'s
low and high halves and `edgeB.y`'s low half: for an edge whose x lies within 16 bits, (x, 0 or -1, y) in place of
(x, y, z).

## The computer opponent

**The computer never evades a vertical swing.** `IsWeaponSwingPlaneVertical` (`src/geometry.c`) compares the
swing plane's normal with the window -2896..2896 held in an unsigned value: the lower bound becomes
0xFFFFF4B0, and no value is both below 2896 and above it. The routine always returns 0, so
`RunAiDefendOrEvade` (`src/fighter_ai.c`) always guards and never chooses one of its sidesteps or backsteps
for a vertical swing.

**The computer's flank looks at one side only.** `EnterAiFlankDirection` (`src/fighter_ai.c`) probes the floor
at a point to the left and a point to the right before choosing which way to flank, but builds both points by
adding the same side offset, so the two probes are one point. It flanks up (`AI_STATE_FLANK_UP`) when that
point's floor lies more than a floor band above its own; the second test repeats the first and never decides,
so otherwise the choice is random, and the floor on the other side is never looked at.

**Katze never sidesteps down.** When Katze's attack roll fails, `RunAiIdleDecision` (`src/fighter_ai.c`) tosses
a coin for each of a backflip, a sidestep up, a sidestep up again and a run back, so it sidesteps up three times
in eight. Nothing in the game enters `AI_STATE_SIDESTEP_DOWN` or `AI_STATE_WALK_BACKWARD`.

## Story mode

**A ring-out is player one's win whichever fighter fell.** When one fighter's `pitFallTimer` leads the other's
by more than 150 frames (the opponent's) or more than 210 (player one's), `UpdateCombatState`
(`src/combat_state.c`) sets `gMatchWinnerSide` to 0 in both cases, advances the bout and goes to the victory
scene. Player one falling off stage 4 or 10 therefore wins the bout; on stage 9 player one's fall is
caught on its first frame by the stage-9 restart, before any lead can build. [story.md](story.md) gives the
ring-out rules.

**The approach cutscene's table names a part for stages bout 3 cannot end on.** `gApproachCutsceneStartPart`
(`src/story_state.c`), indexed by the stage bout 3 ended on, gives the part the approach starts from: 0 or 1,
and 255 for the stages bout 3 cannot end on. Read as a part, 255 would take `gApproachCutsceneCameras` far
past its end; no bout reaches it.

**Bosses' wound textures follow a playable class's record.** A won bout sets bit
`gFighters[1].characterClass` of `gStoryDefeatedMask` (`src/replay.c`); the next bout gives the opponent every
wound texture when bit `gSelectedFighter[1] % 6` is set (`src/combat_state.c`; outside a challenger bout,
Kannuki's final bout gives them regardless). Fighter ids 0 to 11 map to classes 0 to 5, so for them the two
agree; the bosses, ids 12 to 17, are classes 6 to 11, so their own bits are set and never read, and whether a
boss appears wounded depends on whether the playable class with bit `id % 6` was beaten.

**Striking a defeated fighter scars player one.** When a fighter in the defeated action (21) is hit,
`ApplyFighterHitImpact` (`src/combat_hit.c`) marks every injury region in `gFighterInjuredRegions[0]`, player
one's, whichever fighter was struck. In Story mode that array gives player one's wound textures at the start of
each following bout, so hitting a defeated opponent leaves player one showing every wound from then on.

**The prologue's fades write one entry past each card.** `UpdateStoryPrologue` (`src/story_slideshow.c`) clears
each of the prologue's three picture cards before its fade-in and brightens it during the fade through one entry
more than the card's list holds. Past card 0 and card 1 lie the first entries of cards 1 and 2, which are
cleared again before they show. Past card 2 lies the first rectangle of the bad ending's first slide
(`gBadEndingSlideRects[0]` in the Japanese build, the first strip of `gBadEndingSlide0Sprites` in the others). A
fade that runs to its end leaves it at 128, its own brightness; skipping the prologue with Cancel during the
first 32 frames of card 2's fade leaves it darker, and that part of the slide is drawn dim until a later
prologue's fade runs to its end.

**The prologue's picture grids start from an unset row offset.** `BuildTiledPolyFt4Grid`
(`src/story_slideshow.c`) cuts a picture into rows of 64 less the previous row's offset in its texture page,
modulo 64, but reads that offset (`pageY`) for the first row before anything sets it, so the first row's height
depends on an unset value. Every row is clamped to the rows left, so the grid always covers the picture; only
where it is cut into rows differs.

**Cutscene and opening poses pass an unset two-word root position.** `UpdateStoryCutscene`
(`src/story_cutscene.c`) and `UpdateOpeningDemoFighterScene` (`src/opening_demo.c`) pass
`UpdateAndEnqueueFighterPose` (`src/fighter_pose.c`) a root rotation and a two-word position they never set; it
copies three position words to the root node, the third from the local that follows the array (`poseAngles`).
Nothing comes of it: `ApplyCutsceneFighterPose` and `ApplyOpeningDemoPose` then overwrite the root's rotation and
translation.

## Menus and states

**The all-modes switch is always on.** `InitSystem` (`src/main.c`) sets `gAllMenuModesEnabled` to 1, and
nothing else writes it except `InitMainMenuState` (`src/menu_state.c`), whose check of L1, R1 and R2 held on both
pads runs only while it is negative. The check never runs, and every main-menu entry whose own flag would lock
it (the link cable among them) is always selectable.

**Main-menu entries 9 to 11 are never selected.** `UpdateAndDrawMainMenu` (`src/main_menu_ui.c`) wraps the
cursor after 8 entries (9 in the Japanese build with an analog pad), so entries 9 to 11 of `gMainMenuEntries`
(first-person VS and Practice for two pads, and a Story whose computer plays its attack lists in turn) are
never chosen, and `gAiMoveCycleMode`, set only by choosing entry 11, stays 0.

**Four states nothing enters.** States 30, 42 and 43 are never assigned to `gGameState`, and 44 only by 43's
handler (`GAME_STATE_WAIT_CANCEL_TO_CREDITS` and the others in `include/game_state.h`; the handlers in
`src/story_state.c`). State 30 waits for Cancel and goes to the credits, 42 waits for Cross and goes to the main
menu, 43 goes straight to 44, and 44 waits for Cancel and goes to the main menu.

**Formatting a card always succeeds.** `StepMemoryCardCommand` (`src/memcard.c`) ignores what
`FormatMemoryCard` returns and sets `MEMORY_CARD_STATUS_FORMAT_OK`; nothing sets
`MEMORY_CARD_STATUS_FORMAT_ERROR`, which the options menu and the auto-save are ready to show
([memory-card.md](memory-card.md)).

**Stage 7 is offered under Dozaemon Moat's name.** The stage list's rows draw name plates fixed by the layout
(`gStageSelectLayouts`, `src/select_ui.c`) and start the stage of the mode's table (`src/menu_state.c`). In VS,
row 5 shows plate 5 and starts stage 19, Dozaemon Moat. First-person VS uses the same layout, but its table
(`gPovStageSelectTable`) has stage 7 in row 5; Link shows plate 5 in row 4, and its table (`gLinkStageSelectTable`)
has stage 7 there too. In both modes the Dozaemon Moat plate starts stage 7, and Dozaemon Moat cannot be chosen
([character-select.md](character-select.md)).

**The select screen's buffer checks measure in words.** `InitCharacterSelectScene` (`src/select_scene.c`)
traps when the data it loaded runs past 0xAF000 bytes in the main buffer or 0x32000 in the auxiliary one, but
measures the distance as the difference of two `int` pointers, in words, so a load would have to run four times
past either limit before the check fired.

**The Japanese options dim a value in red only.** `DrawOptionsMenu` (`src/options_ui.c`) copies each row's value
sprite from its choice texture and then sets the shade of the selected row's value to 0x80 and the others' to
0x40, but writes the red channel three times and leaves green and blue as the texture has them, so the rows not
selected show their values with less red rather than darker. The other builds set all three.

**The select screen's weapon nodes take three parents in a row.** `ResetCharacterSelectState`
(`src/select_scene.c`) sets each weapon node's parent to the host's right hand, then player one's, then player
two's, leaving both on player two's hand. Nothing comes of it: `UpdateCharacterSelectMenu` sets the parent again
before a weapon is shown.

**The select screen's host figure writes past a table.** `UpdateCharacterSelectMenu` (`src/select_scene.c`)
poses the host figure each frame with `UpdateAndEnqueueFighterPose` (`src/fighter_pose.c`) as side 2, and that
routine stores the figure's 18 joint matrix pointers in `gFighterJointWorldMatrices[side]`, which has rows for
sides 0 and 1 only. The 72 bytes land on the first of Dozaemon Moat's billboard nodes (`gMoatBillboardNodes`,
`src/stage_moat.c`), which follows the table in every build. Nothing comes of it: the select screen resets the
model arena, so the moat is loaded afresh after it, and loading it sets the node's draw type, rotation and matrix
again; nothing reads the node's local matrix.

**An empty Japanese text line marks a word before the glyph buffer.** In the Japanese build
`DrawFormattedText` (`src/font_hud.c`) closes its glyph list even when it appended nothing, by setting the
last-group bit of the entry before the write cursor. `DrawSlashModeHud`'s call before the kill count
(`"#X#Y#w#h#S#C"`) only sets the position, size, scale and colour, so it sets bit 31 of the word 12 bytes before
`gFontGlyphBuffer` and draws the glyphs the previous call left in the buffer again. The other builds close the
list only when it holds a glyph.

**Outside Japan the Slash kill count loses its tint.** `DrawSlashModeHud` (`src/font_hud.c`) sets the count's
place and its tint (0x808000) with a call that draws nothing, then prints the count with another call. Outside
Japan `DrawFormattedText` resets the tint to 128 at the start of every call, so the count is drawn untinted while
KILLS and the level take the tint; the Japanese build keeps the tint between calls.

**The save title's terminator lands past it.** `BuildMemoryCardHeader` (`src/memcard.c`) copies the title into
the save header's 64-byte `title` with `strcpy`, and every title is 64 bytes, so its terminating zero is written
to the next byte, `unused[0]`. Nothing comes of it: the next loop zeroes those bytes.

## Controllers

**Analog steering only in Japan.** `UpdateFighterRunSteering` (`src/fighter.c`) steers a running fighter by the
left stick when the pad reports an analog type, but outside Japan `PollControllers` (`src/pad.c`) reports an
analog pad as digital (type 4), so the stick branch runs only in the Japanese build.

**Rumble only in Japan.** `gOptionRumbleEnabled` starts at 1 in the Japanese build and 0 in the others
(`ResetGameOptions`, `src/main.c`); only the Japanese analog controller menu changes it, and the save keeps the
value the build wrote. Outside Japan the motors are always sent 0 (`UpdateControllerRumble`, `src/pad.c`).

## Stages and rendering

**A one-way crossing.** `gStageTransitions` (`src/stage_tables.c`) pairs every crossing with its return except
stage 2 to stage 3, which has none: a bout that crosses there cannot come back the same way
([stages.md](stages.md) lists the crossings).

**The special light is black outside Japan.** `gSpecialLightPreset` (`src/stage_light.c`) is seven ints;
`SetLightSlotFromHsvPreset` reads them as ints in the Japanese build and as shorts in the others, where they
give no direct light and no ambient: what Dozaemon Moat (stage 19) lights with the special slot is unlit there
([lighting.md](lighting.md)).

**Depth bounds that always pass.** `InsertDeferredPrimsIntoOt` (`src/stage_callbacks.c`) tests the ordering
table bucket against 8192, and the billboard and decal drivers test it against 4096, but `CompressDepthToOtz`
(`src/otz.c`) returns at most 4095. (The decal driver's lower bound does reject.)

**A leftover register in the subdivided triangles' screen test.** `DrawSubdivPolyFt3Dynamic` (`src/render.s`)
rejects a triangle wholly above or left of the screen by ANDing its three screen coordinates with `t3`, which
it does not set for the test. `t3` changes only when a triangle is subdivided: it then holds that triangle's
third texture coordinate, or, when it was split further, the last midpoint texture coordinate
`AverageVertexAndUv` computed. Triangles drawn whole leave it, and before the first subdivided triangle it holds
the caller's value. The extra term can only stop a reject, never cause one, so some off-screen triangles are
subdivided and sent anyway.

**A distant fighter can be lit by a flash.** `UpdatePointLight` (`src/point_light.c`) gives no flash light to a
fighter more than 5,000 units from the flash, but measures the distance only while each of the three offsets is
under 0x7000 (28,672); when any of them is 0x7000 or more, the fighter is lit however far it is
([lighting.md](lighting.md)).

**The sea's edge is sampled at the next column.** `SampleOceanWaveEdgeX` (`src/ocean.c`) blends two columns of
the wave grid by a weight of `(z % 3200) & 1`, which is 0 or 1 out of 4096, so it returns the edge of the column
after the one the point is in (to within one unit), wherever in the column the point lies;
`SampleOceanWaveVelocity` blends the edge speeds by the same weight. Executioner's Cove's footstep surface,
shoreline splashes and footprint fading follow that column's edge, not the wave between columns.

**One drawer's off-screen test never fires.** `DrawPolyFt4Unlit` (`src/render.s`) ANDs its first three screen
coordinates with `t3`, which holds the first vertex's index times 4, where the other fourteen quad drawers AND
the fourth coordinate (`t7`). The index has neither the sign bit nor bit 15 set, so neither the above-the-screen
nor the left-of-the-screen reject fires: every unlit textured quad goes on to the back-face test, and those
wholly off screen are sent for the GPU to clip.

**Back-facing Gouraud quads keep part of their normals.** `DrawPolyGt4DoubleLit` (`src/render.s`), which draws a
quad turned away by inverting its normals, inverts the first two corners' normals and the third's X and Y, but
not the third's Z or any of the fourth's: the fourth corner is lit by its front face's normal and the third by
a mix of the two.

**A zero count would move the packet cursor.** The dynamic drawers (`DrawPolyFt3LitDynamic` and the others in
`src/render.s`, `DrawFadingPolyFt3` and `DrawFadingPolyFt4` in `src/cuttable_fade.s`) skip loading the
packet cursor when the face count is 0 but still store their packet register to `gPacketCursor`. Their only
caller, `DispatchMeshPrimitiveGroups`, ends a batch at a count of 0 and never calls a drawer with it, so this
never happens.

**A cloth mesh with no faces would derail its drawing.** `RenderClothSoftbodyNode` (`src/render.s`) passes the
first primitive group's count to its drawer without testing it. A cloth drawer given a count of 0 returns a
record cursor that points before the group, and the next count is read from there. `InitClothPartFromMesh`
(`src/fighter_cloth.c`) accepts a mesh whose first count is 0; whether any character's cloth mesh has none is
not established.

**The ground decals' texture is set for thirteen stages.** `InitStageGroundDecals` (`src/decal_driver.c`)
fills its texture record for stages 0 to 10, 12 and 22 only; on any other it would pass `InitGroundDecalAtlas`
the record unwritten. Nothing comes of it: only the setup routines of stages 0 to 8, 10, 12 and 22 call it
(`gStageCallbacks`, `src/stage_cull.c`).

**The first-person overlay's camera stands on three joints.** `UpdatePovOverlayModel` (`src/pov_overlay.c`)
adds to the orbit offset the x of joint 0's world position, the y of joint 1's and the z of joint 2's, not one
joint's position.

**The opening's grey palettes scale green and blue twice.** `TintVramClut` (`src/tim.c`), in its luminance
mode, scales the grey by the red factor and then computes green and blue from that red value, not from the grey.
The opening's palettes (`BuildOpeningTintedCluts`, `src/opening_demo.c`: 0xE00 for red and green, 0xB00 for blue,
of 0x1000) therefore come out with green at 0.77 of the grey rather than 0.88, and blue at 0.60 rather than 0.69.

## Effects and sound

**A cut prop's delayed sound ignores its variant.** Billboard kind 15 (`BILLBOARD_EFFECT_DELAYED_AMBIENT`) is run
through `gBillboardEffectHandlers` like the drawn kinds, so `UpdateDelayedAmbientTrigger` (`src/billboard.c`)
receives the address of the caller's packet cursor where it expects a variant. A stack address is negative as an
int, so `variant > 0 ? variant + 5 : 0` is always 0; and `TriggerPositionalAmbientSound` (`src/combat_state.c`)
uses its argument only to pick the voice, always playing sound 0x79. The `gAmbientTriggerCycle` counter it
advances is read by nothing ([effects.md](effects.md)).

**A thrown item's bounce plays a weapon sound chosen by the camera.** `EmitSubWeaponBounceEffect`
(`src/subweapon.c`) raises the bounce event for fighter `GetSoundListenerSide(position)`, which is 0 or 1 by the
side of the camera the item lies on, so the bounce plays that fighter's weapon sound 0x39 and is recorded on that
fighter's replay frame. A thrown item's moat splash is raised the same way ([combat-events.md](combat-events.md)).

**A cut prop is missing from the replay.** `CheckWeaponStagePropHit` (`src/stage_prop_hit.c`) calls
`DispatchCombatEventEffect` for `COMBAT_EVENT_PROP_CUT` rather than `EmitCombatEvent`, the only event raised
that way, so it is never recorded and the replay plays neither its sound nor its delayed sound.

**Snow restarts on six of the eleven snowy stages.** `InitStageSnowIfEnabled` (`src/stage_callbacks.c`), called
when a bout starts and when the replay, victory scene and story states reset the scene, restarts the snow on
stages 0, 1, 3, 4, 5 and 10; stages 2, 6, 7, 8 and 22 also snow (`gStageCallbacks`, `src/stage_cull.c`), and
there the flakes carry on from where they were.

**Katze's muzzle flash blends one way for each player.** The two sprite meshes of the flash
(`gGunMuzzleFlashP1Mesh`, `gGunMuzzleFlashP2Mesh`, `src/effect.c`) are both semi-transparent, but player one's
names texture page 0x000C, whose blend mode is the average of screen and sprite, and player two's 0x002F, whose
mode adds them, so the same shot flashes brighter when player two fires it ([effects.md](effects.md)).

**Player one's textures are uploaded twice.** `LoadCharacterArchive` (`src/model_arena.c`) uploads player one's
texture group, then uploads it again: unchanged when its fog is off, with the palettes moved to the fog block
when it is on ([vram.md](vram.md)). Player two's is uploaded once.

## Movies

**Two values the movie player never sets.** In the USA and European builds `BuildMovieTileQuads`
(`src/movie.c`) takes the movie tiles' texture pages from `dec->frameRects[frameRectIndex]` with an index it
never sets, and nothing on the way to it from the program's start sets the register that holds it. In the
Japanese build `RunMoviePlaybackLoop` hands `CdSync` and `CdReady` a result pointer it never sets, where the
other builds pass an 8-byte buffer: whenever a CD command completes during playback, its 8 result bytes are
copied to wherever that value points (nowhere when it is 0).

**Outside Japan the decode wait's time-out does not end the wait.** `WaitMovieFrameReady` (`src/movie.c`) counts
down 0x800000 passes while it waits for the decoder's `frameReady`. In the Japanese build the time-out sets the
flag and moves to the other frame; in the others it moves to the other frame but leaves the flag clear, so the
loop goes on waiting for the decoder, the counter now below zero ([movies.md](movies.md)).

**A subtitle line has 56 bytes.** `UpdateAndDrawSubtitle` (`src/subtitle.c`) copies each line of a subtitle
into a 56-byte buffer on the stack without checking its length, so a line of more than 55 characters would run
past it. The lines are in the subtitle banks on the disc ([story-presentation.md](story-presentation.md)).

## Written and never read, or never called

Read by nothing but their own update: `gFighterInputHistory` (eight frames of held buttons, `src/combat_state.c`),
`gStoryFallCount` (story bouts ended by a fall), `gLinkRxButtonBitCount` and `gLinkTxButtonBitCount` (`src/link.c`),
`gStageLitColor` (`src/stage_cull.c`); the one reader of `SceneNode.flags`, a test in `RenderSceneDisplayList`,
branches to the same place either way. Never called, besides those the system pages name: `SaveScratchpad` and
`RestoreScratchpad` (`src/cloth.c`), `MarkCellsHighDetailAroundPoint` (`src/stage_cull.c`), `SeedRandomFromLoadArea`
(Japanese build only, `src/main.c`), `ComputeAcosRatio` and `ComputeAcosComplementRatio` (`src/scene_math.c`), and
the assembly routines whose headers say `unused`. Set up and never drawn: the fifteen prop nodes of stage 9
(`InitStage9PropsAndCloth`, `src/stage_callbacks.c`), which the stage's update never queues.
