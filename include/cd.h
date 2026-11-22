#ifndef CD_H
#define CD_H

#include <sys/types.h>
#include <libcd.h>

#define CD_SECTOR_SIZE 2048

#define CD_CATEGORY_COUNT 6

extern short gCdCategoryFirstFile[CD_CATEGORY_COUNT];
#define XA_CHANNELS_PER_FILE 4

typedef enum CdFileCategory {
    CD_CATEGORY_DATA = 0,
    CD_CATEGORY_MOTION = 1,
    CD_CATEGORY_SOUND = 2,
    CD_CATEGORY_XA = 4,
    CD_CATEGORY_MOVIE = 5
} CdFileCategory;

/* An XA track is channel (track & 3) of XA file (track >> 2). */
typedef enum XaTrack {
    XA_TRACK_PROLOGUE_BASE = 0,
    XA_TRACK_PROLOGUE_INTRO = 6,
    XA_TRACK_OPENING = 8,
    XA_TRACK_SELECT = 9,
    XA_TRACK_CREDITS = 10,
    XA_TRACK_BAD_ENDING = 11,
    XA_TRACK_VS_BGM_A = 12,
    XA_TRACK_PRACTICE_BGM = 13,
    XA_TRACK_VS_BGM_B = 14,
    XA_TRACK_SLASH_BGM = 15
} XaTrack;

/* States below 0x10 read data; from 0x10 on they play XA or CD audio. */
typedef enum CdStreamState {
    CD_STREAM_IDLE = 0,

    CD_STREAM_READ_SETMODE = 2,
    CD_STREAM_READ_SETMODE_WAIT = 3,
    CD_STREAM_READ_SEEK_DELAY = 4,
    CD_STREAM_READ_START = 5,
    CD_STREAM_READ_ACTIVE = 6,
    CD_STREAM_READ_DONE = 8,
    CD_STREAM_READ_ERROR = 9,
    CD_STREAM_READ_NOP = 10,
    CD_STREAM_READ_NOP_WAIT = 11,
    CD_STREAM_READ_GET_TN = 12,
    CD_STREAM_READ_GET_TN_WAIT = 13,

    CD_STREAM_AUDIO_SETMODE = 0x10,
    CD_STREAM_AUDIO_SETMODE_WAIT = 0x11,
    CD_STREAM_AUDIO_SEEK = 0x12,
    CD_STREAM_AUDIO_SEEK_WAIT = 0x13,
    CD_STREAM_AUDIO_PLAY_WAIT = 0x14,
    CD_STREAM_AUDIO_POLL = 0x15,
    CD_STREAM_AUDIO_POLL_WAIT = 0x16,
    CD_STREAM_AUDIO_ERROR = 0x17,
    CD_STREAM_AUDIO_NOP = 0x18,
    CD_STREAM_AUDIO_NOP_WAIT = 0x19,
    CD_STREAM_AUDIO_GET_TN = 0x1A,
    CD_STREAM_AUDIO_GET_TN_WAIT = 0x1B,
    CD_STREAM_AUDIO_STOP = 0x1C,
    CD_STREAM_AUDIO_STOP_WAIT = 0x1D,
    CD_STREAM_AUDIO_COOLDOWN = 0x1E
} CdStreamState;

typedef struct CdFileEntry {
    CdlLOC location;
    u_int size; /* bytes */
} CdFileEntry;

typedef struct CdStream {
    int retryWait;
    short handle;
    short state;
    short hold;
    short unreadWord; /* cleared when a read starts; nothing reads it */
    short stopRequested;
    short repeat;
    CdlLOC *startLoc;
    int endSector;
    int sectorCount;
    u_char *dest;
    int sectorsLeft; /* -1 after a read error, -2 after an unexpected sector */
    u_char *readCursor;
    int ticks;
    int delay;
    CdlFILTER filter;
    u_char mode[4];
    int isXaStream;
    short readWait;
    short syncWait;
    short readyWait;
    int expectedSector;
    int endCountdown;
} CdStream;

/* While a movie plays the stream is idle, and the movie player keeps its movie in the same record. */
typedef struct MovieCdSlot {
    short movieId;
    short streamFields[5];
    CdFileEntry *file;
} MovieCdSlot;

typedef union CdDrive {
    CdStream stream;
    MovieCdSlot movie;
} CdDrive;

extern CdDrive gCdDrive;
extern CdFileEntry gCdFileTable[];

void SetCdAudioMix(int ll, int lr, int rl, int rr);
void StartCdAudioMixFade(int length, int ll, int lr, int rl, int rr);
void EnableCdSpuAudio(void);
void InitCdSubsystem(void);
void UpdateCdSubsystem(void);
int IsCdIdle(void);
int StartCdFileRead(int handle, void *buffer);
int ResolveCdFileHandle(int category, int index);
int GetCdFileSize(int handle);
int WaitCdIdle(void);
int StartXaTrack(int track);
int CueXaTrackPaused(int track);
void SetCdAudioRepeat(int repeat);
int CueCdStreamPaused(int handle, int channel);
void StopCdAudioStream(void);
void ReleaseHeldCdStream(void);
void WaitCdStreamPlaying(void);
int StopAndWaitCdIdle(void);

#endif
