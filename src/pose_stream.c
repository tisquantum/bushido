/* Fighter pose stream: decodes the skeletal animation bitstream into 54-short poses (3 root values
 * and 51 12-bit joint angles), each group of eight frames a key pose then second-order deltas, with
 * a four-slot cache of decoded frames and their bit-reader state. */

#include "pose_stream.h"

int gPoseReuseModes = 0; /* set while DecodePoseFramePair decodes its second frame */

unsigned long *gPoseStreamBase;
int gPoseCacheSerial;            /* serial of the last decode; picks the cache slot */
PoseCache gPoseDecodeCache[POSE_CACHE_SLOTS];
unsigned short gPoseChannelModes[POSE_TOTAL_COMPONENTS];
short gPoseDefaultPose[POSE_TOTAL_COMPONENTS];

/* The unread bits occupy the high end of word. Refill across a word boundary
 * before discarding the bits that were just read. */
#define POSE_READ_BITS_WITH_PREFIX(dst, n, prefix) \
    if (left < (n)) { \
        int need = (n) - left; \
        unsigned long bits = word >> (32 - left); \
        word = *ptr++; \
        (dst) = (prefix) | ((bits << need) | (word >> (32 - need))); \
        left = 32 - need; \
        word <<= need; \
    } else { \
        (dst) = (prefix) | (word >> (32 - (n))); \
        word <<= (n); \
        left -= (n); \
    }

#define POSE_READ_BITS(dst, n) POSE_READ_BITS_WITH_PREFIX(dst, n, 0)

#define POSE_READ_BIT(dst) \
    if (left == 0) { \
        word = *ptr++; \
        left = 32; \
    } \
    (dst) = word >> 31; \
    word <<= 1; \
    left--

#define POSE_COPY_WORDS(dstBuf, srcBuf) \
    { \
        unsigned int i; \
        int *src = (int *)(srcBuf); \
        int *dst = (int *)(dstBuf); \
        for (i = 0; i < POSE_WORD_COUNT; i++) { \
            *dst++ = *src++; \
        } \
    }

void InitPoseStream(unsigned long *stream)
{
    unsigned long *ptr;
    int left;
    unsigned long word;
    int component;
    int slot;

    ptr = stream;
    left = 32;
    gPoseStreamBase = stream;
    word = *ptr++;

    for (component = 0; component < POSE_ANGLE_COMPONENTS; ++component) {
        POSE_READ_BITS(gPoseDefaultPose[component + POSE_ROOT_COMPONENTS], 12);
    }

    for (component = 0; component < POSE_ANGLE_COMPONENTS; ++component) {
        POSE_READ_BITS(gPoseChannelModes[component + POSE_ROOT_COMPONENTS], 2);
    }

    for (slot = 0; slot < POSE_CACHE_SLOTS; ++slot) {
        gPoseDecodeCache[slot].frame = POSE_CACHE_INVALID_FRAME;
    }
    gPoseCacheSerial = 0;
}

