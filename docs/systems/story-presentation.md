# Story presentation

Story mode tells its story in five ways besides the bouts: a picture slideshow before the first bout, a 3D cutscene
after each won bout, a scripted walk (the approach cutscene), an ending movie or the dishonourable ending's
pictures, and the staff credits. Outside Japan most of these carry English subtitles. When each plays is in
[story.md](story.md); the movies are in [movies.md](movies.md). The states are in `src/story_state.c`.

## Cutscenes between bouts

After a won bout `InitStoryCutsceneState` loads the dialogue sound bank and calls
`InitStoryCutscene(gStoryBoutIndex - 1, class, DYNAMIC_LOAD_BUFFER)` (`src/story_cutscene.c`): scene `n` is the
file `DATA7/V0n.DAT`, played after bout `n` (0 to 8), and the variant is player one's character class. A file
starts with a `CutsceneArchiveHeader`: a variant table (`SceneVariant`: flags, of which bit 0 says a second fighter
takes part, and a frame count; flags 0 means the variant is empty and nothing is set up), for each of the six
variants the offsets of a camera track and of pose data, a face script per variant, and outside Japan the scene's
subtitle bank.

The scene is played where the bout ended. `SelectCutsceneAnchorPoint` takes the midpoint of the two fighters and
picks, among the four spawn pairs of the stage (`GetStageSpawnTable`, [stages.md](stages.md)), the pair whose
midpoint is nearest. On three stages with two levels the pick is limited to the fighters' level when both stand on
it: stage 4 split at height 4,000, stage 10 at 0 and stage 25 at 500. The floor height under the anchor
becomes the scene's floor. `gCutsceneWeaponPairs` may replace either fighter's weapon for the scene: a nibble per
fighter (player one high), 0 to 3 loading two of the four meshes the cutscene weapon file holds for the character
(`ReloadFighterWeaponModel` with `WEAPON_CUTSCENE_OVERRIDE_BASE`, `src/model_arena.c`), 5 keeping the bout's
weapon. The cloth is rebuilt and the effects cleared.

Each frame `UpdateStoryCutscene` plays one frame of three streams:

- **The camera** (`DecodeCameraTrackFrame`, `src/camera_track.c`): a bit stream read most significant bit first,
  each frame six values (an offset X, Y, Z from the anchor and three angles), each a 4-bit width (0 meaning 16)
  then a signed delta of that width added to the value. The camera's yaw is turned by half a turn
  (`CUTSCENE_CAMERA_YAW_OFFSET`). The same decoder runs the opening demo's cameras, on channel 0 for
  the fighter scene and 1 for the camera scene ([opening.md](opening.md)).
- **The poses** (`ApplyCutsceneFighterPose`): one `FighterPose` of 18 entries per fighter per frame, player two's
  frames following player one's. Entry 0 places the root in polar form about the anchor (height, yaw, radius);
  entries 1 to 17 are joint angles, taken in the order of `gCutsceneBoneOrder` and built into the skeleton's local
  matrices directly, bypassing the motion graph. The scene's floor is passed for the shadows.
- **The faces** (the face script): a count of streams (1 or 2), their lengths in words, then a 4-bit code per
  fighter per frame, eight to a word. The low three bits are the facial expression (`SetFighterFacialExpression`);
  the fourth bit of player one's code is a voice cue.

On a voice cue `UpdateStoryCutsceneState` plays the scene's next dialogue line on voice 0, sound 0x6C plus the
cue's index, from the dialogue bank ([audio.md](audio.md)); the one exception is Utsusemi's second line in the
cutscene after bout 5, which is counted but not played. The stage's ambience runs at half volume, the cloth,
billboards and effects run, and the stage is culled from the camera's distance to the anchor. The scene ends on its
last frame or when cancel is pressed; the story then starts the next bout, the approach cutscene, the dishonourable
ending or the ending movie ([story.md](story.md)).

## The approach cutscene

After the cutscene that follows bout 3, `UpdateApproachCutsceneState` plays up to two parts of 151 frames, on stage
3 and then stage 9, or only the second (`gApproachCutsceneStartPart`, by the stage bout 3 ended on). It is not a
recording: the fighters are set up at fixed places (`gApproachCutsceneSpawns`) on the stage and the normal fighter
frame runs with a scripted controller for player one and none for player two. In the first part player one holds
right and L1 for eight frames, then L1 alone; in the second it holds up and L1, turns right at frame 60, adds R2 at
frame 84 and R1 at frame 88. The camera aims at player one from a fixed point (`gApproachCutsceneCameras`), with a
weight falling from 1 to 0 over the part (`AimSceneCameraAtFighter`). Cancel on either pad starts bout 4 at once.

## Prologues

