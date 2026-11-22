#ifndef STAGE_AMBIENT_SOUND_H
#define STAGE_AMBIENT_SOUND_H

#include <sys/types.h>

/* Per-slot stage ambient sound timer (frames since its last trigger or reset). */
struct AmbientSoundTimer {
    int elapsedFrames;
};

/* Stage ambient spu voice range (17..23), timer slots (0..7), and base sound ID (0x79 + slot). */
#define STAGE_AMBIENT_VOICE_BASE    17     /* First spu voice reserved for stage ambient sounds */
#define STAGE_AMBIENT_VOICE_COUNT   7      /* Number of spu voices reserved for stage ambient sounds (17..23) */
#define STAGE_AMBIENT_TIMER_COUNT   8      /* Number of per-slot ambient sound timers (gAmbientSoundTimers[8]) */
#define STAGE_AMBIENT_SOUND_BASE    0x79   /* First sound ID for stage ambient slots (0x79 + slotIndex) */

/* Q12 ambient volume scales (0x1000 = 1.0, halved during Story dialogue cutscenes). */
#define STAGE_AMBIENT_VOLUME_NORMAL   0x1000
#define STAGE_AMBIENT_VOLUME_CUTSCENE 0x800

/* PlaySpatialSound command flags (packed with (voice << 8) | soundId). */
#define SPATIAL_SOUND_TRIGGER_VOICE 0x10000 /* Stop voice and start soundId with spatialized stereo volume */
#define SPATIAL_SOUND_UPDATE_VOICE  0x20000 /* Update spatialized stereo volume of an already-playing voice */

/* Stage 24 daytime manual ambient sound state machine (gYahiroRoadAmbientState). */
typedef enum YahiroRoadAmbientState {
    YAHIRO_ROAD_AMBIENT_IDLE = 0,        /* Waiting for random roll to start slot 2 (sound 0x7b, voice 19) */
    YAHIRO_ROAD_AMBIENT_PLAY_FIRST = 1,  /* Playing slot 2 (30..151 frames) before switching to slot 3 (sound 0x7c) */
    YAHIRO_ROAD_AMBIENT_PLAY_SECOND = 2  /* Playing slot 3 for 91 frames before stopping voice 19 */
} YahiroRoadAmbientState;

/* StageAmbientSound.flags bits (read by StartStageAmbientSounds and UpdateAmbientSounds). */
#define STAGE_AMBIENT_LOOP          0x00 /* Continuous loop started on stage entry */
#define STAGE_AMBIENT_RANDOM        0x01 /* Periodic random trigger */
#define STAGE_AMBIENT_RANDOM_PAIR   0x02 /* Random trigger that also fires the next slot */
#define STAGE_AMBIENT_PAIR_FOLLOWUP 0x03 /* Paired second slot fired by STAGE_AMBIENT_RANDOM_PAIR */
#define STAGE_AMBIENT_MODE_MASK     0x03 /* Mode bits (0..1) */
#define STAGE_AMBIENT_NIGHT         0x04 /* Nighttime only (!GetDaytimeOption()) */
#define STAGE_AMBIENT_DAY           0x08 /* Daytime only (GetDaytimeOption()) */
#define STAGE_AMBIENT_TIME_MASK     0x0c /* Day/night filter bits (2..3); 0 = day and night */
#define STAGE_AMBIENT_MANUAL        0x40 /* Skipped by generic loop (driven by stage event/state machine) */
#define STAGE_AMBIENT_END           0x80 /* Last descriptor in a stage's ambient sound list */

/* 4-byte per-stage ambient sound descriptor indexed via gStageAmbientSoundLists[stage]. */
typedef struct StageAmbientSound {
    u_char flags;         /* +0x00: Mode (bits 0..1), day/night filter (2..3), manual (6), end-of-list (7) */
    u_char minDelaySec;   /* +0x01: Minimum delay, in 30-frame units, before a random re-trigger */
    u_char chanceDivisor; /* +0x02: Probability denominator (chance per frame = 1 / (chanceDivisor * 30)) */
    u_char volume;        /* +0x03: Base playback volume (0..127) */
} StageAmbientSound;

#endif
