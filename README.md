# Bushido

A matching decompilation of **Bushido Blade** for the Sony PlayStation (1997), developed by Lightweight and
published by Square in Japan and by Sony Computer Entertainment in North America and Europe. One tree of C
and MIPS assembly builds the Japanese, North American and European executables, each identical byte for byte
to the one on its retail disc, with the original compiler, assembler and linker. Sony's Runtime Library, which
the game links, is decompiled here too.

![Matching progress](https://github.com/virtualonno/bushido/blob/pages/assets/progress.png?raw=true)

![Naming progress](https://github.com/virtualonno/bushido/blob/pages/assets/naming.png?raw=true)

## Builds

| Build | Region | Serial | Version | Executable | Size | Executable SHA-256 |
| --- | --- | --- | --- | --- | ---: | --- |
| `japan` | NTSC-J | SLPM-86020 | 1.1, built 1997-02-13 | `SLPM_860.20` | 604,160 B | `0164189a870ba87495a3c0010adb85535ae5a798bbd0dd276b88bf24ecc3ceda` |
| `usa` | NTSC-U/C | SCUS-94180 | 1.1c, built 1997-05-29 | `SCUS_941.80` | 595,968 B | `3db8eb15607e5f67a5efbf52143e57515bbfbfc3976e1ca6ef9150e67094e264` |
| `europe` | PAL | SCES-00899 | 1.1d, built 1997-06-19 | `SCES_008.99` | 595,968 B | `7f9c11d57d08358f8513a0886233ebbcfbcfbda178f7eceb725af70deb2db3ce` |

The version is the banner the executable prints at boot and the date its build stamp records. Each disc is
one MODE2/2352 data track:

| Build | Disc image | Size | `.bin` SHA-256 | `.cue` SHA-256 |
| --- | --- | ---: | --- | --- |
| `japan` | `Bushido Blade (Japan).bin` | 510,021,792 B | `7d89b9254c5001ad7fc4f0c2fc3cda9600417d075292c34d3b60aa3b600dba96` | `f68bde9f810797f7c1343f0f88653e3d2821e63685be4535f722987625db765d` |
| `usa` | `Bushido Blade (USA).bin` | 582,258,768 B | `c5cba1e3e8bc68f15e32492907ac37add7f8e8d3e10ddb36742a729ce1a970bf` | `20f9af8aa85f6e2c6b2fa9eaa8a5b204634dd573b1aab4a2ef24de15f875546d` |
| `europe` | `Bushido Blade (Europe).bin` | 582,475,152 B | `c8851f243fa8d7e6350488039024640beb5aa80b9a53473de86993201733ac4f` | `100a49823ccbf1adc754f778ab09dafc8ba8e4cc77b2aa8267523ae2529472b8` |

## State

- **Every byte of the three executables comes from this tree,** except the loading-screen picture (below).
  Built with the original tools, each relinks identical to its retail executable: the game's own code (983
  routines in the North American and European builds, 1,005 in the Japanese), all its initialized data, and the
  Runtime Library modules the game links, each decompiled in `src/lib`.
- **One link input is not source:** the order in which the linker placed the game's uninitialized globals
  (`config/bushidoblade/commons.tsv`). The original linker ordered them by the authors' own names, most of
  which are lost; [docs/building.md](docs/building.md) explains how the table was derived.
- **What matching means here.** A routine, datum or executable matches when the original tools turn this
  source into the same bytes the retail disc holds. That is a claim about the output, not about the text:
  where the compiler's output does not decide a construct, the source's form is a reconstruction, and the
  names are mostly descriptive names chosen here ([docs/names.md](docs/names.md) says which names are the
  authors' and how each was proven).
- **Only the executables are rebuilt.** Everything else on the disc (models, motion, stages, textures,
  sound, movies) is data the game loads and is not part of this tree; [docs/formats.md](docs/formats.md)
  describes it.

## The build

This repository holds no file of the game or of Sony's development kit, and no build scripts. It states the
build as facts: each tool by name, version and SHA-256, the options each part is compiled and assembled with,
the link order, the kit's headers by SHA-256, and each executable's SHA-256
([docs/building.md](docs/building.md) and the tables in `config/bushidoblade/`). With exactly those tools, this
source was built into all three executables, and each is identical byte for byte to its retail one.

To do the same, a reader needs their own disc image matching a SHA-256 above and the period tools: SN Systems'
PsyQ preprocessor, compilers, assemblers, GTE post-processor and linker, and Sony's Runtime Library 3.6
headers. One array the source includes is not in the tree: the loading-screen picture, which
`src/font_hud.c` includes from the reader's own executable (`config/bushidoblade/assets.tsv` gives its address,
size and SHA-256 in each build). Several of the tools are DOS programs of 1994 and 1995; how to run them is
the reader's choice.

## Layout

| Path | Holds |
| --- | --- |
| `src/*.c`, `src/*.s` | The game's own code: 97 C units and 5 hand-written assembly units, one per object of the original link, each named for what it holds, and `src/globals.c`, which defines the uninitialized globals no other unit defines. |
| `src/lib/<library>/` | Sony's Runtime Library, decompiled, one file per module, under the directories Sony's objects record (`api`, `c2`, `card`, `cd`, `etc`, `gpu`, `gte`, `press`, `snd`, `spu`; LIBCOMB's and LIBSN's record none, and sit in `comb` and `libsn`), with a header beside the modules when only they include it. Each `<LIB>.LST` lists its library's members in archive order. |
| `include/` | The game's headers: its records, constants and declarations; and the library's headers that modules of several directories, or the game, include. |
| `config/bushidoblade/*.tsv`, `headers.sha256` | The build as tables: the tools by name, version and SHA-256 (`tools.tsv`), each part's options (`flags.tsv`), each executable with its define and SHA-256 (`builds.tsv`), the libraries and their member lists (`libraries.tsv`), the loading-screen picture each executable embeds (`assets.tsv`), and the kit's headers by SHA-256 (`headers.sha256`). |
| `config/bushidoblade/SCUS.LNK` | The linker command file all three builds share: the objects in the original's order, then the libraries in the order its linker read them. |
| `config/bushidoblade/commons.tsv` | The declared order of uninitialized globals, the one link input that is not source. |
| `docs/` | The documents below. |
| `assets/` | Cover art; the progress images live on the `pages` branch. |

Where the builds differ, the source says so with `#ifdef VERSION_JP` (Japan) or `VERSION_EU` (Europe); the
North American build is the unconditioned text.

## Documents

- [docs/building.md](docs/building.md): the toolchain, how each tool was identified, the library recipes, the
  declared commons order, and the build.
- [docs/program-map.md](docs/program-map.md): how the game is organised, how a frame runs, its main records,
  and where to find things in the source.
- [docs/formats.md](docs/formats.md): the files on the disc and the formats the game reads, with the routine
  that reads each.
- [docs/names.md](docs/names.md): which names are the authors' own and how each was proven; how the rest were
  chosen.
- [docs/history.md](docs/history.md): how the Japanese, North American and European builds differ.
- [docs/kept-constructs.md](docs/kept-constructs.md): the few constructs the source keeps in an unusual form because the
  original's bytes need it, by file.
- [docs/systems/](docs/systems/): one document per system. The game engine's: [the frame](docs/systems/frame.md),
  [rendering](docs/systems/rendering.md), [models and animation](docs/systems/models.md),
  [cloth](docs/systems/cloth.md), [lighting and fog](docs/systems/lighting.md), [the camera](docs/systems/camera.md),
  [stages](docs/systems/stages.md), [audio](docs/systems/audio.md),
  [files and memory](docs/systems/files-and-memory.md), [the memory card](docs/systems/memory-card.md),
  [numbers and units](docs/systems/fixed-point.md),
  [effects](docs/systems/effects.md), [weather](docs/systems/weather.md), [video memory](docs/systems/vram.md). The
  game's:
  [the fighter](docs/systems/fighter.md), [input](docs/systems/input.md), [combat](docs/systems/combat.md),
  [frame and combat events](docs/systems/combat-events.md),
  [weapons](docs/systems/weapons.md), [the computer opponent](docs/systems/ai.md),
  [game flow](docs/systems/game-flow.md), [story mode and honour](docs/systems/story.md),
  [the menus](docs/systems/menus.md), [character select](docs/systems/character-select.md),
  [the opening](docs/systems/opening.md), [story presentation](docs/systems/story-presentation.md),
  [movies](docs/systems/movies.md),
  [the rules of a match](docs/systems/match-rules.md). Both: the
  [oddities of the original program](docs/systems/oddities.md).

## Licence

Original game: **Bushido Blade** © 1997 Square / Lightweight, published by Sony Computer Entertainment. No
original game files are in this repository.

The material authored here — reconstructed source text, names, comments, documentation and build
configuration — is licensed under both the Viral Public License ([LICENSE-VPL](LICENSE-VPL)) and the Source
Seppuku License ([LICENSE-SSL](LICENSE-SSL)). That grant covers only what is authored here; the original game
and Sony's PlayStation SDK remain their respective owners'.
