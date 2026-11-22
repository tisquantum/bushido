# Movies

The game has thirteen movies: the opening (movie 0) and the twelve endings, the normal ending of character class
`c` as movie `c + 1` and its true ending as `c + 7` ([story.md](story.md)). They are STR files, category 5 of the
disc's file table: MDEC-compressed video sectors interleaved with XA sound ([formats.md](../formats.md)). The
player is `src/movie.c`, built two ways: the Japanese build decodes 24-bit frames straight into its display area,
the North American and European builds decode 16-bit frames into memory and draw them as textured tiles, so that
subtitles can be drawn over them ([history.md](../history.md)). The movie theatre, the one place the opening movie
plays, is in `src/story_state.c` and `src/theater_ui.c`.

## Playing a movie

`PlayFmvMovie(movieId)` (`src/story_state.c`) is the whole of it, called by the ending state and the theatre. It
saves the display buffers' environments, turns on CD audio to the sound processor (`EnableCdSpuAudio`), plays the
movie and restores the environments. Outside Japan it first starts the movie's subtitles if it has any
(`gMovieSubtitleTracks`: none for the opening, the normal endings on tracks 0, 2, ... 10 and the true endings on 1,
3, ... 11, added to subtitle id 60), and resets the CD library (`CdInit`). The player runs in four calls:

1. `PrepareMoviePlayback(movieId, workspace)` lays out the workspace at the dynamic load area, 0x8014C000
   (`SetupMovieWorkspace`): the decoder's state record, a ring of 32 CD sectors, two buffers of run-length codes
   (0x28000 bytes each; the second 0x50000 in Japan), two image buffers and, outside Japan, the 80 tiles, two
   4-entry ordering tables and two display environments. It finds the movie's file and sets the XA mix to the
   movie's volume (`gMovieCdVolumes`: 127 to 255 by movie).
2. `StartMoviePlayback` resets the MDEC with `AdvanceMovieSlice` as its output callback, gives the streaming
   library the ring (`StSetRing`, `StSetStream`), seeks and starts a streaming read at double speed
   (`StartMovieCdStream`), and decodes the first frame's codes. A failed start skips the movie.
3. `RunMoviePlaybackLoop` runs until the movie ends, cancel is pressed on either pad, or the stream fails.
4. On the way out the ring is released, the drive paused, after an error the drive put on standby until it reports
   no error, and `CleanupMoviePlayback` blanks the display, clears the callbacks, puts back `RumbleVSyncCallback`
   and mutes the CD mix.

Each frame of the loop follows the same pipeline. `DecodeNextMovieVlc` takes the next complete frame from the ring
(`FetchNextMovieFrame`, which retries `StGetNext` up to 0x10000000 times, 0x100000 in Japan, before giving up) and
unpacks its codes (`DecDCTvlc`) into one code buffer, then frees the ring sectors. Meanwhile the MDEC turns the
other code buffer into pixels (`DecDCTin`, `DecDCTout`), and its callback, `AdvanceMovieSlice`, moves the output
on, raising `frameReady` when the frame is complete; `WaitMovieFrameReady` waits for that flag. A movie ends when
the frame number in the sector header reaches its last frame: 3,000 for the opening, 990 to 2,131 for the endings
(`gMovieMaxFrames`; in Japan the `endFrame` of `gMovieStreams`, the same numbers). Two values the player never sets
and, outside Japan, a time-out that does not end the wait are in [oddities](oddities.md).

## The North American and European player

The MDEC decodes each frame whole, 320 by 240 at 16 bits, into one of two image buffers, which it writes as twenty
columns 16 pixels wide. While it decodes the next, `UploadMovieFrameSlices` loads the twenty columns of the
previous frame into VRAM at (640, 0) to (959, 239), beside the display area. The picture is 80 `POLY_FT4` tiles
(`BuildMovieTileQuads`): ten across and eight down, each 32 texels wide and 32 high (16 on the last row), drawn 64
pixels wide on the 640 by 240 screen, so the movie is doubled horizontally. The frame spans two texture pages.

The loop draws into the two halves of VRAM in turn (draw areas at Y 0 and 240, each shown while the other is
drawn) with its own 4-entry ordering tables. Per frame: decode, wait until root counter 1 has counted 0x180
horizontal blanks (`FRAME_PACING_HBLANKS`; 30 frames a second on NTSC, 25 on PAL), wait for the vertical blank
and the GPU, poll the controllers, put the subtitles in the last ordering-table entry so they draw over the
tiles (`UpdateAndDrawSubtitle`, [subtitles](story-presentation.md)), and draw. Movie 5 (Red Shadow's normal
ending) mutes the CD mix 30 frames before its end.

The European build adds a check of the stream: if a frame's number is lower than the last one or more than 20 ahead
of it, playback stops as an error. Its display environments are moved down by `DISPLAY_SCREEN_Y` for the PAL
screen.

## The Japanese player

`gMovieStreams` gives each movie a start and end frame and a frame rectangle of 480 by 240 VRAM halfwords, which is
320 pixels at 24 bits (a pixel takes one and a half halfwords). The two frames are kept in VRAM at (0, 0) and (480,
240), in the display area itself. The MDEC decodes 16-pixel slices at 24 bits into two small image buffers
(`imgbuf0`, `imgbuf1`, 16 by 240 pixels each), and `AdvanceMovieSlice` loads each finished slice straight to its
place in the frame being built and, when the frame is complete, switches to the other. `RunMoviePlaybackLoop` then
points the display at the finished frame (`SetDefDispEnv` with `isrgb24` set and the width taken as two thirds),
waiting only for the vertical blank: there are no ordering tables, no tiles and no subtitles. The callback also
services the CD (`StCdInterrupt`) when the streaming library has flagged it.

## The movie theatre

States 49 and 50 (`InitMovieTheaterState`, `UpdateMovieTheaterState`). `LoadMovieTheaterAssets`
(`src/theater_ui.c`) reads `DATA3/O14.DAT` into the model arena and uploads its textures; outside Japan the file
also holds the movie subtitle bank, which it relocates. `DrawMovieTheaterMenu` draws the header, the opening's and
endings' labels, six portraits and, for a character, its name and two ending rows, each the ending's title or a
locked bar, over a backdrop that repeats a 32 by 16 texel pattern through a texture window.

The cursor has seven places: the opening, then the characters in the order of `gMovieTheaterSlotToCharacter`, Red
Shadow, Tatsumi, Mikado, Black Lotus, Utsusemi and Kannuki. Left and right move it, skipping characters with
neither ending seen (`gUnlockedMoviesMask`). A portrait is bright under the cursor, dimmer when an ending has been
seen, darkest when none has. Confirm plays the opening or the normal ending; square (triangle in Japan) plays the
true ending; a movie not seen plays a refusal sound. Cancel leaves for the main menu, and so does triangle outside
Japan. A chosen movie fades the screen out over 30 frames with a subtractive quad growing by 8 a frame, plays, and
the theatre reloads its art and the menu sounds.

An ending reached in Story mode is played by `PlayStoryEndingMovieState` (state 29), which marks the ending in
`gUnlockedMoviesMask`, loads the movie subtitle bank from `DATA3/O20.DAT` outside Japan, plays the movie and goes
to the credits.
