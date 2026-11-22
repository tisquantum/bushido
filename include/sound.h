#ifndef SOUND_H
#define SOUND_H

/* Resident VAB header work buffer used in 100-man Slash mode. */
#define SLASH_SOUND_VAB_HEADER_WORK ((char *)0x801EF000)

/* Resident VAB header work buffer capacity checked by LoadCombatSoundBanks (gSoundVabHeaderWork). */
#define SOUND_VAB_HEADER_WORK_SIZE 0x8000

extern char gSoundVabHeaderWork[SOUND_VAB_HEADER_WORK_SIZE];

/* Weapon-pair sound ID used for Practice mode's wooden-weapon VAB archive (gWeaponPairSoundTable entry 0x0400). */
#define SOUND_WEAPON_PRACTICE_ID 9

/* Number of individual fighter voice/SFX archives (fighterId 0..17 -> CD files 45..62); >= 18 falls back to 0. */
#define SOUND_FIGHTER_BANK_COUNT 18

/* VAB bank capacity, body staging capacity and the ambient sentinel. */
#define SOUND_VAB_BANK_COUNT     11
#define SOUND_VAB_BODY_MAX       6
#define SOUND_AMBIENT_FILE_NONE  255

/* SoundLoadRequest.mode (+0x08) VAB bank-set selector for RequestSoundBankLoad / CommitSoundBankLoad. */
typedef enum SoundLoadMode {
    SOUND_LOAD_WEAPON_PAIR = 0,          /* weapon-pair archive via firstId/secondId -> VAB banks 0, 3, 4 */
    SOUND_LOAD_FIRST_FIGHTER = 1,        /* P1 fighter archive (firstId + 45) -> VAB banks 1, 2 */
    SOUND_LOAD_SECOND_FIGHTER = 2,       /* P2 fighter archive (secondId + 45) -> VAB banks 5, 6 */
    SOUND_LOAD_DIALOGUE = 3,             /* Story dialogue pair (firstId, secondId) -> closes 0..6, opens VAB bank 7 */
    SOUND_LOAD_MENU = 4,                 /* menu/select UI archive (file 122) -> VAB bank 8 */
    SOUND_LOAD_STAGE_AMBIENT = 5,        /* stage ambient archive (gStageAmbientSoundFile[stageId] + 123)
                                            -> VAB bank 9 */
    SOUND_LOAD_STAGE_AMBIENT_RELOAD = 6, /* reloads VAB bank 9 in-place at saved cursor (falls back to mode 5) */
    SOUND_LOAD_LINK_LOBBY = 7,           /* Link lobby waiting-SFX archive (file 63) -> VAB bank 10 */
    SOUND_LOAD_SLASH_MODE = 8            /* 100-man Slash archive (file 146) -> closes 4, opens VAB banks 5..10 */
} SoundLoadMode;

/* 12-byte stack-allocated VAB load specification passed to RequestSoundBankLoad / CommitSoundBankLoad. */
typedef struct SoundLoadRequest {
    unsigned int *cdBuffer;  /* +0x00: temp RAM buffer receiving CD archive (starts with 32-bit offset table) */
    char *headerWork;        /* +0x04: resident RAM buffer receiving copied VAB headers (e.g. gSoundVabHeaderWork) */
    unsigned char mode;      /* +0x08: SoundLoadMode (0..8) */
    unsigned char stageId;   /* +0x09: stage index (0..25) for SOUND_LOAD_STAGE_AMBIENT / _RELOAD */
    unsigned char firstId;   /* +0x0A: P1 weaponId (mode 0) or fighterId (modes 1, 3) */
    unsigned char secondId;  /* +0x0B: P2 weaponId (mode 0) or fighterId (modes 2, 3) */
} SoundLoadRequest;

void QueueSoundEffect(int sound, int right, int left);
void SetSoundVoiceVolume(int right, int left, short voice);
void QueueSoundOnVoice(int sound, int right, int left, int slot);
void StopSoundVoice(int slot);
void QueuePriorityVoice0(int sound, int right, int left);
void StopPriorityVoice0(void);
void QueuePriorityVoice1(int sound, int right, int left);
void StopPriorityVoice1(void);
int TickSoundVoiceCooldowns(void);
int RequestSoundBankLoad(SoundLoadRequest *request);
void SelectSlashEnemySoundBank(int fighterId);
void InitSoundSystem(int stageId);
int CommitSoundBankLoad(SoundLoadRequest *request);
int FlushQueuedSounds(void);
void TransferSoundVabSet(unsigned char **headers, unsigned char **bodies, int bankSet);
void StartSoundSystem(int stageId);

#endif
