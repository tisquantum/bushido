#ifndef POSE_STREAM_H
#define POSE_STREAM_H

/* A pose stream: a 92-byte header of 51 12-bit default joint angles and 51 2-bit delta modes, then
 * a big-endian 24-bit bit offset (from byte 0x5c) for each group of eight frames: a key pose and
 * seven second-order deltas. */

#define POSE_ROOT_COMPONENTS   3
#define POSE_ANGLE_COMPONENTS  51
#define POSE_TOTAL_COMPONENTS  54
#define POSE_WORD_COUNT        27 /* 54 shorts copied as 27 32-bit words */

#define POSE_STREAM_HEADER_BYTES 0x5c
#define POSE_STREAM_HEADER_BITS  (POSE_STREAM_HEADER_BYTES * 8)

#define POSE_CACHE_SLOTS         4
#define POSE_CACHE_INVALID_FRAME (-2) /* -1 would match (frame - 1) when frame == 0 */

typedef enum PoseDeltaMode {
    POSE_DELTA_NONE = 0,         /* channel stays constant across delta frames */
    POSE_DELTA_EXP_GOLOMB = 1,   /* zigzag-signed order-0 Exp-Golomb */
    POSE_DELTA_VAR_WIDTH = 2,    /* 1-bit flag + 4-bit width/shortcut + signed N-bit */
    POSE_DELTA_EXP_GOLOMB_3 = 3  /* 1-bit flag + sign + 3 LSBs + Exp-Golomb quotient */
} PoseDeltaMode;

typedef struct PoseWork {
    short pose[POSE_TOTAL_COMPONENTS];
    short vel[POSE_TOTAL_COMPONENTS];
    unsigned short mode[POSE_TOTAL_COMPONENTS];
} PoseWork;

#define POSE_DECODE_WORK ((PoseWork *)getScratchAddr(0))

typedef struct PoseBitState {
    unsigned long *ptr;
    int left;
    unsigned long word;
} PoseBitState;

typedef struct PoseCache {
    int frame;                           /* cached frame index, or POSE_CACHE_INVALID_FRAME */
    short pose[POSE_TOTAL_COMPONENTS];
    short vel[POSE_TOTAL_COMPONENTS];
    PoseBitState bitState;               /* the bit reader after this frame */
} PoseCache;

void InitPoseStream(unsigned long *stream);
void DecodePoseFrame(int frame, void *output, PoseWork *work);
void DecodePoseFramePair(int frameA, int frameB, void *outputA, void *outputB, PoseWork *work);

#endif
