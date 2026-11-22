# The disc and its formats

Each disc is one MODE2/2352 data track holding an ISO 9660 file system. `SYSTEM.CNF` boots the executable,
which stays resident and is the whole program: no other file on the disc holds code. Everything else is data
the executable loads or streams: below are the files, how the game finds them, and every format it reads, with
the routine that reads each and the record the source declares for it.

Standard PlayStation formats are named as such and not described again. For the game's own formats, the
source is the specification: each section names the routine to read. Where the source has not yet declared a
layout as a C record, the section says so.

## The files

| Directory | USA | Japan | Europe | Holds |
| --- | ---: | ---: | ---: | --- |
| (root) | 3 | 3 | 3 | `SYSTEM.CNF`, the executable, and a file of zeros that pads the disc (`DUMMY2.DAT`; `DUMMY.DAT`, half its size, in Japan) |
| `BIN0/` | 30 | 30 | 30 | Weapon-pair motion archives: each fighter's motion graph, animation table, AI tables and compressed poses |
| `BIN1/` | 15 | 14 | 16 | 14 more motion archives, and the boot splash images (`SCEAP.TIM` in North America; `SCEEP.TIM` and `WARNING.TIM` in Europe) |
| `DATA0/` | 26 | 26 | 26 | Character archives (`M*.DAT`): models, pose-part overrides, shadow models and textures |
| `DATA1/` | 20 | 20 | 20 | Weapon archives (`H*.DAT`): models and textures |
| `DATA2/` | 26 | 26 | 26 | Stage archives (`S00.DAT` to `S25.DAT`): models, floor collision, camera walls, props, layout, textures |
| `DATA3/` | 21 | 20 | 21 | Menus, interface and effect textures, the opening demo, the select scene, credits, subtitles, visibility masks |
| `DATA4/` | 24 | 24 | 24 | Fighter texture archives (`D*.DAT`): base, alternate and injury overlays |
| `DATA5/` | 2 | 2 | 2 | The first-person view's mannequin model (`T0.DAT`, `T1.DAT`) |
| `DATA6/` | 10 | 10 | 10 | Character archives for the Slash mode's enemies and the opening (`A*.DAT`) |
| `DATA7/` | 9 | 9 | 9 | Story mode 3D cutscenes (`V00.DAT` to `V08.DAT`) |
| `SE0/` to `SE5/` | 148 | 148 | 148 | Sound archives: weapon-pair and fighter effects, voices, story dialogue, menu sounds, stage ambience |
| `STR16/`, `STR/` | 13 | 13 | 13 | Movies: the opening and twelve endings (`STR/` in Japan, `STR16/` elsewhere) |
| `XA/` | 4 | 4 | 4 | Music: four XA audio files of four channels each |
| **Total** | **351** | **349** | **352** | |

### How the game finds a file

The game never searches the file system. `gCdFileTable` in `src/cd.c` holds, for every file it reads, its start
position on the disc (a BCD minute, second and sector) and its size in bytes (`CdFileEntry`, `include/cd.h`):
346 entries in North America, 344 in Japan, 347 in Europe. Every entry of each build's table points at the start
of a file on that build's disc. Code asks for a file by category and index: `ResolveCdFileHandle` turns a
`CdFileCategory` (data, motion, sound, XA, movie) and an index into a handle, and `StartCdFileRead` reads that
file to a buffer. A table of the files' names (`gCdFileNames`, `src/asset_load.c`) is present but nothing reads
it.

The table ties the executable to its disc: moving a file on the disc means changing the table. The disc does not
depend on the executable.

Eight files of each disc are outside the table: `SYSTEM.CNF`, the executable, the padding file, and five data
files nothing loads (`DATA0/M26.DAT`, `DATA6/A04.DAT`, `DATA6/A05.DAT`, `DATA6/A06.DAT` and `SE0/SFX.`). Each is
a well-formed archive of its kind; the table's slots where `A04` to `A06` would sit all point at `DATA6/A00.DAT`.

## Standard PlayStation formats

| Format | Files | Read by |
| --- | --- | --- |
| PS-X executable and `SYSTEM.CNF` | the root | The console's BIOS |
| TIM image | `BIN1/*.TIM`, and inside every texture group below | `PlayBootSplashScreen` (`src/splash.c`) for the splash images; `src/tim.c` for the rest |
| VAB sound bank (header `VH` and body `VB`) | inside every sound archive | Sony's LIBSND (`SsVabOpenHead`, `SsVabTransBody`), called from `src/sound.c` |
| STR movie: MDEC video sectors interleaved with XA audio | `STR16/*.STR`, `STR/*.STR` | `src/movie.c` |
| XA-ADPCM audio | `XA/*.XAI` | `src/cd.c` (`StartXaTrack`, `StartCdAudioStream`), choosing one of the four channels by the CD filter |

