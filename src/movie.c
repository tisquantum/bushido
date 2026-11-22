/*
 * Movie playback: streams STR movies from CD through the MDEC. There are two players: the USA and
 * Europe builds decode each 16-bit frame whole and draw it as 80 tiles scaled to 640x240, under the
 * subtitles; the Japanese build decodes 24-bit slices straight into its display pages.
 */
#include <sys/types.h>
#include <libcd.h>
#include <libgte.h>
#include <libgpu.h>
#include <libpress.h>
#include <libetc.h>

#include "main.h"
#include "pad.h"
#ifndef VERSION_JP
#include <kernel.h>
#endif
#include "cd.h"
#include "display_buffer.h"
#include "movie.h"
#include "subtitle.h"
#include "theater_ui.h"

/* DecDCTin's mode for each output depth. */
#define MOVIE_DCT_MODE_16BIT 2
#define MOVIE_DCT_MODE_24BIT 3

#define ROUND_UP_16(n) ((((n) - 1) / 16 + 1) * 16)

#define MOVIE_DECODE_WAIT_LIMIT 0x800000

u_short *imgbuf0;
u_short *imgbuf1;
u_long *vlcbuf0;
u_long *vlcbuf1;
u_long *Ring_Buff;
#ifdef VERSION_JP
/* The Japanese player's decoder. Routines named alike in the two players are each player's own;
 * five take different arguments. */
MovieDecodeEnv *dec;
#else
MovieState *dec;
MovieDisplayEnv *gMovieDisplayEnv0;
MovieDisplayEnv *gMovieDisplayEnv1;
MovieDisplayEnv *gMovieDisplayEnvs[2];
u_long *ot0;
u_long *ot1;
POLY_FT4 *gMovieTileQuads;
#ifdef VERSION_EU
StHEADER *gMovieHeader;
int gMovieFrameNumber;
int gMovieLastFrameNumber;
#endif
/* Written only by the Europe build; nothing reads it. */
static u_long *gMovieSectorData;
#endif

/* The workspace holds, in order: the decoder state, the CD ring, two VLC buffers, two image
 * buffers and, outside Japan, the tiles, two ordering tables and two display environments. */
static void SetupMovieWorkspace(u_char *workspace)
{
    u_char *ringBuffer;
    u_char *vlcPrimary;
    u_char *vlcSecondary;
    u_char *imagePrimary;
#ifndef VERSION_JP
    u_char *imageSecondary;
    POLY_FT4 *tiles;
#endif

#ifdef VERSION_JP
    dec = (MovieDecodeEnv *)workspace;
    ringBuffer = workspace + 900;
#else
    dec = (MovieState *)workspace;
    ringBuffer = workspace + sizeof(MovieState);
#endif
    Ring_Buff = (u_long *)ringBuffer;
    vlcPrimary = ringBuffer + MOVIE_RING_SECTORS * CD_SECTOR_SIZE;
    vlcbuf0 = (u_long *)vlcPrimary;
    vlcSecondary = vlcPrimary + MOVIE_VLC_BUF_BYTES;
    vlcbuf1 = (u_long *)vlcSecondary;
#ifdef VERSION_JP
    imagePrimary = vlcSecondary + MOVIE_JP_VLC_SECOND_BYTES;
    imgbuf0 = (u_short *)imagePrimary;
    imgbuf1 = (u_short *)(imagePrimary + MOVIE_SLICE_WIDTH * MOVIE_HEIGHT * 3);
#else
    imagePrimary = vlcSecondary + MOVIE_VLC_BUF_BYTES;
    imgbuf0 = (u_short *)imagePrimary;
    imageSecondary = imagePrimary + MOVIE_WIDTH * MOVIE_HEIGHT * 2;
    imgbuf1 = (u_short *)imageSecondary;
    tiles = (POLY_FT4 *)(imageSecondary + MOVIE_WIDTH * MOVIE_HEIGHT * 2);
    ot0 = (u_long *)(tiles + MOVIE_TILE_COUNT);
    ot1 = (u_long *)(tiles + MOVIE_TILE_COUNT) + MOVIE_OT_LEN;
    gMovieTileQuads = tiles;
    gMovieDisplayEnv0 = (MovieDisplayEnv *)((u_long *)(tiles + MOVIE_TILE_COUNT) + MOVIE_OT_LEN * 2);
    gMovieDisplayEnv1 = (MovieDisplayEnv *)((u_long *)(tiles + MOVIE_TILE_COUNT) + MOVIE_OT_LEN * 2) + 1;
#ifdef VERSION_EU
    gMovieSectorData = (u_long *)(gMovieHeader + 1);
#endif
#endif
}