void DecodePoseFrame(int frame, void *output, PoseWork *work)
{
    int step;
    int group;
    unsigned char *directoryBytes;
    PoseCache *cached;
    PoseCache *prev;
    unsigned long *ptr;
    int left;
    unsigned long word;
    int component;
    int deltaStep;
    int width;
    int value;
    int bit;
    int bitOffset;
    short change;
    int serial;

    step = frame & 7;
    group = frame >> 3;
    directoryBytes = (unsigned char *)gPoseStreamBase + POSE_STREAM_HEADER_BYTES;
    if (gPoseReuseModes == 0) {
        POSE_COPY_WORDS(work->mode, gPoseChannelModes);
    }
    serial = gPoseCacheSerial;
    gPoseCacheSerial = serial + 1;
    cached = &gPoseDecodeCache[(serial + 1) & 3];
    if (cached->frame == frame) {
        POSE_COPY_WORDS(output, cached->pose);
        return;
    }
    prev = &gPoseDecodeCache[serial & 3];
    if (prev->frame == frame) {
        POSE_COPY_WORDS(output, prev->pose);
        return;
    }
    if (cached->frame != frame - 1) {
        if (prev->frame == frame - 1) {
            cached = prev;
        } else {
            /* The group directory contains big-endian 24-bit bit offsets. */
            const unsigned char (*directory)[3] = (const unsigned char (*)[3])directoryBytes;
            bitOffset = ((directory[group][0] << 16) | (directory[group][1] << 8) |
                         directory[group][2]) + POSE_STREAM_HEADER_BITS;
            ptr = gPoseStreamBase + (bitOffset >> 5);
            left = 32 - (bitOffset & 31);
            word = *ptr++ << (bitOffset & 31);
            POSE_COPY_WORDS(work->pose, gPoseDefaultPose);
            POSE_READ_BITS(value, 1);
            if (value) {
                POSE_READ_BITS(value, 16);
            }
            work->pose[0] = value;
            POSE_READ_BITS(value, 1);
            if (value) {
                POSE_READ_BITS(value, 16);
            }
            work->pose[1] = value;
            POSE_READ_BITS(value, 1);
            if (value) {
                POSE_READ_BITS(value, 16);
            }
            work->pose[2] = value;
            for (component = 0; component < POSE_ANGLE_COMPONENTS; component++) {
                POSE_READ_BITS(value, 1);
                if (value) {
                    POSE_READ_BITS(work->pose[component + POSE_ROOT_COMPONENTS], 12);
                }
            }
            deltaStep = 0;
            goto decode;
        }
    }
    ptr = cached->bitState.ptr;
    left = cached->bitState.left;
    word = cached->bitState.word;
    POSE_COPY_WORDS(work->pose, cached->pose);
    if (step >= 2) {
        POSE_COPY_WORDS(work->vel, cached->vel);
    }
    deltaStep = step - 1;
decode:
    for (; deltaStep < step; ++deltaStep) {
        POSE_READ_BITS(value, 1);
        if (value) {
            POSE_READ_BITS(value, 16);
        }
        work->pose[0] = value;
        POSE_READ_BITS(value, 1);
        if (value) {
            POSE_READ_BITS(value, 16);
        }
        work->pose[1] = value;
        POSE_READ_BITS(value, 1);
        if (value) {
            POSE_READ_BITS(value, 16);
        }
        work->pose[2] = value;
        for (component = 0; component < POSE_ANGLE_COMPONENTS; component++) {
            if (work->mode[component + POSE_ROOT_COMPONENTS] == POSE_DELTA_NONE) {
                continue;
            }
            switch (work->mode[component + POSE_ROOT_COMPONENTS]) {
            case POSE_DELTA_EXP_GOLOMB:
                width = 0;
                do {
                    POSE_READ_BIT(bit);
                    if (bit) {
                        break;
                    }
                } while (++width < 12);
                if (width == 12) {
                    POSE_READ_BITS_WITH_PREFIX(change, 11, 0x800);
                } else if (width >= 2) {
                    width--;
                    value = 1 << width;
                    POSE_READ_BITS_WITH_PREFIX(change, width, value);
                } else {
                    change = width;
                }
                {
                    short delta;

                    if (change & 1) {
                        delta = ~(change / 2);
                    } else {
                        delta = change / 2;
                    }
                    change = delta;
                }
                break;
            case POSE_DELTA_VAR_WIDTH:
                POSE_READ_BITS(change, 1);
                if (change) {
                    POSE_READ_BITS(value, 4);
                    if (value == 15) {
                        change = 1;
                    } else if (value == 14) {
                        change = -1;
                    } else if (value == 13) {
                        change = 2;
                    } else if (value == 0) {
                        change = -2;
                    } else {
                        POSE_READ_BITS(change, value);
                        if ((change >> (value - 1)) & 1) {
                            change |= ~0U << value;
                        }
                    }
                }
                break;
            case POSE_DELTA_EXP_GOLOMB_3:
                POSE_READ_BITS(change, 1);
                if (change) {
                    POSE_READ_BITS(value, 4);
                    width = 0;
                    do {
                        POSE_READ_BIT(bit);
                        if (bit) {
                            break;
                        }
                    } while (++width < 8);
                    if (width == 8) {
                        POSE_READ_BITS_WITH_PREFIX(change, 7, 0x80);
                    } else if (width < 2) {
                        change = width;
                    } else {
                        width--;
                        change = 1 << width;
                        POSE_READ_BITS_WITH_PREFIX(change, width, change);
                    }
                    change = ((change << 3) | (value & 7)) + 1;
                    if (value & 8) {
                        change = -change;
                    }
                }
                break;
            }
            if (deltaStep == 0) {
                work->vel[component + POSE_ROOT_COMPONENTS] = change;
                work->pose[component + POSE_ROOT_COMPONENTS] += change;
            } else {
                work->pose[component + POSE_ROOT_COMPONENTS] += work->vel[component + POSE_ROOT_COMPONENTS] + change;
                work->vel[component + POSE_ROOT_COMPONENTS] += change;
            }
        }
    }
    POSE_COPY_WORDS(output, work->pose);
    if (step < 7) {
        cached->frame = frame;
        POSE_COPY_WORDS(cached->pose, work->pose);
        POSE_COPY_WORDS(cached->vel, work->vel);
        cached->bitState.ptr = ptr;
        cached->bitState.left = left;
        cached->bitState.word = word;
    }
}

void DecodePoseFramePair(int frameA, int frameB, void *outputA, void *outputB, PoseWork *work)
{
    DecodePoseFrame(frameA, outputA, work);
    gPoseReuseModes = 1;
    DecodePoseFrame(frameB, outputB, work);
    gPoseReuseModes = 0;
}