The movies are 320 by 240. Japan's are 24-bit and are decoded straight to the screen; the North American and
European movies are 16-bit, decoded into memory and drawn as 80 textured strips so the English subtitles can be
drawn over them ([history.md](history.md)).

The discs hold no SEQ music and no TMD models: the music is all XA audio, and the models are the game's own
format below.

## The game's own formats

### Section archives

Most data files begin with a word count of sections followed by each section's byte offset from the start of the
file, and the file's total length after the last. Each loader declares the header it expects:

| Files | Header | Read by |
| --- | --- | --- |
| `DATA0/M*.DAT`, `DATA6/A*.DAT` (characters) | `CharacterArchiveHeader` (`include/model_arena.h`): models, pose-part override table, shadow models, texture group | `LoadCharacterArchive` |
| `DATA1/H*.DAT` (weapons) | `WeaponArchiveHeader` (`include/model_arena.h`): weapon models, texture group | `LoadWeaponArchive` |
| `DATA2/S*.DAT` (stages) | not declared as a record; six or seven sections, below | `LoadStageArchive` |
| `DATA3/O01.DAT` (opening demo) | `OpeningDemoArchiveHeader` (`src/opening_demo.c`) | `src/opening_demo.c` |
| `DATA3/O03.DAT` (select scene) | `SelectSceneArchiveHeader` (`src/select_scene.c`) | `src/select_scene.c` |
| select screen textures | `SelectTimArchiveHeader` | `src/select_scene.c` |
| `DATA5/T*.DAT` (first-person mannequin) | `PovOverlayArchiveHeader` (`src/pov_overlay.c`): one model table of 15 parts | `LoadPovOverlayModel` |
| movie theatre screens | `MovieTheaterArchiveHeader` | `src/theater_ui.c` |
| `DATA7/V*.DAT` (story cutscenes) | `CutsceneArchiveHeader` (`src/story_cutscene.c`); below | `InitStoryCutscene` (`src/story_cutscene.c`) |

### Texture groups

A texture group starts with a table of word offsets, one per group (the files measured hold 32, so the first
offset is 0x80). Each group is a word count followed by that many 12-byte entries: the TIM's byte offset (a
word), then its VRAM position and its CLUT's position (four halfwords: x, y, CLUT x, CLUT y). The TIMs follow.
`LoadTimGroup` and its variants in `src/tim.c` upload a group; `GetTimGroupEntryInfo` reads an entry. The
layout is read by these routines and is not declared as a record. Groups appear in their own files (the
`DATA3` interface files, `DATA4`) and as the texture section of the archives above.

### Models

A model section is a word count followed by that many offsets, one per mesh. `RegisterModelTable`
(`src/model_table.c`) turns the offsets into pointers when the archive is loaded and `RelocateModelTable` moves
them when the model arena compacts. A mesh is a vertex block followed by groups of primitives, each group a
count and a primitive type followed by its records. Vertices come in three forms: plain (`SVECTOR` triples),
compact (six bytes each, with `CompactTexturedPolyHeader`, `src/tpage_reloc.c`), and skinned (flag 0x8000:
thirteen bone lists with pre-lit normals). There are 44 primitive types; `gPrimRecordHalfwords` gives each
record's size, and the hand-written renderer (`src/render.s`, with `src/pov_tex.s`, `src/pov_flat.s` and
`src/cuttable_fade.s`) reads the records. `include/mesh_stream.h` declares part of the layout; the records of
the 44 primitive types are read in assembly and not yet declared in C.

### Stage archives (`DATA2/S*.DAT`)

`LoadStageArchive` (`src/model_arena.c`) reads six sections, seven on stages 3, 9, 11, 12, 14, 17 and 19
([stages.md](systems/stages.md) says what each is kept for):

