# The opening

Left alone, the game alternates between the opening and a demo battle ([game-flow.md](game-flow.md)). The opening
is not a movie: it is drawn in real time from one file, `DATA3/O01.DAT`, played by a small script of fades,
overlays and two kinds of 3D scene. There are three variants, played in turn. Before it, outside Japan, the boot
shows the publisher's splash screen. The code is `src/opening_state.c`, `src/opening_demo.c` and
`src/opening_overlay.c`, with the ribbon trail of [effects.md](effects.md).

## The splash screen

`PlayBootSplashScreen` (`src/splash.c`) runs once from `main`, before the main loop, with its own display set-up
and loop. It reads its pictures from the motion archive category of the file table, files 44 and 45 after the
weapon pairs, into the dynamic load area and uploads them to VRAM at X 640. The North American build shows one
4-bit picture 640 by 60 at line 210 of a 640 by 480 screen; the European build shows an 8-bit picture of 640 by
256, then a 4-bit picture of 320 by 256, on PAL-height screens. Each screen lasts 300 frames, fading in from black
over 64 and out from frame 237, through a subtractive tile over the picture. VRAM is cleared after it. The Japanese
build has no splash.

## The opening state

`InitOpeningState` (state 3) loads the next variant (`gOpeningVariantIndex`, 0, 1, 2 in turn) and cues the opening
music, XA track 8, paused; `UpdateOpeningState` lets the stream go at frame 195, runs the demo each frame, and
moves to the demo battle when the script ends or to the main menu when start is pressed on either pad.

`InitOpeningDemo` reads `DATA3/O01.DAT` (an `OpeningDemoArchiveHeader`: a pose stream, one camera track per variant
for the fighter scene, one camera track for the stage scene, a TIM archive) into the dynamic load area and sets up
the scene (`SetupOpeningDemoStage`): the cameras and lights, stage 23, which only the opening loads
(`STAGE_ID_OPENING_DEMO`), with its lighting, a copy of the stage palette row (Y 0x1E0) into rows 0x1E1 and 0x1E2,
the second tinted to 7/8 red and green and 11/16 blue (`BuildOpeningTintedCluts`), and one fighter model loaded
without a weapon: character file 34, 32 or 33 (`DATA6/A09.DAT`, `A07.DAT`, `A08.DAT`) for variants 0, 1, 2
(`gOpeningDemoModels`). It starts the pose stream decoder on the file's pose stream, both camera track readers, the
overlays and ribbon trail 0.

## The script

Each variant has a script of shorts (`gOpeningDemoScripts`): a delay in frames since the previous step, then one or
more operations, each an opcode and an argument, chained by a 0 between them. `StepOpeningDemoScript` waits for the
delay, runs the step's operations and moves on; opcode 5 ends the script, and `UpdateOpeningDemo` then returns 0.

| Opcode | Operation |
| ---: | --- |
| 1 | Start a screen fade with the argument as preset ([rendering.md](rendering.md)), or stop fading with -1 |
| 2 | Overlay cue 0 to 9: upload its pictures and start its banner and cards (below) |
| 3 | Upload a group of the TIM archive (no script uses it) |
| 4 | The fighter scene: 0 start, 1 stop, 2 restart its pose frame |
| 5 | End |
| 6 | The camera scene: 0 start, 1 stop, 2 restart the pose frame |
| 7 | `OpeningDemoScriptNoOp`, which does nothing; only variant 0's script calls it |

All three scripts have one shape, apart from variant 0's calls of opcode 7; only four delays differ:

| Delay (variant 0, 1, 2) | Step |
| --- | --- |
| 0 | cue 0 |
| 190 | fade in from black (preset 2); the fighter scene starts |
| 60 | cue 1 |
| 120, 75, 80 | cue 2 |
| 150, 165, 140 | fade out to black (preset 3) |
| 20 | the fighter scene stops, the camera scene starts, the fade stops; cue 3 |
| 166, 166 | cues 4 and 5 |
| 166 | the camera scene stops; fade in from black; the fighter scene starts |
| 300, 260, 280 | fade out to black |
| 20 | the fighter scene stops, the camera scene starts, the fade stops; cue 6 |
| 166, 166 | cues 7 and 8 |
| 166 | the camera scene stops; fade in from black; the fighter scene starts |
| 210, 280, 280 | fade out to white (preset 0) |
| 20 | the fighter scene stops, the fade stops; cue 9 |
| 300 | end |

