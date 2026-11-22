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

#endif
