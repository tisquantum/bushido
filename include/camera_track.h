#ifndef CAMERA_TRACK_H
#define CAMERA_TRACK_H

#include <sys/types.h>

/* MSB-first 32-bit word bitstream reader for compressed cutscene camera tracks. */
typedef struct SceneBitReader {
    u_int *next;
    u_int word;
    int remaining;
} SceneBitReader;

/* Cutscene camera track decoder state (values[0..2] = relative x/y/z, values[3..5] = Euler rx/ry/rz
 * in 4096-unit PS1 angles, 0x1000 = 360 deg). */
typedef struct SceneCameraTrack {
    SceneBitReader bits;
    short values[6];
} SceneCameraTrack;

void InitCameraTrackReader(void *streamData, int channel);
short *DecodeCameraTrackFrame(int channel);

/* A scene's camera this tick: on the PC, the frame's key decoded on its first tick and blended to from the last
 * frame's over the frame's ticks (pc_substep.h); on the console, the frame's key. */
#ifdef TARGET_PC
short *PcCameraTrackFrame(int channel);
#define CAMERA_TRACK_FRAME(channel) PcCameraTrackFrame(channel)
#else
#define CAMERA_TRACK_FRAME(channel) DecodeCameraTrackFrame(channel)
#endif

#endif
