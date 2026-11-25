/* Cutscene camera track bitstream decoder (two MSB-first delta-compressed channels used by
 * story cutscenes and the opening demo). */

#include <sys/types.h>
#include "camera_track.h"
#include "substep.h"

enum {
    CAMERA_TRACK_CHANNEL_COUNT = 2,
    CAMERA_TRACK_VALUE_COUNT = 6 /* relative x, y, z followed by Euler rx, ry, rz */
};

static SceneCameraTrack gCameraTrackReaders[CAMERA_TRACK_CHANNEL_COUNT];
#ifdef TARGET_PC
/* Each channel's last frame's key, and whether there is one to blend from (not after a reset or a cut). */
static short sPcLastKey[CAMERA_TRACK_CHANNEL_COUNT][CAMERA_TRACK_VALUE_COUNT];
static short sPcBlendedKey[CAMERA_TRACK_CHANNEL_COUNT][CAMERA_TRACK_VALUE_COUNT];
static int sPcHaveLastKey[CAMERA_TRACK_CHANNEL_COUNT];
#define PC_CAMERA_CUT 1500 /* a move further than this between two frames is a cut, not blended over */
#endif

static unsigned int ReadCameraTrackBits(SceneBitReader *reader, int width);

void InitCameraTrackReader(void *streamData, int channel)
{
    SceneCameraTrack *track;

    track = &gCameraTrackReaders[channel];
    track->bits.next = streamData;
    track->bits.word = 0;
    track->bits.remaining = 0;
    track->values[0] = track->values[1] = track->values[2] =
        track->values[3] = track->values[4] = track->values[5] = 0;
#ifdef TARGET_PC
    sPcHaveLastKey[channel] = 0;
#endif
}

#ifdef TARGET_PC
short *PcCameraTrackFrame(int channel)
{
    short *key = gCameraTrackReaders[channel].values;
    int i, cut = 0;
    if (PC_FIRST_SUBTICK) {
        for (i = 0; i < CAMERA_TRACK_VALUE_COUNT; i++) {
            sPcLastKey[channel][i] = key[i];
        }
        DecodeCameraTrackFrame(channel);
    }
    for (i = 0; i < 3; i++) {
        int move = key[i] - sPcLastKey[channel][i];
        cut |= move > PC_CAMERA_CUT || move < -PC_CAMERA_CUT;
    }
    if (!sPcHaveLastKey[channel] || cut) {
        if (PC_LAST_SUBTICK) {
            sPcHaveLastKey[channel] = 1;
        }
        return key;
    }
    for (i = 0; i < CAMERA_TRACK_VALUE_COUNT; i++) {
        sPcBlendedKey[channel][i] = (short)(i < 3 ? PC_BLEND(sPcLastKey[channel][i], key[i])
                                                  : PC_BLEND_ANGLE(sPcLastKey[channel][i], key[i]));
    }
    return sPcBlendedKey[channel];
}
#endif

/* Each frame encodes 6 components (x, y, z, rx, ry, rz) as a 4-bit width (0 means 16 bits)
 * followed by a signed two's-complement delta of that width, accumulated into track->values. */
short *DecodeCameraTrackFrame(int channel)
{
    SceneCameraTrack *track;
    int i;
    int width;
    short delta;

    track = &gCameraTrackReaders[channel];
    for (i = 0; i < CAMERA_TRACK_VALUE_COUNT; i++) {
        width = ReadCameraTrackBits(&track->bits, 4);
        if (width == 0) {
            width = 16;
        }
        delta = ReadCameraTrackBits(&track->bits, width);
        if ((delta >> (width - 1)) & 1) {
            delta |= 0xffff << (width - 1);
        }
        track->values[i] += delta;
    }

    return track->values;
}

static unsigned int ReadCameraTrackBits(SceneBitReader *reader, int width)
{
    unsigned int highMask;
    unsigned int lowMask;
    unsigned int word;
    unsigned int low;
    unsigned int result;

    if (reader->remaining < width) {
        highMask = (1 << reader->remaining) - 1;
        width -= reader->remaining;
        lowMask = (1 << width) - 1;
        result = reader->word & highMask;
        word = *reader->next++;
        reader->remaining = 32 - width;
        low = (word >> reader->remaining) & lowMask;
        result = (result << width) | low;
        reader->word = word;
    } else {
        reader->remaining -= width;
        result = (reader->word >> reader->remaining) & ((1 << width) - 1);
    }
    return result;
}