static int gMovieFirstSlice;
static int gMoviePlaybackState;

/* XA volume per movie: 0 the opening, 1 to 12 the endings. */
u_char gMovieCdVolumes[16] = {
    127, 159, 175, 175, 191, 255, 127, 159, 127, 159, 159, 127, 159, 0, 0, 0,
};
#ifdef VERSION_JP
MovieStream gMovieStreams[13] = {
    { 1, 3000, 0, 0, 480, 240 },
    { 1, 2131, 0, 0, 480, 240 },
    { 1, 2060, 0, 0, 480, 240 },
    { 1, 2040, 0, 0, 480, 240 },
    { 1, 2040, 0, 0, 480, 240 },
    { 1, 2060, 0, 0, 480, 240 },
    { 1, 2020, 0, 0, 480, 240 },
    { 1, 1030, 0, 0, 480, 240 },
    { 1, 1170, 0, 0, 480, 240 },
    { 1, 1140, 0, 0, 480, 240 },
    { 1, 990, 0, 0, 480, 240 },
    { 1, 990, 0, 0, 480, 240 },
    { 1, 1500, 0, 0, 480, 240 },
};
#else
/* Each movie's last frame. */
short gMovieMaxFrames[14] = {
    3000, 2131, 2060, 2040, 2040, 2060, 2020, 1030, 1170, 1140, 990, 990, 1500, 0,
};
#endif

int PrepareMoviePlayback(int movieId, u_char *workspace)
{
    u_char volume;

    SetupMovieWorkspace(workspace);
    gCdDrive.movie.movieId = movieId;
    gCdDrive.movie.file = &gCdFileTable[ResolveCdFileHandle(CD_CATEGORY_MOVIE, movieId)];
    gCdDrive.stream.state = CD_STREAM_IDLE;
    volume = gMovieCdVolumes[movieId];
    SetCdAudioMix(volume, 0, volume, 0);
    return 1;
}

/* libcd's, which no kit header declares. */
extern u_long StCdIntrFlag;

#ifndef VERSION_JP
int StartMoviePlayback(void)
{
    int i;
    CdlLOC *location;
    RECT screen;

    SetDispMask(0);
    for (i = MOVIE_RING_SECTORS * (CD_SECTOR_SIZE / 4) - 1; i >= 0; --i) {
        Ring_Buff[i] = 0;
    }
    for (i = 0; i < (MOVIE_VLC_BUF_BYTES * 2) / 4; ++i) {
        vlcbuf0[i] = 0;
    }
    for (i = 0; i < MOVIE_WIDTH * MOVIE_HEIGHT * 2; ++i) {
        imgbuf0[i] = 0;
    }
    location = &gCdDrive.movie.file->location;
    gMovieFirstSlice = 1;
    InitMovieDecodeEnv();
    if (InitMovieDecoderAndStream(location, AdvanceMovieSlice) == 0) {
        DecDCToutCallback(0);
        StUnSetRing();
        StClearRing();
        return 0;
    }

    BuildMovieTileQuads();
    for (i = 0; i < 2; ++i) {
        gCurrentOt = (dec->orderingTables[i] = ot0 + i * MOVIE_OT_LEN);
        ClearOTag(gCurrentOt, MOVIE_OT_LEN);
    }
    EnqueueMovieTileQuads();
    gMovieDisplayEnvs[0] = gMovieDisplayEnv0;
    gMovieDisplayEnvs[1] = gMovieDisplayEnv1;
    SetDefDrawEnv(&gMovieDisplayEnvs[0]->draw, 0, 0, MOVIE_DISPLAY_WIDTH, MOVIE_DISPLAY_HEIGHT);
    SetDefDrawEnv(&gMovieDisplayEnvs[1]->draw, 0, MOVIE_DISPLAY_HEIGHT, MOVIE_DISPLAY_WIDTH, MOVIE_DISPLAY_HEIGHT);
    SetDefDispEnv(&gMovieDisplayEnvs[0]->disp, 0, MOVIE_DISPLAY_HEIGHT, MOVIE_DISPLAY_WIDTH, MOVIE_DISPLAY_HEIGHT);
    SetDefDispEnv(&gMovieDisplayEnvs[1]->disp, 0, 0, MOVIE_DISPLAY_WIDTH, MOVIE_DISPLAY_HEIGHT);
#ifdef VERSION_EU
    gMovieDisplayEnvs[1]->disp.screen.x = gMovieDisplayEnvs[0]->disp.screen.x = 0;
    gMovieDisplayEnvs[1]->disp.screen.y = gMovieDisplayEnvs[0]->disp.screen.y = DISPLAY_SCREEN_Y;
#endif
    screen.w = MOVIE_DISPLAY_WIDTH;
    screen.x = 0;
    screen.y = 0;
    screen.h = MOVIE_DISPLAY_HEIGHT * 2;
    ClearImage(&screen, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);
    DecodeNextMovieVlc();
    gMoviePlaybackState = MOVIE_PLAYBACK_PLAYING;
    return 1;
}

