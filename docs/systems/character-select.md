# Character select

Character, weapon and stage are chosen on one screen that is half 3D scene and half 2D layer. Three figures stand
on a stage: player one's fighter on one side, player two's on the other, and between them a third figure, the host,
who hands each fighter the weapon chosen; a scripted camera moves between them. Over the scene lie the portraits,
cursors, weapon cards and stage list. The states are `InitCharacterSelect` and `UpdateCharacterSelect`
(`src/menu_state.c`, states 9 and 10), the scene `src/select_scene.c` and the 2D layer `src/select_ui.c`. What each
mode does with the choice is in [game-flow.md](game-flow.md).

## Modes

`InitCharacterSelect` turns the play mode into one of eight `SelectGameMode`s and a costume layout
(`gModeCharSelectLayout`): Story and Slash use the story set of fighters (ids 6 to 11), the others the versus set
(0 to 5).

| Mode | Players | Choices | Stage list |
| --- | --- | --- | --- |
| 0 Story | 1 | fighter, weapon | none: the story decides |
| 1 VS | 2 | each player its fighter and weapon | eight stages (`gVsStageSelectTable`) |
| 2 Story challenger | 2 | player one keeps its choice, player two chooses | none |
| 3 First-person VS | 1 | player one chooses, then chooses the computer's fighter and weapon with the same pad | eight (`gPovStageSelectTable`) |
| 4 Practice | 1 | as mode 3 | three (`gPracticeStageSelectTable`) |
| 5 Link | 2 | each console its own | seven (`gLinkStageSelectTable`) |
| 6 Slash | 1 | fighter; the weapon is set to the katana without asking | none: stage 20 |
| 7 VS with Katze | 2 | as VS, with Katze as a seventh fighter | eight (`gVsStageSelectTable`) |

It then loads the screen (`InitCharacterSelectScene`) into the dynamic load area and the auxiliary resource buffer,
starts XA track 9 repeating, and waits for the link cable's partner if there is one. `UpdateCharacterSelect` reads
left, right and confirm from each pad (and up and down outside Japan, for the stage list) and calls
`UpdateCharacterSelectFrame`. When that reports the end, it reads the choices (`FinalizeCharacterWeaponSelect`),
stores them in `gSelectedFighter` and `gSelectedWeapon` with the costume set's offset, sets the stage from the
mode's table and starts the mode: Story through the prologue, the rest straight into a bout. Player two pressing
start during Story's select starts a challenger: a banner for 90 frames, then the screen resets in mode 2
([story.md](story.md)).

## Loading

