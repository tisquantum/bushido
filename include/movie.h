#ifndef MOVIE_H
#define MOVIE_H

#include <sys/types.h>
#include <libcd.h>
#include <libgte.h>
#include <libgpu.h>

#define MOVIE_OT_LEN 4
#define MOVIE_TILE_COUNT 80
#define MOVIE_RING_SECTORS 32

#define MOVIE_WIDTH 320
#define MOVIE_HEIGHT 240
#define MOVIE_DISPLAY_WIDTH 640
#define MOVIE_DISPLAY_HEIGHT 240
#define MOVIE_VRAM_X 640
#define MOVIE_SLICE_WIDTH 16
#define MOVIE_SLICE_COUNT (MOVIE_WIDTH / MOVIE_SLICE_WIDTH)
#define MOVIE_VLC_BUF_BYTES 0x28000
#define MOVIE_JP_VLC_SECOND_BYTES 0x50000

typedef enum MoviePlaybackState {
    MOVIE_PLAYBACK_STOPPED = 0,
    MOVIE_PLAYBACK_PLAYING = 1,
    MOVIE_PLAYBACK_ERROR = 2
} MoviePlaybackState;

/* StSetStream's mode. */
typedef enum MovieStreamMode {
    MOVIE_STREAM_MODE_16BIT = 0,
    MOVIE_STREAM_MODE_24BIT = 1
} MovieStreamMode;

typedef struct MovieDisplayEnv {
    DRAWENV draw;
    DISPENV disp;
} MovieDisplayEnv;

/* The Japanese build's decoder state. Horizontal units are VRAM halfwords: a 24-bit pixel takes
 * one and a half. */
typedef struct MovieDecodeEnv {
    u_long *vlcBuffers[2];
    int vlcIndex;
    u_short *imageBuffers[2]; /* one 16-pixel slice each */
    int imageIndex;
    RECT frameRects[2];
    int frameIndex;
    RECT slice;
    int frameReady;
    int streamMode;
} MovieDecodeEnv;

/* The USA and Europe builds' decoder state, which decodes whole 16-bit frames into RAM and draws
 * them as tiles. */
typedef struct MovieState {
    int vlcIndex;
    int imageIndex;
    RECT frameRects[2];
    int frameIndex;
    RECT sliceRects[2];
    int frameReady;
    int streamMode;
    u_long *vlcBuffers[2];
    u_short *imageBuffers[2];
    POLY_FT4 *tiles;
    u_long *orderingTables[2];
} MovieState;

/* One movie of the Japanese build's table; horizontal units are VRAM halfwords. */
typedef struct MovieStream {
    short startFrame;
    short endFrame;
    short x;
    short y;
    short width;
    short height;
} MovieStream;

int PrepareMoviePlayback(int movieId, u_char *workspace);
int StartMoviePlayback(void);
#ifdef VERSION_JP
int RunMoviePlaybackLoop(DISPENV *disp);
#else
int RunMoviePlaybackLoop(void);
#endif
void CleanupMoviePlayback(void);

#ifndef VERSION_JP
void BuildMovieTileQuads(void);
void InitMovieDecodeEnv(void);
int InitMovieDecoderAndStream(CdlLOC *location, void (*callback)(void));
void AdvanceMovieSlice(void);
void DecodeNextMovieVlc(void);
void EnqueueMovieTileQuads(void);
void UploadMovieFrameSlices(void);
#endif

#endif