void BuildMovieTileQuads(void)
{
    POLY_FT4 *tile;
    int u, v;
    int right, bottomRight;
    int bottomTop, bottomEnd;
    int frameRectIndex; /* never set: the tiles' texture pages come from frameRects at whatever index it holds */

    tile = (dec->tiles = gMovieTileQuads);
    for (v = 0; v < 224; v += 32) {
        int bottom;

        u = 0;
        bottom = v + 32;
        for (right = 64; u < 320; right += 64, u += 32) {
            SetPolyFT4(tile);
            setXY4(tile, 2 * u, v, right, v,
                   2 * u, bottom, right, bottom);
            setUV4(tile, u % 256, v, u % 256 + 31, v,
                   u % 256, bottom, u % 256 + 31, bottom);
            tile->tpage = GetTPage(2, 0,
                dec->frameRects[frameRectIndex].x + u / 256 * 256,
                dec->frameRects[frameRectIndex].y);
            SetShadeTex(tile, 1);
            tile++;
        }
    }
    u = 0;
    bottomTop = 224;
    bottomEnd = 240;
    bottomRight = 64;
    for (; u < 320; bottomRight += 64, u += 32) {
        SetPolyFT4(tile);
        setXY4(tile, 2 * u, bottomTop, bottomRight, bottomTop,
               2 * u, bottomEnd, bottomRight, bottomEnd);
        setUV4(tile, u % 256, bottomTop, u % 256 + 31, bottomTop,
               u % 256, bottomEnd, u % 256 + 31, bottomEnd);
        tile->tpage = GetTPage(2, 0,
            dec->frameRects[frameRectIndex].x + u / 256 * 256,
            dec->frameRects[frameRectIndex].y);
        SetShadeTex(tile, 1);
        tile++;
    }
}

#endif

#ifdef VERSION_JP
void InitMovieDecodeEnv(MovieDecodeEnv *env, int x0, int y0, int x1, int y1, MovieStream *movie)
{
    env->vlcIndex = 0;
    env->imageIndex = 0;
    env->frameIndex = 0;
    env->frameReady = 0;
    env->frameRects[0].x = x0;
    env->frameRects[0].y = y0;
    env->frameRects[1].x = x1;
    env->slice.w = MOVIE_SLICE_WIDTH * 3 / 2;
    env->vlcBuffers[0] = vlcbuf0;
    env->vlcBuffers[1] = vlcbuf1;
    env->imageBuffers[0] = imgbuf0;
    env->imageBuffers[1] = imgbuf1;
    env->streamMode = MOVIE_STREAM_MODE_24BIT;
    env->frameRects[1].y = y1;
    /* always 0; the original still tests it */
    if (env->frameIndex == 0) {
        env->slice.x = x0;
        env->slice.y = y0;
    } else {
        env->slice.x = x1;
        env->slice.y = y1;
    }
}
#else
void InitMovieDecodeEnv(void)
{
    MovieState *state;

    state = dec;
    state->vlcBuffers[0] = vlcbuf0;
    state->frameRects[0].x = MOVIE_VRAM_X;
    state->frameRects[1].x = MOVIE_VRAM_X;
    state->sliceRects[0].x = MOVIE_VRAM_X;
    state->vlcBuffers[1] = vlcbuf1;
    state->vlcIndex = 0;
    state->imageBuffers[0] = imgbuf0;
    state->imageBuffers[1] = imgbuf1;
    state->imageIndex = 0;
    state->frameRects[0].y = 0;
    state->frameRects[1].y = 0;
    state->frameIndex = 0;
    state->sliceRects[0].y = 0;
    state->sliceRects[0].w = MOVIE_WIDTH;
    state->streamMode = MOVIE_STREAM_MODE_16BIT;
    state->frameReady = 0;
}
#endif