`InitCharacterSelectScene` reads `DATA3/O02.DAT` (file 0x53: three groups of interface pictures, uploaded at once,
and Katze's group, uploaded only when the gun is chosen), then loads the host, character archive 18, into the model
arena (`LoadSelectAuxModel`). Five of the six fighters of the costume set are cached in the main buffer and the
sixth in the auxiliary one (`LoadResourceGroup`, groups 3 to 6 of `gResourceGroupIds`,
[files-and-memory.md](files-and-memory.md)), so moving the cursor copies a fighter from memory instead of reading
the disc (`ReloadSelectFighterModels`, which also gives the fighter's hands the select screen's hand meshes, two
per character). `DATA3/O03.DAT` (file 0x54) goes to the auxiliary buffer: the screen's pose stream, a camera track
of one `SelectCameraKey` (position and three angles) per frame, the weapon models and the hand models, registered
as model slots 0x10 and 0x11. The lights use the preset after the 26 stages' (`ApplyStageLighting` with 0x1A). The
scene draws from the auxiliary packet buffers and tracks their peak use in `gSelectPeakPacketBytes`.

## The figures and their scripts

Each figure is a `MenuAnimState` (`include/menu_animation.h`) running a script of halfwords: an entry without bit
15 plays that clip of `gMenuAnimClips` (42 clips, a first frame in the pose stream and a length), one with bit 15
is an opcode (`MenuAnimOpcode`): wait for a flag, holding the current frame or looping the clip; halt; play the
character's own clip (the argument plus four times the class: clips 12 to 35 are four per character); branch by one
or two players or by a condition (`MenuAnimBranchCond`: `MENU_ANIM_COND_P1_WEAPON_UNCONFIRMED`,
`MENU_ANIM_COND_P2_IS_KATZE`); jump; do nothing. `UpdateMenuAnimScript` advances a figure when its clip ends.
`ApplyMenuShowcasePose` decodes frame `startFrame + timer` from the pose stream and places the root in polar form
(height, yaw, radius) about the figure's spot: player one at (900, 0, 900), player two at (-900, 0, 900), the host
at the origin.

- **Player one** (`gSelectP1Script`): its character's idle clip, looped until confirmed; its confirm clip and a
  shared clip; it holds until the weapon is confirmed; two clips of taking the weapon; it holds until the host is
  done; a last clip.
- **Player two** (`gSelectP2Script`): the same with its own clips, waiting after its weapon for a cue the host
  gives at step 18 (`SELECT_HOST_STEP_WAIT_P2_WEAPON`) of its script.
- **The host** (`gSelectHostScript`): clips 0 to 11, branching on one or two players, on whether player one's
  weapon is confirmed and on whether player two chose Katze.

The flags tie them together through `SelectPlayerStep` and `SelectHostStep` (`src/select_scene.c`):

| Enumerator | Step | Meaning |
| --- | ---: | --- |
| `SELECT_PLAYER_STEP_WAIT_CHAR` | 1 | Player one or two waiting for character confirm |
| `SELECT_P1_STEP_CHAR_CONFIRMED` | 3 | Player one clip 36 before weapon wait (challenger resume) |
| `SELECT_PLAYER_STEP_WAIT_WEAPON` | 4 | Player one or two waiting for weapon confirm |
| `SELECT_P1_STEP_AFTER_WEAPON` | 6 | Player one clip 39 after weapon handoff |
| `SELECT_P2_STEP_WAIT_HOST` | 7 | Player two waiting for the host to halt |
| `SELECT_P1_STEP_WAIT_HOST` | 10 | Player one waiting for the host to halt |
| `SELECT_HOST_STEP_WAIT_P1_WEAPON` | 9 | Host waiting for player one's weapon |
| `SELECT_HOST_STEP_HALT_1P` | 14 | Host halt on the 1P and player-two-Katze path |
| `SELECT_HOST_STEP_WAIT_P2_WEAPON` | 18 | Host waiting for player two's weapon |
| `SELECT_HOST_STEP_HALT_2P` | 21 | Host halt on the 2P path |

When the active players reach step 4 (`SELECT_PLAYER_STEP_WAIT_WEAPON`) the weapon phase begins; the host's clips
`SELECT_CLIP_HOST_HOLD_P1_WEAPON` (3) and `SELECT_CLIP_HOST_HOLD_P2_WEAPON` (8) put player one's and player two's
weapon in its hand, and the players' clips `SELECT_CLIP_P1_TAKE_WEAPON` (38, frame 19) and `SELECT_CLIP_P2_TAKE_WEAPON`
(37, frame 17) move it to theirs, at a fixed offset from the right hand. Weapon nodes draw from model slot 0x10,
mesh = weapon id, shadow = weapon id + 8.

The camera steps through `SelectCamPhase` (`src/select_scene.c`) while playing segments of the track
(`gSelectCamScript1P`: four, `gSelectCamScript2P`: six), each a first key and a length, with yaw turned half a turn;
the eight tested clip cues are named in `SelectAnimClip` (`src/select_scene.c`):

| Phase | Value | Advances on |
| --- | ---: | --- |
| `SELECT_CAM_CHAR_SELECT` | 0 | `SELECT_CLIP_HOST_PRESENT_P1` (2, t >= 25) to player one's weapon, or `SELECT_CLIP_HOST_PRESENT_P2` (7, t >= 5) jumping three segments to `SELECT_CAM_P2_WEAPON` when player one is already confirmed |
| `SELECT_CAM_P1_WEAPON` | 1 | `SELECT_CLIP_P1_TAKE_WEAPON` (38) to `SELECT_CAM_1P_OUTRO_WAIT` in 1P or when player two is Katze, else `SELECT_CAM_TO_P2_WEAPON` |
| `SELECT_CAM_TO_P2_WEAPON` | 2 | `SELECT_CLIP_HOST_PRESENT_P2` (7, t >= 5, presenting player two's weapon) |
| `SELECT_CAM_P2_WEAPON` | 3 | `SELECT_CLIP_P2_TAKE_WEAPON` (37) |
| `SELECT_CAM_2P_OUTRO_WAIT` | 4 | `SELECT_CLIP_HOST_2P_OUTRO` (10), starting `SCREEN_FADE_OUT_TO_WHITE_SLOW` |
| `SELECT_CAM_2P_FADE_OUT` | 5 | Screen fade until the final 2P segment ends, then finishes or enters stage select |
| `SELECT_CAM_1P_OUTRO_WAIT` | 6 | `SELECT_CLIP_HOST_1P_OUTRO` (5), starting `SCREEN_FADE_OUT_TO_WHITE_SLOW` |
| `SELECT_CAM_1P_FADE_OUT` | 7 | Screen fade (completes segment at once when player two is Katze), then finishes or enters stage select |

When the last segment ends, Story, the challenger and Slash are done; the other modes fade in from white (fast) onto
the stage list.

## The 2D layer

`UpdateAndDrawSelectScreenUi` draws the phase's sprites, the frame pieces in the last ordering-table entry and the
rest at the front.

**Characters.** Six portraits in a row at Y 152, X 32 to 512 every 96, in the order of `gSelectSlotToCharacter`:
Red Shadow, Tatsumi, Mikado, Black Lotus, Utsusemi, Kannuki, then Katze (slot 6, fighter id 14) in mode 7. Player
one starts on slot 0 and player two on slot 5; left and right move, wrapping, and two players cannot rest on the
same fighter (the mover skips past). A cursor's brightness follows the absolute sine of a phase that grows by 0x60
a frame; once confirmed it blinks for 30 frames and then stays at 128. Player two's cursor has a tag beside it; in
modes 3 and 4 the second cursor is the computer's (another sprite), hidden until player one has chosen. Outside
Japan the name under each cursor is printed with the font (`gCharacterNameLabels`, at Y 120).

**Weapons.** Each player's card at (36, 136) or (340, 136) shows the weapon's picture (`gWeaponCardArtSprites`, by
weapon id) in a frame. The screen's file (`DATA3/O02.DAT`) holds nine cards, for weapons 0 to 8; weapon 9 has
none. The cursor runs through eight slots in the order of `gSelectSlotToWeapon`: Katana, Rapier, Naginata,
Sledgehammer, Nodachi, Long Sword, Saber, Broadsword; slot 8, the Gun, is set when Katze is chosen, which also
confirms the weapon and skips the phase. A weapon can be moved to only while the host is at the right step of its
script. Confirming stamps the card: a seal (`gWeaponConfirmStampQuad`) turned to a random angle, drawn 120 right
and 40 down of the card, while the card shakes by -1 to 2 pixels, the shake halving each frame
(`UpdateAndDrawWeaponConfirmStamp`).

**Stages.** The list is a set of name plates laid out by mode (`gStageSelectLayouts`, each a row count and, per
row, a name sprite and a place). Outside Japan VS and Link have two columns of four at X 64 and 328 (Link's last
place empty), Practice one column of three; up and down move within a column and left and right between them
(`StepStageSelectCursor`). The Japanese build lays the plates in one row across the screen and moves with left and
right only. In the two-player modes either pad may move and confirm. Confirm blinks the chosen plate for 30 frames
and ends the screen. `DATA3/O02.DAT` holds these nine plates and no other, so the stages without a name in
`include/stage_id.h` have none. The name sprite each row shows is fixed by the layout, the stage by the mode's
table:

| Row | VS and VS with Katze | First-person VS | Link | Practice |
| ---: | --- | --- | --- | --- |
| 0 | 17 Yagura Point | 17 | 21 Warrior's Dwelling | 15 Meikyokan |
| 1 | 24 Yahiro Road | 24 | 24 | 16 Cherry Blossom Grove |
| 2 | 25 Underground Grotto | 25 | 14 | 12 Executioner's Cove |
| 3 | 14 Bamboo Thicket | 14 | 16 | |
| 4 | 16 Cherry Blossom Grove | 16 | 7, under Dozaemon Moat's name | |
| 5 | 19 Dozaemon Moat | 7, under Dozaemon Moat's name | 12 | |
| 6 | 12 Executioner's Cove | 12 | 15 | |
| 7 | 15 Meikyokan | 15 | | |

## Leaving

`FinalizeCharacterWeaponSelect` turns the slots into fighter and weapon ids, reloads the models when either fighter
uses the fog palettes (`IsCharFogSlotActive`; `UpdateCharacterSelect` turns them on for the first-person modes with
`EnableCharFogSlot`, [lighting.md](lighting.md)), clears the resource cache, marks the cloth for rebuilding and
sets day or night. `UpdateCharacterSelect` stops the CD before the next state.