Before the first bout `InitStoryPrologueState` runs a picture slideshow (`InitStorySlideshow` in prologue mode,
`src/story_slideshow.c`). It reads the common prologue file (`CD_FILE_PROLOGUE_COMMON`: two TIM groups and, outside
Japan, the prologue subtitle bank) and the character's own file after it, and builds its packets in the model
arena. Full-screen pictures are 8-bit images kept in VRAM from X 640, drawn as grids of tiles up to 32 texels
wide and 64 high (`BuildTiledPolyFt4Grid`, two copies of each packet for the two frame buffers) at the back of
the ordering table.

The slideshow is a timed state machine: each state lasts the frames in `gPrologueStateDurations` and, as it ends,
does its work. In the North American and Japanese builds (the European durations are about five sixths of these):

| State | Frames | Shows; at its end |
| --- | ---: | --- |
| cue audio | 0 | nothing; cue the prologue introduction's music (XA track 6) |
| wait | 60 | nothing; upload the common pictures, fade in from black, start the music |
| intro | 111 | the first picture fading in |
| card 0, 1, 2 | 102, 99, 144 | three picture cards fading in over it one after another, 4 levels a frame; at the end of the third, fade to black |
| cards out | 45 | the fade; upload the second picture group, fade in |
| scroll | 585 | the second group scrolling up the screen one line a frame for 240 lines, then still; stop the music |
| hold | 30 | the same; flash to white |
| flash | 30 | the flash; upload the character's picture (a band 89 lines high, 85 from the top), fade in from white, start the character's music (XA track 0 to 5) |
| subtitle in | 240, 219, 534, 264, 270, 300 by class | the character's picture and, outside Japan, its subtitle; fade to black |
| subtitle out | 195, 180, 180, 150, 171, 180 by class | the same, fading out |

The prologue then ends and the bout starts. Cancel before the introduction's music has stopped skips to the
character's part (through a flash of 45 frames); after it, cancel starts the bout. While the cards fade in, each
fade also writes the colour of the entry after the card's sprite list ([oddities](oddities.md)).

## The dishonourable ending

`InitBadEndingSlideState` shows one of five pictures at random (`gBadEndingSlideSprites`, in Japan
`gBadEndingSlideRects`, from `CD_FILE_BAD_ENDING_TIMS`) with a footer, over XA track 11, until cancel returns to
the main menu.

## Subtitles

Only the North American and European builds have them (`src/subtitle.c`). A script is an array of caption strings,
each opened by five digits: a four-digit start frame and a line count, then the lines each ended by a zero byte; a
last entry with a count of 0 gives the frame at which the script ends. `UpdateAndDrawSubtitle` draws the current
caption from its start frame, each line centred at X 320 (the routine's other choice, 160, is never asked for) with
the font's glyph widths, the lines 16 apart and centred on Y 200, through `DrawFormattedText` in chunks of twenty
characters, and advances when the frame counter reaches the next caption's start. A subtitle id chooses the source
(`StartSubtitleById`):

| Ids | Source | Used for |
| --- | --- | --- |
| 0 to 5 | prologue bank (`DATA3`, the common prologue file) | the prologue's character part, by class |
| 6 to 59 | cutscene bank (in the cutscene file) | each cutscene starts its variant directly |
| 60 to 71 | movie bank (`DATA3/O20.DAT`, or the theatre's file) | the endings ([movies.md](movies.md)) |
| 72 to 83 | resident scripts, intro lines | a Story bout's opening line by the opponent (`gStoryIntroTauntScripts`), when `gPrevStageId` equals `gCurrentStageId` |
| 84 to 92 | resident scripts, select-button lines | a fighter's line when its select-button move starts (`gCharTauntSubtitleOffset`) |

A bank is relocated in place (`RelocateSubtitleBank`): a script count and word offsets, then a caption count and
byte offsets, all made into pointers. The select-button lines are reached through `gCharTauntSubtitleOffset` (0, 2,
3, 4, 5, 6 by class), so three of the nine scripts, a second Kannuki line and two more Utsusemi lines, are never
shown. Losing honour clears `gSubtitleActive` and so stops the opponent's line.

## The credits

`InitCreditsState` uploads the credits pictures (`DATA3/O18.DAT`) and starts XA track 10.
`UpdateAndDrawCreditsRoll` (`src/credits.c`) scrolls a list of strips up the screen one line a frame: each
`CreditsScrollEntry` is a 4-bit picture in VRAM (from X 640), its palette, size and the scroll line at which it
enters, a strip 256 or more wide drawn as two rectangles. Japan and Europe have 31 strips, Europe's spaced more
closely; the North American list has 34, three more inserted before the last three. Top and bottom bands 96 lines
high darken the scroll toward the edges (subtractive gradients). When no strip is left on screen an end picture of
120 by 56 fades in over 128 frames, holds until frame 192 and fades out by frame 320; the roll then ends, as it
does on cancel, and the game saves if auto-save is on and returns to the opening.