static int StartMovieCdStream(CdlLOC *location)
{
    if (CdControlB(CdlSetloc, (u_char *)location, 0) != 0) {
        return CdRead2(CdlModeStream | CdlModeSpeed | CdlModeRT) != 0;
    } else {
        return 0;
    }
}

#ifdef VERSION_JP
int InitMovieDecoderAndStream(CdlLOC *location, void (*callback)(void), MovieStream *movie)
#else
int InitMovieDecoderAndStream(CdlLOC *location, void (*callback)(void))
#endif
{
    DecDCTReset(0);
    DecDCToutCallback(callback);
    StSetRing(Ring_Buff, MOVIE_RING_SECTORS);
#ifdef VERSION_JP
    StSetStream(MOVIE_STREAM_MODE_24BIT, movie->startFrame, -1, 0, 0);
#else
    StSetStream(dec->streamMode, 1, -1, 0, 0);
#endif
    return StartMovieCdStream(location);
}

/* The build's date and time; nothing reads it. */
#if defined(VERSION_JP)
char *gBuildTimestamp = " \nThu Feb 13 17:40:18 1997\n";
#elif defined(VERSION_EU)
char *gBuildTimestamp = " \nThu Jun 19 02:22:48 1997\n";
#else
char *gBuildTimestamp = " \nThu May 29 16:48:29 1997\n";
#endif
int gMovieFrameWidth = 0;
int gMovieFrameHeight = 0;

#ifdef VERSION_JP
void AdvanceMovieSlice(void)
{
    RECT decoded;
    int imageIndex;
    int remainingWidth;

    if (StCdIntrFlag) {
        StCdInterrupt();
        StCdIntrFlag = 0;
    }
    imageIndex = dec->imageIndex;
    decoded = dec->slice;
    dec->imageIndex = !dec->imageIndex;
    if (gMovieFirstSlice && (remainingWidth = dec->frameRects[dec->frameIndex].w % dec->slice.w) != 0) {
        gMovieFirstSlice = 0;
        dec->slice.x += remainingWidth;
    } else {
        dec->slice.x += dec->slice.w;
    }

    if (dec->slice.x < dec->frameRects[dec->frameIndex].x + dec->frameRects[dec->frameIndex].w) {
        DecDCTout((u_long *)dec->imageBuffers[dec->imageIndex], dec->slice.w * ROUND_UP_16(dec->slice.h) / 2);
    } else {
        dec->frameReady = 1;
        dec->frameIndex = !dec->frameIndex;
        dec->slice.x = dec->frameRects[dec->frameIndex].x;
        dec->slice.y = dec->frameRects[dec->frameIndex].y;
        gMovieFirstSlice = 1;
    }
    LoadImage(&decoded, (u_long *)dec->imageBuffers[imageIndex]);
}
#else
void AdvanceMovieSlice(void)
{
    int remainingWidth;

    DecDCToutSync(0);
    dec->imageIndex = !dec->imageIndex;
    if (gMovieFirstSlice && (remainingWidth = dec->frameRects[dec->frameIndex].w % dec->sliceRects[0].w) != 0) {
        gMovieFirstSlice = 0;
        dec->sliceRects[0].x += remainingWidth;
    } else {
        dec->sliceRects[0].x += dec->sliceRects[0].w;
    }

    if (dec->sliceRects[0].x < dec->frameRects[dec->frameIndex].x + dec->frameRects[dec->frameIndex].w) {
        DecDCTout((u_long *)(dec->imageBuffers[dec->imageIndex] + dec->sliceRects[0].x * dec->sliceRects[0].h),
                  dec->sliceRects[0].w * dec->sliceRects[0].h / 2);
    } else {
        dec->frameReady = 1;
        dec->frameIndex = !dec->frameIndex;
        dec->sliceRects[0].x = dec->frameRects[dec->frameIndex].x;
        dec->sliceRects[0].y = dec->frameRects[dec->frameIndex].y;
        gMovieFirstSlice = 1;
    }
}
#endif