The delays of every variant add up to 2,386 frames. In the European build each 166 is 102 and the last 300 is 210
(1,912 frames).

## The two scenes

**The fighter scene** (`UpdateOpeningDemoFighterScene`) is a performance: the camera comes from the variant's
camera track (channel 0 of the decoder in [story-presentation.md](story-presentation.md)), offset from the world
origin and turned half a turn, and the fighter's pose from frame `variant x 900 + n` of the file's pose stream,
decoded like a bout's ([models.md](models.md)) but applied by `ApplyOpeningDemoPose`: entry 0 places the root in
polar form (height, heading, distance), entries 1 to 17 are joint angles in the order of `gOpeningDemoBoneOrder`. A
ribbon trail follows two points on the right hand node, at offsets that differ per variant (from about 70 to 1,320
along it). The fighter's cloth runs. The stage is culled as in a bout, with the sixteen cells around the origin
forced to detail level 2. After 900 frames the pose and both camera tracks start over.

**The camera scene** (`UpdateOpeningDemoCameraScene`) flies the camera along the stage track (channel 1, shared by
the three variants) over the empty stage.

## Overlays

Each cue uploads one group of the TIM archive (cue 0 group 8, cues 1 and 3 to 9 groups 0 to 7; cue 2 none), starts
a title banner for cues 3 to 8 (`gOpeningOverlayToBanner`), and starts up to two card overlays from
`gOpeningOverlayGroups`. Their textures are in `gOpeningOverlayTextures`: five frames of each of six banners, then
seventeen cards.

**Banners** (`UpdateOpeningBannerPackets`, one at a time): a 160-pixel-wide sprite at the banner's place
(`gOpeningBannerEntries`) whose texture slides 32 texels left or right over 128 frames; after 60 frames a second
sprite below it steps through the banner's other frames, one every two frames. The banner fades in over 8 frames,
holds until frame 150 (86 in Europe) and fades out by 8 a frame.

**Cards** (`gOpeningOverlayDescs`, four slots, `UpdateSlashOverlays`): each has a place, a glow colour, a hold
time, a kind and a delay. The slash kinds (0 to 4) draw a horizontal or diagonal cut as gradient lines in the glow
colour, fading as the card holds, while the picture grows from its middle line one line a frame, its upper half
drawn to the right and its lower half to the left by two pixels a line until they meet (`EmitHorizontalSplitQuad`,
or two growing triangles for the diagonal kind); then the card fades out over 16 frames. The wide kinds draw a
picture wider than a texture page as a row of sprites: kind 5 after a white flash fading over 20 frames, kind 6 at
once, kind 7 fading in over 20 frames, each holding and fading out over 20; kinds 6 and 7 also cover the screen
with a quad textured by a single texel. Cue 0 shows a wide picture for 150 frames, cues 1 and 2 quick horizontal
cuts, cues 3 to 8 one or two cuts after 12 frames, held 108 frames (44 in Europe), and cue 9 two wide pictures for
240 frames. Card 14 is in no group. In monochrome the glow colours are greyed.

All overlay packets are linked at the front of the ordering table.

## Builds

Apart from the European timings above, the Japanese build builds the pose's joint matrices with
`BuildRotMatrixZYX`, the others with `BuildRotationMatrixFast`.

## A title menu nothing calls

`src/opening_demo.c` also holds a menu of seven entries over a logo and a white backdrop: `InitOpeningTitleMenu`,
`UpdateAndDrawOpeningTitleMenu` (an entry scrolling in from above or below at 3 lines a frame, arrows that flash
when pressed) and their sprite tables. No routine of any build calls them.
