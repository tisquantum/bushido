#ifndef VOICE_STATE_H
#define VOICE_STATE_H

/* SPU voices, as gSoundVoiceQueue and gSoundVoiceCooldowns index them: 0 P1's fighter voice, Story dialogue and
 * the pause and menu cues; 1 P2's fighter voice; 2..16 the effects pool; 17..23 the stage's ambient sounds
 * (STAGE_AMBIENT_VOICE_BASE). */
#define SOUND_VOICE_COUNT         24
#define SOUND_PRIORITY_VOICE_0    0
#define SOUND_PRIORITY_VOICE_1    1
#define SOUND_SFX_VOICE_FIRST     2
#define SOUND_SFX_VOICE_END       17
#define SOUND_SFX_COOLDOWN_FRAMES 30
#define SOUND_MAX_VOLUME          127
#define SOUND_DEFAULT_NOTE        60

/* 6-byte queued SPU voice trigger in gSoundVoiceQueue[24]. */
typedef struct SoundVoiceState {
    short soundId;  /* +0x00: queued sound table index (0 = idle) */
    short volRight; /* +0x02: scaled right volume (volr in SsUtKeyOnV) */
    short volLeft;  /* +0x04: scaled left volume (voll in SsUtKeyOnV) */
} SoundVoiceState;

#endif