#ifdef VERSION_JP
static u_long *FetchNextMovieFrame(MovieDecodeEnv *env, MovieStream *movie)
{
    u_long *frame;
    StHEADER *header;
    int retries;

    retries = 0x100000;
    while (StGetNext(&frame, (u_long **)&header) != 0) {
        if (--retries == 0) {
            return 0;
        }
    }

    if (header->frameCount >= movie->endFrame) {
        gMoviePlaybackState = MOVIE_PLAYBACK_STOPPED;
    }
    if (gMovieFrameWidth != header->width || gMovieFrameHeight != header->height) {
        RECT screen;

        screen.x = 0;
        screen.y = 0;
        screen.w = movie->width;
        screen.h = movie->height << 1;
        ClearImage(&screen, 0, 0, 0);
        gMovieFrameWidth = header->width;
        gMovieFrameHeight = header->height;
    }
    env->frameRects[0].w = env->frameRects[1].w = gMovieFrameWidth * 3 / 2;
    env->frameRects[0].h = env->frameRects[1].h = gMovieFrameHeight;
    env->slice.h = gMovieFrameHeight;
    return frame;
}
#else
static u_long *FetchNextMovieFrame(void)
{
    int retries;
    u_long *frame;
#ifndef VERSION_EU
    StHEADER *gMovieHeader;
#endif

    retries = 0x10000000;
    while (StGetNext(&frame, (u_long **)&gMovieHeader) != 0) {
        if (--retries == 0) {
            return 0;
        }
    }

#ifdef VERSION_EU
    gMovieFrameNumber = gMovieHeader->frameCount;
#endif
    /* Ending 5 falls silent 30 frames before its end. */
    if (gCdDrive.movie.movieId == 5 && gMovieHeader->frameCount >= (u_int)(gMovieMaxFrames[5] - 30)) {
        SetCdAudioMix(0, 0, 0, 0);
    }
    if (gMovieHeader->frameCount >= (u_int)gMovieMaxFrames[gCdDrive.movie.movieId]) {
        gMoviePlaybackState = MOVIE_PLAYBACK_STOPPED;
    }
    if (gMovieFrameWidth != gMovieHeader->width || gMovieFrameHeight != gMovieHeader->height) {
        gMovieFrameWidth = gMovieHeader->width;
        gMovieFrameHeight = gMovieHeader->height;
    }
    dec->frameRects[0].w = dec->frameRects[1].w = gMovieFrameWidth;
    dec->frameRects[0].h = dec->frameRects[1].h = gMovieFrameHeight;
    dec->sliceRects[0].h = dec->sliceRects[1].h = gMovieFrameHeight;
    return frame;
}
#endif

#ifdef VERSION_JP
void DecodeNextMovieVlc(MovieStream *movie)
{
    int retries;
    u_long *frame;

    retries = 1;
    while ((frame = FetchNextMovieFrame(dec, movie)) == 0) {
        if (--retries == 0) {
            gMoviePlaybackState = MOVIE_PLAYBACK_ERROR;
            return;
        }
    }
    dec->vlcIndex = !dec->vlcIndex;
    DecDCTvlc(frame, dec->vlcBuffers[dec->vlcIndex]);
    StFreeRing(frame);
}
#else
void DecodeNextMovieVlc(void)
{
    u_long *frame;

    frame = FetchNextMovieFrame();
    if (frame == 0) {
        gMoviePlaybackState = MOVIE_PLAYBACK_ERROR;
    } else {
        dec->vlcIndex = !dec->vlcIndex;
        DecDCTvlc(frame, dec->vlcBuffers[dec->vlcIndex]);
        StFreeRing(frame);
    }
}
#endif

static void WaitMovieFrameReady(int mode)
{
    volatile int timeout = MOVIE_DECODE_WAIT_LIMIT;

    while (dec->frameReady == 0) {
        if (--timeout == 0) {
#ifdef VERSION_JP
            dec->frameReady = 1;
            dec->frameIndex = !dec->frameIndex;
            dec->slice.x = dec->frameRects[dec->frameIndex].x;
            dec->slice.y = dec->frameRects[dec->frameIndex].y;
#else
            dec->frameIndex = !dec->frameIndex;
            dec->sliceRects[0].x = dec->frameRects[dec->frameIndex].x;
            dec->sliceRects[0].y = dec->frameRects[dec->frameIndex].y;
#endif
        }
    }
    dec->frameReady = 0;
}