| Section | Holds | Read by |
| --- | --- | --- |
| 1: models | the stage's meshes | `RegisterModelTable` |
| 2: floor collision | a 32 by 32 grid of cells 3,200 world units wide, each a list of floor polygons (flags with a vertex count and a surface attribute, the polygon's 2D vertices, then a height or a plane); an empty cell is -1 | `QueryFloorHeight` (`src/floor_collision.c`) |
| 3: camera walls | a count and `StageCameraWall` records (`include/camera.h`) | `src/camera.c` |
| 4: prop markers | `StagePropMarker` records ending at kind 0 (`include/stage_prop_hit.h`) | `src/stage_prop_hit.c` |
| 5, seven-section stages only | on stages 3 and 14 the cuttable props: a model offset, a keyframe offset and a count, each keyframe six halfwords; on the others a second model table | `src/cuttable_prop.c`; `RegisterModelTable` |
| next to last: scene layout | three lists of halfwords ending at -1: floor tiles, static props with position and rotation, and each grid cell's tiles and props | `ParseStageSceneLayout` (`src/stage_cull.c`) |
| last: textures | a texture group | `src/tim.c` |

The floor collision and scene layout are read by those routines and are not declared as records.

### Motion archives (`BIN0/`, `BIN1/`)

One archive per pair of weapons is loaded for each bout to the fixed address `0x8014C000`, and every pointer in it
is already an address there: the game does not relocate it. Its header is `MotionFileHeader`
(`include/motion_file.h`): the placements a bout starts in, the animation table (`AnimInfo`, `include/fighter.h`),
each clip's first pose frame, the pose stream, the story intro and victory placements, and the AI's attack tables
per stance (`AiAttackChoice`, `include/fighter_ai.h`). `LoadWeaponPairMotions` (`src/match_setup.c`) loads it.

The motion graph is a set of `MotionPlacement` nodes (`include/fighter.h`): each plays a span of one clip
and lists `MotionTrigger`s, each a controller mask (with the edge it fires on, and optionally the character
classes it applies to) and the placement it leads to. `UpdateFighterFrame` (`src/fighter.c`) walks them. Each
`AnimInfo` carries a byte code of frame events (sounds, footprints, effects) that `src/combat_event.c`
interprets, and the AI's attack choices carry input scripts that `src/fighter_ai.c` plays.

### Pose stream

The fighters' skeletal animation is a bitstream of 54-halfword poses: three root values and 51 joint angles of 12
bits. A header gives each joint's default angle and delta mode; frames come in groups of eight, a key pose then
seven frames of second-order deltas, each channel coded one of four ways. `InitPoseStream`, `DecodePoseFrame` and
`DecodePoseFramePair` (`src/pose_stream.c`, records in `include/pose_stream.h`) decode it, keeping four decoded
frames in a cache. It appears in the motion archives and in `DATA3/O01.DAT` and `O03.DAT`.

### Camera tracks

Cutscene and opening cameras are a bitstream read most significant bit first: each frame codes six channels
(position and rotation), each as a 4-bit width (0 meaning 16) and a signed delta of that width added to the
channel. `src/camera_track.c` decodes it (`SceneCameraTrack`, `include/camera_track.h`).

### Story cutscenes (`DATA7/V*.DAT`)

A section archive of 19 sections in Japan and 20 elsewhere: six scene variants (`SceneVariant`,
`include/story_cutscene.h`), each with a camera track, uncompressed poses of 108 bytes a frame, and facial expression
scripts of 4-bit codes; outside Japan the last section is the cutscene's subtitle bank. `InitStoryCutscene`,
`UpdateStoryCutscene` and `ApplyCutsceneFighterPose` (`src/story_cutscene.c`) play it. The header is
`CutsceneArchiveHeader`.

### Subtitle banks (outside Japan)

A bank is a count of scripts, their word offsets, then a count of string offsets and the strings. Each script is
lines of text, each headed by five characters: a four-digit frame time and a line code. `RelocateSubtitleBank`
(`src/subtitle.c`) turns the offsets into pointers in place, and `UpdateAndDrawSubtitle` plays a script. Banks
are `DATA3/O20.DAT` and sections of `O05.DAT`, `O14.DAT` and the story cutscenes.

### Visibility masks (`DATA3/O16.DAT`, `O17.DAT`)

Each is 8,192 bytes: 64 rows of 32 words, one row per camera heading and a bit per grid cell, loaded straight
over the first 64 of `gStageVisibilityMask`'s 65 rows by `LoadVisibilityMaskPreset` (`src/stage_cull.c`): `O16`
for the normal camera, `O17` for the first-person view. The 65th row, which no heading selects, keeps the
values the executable holds.

### Sound archives (`SE0/` to `SE5/`)

Each starts with a few byte offsets: the first two bound the block of VAB headers, which is copied to memory;
the others locate each VAB header and each body, which is sent to the sound processor. An archive holds one to
six VABs. `RequestSoundBankLoad` and `CommitSoundBankLoad` (`src/sound.c`) read them, and
`TransferSoundVabSet` hands the VABs to Sony's library. The offsets are `SoundArchive`. Which
archive a sound comes from is listed at the top of `src/sound.c`.

## What is not known

- **How the data was written.** Every format above can be read from the source, but writing a file the original
  way would need the choices of the studio's own tools: which coding each pose channel used, the order of the
  records inside a motion archive, the settings of the movie encoder.
- **The primitive records of the model format** are read only by the hand-written renderer and are not yet
  declared in C.
- **Unused words:** some header words nothing reads (in the stage archive's sections, the sound archives of
  `SE5/`, `AiAttackChoice`) have no known meaning.
- **The five files nothing loads** are complete archives of their kinds; why they are on the disc is not known.
