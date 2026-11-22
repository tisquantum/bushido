# The three builds

Each executable prints its version at boot and carries the time it was built (`src/main.c`, `src/movie.c`):

| Build | Banner | Built |
| --- | --- | --- |
| Japan, SLPM-86020 | `BUSHIDO BLADE VER.1.1` | Thu Feb 13 17:40:18 1997 |
| North America, SCUS-94180 | `BUSHIDO BLADE VER.1.1c` | Thu May 29 16:48:29 1997 |
| Europe, SCES-00899 | `BUSHIDO BLADE VER.1.1d`, `EUROPE VERSION` | Thu Jun 19 02:22:48 1997 |

The Japanese release came first. The North American build is the game localised for English, and the European
build is the North American one made for PAL televisions. One source tree builds all three: the North American
build is the plain text, and `#ifdef VERSION_JP` or `VERSION_EU` marks where another build differs.

## How much differs

Compared routine by routine, each routine paired with the one the other build compiles from the same source, and
with addresses set aside (two routines are the same when their bytes agree once relocation is undone):

- **Japan against North America:** of the North American build's 983 routines, 783 are the same in Japan, 183
  differ and 17 have no counterpart; Japan has 39 routines the North American build lacks.
- **Europe against North America:** 975 are the same and 8 differ; neither build has a routine the other lacks.

Many of the 183 differ only in a record's layout. The fighter record is 808 bytes outside Japan and 792 in it (a
fighter carries up to 16 thrown sub-weapons outside Japan and 11 in it, `include/fighter.h`), and every routine
that reaches a field past the change differs in its offsets.

## What only the Japanese build has

- **Analog controller support.** The Analog Controller menu (`src/analog_menu.c`: rumble on or off and a stick test
  for both pads) and the stick handling of `src/pad.c` (`IsPadAnalog`, `CalibratePadSticks`,
  `CapturePadStickCentres`, `StickToDpadButtons`, `StickToFaceButtons`) exist only here.
- **Routines the later builds do not have,** among them four more rotation-matrix builders, a ground
  projection and two double-sided polygon drawers in `src/rot_matrix.c`, mesh scaling, flat-primitive and
  billboard drawers and screen-depth helpers in `src/model_table.c`, and packed sine and cosine lookups in
  `src/render.s`.
- **Movies decoded at 24 bits,** straight to the screen (`STR/` on its disc).

## What only the North American and European builds have

- **Subtitles.** All of `src/subtitle.c`: English captions for the story cutscenes, prologues, endings and movies,
  and the fighters' taunts. The discs carry the subtitle banks the Japanese disc lacks.
- **A new movie player.** The movies were re-encoded at 16 bits (`STR16/`); each frame is decoded into memory
  and drawn as 80 textured strips across a 640 by 240 screen (`StartMoviePlayback`, `BuildMovieTileQuads`,
  `UploadMovieFrameSlices` in `src/movie.c`), so the subtitles can be drawn over it
  ([movies.md](systems/movies.md)).
- **The publisher's splash screen** at boot (`PlayBootSplashScreen`, `src/splash.c`): one screen in North
  America, two PAL screens in Europe.
- **Routines of their own for the pause, demo and replay overlays** (`ShowPauseOverlay`, `DrawDemoOverlayText`,
  `DrawReplayOverlayText` in `src/font_hud.c`), which have no counterpart in the Japanese build.
- **The confirm button.** Menus confirm with Circle and cancel with Cross in Japan, the reverse elsewhere
  (`include/pad.h`).

## What the European build changes

- **PAL video.** `main` sets the PAL mode before anything else, and the 240 lines of the picture are shown 28 lines
  down the taller PAL screen (`include/display_buffer.h`).
- **Timing.** The credits' scroll, the prologue's durations and the opening's delays, overlay hold and title fade
  are retimed for 25 frames (50 fields) a second (`src/credits.c`, `src/story_slideshow.c`, `src/opening_demo.c`,
  `src/opening_overlay.c`). The subtitles the executable holds (`src/subtitle.c`) keep the same frames in both
  builds.
- **The splash screens** and the boot banner, and the memory card file name.

The eight routines that differ from the North American build are `main`, `InitSystem`, `PlayBootSplashScreen`,
`UpdateOpeningBannerPackets`, `SetupMovieWorkspace`, `StartMoviePlayback`, `RunMoviePlaybackLoop` and
`FetchNextMovieFrame`.

## Saves

Each build names its memory card file by its own serial: `BISLPM-86020BUSHIDO1`, `BASCUS-94180BUSHIDO1`,
`BESCES-00899BUSHIDO1` (`src/memcard.c`). The Japanese save's icon cycles through its three frames (format
0x13), the others show the first alone (0x11), and Japan's save titles are in Japanese
([files-and-memory.md](systems/files-and-memory.md) gives the save's format).