#ifdef VERSION_JP
int StartMoviePlayback(void)
{
    MovieStream *movie;
    CdlLOC *location;
    RECT screen;

    movie = &gMovieStreams[gCdDrive.movie.movieId];
    location = &gCdDrive.movie.file->location;
    gMovieFirstSlice = 1;
    InitMovieDecodeEnv(dec, movie->x, movie->y, movie->x + movie->width, movie->y + movie->height, movie);
    if (InitMovieDecoderAndStream(location, AdvanceMovieSlice, movie) == 0) {
        DecDCToutCallback(0);
        StUnSetRing();
        StClearRing();
        return 0;
    }
    DecodeNextMovieVlc(movie);
    gMoviePlaybackState = MOVIE_PLAYBACK_PLAYING;
    SetDispMask(0);
    screen.x = 0;
    screen.y = 0;
    screen.w = movie->width;
    screen.h = movie->height << 1;
    ClearImage(&screen, 0, 0, 0);
    return 1;
}
#endif

#ifndef VERSION_JP
void EnqueueMovieTileQuads(void)
{
    POLY_FT4 *quads = dec->tiles;
    int i;

    for (i = 0; i < MOVIE_TILE_COUNT; ++i) {
        AddPrim(gCurrentOt, &quads[i]);
    }
}
#endif

void CleanupMoviePlayback(void)
{
    RECT screen; /* not used in Japan */

    SetDispMask(0);
#ifdef VERSION_JP
    WaitGpuIdle();
#else
    screen.w = MOVIE_DISPLAY_WIDTH;
    screen.x = 0;
    screen.y = 0;
    screen.h = MOVIE_DISPLAY_HEIGHT * 2;
    ClearImage(&screen, 0, 0, 0);
    DrawSync(0);
#endif
    ResetCallback();
    VSyncCallback(RumbleVSyncCallback);
    SetCdAudioMix(0, 0, 0, 0);
#ifndef VERSION_JP
    SetDispMask(1);
#endif
}

#ifndef VERSION_JP
static int DecodeMovieFrame(u_char *cdResult)
{
    if (CdSync(1, cdResult) == CdlDiskError) {
        gMoviePlaybackState = MOVIE_PLAYBACK_ERROR;
        return -1;
    }

    DecDCTin(dec->vlcBuffers[dec->vlcIndex], MOVIE_DCT_MODE_16BIT);
    DecDCTout((u_long *)dec->imageBuffers[dec->imageIndex],
              dec->sliceRects[0].w * ROUND_UP_16(dec->sliceRects[0].h) / 2);
    UploadMovieFrameSlices();
    DecodeNextMovieVlc();
    WaitMovieFrameReady(0);
    return 0;
}

void UploadMovieFrameSlices(void)
{
    int slice;
    int x;
    int width;
    int height;
    int offset;
    int image;
    RECT rect;

    slice = 0;
    x = 0;
    width = MOVIE_SLICE_WIDTH;
    height = MOVIE_HEIGHT;
    image = !dec->imageIndex;
    offset = 0;
    /* the original computes image * 4 on each pass; as for, while or do, it is computed once before the loop */
next_slice:
    rect.x = dec->sliceRects[0].x + x;
    rect.y = dec->sliceRects[0].y;
    rect.w = width;
    rect.h = height;
    x += MOVIE_SLICE_WIDTH;
    LoadImage(&rect, (u_long *)((char *)dec->imageBuffers[image] + offset));
    offset += MOVIE_SLICE_WIDTH * MOVIE_HEIGHT * sizeof(u_short);
    if (++slice < MOVIE_SLICE_COUNT) {
        goto next_slice;
    }
}
#endif

#ifdef VERSION_JP
int RunMoviePlaybackLoop(DISPENV *disp)
{
    MovieStream *movie;
    u_char *cdResult; /* never set: CdSync and CdReady copy the drive's 8-byte result to wherever it points */
    unsigned char unused[8]; /* not used */

    movie = &gMovieStreams[gCdDrive.movie.movieId];
    do {
        if (CdSync(1, cdResult) == CdlDiskError) {
            gMoviePlaybackState = MOVIE_PLAYBACK_ERROR;
            break;
        }
        DecDCTin(dec->vlcBuffers[dec->vlcIndex], MOVIE_DCT_MODE_24BIT);
        DecDCTout((u_long *)dec->imageBuffers[dec->imageIndex], dec->slice.w * ROUND_UP_16(dec->slice.h) / 2);
        DecodeNextMovieVlc(movie);
        WaitMovieFrameReady(0);
        if (gPadState.pressed & (_PAD(0, MENU_CANCEL) | _PAD(1, MENU_CANCEL))) {
            gMoviePlaybackState = MOVIE_PLAYBACK_STOPPED;
            break;
        }
        VSync(0);
        PollControllers();
        SetDefDispEnv(disp, dec->frameRects[!dec->frameIndex].x - movie->x,
                      dec->frameRects[!dec->frameIndex].y - movie->y, movie->width, movie->height);
        disp->isrgb24 = 1;
        disp->disp.w = disp->disp.w * 2 / 3;
        PutDispEnv(disp);
        SetDispMask(1);
    } while (gMoviePlaybackState == MOVIE_PLAYBACK_PLAYING);
    DecDCToutCallback(0);
    StUnSetRing();
    StClearRing();
    CdControlF(CdlPause, 0);
    if (gMoviePlaybackState == MOVIE_PLAYBACK_ERROR) {
        SetCdAudioMix(0, 0, 0, 0);
        while (CdReady(1, cdResult) == CdlDiskError) {
            CdControlF(CdlStandby, 0);
            VSync(4);
        }
    }
    CleanupMoviePlayback();
    return 1;
}
#else
int RunMoviePlaybackLoop(void)
{
    MovieState *state;
    int page;
    MovieDisplayEnv *display;
    u_long *ot;
    u_long *frameOt;
    u_char cdResult[8];

#ifdef VERSION_EU
    gMovieLastFrameNumber = 0;
#endif

    while (DecodeMovieFrame(cdResult) == 0) {
        state = dec;
        page = state->frameIndex == 0;
        ot = state->orderingTables[page];
        display = gMovieDisplayEnvs[page];
        gFrameCounter = page;
        gCurrentOt = ot;
        ClearOTag(ot, MOVIE_OT_LEN);
        if ((gPadState.pressed & (_PAD(0, MENU_CANCEL) | _PAD(1, MENU_CANCEL))) != 0) {
            gMoviePlaybackState = MOVIE_PLAYBACK_STOPPED;
            break;
        }
        EnqueueMovieTileQuads();
        do {
        } while (GetRCnt(RCntCNT1) < FRAME_PACING_HBLANKS);
        VSync(1);
        DrawSync(0);
        VSync(0);
        ResetRCnt(RCntCNT1);
        PollControllers();
        if (gSubtitleActive != 0) {
            frameOt = gCurrentOt;
            /* The last bucket draws last: the subtitles over the tiles. */
            gCurrentOt = frameOt + (MOVIE_OT_LEN - 1);
            ResetFramePacketCursor();
            UpdateAndDrawSubtitle(0);
            gCurrentOt = frameOt;
        }
        PutDrawEnv(&display->draw);
        PutDispEnv(&display->disp);
        DrawOTag(gCurrentOt);
        if (gMoviePlaybackState != MOVIE_PLAYBACK_PLAYING) {
            break;
        }
#ifdef VERSION_EU
        if (gMovieLastFrameNumber > gMovieFrameNumber || gMovieLastFrameNumber + 20 < gMovieFrameNumber) {
            gMoviePlaybackState = MOVIE_PLAYBACK_ERROR;
            break;
        }
        gMovieLastFrameNumber = gMovieFrameNumber;
#endif
    }
    DecDCToutCallback(0);
    StUnSetRing();
    StClearRing();
    CdControlF(CdlPause, 0);
    if (gMoviePlaybackState == MOVIE_PLAYBACK_ERROR) {
        SetCdAudioMix(0, 0, 0, 0);
        while (CdReady(1, cdResult) == CdlDiskError) {
            CdControlF(CdlStandby, 0);
            VSync(4);
        }
        ResetCallback();
    }
    CleanupMoviePlayback();
    return 1;
}
#endif
