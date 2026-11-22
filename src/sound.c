/* The sound manager: the game's VAB banks loaded from CD archives, a queue of SPU voices keyed on
 * each frame, the Slash mode's enemy bank patching, and per-stage reverb. */

#include <sys/types.h>
#include <libsnd.h>

#include "cd.h"
#include "voice_state.h"
#include "sound.h"
#include "stage_id.h"

#define SOUND_FILE_FIGHTER_BASE        45
#define SOUND_FILE_LINK_LOBBY          63
#define SOUND_FILE_DIALOGUE_BASE       68
#define SOUND_FILE_MENU                122
#define SOUND_FILE_AMBIENT_BASE        123
#define SOUND_FILE_SLASH               146

#define SOUND_MAP_BANK_MASK            0x3f
#define SOUND_MAP_PROG_MASK            0xc0
#define SOUND_MAP_PROG1_FLAG           0x80
#define SOUND_MAP_PROG2_FLAG           0x40

#define SOUND_SLASH_WEAPON_FIRST       0x41
#define SOUND_SLASH_WEAPON_END         0x53
#define SOUND_SLASH_ENEMY_FIRST        0x53
#define SOUND_SLASH_ENEMY_END          0x5d
#define SOUND_SLASH_WEAPON_BANK_BASE   5
#define SOUND_SLASH_ENEMY_BANK_BASE    7
#define SOUND_SLASH_ENEMY_ID_BASE      19
#define SOUND_SLASH_ALT_WEAPON_VARIANT 2

#define SOUND_REVERB_DEPTH_HALL        0x40
#define SOUND_REVERB_DEPTH_ROOM        0x20

#define SOUND_SCALE_VOLUME(vol)        ((vol) * 114 / 128)

short gSoundVoiceCooldowns[SOUND_VOICE_COUNT];
SoundVoiceState gSoundVoiceQueue[SOUND_VOICE_COUNT];

static void ShutdownSoundSystem(void)
{
    int voice;

    SsVabClose(0);
    SsVabClose(1);
    SsVabClose(2);
    SsVabClose(3);
    SsVabClose(4);
    SsVabClose(5);
    SsVabClose(6);
    SsVabClose(7);
    SsVabClose(8);
    SsVabClose(9);
    SsVabClose(10);
    SsVabClose(11);
    SsVabClose(12);
    SsVabClose(13);
    SsVabClose(14);
    SsEnd();
    SsQuit();
    for (voice = 0; voice < SOUND_VOICE_COUNT; voice++) {
        if (gSoundVoiceCooldowns[voice] > 0) {
            gSoundVoiceCooldowns[voice] = 0;
        }
    }
}

void QueueSoundEffect(int sound, int right, int left)
{
    short slot;

    for (slot = SOUND_SFX_VOICE_FIRST; slot < SOUND_SFX_VOICE_END; ++slot) {
        if (gSoundVoiceCooldowns[slot] <= 0) {
            gSoundVoiceQueue[slot].soundId = sound;
            gSoundVoiceQueue[slot].volRight = SOUND_SCALE_VOLUME(right);
            gSoundVoiceQueue[slot].volLeft = SOUND_SCALE_VOLUME(left);
            gSoundVoiceCooldowns[slot] = SOUND_SFX_COOLDOWN_FRAMES;
            break;
        }
    }
}

void SetSoundVoiceVolume(int right, int left, short voice)
{
    SsUtSetVVol(voice, SOUND_SCALE_VOLUME(left), SOUND_SCALE_VOLUME(right));
}

void QueueSoundOnVoice(int sound, int right, int left, int slot)
{
    gSoundVoiceQueue[slot].soundId = sound;
    gSoundVoiceQueue[slot].volRight = SOUND_SCALE_VOLUME(right);
    gSoundVoiceQueue[slot].volLeft = SOUND_SCALE_VOLUME(left);
}

void StopSoundVoice(int slot)
{
    gSoundVoiceQueue[slot].soundId = 0;
    gSoundVoiceCooldowns[slot] = 0;
    SsUtKeyOffV((short)slot);
}

void QueuePriorityVoice0(int sound, int right, int left)
{
    gSoundVoiceQueue[SOUND_PRIORITY_VOICE_0].soundId = sound;
    gSoundVoiceQueue[SOUND_PRIORITY_VOICE_0].volRight = SOUND_SCALE_VOLUME(right);
    gSoundVoiceQueue[SOUND_PRIORITY_VOICE_0].volLeft = SOUND_SCALE_VOLUME(left);
}

void StopPriorityVoice0(void)
{
    gSoundVoiceQueue[SOUND_PRIORITY_VOICE_0].soundId = 0;
    gSoundVoiceCooldowns[SOUND_PRIORITY_VOICE_0] = 0;
    SsUtKeyOffV(SOUND_PRIORITY_VOICE_0);
}

void QueuePriorityVoice1(int sound, int right, int left)
{
    gSoundVoiceQueue[SOUND_PRIORITY_VOICE_1].soundId = sound;
    gSoundVoiceQueue[SOUND_PRIORITY_VOICE_1].volRight = SOUND_SCALE_VOLUME(right);
    gSoundVoiceQueue[SOUND_PRIORITY_VOICE_1].volLeft = SOUND_SCALE_VOLUME(left);
}

void StopPriorityVoice1(void)
{
    gSoundVoiceQueue[SOUND_PRIORITY_VOICE_1].soundId = 0;
    gSoundVoiceCooldowns[SOUND_PRIORITY_VOICE_1] = 0;
    SsUtKeyOffV(SOUND_PRIORITY_VOICE_1);
}

/* bank: the VAB bank in bits 0..5; 0x80 selects program 1 and 0x40 program 2. */
typedef struct SoundMapEntry {
    unsigned char bank;
    unsigned char tone;
} SoundMapEntry;

extern SoundMapEntry gSlashSoundMap[];

typedef struct SoundFilePair {
    unsigned short mask;
    unsigned char file;
    unsigned char pad;
} SoundFilePair;

/* A pair of weapons w1, w2 is the mask (1 << (w1 + 1)) | (1 << (w2 + 1)); a zero mask ends the table. */
SoundFilePair gWeaponPairSoundTable[46] = {
    { 0x0002, 0x0000, 0x0000 },
    { 0x0006, 0x0001, 0x0000 },
    { 0x000a, 0x0002, 0x0000 },
    { 0x0012, 0x0003, 0x0000 },
    { 0x0022, 0x0004, 0x0000 },
    { 0x0042, 0x0005, 0x0000 },
    { 0x0082, 0x0006, 0x0000 },
    { 0x0102, 0x0007, 0x0000 },
    { 0x0004, 0x0008, 0x0000 },
    { 0x000c, 0x0009, 0x0000 },
    { 0x0014, 0x000a, 0x0000 },
    { 0x0024, 0x000b, 0x0000 },
    { 0x0044, 0x000c, 0x0000 },
    { 0x0084, 0x000d, 0x0000 },
    { 0x0104, 0x000e, 0x0000 },
    { 0x0008, 0x000f, 0x0000 },
    { 0x0018, 0x0010, 0x0000 },
    { 0x0028, 0x0011, 0x0000 },
    { 0x0048, 0x0012, 0x0000 },
    { 0x0088, 0x0013, 0x0000 },
    { 0x0108, 0x0014, 0x0000 },
    { 0x0010, 0x0015, 0x0000 },
    { 0x0030, 0x0016, 0x0000 },
    { 0x0050, 0x0017, 0x0000 },
    { 0x0090, 0x0018, 0x0000 },
    { 0x0110, 0x0019, 0x0000 },
    { 0x0020, 0x001a, 0x0000 },
    { 0x0060, 0x001b, 0x0000 },
    { 0x00a0, 0x001c, 0x0000 },
    { 0x0120, 0x001d, 0x0000 },
    { 0x0040, 0x001e, 0x0000 },
    { 0x00c0, 0x001f, 0x0000 },
    { 0x0140, 0x0020, 0x0000 },
    { 0x0080, 0x0021, 0x0000 },
    { 0x0180, 0x0022, 0x0000 },
    { 0x0100, 0x0023, 0x0000 },
    { 0x0202, 0x0024, 0x0000 },
    { 0x0204, 0x0025, 0x0000 },
    { 0x0208, 0x0026, 0x0000 },
    { 0x0210, 0x0027, 0x0000 },
    { 0x0220, 0x0028, 0x0000 },
    { 0x0240, 0x0029, 0x0000 },
    { 0x0280, 0x002a, 0x0000 },
    { 0x0300, 0x002b, 0x0000 },
    { 0x0400, 0x002c, 0x0000 },
    { 0x0000, 0x0000, 0x0000 },
};
unsigned char gStageAmbientSoundFile[STAGE_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, SOUND_AMBIENT_FILE_NONE,
    18, SOUND_AMBIENT_FILE_NONE, 19, 20, SOUND_AMBIENT_FILE_NONE, 21, 22,
};

/* returns no value */
int TickSoundVoiceCooldowns(void)
{
    short voice;
    short timer;

    for (voice = 0; voice < SOUND_VOICE_COUNT; ++voice) {
        timer = gSoundVoiceCooldowns[voice];
        if (timer > 0) {
            gSoundVoiceCooldowns[voice] = timer - 1;
        }
    }
}

int RequestSoundBankLoad(SoundLoadRequest *request)
{
    unsigned char unused[16]; /* not used */
    int file;
    int handle;
    int second;
    int first;
    unsigned short mask;
    int index;

    if (IsCdIdle() == 1) {
        switch (request->mode) {
        case SOUND_LOAD_WEAPON_PAIR:
            first = request->firstId;
            mask = (1 << (first + 1)) | (1 << (request->secondId + 1));
            for (index = 0; gWeaponPairSoundTable[index].mask != 0; ++index) {
                if (gWeaponPairSoundTable[index].mask == mask) {
                    goto foundPair;
                }
            }
            return 1;
        case SOUND_LOAD_FIRST_FIGHTER:
            handle = ResolveCdFileHandle(CD_CATEGORY_SOUND, request->firstId + SOUND_FILE_FIGHTER_BASE);
            StartCdFileRead(handle, request->cdBuffer);
            return 1;
        case SOUND_LOAD_SECOND_FIGHTER:
            StartCdFileRead(ResolveCdFileHandle(CD_CATEGORY_SOUND, request->secondId + SOUND_FILE_FIGHTER_BASE),
                            request->cdBuffer);
            return 1;
        case SOUND_LOAD_DIALOGUE:
            first = request->firstId;
            second = request->secondId;
            if ((unsigned int)first >= 6) {
                first -= 6;
            }
            if ((unsigned int)second >= 6) {
                second -= 6;
            }
            first = first >= 8 ? first - 2 : first;
            second = second >= 8 ? second - 2 : second;
            index = first * 9;
            if (first >= second) {
                index = index + second + SOUND_FILE_DIALOGUE_BASE;
            } else {
                index = index + second + (SOUND_FILE_DIALOGUE_BASE - 1);
            }
            handle = ResolveCdFileHandle(CD_CATEGORY_SOUND, index);
            StartCdFileRead(handle, request->cdBuffer);
            return 1;
        case SOUND_LOAD_MENU:
            handle = ResolveCdFileHandle(CD_CATEGORY_SOUND, SOUND_FILE_MENU);
            StartCdFileRead(handle, request->cdBuffer);
            return 1;
        case SOUND_LOAD_STAGE_AMBIENT:
        case SOUND_LOAD_STAGE_AMBIENT_RELOAD:
            file = gStageAmbientSoundFile[request->stageId];
            if (file == SOUND_AMBIENT_FILE_NONE) {
                return -1;
            }
            handle = ResolveCdFileHandle(CD_CATEGORY_SOUND, file + SOUND_FILE_AMBIENT_BASE);
            StartCdFileRead(handle, request->cdBuffer);
            return 1;
        case SOUND_LOAD_LINK_LOBBY:
            handle = ResolveCdFileHandle(CD_CATEGORY_SOUND, SOUND_FILE_LINK_LOBBY);
            StartCdFileRead(handle, request->cdBuffer);
            return 1;
foundPair:
            handle = ResolveCdFileHandle(CD_CATEGORY_SOUND, gWeaponPairSoundTable[index].file);
            StartCdFileRead(handle, request->cdBuffer);
            return 1;
        case SOUND_LOAD_SLASH_MODE:
            handle = ResolveCdFileHandle(CD_CATEGORY_SOUND, SOUND_FILE_SLASH);
            StartCdFileRead(handle, request->cdBuffer);
            return 1;
        default:
            return 1;
        }
    }
    return 0;
}

static void PatchSlashWeaponBankIds(int bankOffset)
{
    int sound;

    for (sound = SOUND_SLASH_WEAPON_FIRST; sound < SOUND_SLASH_WEAPON_END; sound++) {
        gSlashSoundMap[sound].bank =
            (gSlashSoundMap[sound].bank & SOUND_MAP_PROG_MASK) + (bankOffset + SOUND_SLASH_WEAPON_BANK_BASE);
    }
}

int gSlashEnemySoundVariant;

static void PatchSlashEnemyBankIds(void)
{
    int sound;

    for (sound = SOUND_SLASH_ENEMY_FIRST; sound < SOUND_SLASH_ENEMY_END; sound++) {
        gSlashSoundMap[sound].bank &= SOUND_MAP_PROG_MASK;
        gSlashSoundMap[sound].bank += gSlashEnemySoundVariant + SOUND_SLASH_ENEMY_BANK_BASE;
    }
}

void SelectSlashEnemySoundBank(int fighterId)
{
    gSlashEnemySoundVariant = fighterId - SOUND_SLASH_ENEMY_ID_BASE;
    PatchSlashEnemyBankIds();
    if (gSlashEnemySoundVariant == SOUND_SLASH_ALT_WEAPON_VARIANT) {
        PatchSlashWeaponBankIds(1);
    } else {
        PatchSlashWeaponBankIds(0);
    }
}


int gSoundVabIds[SOUND_VAB_BANK_COUNT];

SoundMapEntry gNormalSoundMap[134] = {
    { 0x00, 0 }, { 0x00, 0 }, { 0x00, 1 }, { 0x00, 2 }, { 0x00, 3 }, { 0x00, 4 },
    { 0x00, 5 }, { 0x00, 6 }, { 0x00, 7 }, { 0x00, 8 }, { 0x00, 9 }, { 0x00, 10 },
    { 0x00, 11 }, { 0x00, 12 }, { 0x00, 13 }, { 0x00, 14 }, { 0x00, 15 }, { 0x80, 0 },
    { 0x80, 1 }, { 0x80, 2 }, { 0x40, 0 }, { 0x40, 1 }, { 0x01, 0 }, { 0x01, 1 },
    { 0x01, 2 }, { 0x01, 3 }, { 0x01, 4 }, { 0x01, 5 }, { 0x01, 6 }, { 0x01, 7 },
    { 0x01, 8 }, { 0x01, 9 }, { 0x01, 10 }, { 0x01, 11 }, { 0x01, 12 }, { 0x01, 13 },
    { 0x01, 14 }, { 0x01, 15 }, { 0x81, 0 }, { 0x81, 1 }, { 0x02, 0 }, { 0x02, 1 },
    { 0x02, 2 }, { 0x02, 3 }, { 0x02, 4 }, { 0x02, 5 }, { 0x02, 6 }, { 0x02, 7 },
    { 0x02, 8 }, { 0x02, 9 }, { 0x03, 0 }, { 0x03, 1 }, { 0x03, 2 }, { 0x03, 3 },
    { 0x03, 4 }, { 0x03, 5 }, { 0x03, 6 }, { 0x03, 7 }, { 0x03, 8 }, { 0x03, 9 },
    { 0x03, 10 }, { 0x03, 11 }, { 0x03, 12 }, { 0x03, 13 }, { 0x03, 14 }, { 0x05, 0 },
    { 0x05, 1 }, { 0x05, 2 }, { 0x05, 3 }, { 0x05, 4 }, { 0x05, 5 }, { 0x05, 6 },
    { 0x05, 7 }, { 0x05, 8 }, { 0x05, 9 }, { 0x05, 10 }, { 0x05, 11 }, { 0x05, 12 },
    { 0x05, 13 }, { 0x05, 14 }, { 0x05, 15 }, { 0x85, 0 }, { 0x85, 1 }, { 0x06, 0 },
    { 0x06, 1 }, { 0x06, 2 }, { 0x06, 3 }, { 0x06, 4 }, { 0x06, 5 }, { 0x06, 6 },
    { 0x06, 7 }, { 0x06, 8 }, { 0x06, 9 }, { 0x04, 0 }, { 0x04, 1 }, { 0x04, 2 },
    { 0x04, 3 }, { 0x04, 4 }, { 0x04, 5 }, { 0x04, 6 }, { 0x04, 7 }, { 0x04, 8 },
    { 0x04, 9 }, { 0x04, 10 }, { 0x04, 11 }, { 0x04, 12 }, { 0x04, 13 }, { 0x04, 14 },
    { 0x07, 0 }, { 0x07, 1 }, { 0x07, 2 }, { 0x07, 3 }, { 0x07, 4 }, { 0x08, 0 },
    { 0x08, 1 }, { 0x08, 2 }, { 0x08, 3 }, { 0x08, 4 }, { 0x08, 5 }, { 0x08, 6 },
    { 0x08, 7 }, { 0x09, 0 }, { 0x09, 1 }, { 0x09, 2 }, { 0x09, 3 }, { 0x09, 4 },
    { 0x09, 5 }, { 0x09, 6 }, { 0x0a, 0 }, { 0x0a, 1 }, { 0x0a, 2 }, { 0x0a, 3 },
    { 0x0a, 4 }, { 0x00, 0 },
};
SoundMapEntry gSlashSoundMap[108] = {
    { 0x00, 0 }, { 0x00, 0 }, { 0x00, 1 }, { 0x00, 2 }, { 0x00, 3 }, { 0x00, 4 },
    { 0x00, 5 }, { 0x00, 6 }, { 0x00, 7 }, { 0x00, 8 }, { 0x00, 9 }, { 0x00, 10 },
    { 0x00, 11 }, { 0x00, 12 }, { 0x00, 13 }, { 0x00, 14 }, { 0x00, 15 }, { 0x80, 0 },
    { 0x80, 1 }, { 0x80, 2 }, { 0x40, 0 }, { 0x40, 1 }, { 0x01, 0 }, { 0x01, 1 },
    { 0x01, 2 }, { 0x01, 3 }, { 0x01, 4 }, { 0x01, 5 }, { 0x01, 6 }, { 0x01, 7 },
    { 0x01, 8 }, { 0x01, 9 }, { 0x01, 10 }, { 0x01, 11 }, { 0x01, 12 }, { 0x01, 13 },
    { 0x01, 14 }, { 0x01, 15 }, { 0x81, 0 }, { 0x81, 1 }, { 0x02, 0 }, { 0x02, 1 },
    { 0x02, 2 }, { 0x02, 3 }, { 0x02, 4 }, { 0x02, 5 }, { 0x02, 6 }, { 0x02, 7 },
    { 0x02, 8 }, { 0x02, 9 }, { 0x03, 0 }, { 0x03, 1 }, { 0x03, 2 }, { 0x03, 3 },
    { 0x03, 4 }, { 0x03, 5 }, { 0x03, 6 }, { 0x03, 7 }, { 0x03, 8 }, { 0x03, 9 },
    { 0x03, 10 }, { 0x03, 11 }, { 0x03, 12 }, { 0x03, 13 }, { 0x03, 14 }, { 0x05, 0 },
    { 0x05, 1 }, { 0x05, 2 }, { 0x05, 3 }, { 0x05, 4 }, { 0x05, 5 }, { 0x05, 6 },
    { 0x05, 7 }, { 0x05, 8 }, { 0x05, 9 }, { 0x05, 10 }, { 0x05, 11 }, { 0x05, 12 },
    { 0x05, 13 }, { 0x05, 14 }, { 0x05, 15 }, { 0x85, 0 }, { 0x85, 1 }, { 0x07, 0 },
    { 0x07, 1 }, { 0x07, 2 }, { 0x07, 3 }, { 0x07, 4 }, { 0x07, 5 }, { 0x07, 6 },
    { 0x07, 7 }, { 0x07, 8 }, { 0x07, 9 }, { 0x03, 0 }, { 0x03, 1 }, { 0x03, 2 },
    { 0x03, 3 }, { 0x03, 4 }, { 0x03, 5 }, { 0x03, 6 }, { 0x03, 7 }, { 0x03, 8 },
    { 0x03, 9 }, { 0x03, 10 }, { 0x03, 11 }, { 0x03, 12 }, { 0x03, 13 }, { 0x03, 14 },
};
unsigned char *gSoundVabHeaders[SOUND_VAB_BANK_COUNT];
char seq_table[SS_SEQ_TABSIZ];
static int gSoundHeaderWorkWords;
SoundMapEntry *gActiveSoundMap;

void InitSoundSystem(int stageId)
{
    unsigned char *empty = (unsigned char *)-1;
    int bank;

    for (bank = 9; bank >= 0; --bank) {
        gSoundVabHeaders[bank] = empty;
    }
    ShutdownSoundSystem();
    SsInit();
    SsUtReverbOff();
    SsSetReservedVoice(0);
    SsSetTableSize(seq_table, 1, 1);
    SsSetTickMode(SS_TICK60);
    if (stageId == STAGE_ID_SLASH) {
        gActiveSoundMap = gSlashSoundMap;
    } else {
        gActiveSoundMap = gNormalSoundMap;
        gSlashEnemySoundVariant = -1;
    }
    if (stageId == STAGE_ID_UNDERGROUND_GROTTO) {
        SsUtSetReverbType(SS_REV_TYPE_HALL);
        SsUtReverbOn();
    } else if (stageId == STAGE_ID_MEIKYOKAN) {
        SsUtSetReverbType(SS_REV_TYPE_ROOM);
        SsUtReverbOn();
    }
    gSoundHeaderWorkWords = 0;
}

unsigned char gCurrentSoundBank;
unsigned char gCurrentSoundTone;

unsigned char *gSoundVabBodies[SOUND_VAB_BODY_MAX];
unsigned int gSoundHeaderWorkPeakBytes; /* read only by its own update */
int gSoundAmbientHeaderWordOffset;

/* A sound archive starts with byte offsets: its VAB header block (start, end), then each further VAB
 * header and each VAB body. In a one-bank archive the body follows the header block. */
typedef struct SoundArchive {
    unsigned int headerStart;
    unsigned int headerEnd;
    unsigned int offsets[12];
} SoundArchive;

int CommitSoundBankLoad(SoundLoadRequest *request)
{
    unsigned int bytes;
    unsigned int highWater;
    int words;
    SoundArchive *archive;
    SoundArchive *ambientArchive;
    unsigned int *dest;
    unsigned int *src;
    unsigned int *end;
    int ambientStartWords;

    if (gStageAmbientSoundFile[request->stageId] == SOUND_AMBIENT_FILE_NONE &&
        request->mode == SOUND_LOAD_STAGE_AMBIENT_RELOAD) {
        return gSoundHeaderWorkWords * 4;
    }
    if (gSoundVabHeaders[9] == (unsigned char *)-1 && request->mode == SOUND_LOAD_STAGE_AMBIENT_RELOAD) {
        request->mode = SOUND_LOAD_STAGE_AMBIENT;
    }

    switch (request->mode) {
    case SOUND_LOAD_WEAPON_PAIR: {
        SoundArchive *archive;
        dest = (unsigned int *)request->headerWork + gSoundHeaderWorkWords;
        archive = (SoundArchive *)request->cdBuffer;
        words = gSoundHeaderWorkWords;
        src = (unsigned int *)archive + archive->headerStart / 4;
        end = (unsigned int *)archive + archive->headerEnd / 4;
        while (src < end) {
            *dest++ = *src++;
            ++words;
        }
        gSoundVabHeaders[0] = (unsigned char *)(archive->offsets[0] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabHeaders[3] = (unsigned char *)(archive->offsets[1] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabHeaders[4] = (unsigned char *)(archive->offsets[2] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabBodies[0] = (unsigned char *)(archive->offsets[3] + (unsigned int)request->cdBuffer);
        gSoundVabBodies[1] = (unsigned char *)(archive->offsets[4] + (unsigned int)request->cdBuffer);
        gSoundVabBodies[2] = (unsigned char *)(archive->offsets[5] + (unsigned int)request->cdBuffer);
        TransferSoundVabSet(gSoundVabHeaders, gSoundVabBodies, SOUND_LOAD_WEAPON_PAIR);
        break;
    }
    case SOUND_LOAD_FIRST_FIGHTER:
        archive = (SoundArchive *)request->cdBuffer;
        dest = (unsigned int *)request->headerWork + gSoundHeaderWorkWords;
        words = gSoundHeaderWorkWords;
        src = (unsigned int *)archive + archive->headerStart / 4;
        end = (unsigned int *)archive + archive->headerEnd / 4;
        while (src < end) {
            *dest++ = *src++;
            ++words;
        }
        gSoundVabHeaders[1] = (unsigned char *)(archive->offsets[0] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabHeaders[2] = (unsigned char *)(archive->offsets[1] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabBodies[0] = (unsigned char *)(archive->offsets[2] + (unsigned int)request->cdBuffer);
        gSoundVabBodies[1] = (unsigned char *)(archive->offsets[3] + (unsigned int)request->cdBuffer);
        TransferSoundVabSet(gSoundVabHeaders, gSoundVabBodies, SOUND_LOAD_FIRST_FIGHTER);
        break;
    case SOUND_LOAD_SECOND_FIGHTER:
        archive = (SoundArchive *)request->cdBuffer;
        dest = (unsigned int *)request->headerWork + gSoundHeaderWorkWords;
        words = gSoundHeaderWorkWords;
        src = (unsigned int *)archive + archive->headerStart / 4;
        end = (unsigned int *)archive + archive->headerEnd / 4;
        while (src < end) {
            *dest++ = *src++;
            ++words;
        }
        gSoundVabHeaders[5] = (unsigned char *)(archive->offsets[0] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabHeaders[6] = (unsigned char *)(archive->offsets[1] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabBodies[0] = (unsigned char *)(archive->offsets[2] + (unsigned int)request->cdBuffer);
        gSoundVabBodies[1] = (unsigned char *)(archive->offsets[3] + (unsigned int)request->cdBuffer);
        TransferSoundVabSet(gSoundVabHeaders, gSoundVabBodies, SOUND_LOAD_SECOND_FIGHTER);
        break;
    case SOUND_LOAD_DIALOGUE:
        archive = (SoundArchive *)request->cdBuffer;
        dest = (unsigned int *)request->headerWork;
        words = gSoundHeaderWorkWords;
        src = (unsigned int *)archive + archive->headerStart / 4;
        end = (unsigned int *)archive + archive->headerEnd / 4;
        while (src < end) {
            *dest++ = *src++;
            ++words;
        }
        gSoundVabHeaders[7] = (unsigned char *)request->headerWork;
        gSoundVabBodies[0] = (unsigned char *)(archive->headerEnd + (unsigned int)request->cdBuffer);
        TransferSoundVabSet(gSoundVabHeaders, gSoundVabBodies, SOUND_LOAD_DIALOGUE);
        break;
    case SOUND_LOAD_MENU:
        archive = (SoundArchive *)request->cdBuffer;
        dest = (unsigned int *)request->headerWork + gSoundHeaderWorkWords;
        words = gSoundHeaderWorkWords;
        src = (unsigned int *)archive + archive->headerStart / 4;
        end = (unsigned int *)archive + archive->headerEnd / 4;
        while (src < end) {
            *dest++ = *src++;
            ++words;
        }
        gSoundVabHeaders[8] = (unsigned char *)((unsigned int *)request->headerWork + gSoundHeaderWorkWords);
        gSoundVabBodies[0] = (unsigned char *)(archive->headerEnd + (unsigned int)request->cdBuffer);
        TransferSoundVabSet(gSoundVabHeaders, gSoundVabBodies, SOUND_LOAD_MENU);
        break;
    case SOUND_LOAD_STAGE_AMBIENT:
        ambientArchive = (SoundArchive *)request->cdBuffer;
        dest = (unsigned int *)request->headerWork + gSoundHeaderWorkWords;
        words = gSoundHeaderWorkWords;
        src = (unsigned int *)ambientArchive + ambientArchive->headerStart / 4;
        end = (unsigned int *)ambientArchive + ambientArchive->headerEnd / 4;
        while (src < end) {
            *dest++ = *src++;
            ++words;
        }
        ambientStartWords = gSoundHeaderWorkWords;
        gSoundVabHeaders[9] = (unsigned char *)((unsigned int *)request->headerWork + ambientStartWords);
        gSoundAmbientHeaderWordOffset = ambientStartWords;
        gSoundVabBodies[0] = (unsigned char *)(ambientArchive->headerEnd + (unsigned int)request->cdBuffer);
        TransferSoundVabSet(gSoundVabHeaders, gSoundVabBodies, SOUND_LOAD_STAGE_AMBIENT);
        break;
    case SOUND_LOAD_STAGE_AMBIENT_RELOAD:
        ambientArchive = (SoundArchive *)request->cdBuffer;
        dest = (unsigned int *)request->headerWork + gSoundAmbientHeaderWordOffset;
        words = gSoundAmbientHeaderWordOffset;
        src = (unsigned int *)ambientArchive + ambientArchive->headerStart / 4;
        end = (unsigned int *)ambientArchive + ambientArchive->headerEnd / 4;
        while (src < end) {
            *dest++ = *src++;
            ++words;
        }
        gSoundVabBodies[0] = (unsigned char *)(ambientArchive->headerEnd + (unsigned int)request->cdBuffer);
        TransferSoundVabSet(gSoundVabHeaders, gSoundVabBodies, SOUND_LOAD_STAGE_AMBIENT_RELOAD);
        break;
    case SOUND_LOAD_LINK_LOBBY:
        archive = (SoundArchive *)request->cdBuffer;
        dest = (unsigned int *)request->headerWork + gSoundHeaderWorkWords;
        words = gSoundHeaderWorkWords;
        src = (unsigned int *)archive + archive->headerStart / 4;
        end = (unsigned int *)archive + archive->headerEnd / 4;
        while (src < end) {
            *dest++ = *src++;
            ++words;
        }
        gSoundVabHeaders[10] = (unsigned char *)((unsigned int *)request->headerWork + gSoundHeaderWorkWords);
        gSoundVabBodies[0] = (unsigned char *)(archive->headerEnd + (unsigned int)request->cdBuffer);
        TransferSoundVabSet(gSoundVabHeaders, gSoundVabBodies, SOUND_LOAD_LINK_LOBBY);
        break;
    case SOUND_LOAD_SLASH_MODE: {
        SoundArchive *archive;

        dest = (unsigned int *)request->headerWork + gSoundHeaderWorkWords;
        archive = (SoundArchive *)request->cdBuffer;
        words = gSoundHeaderWorkWords;
        src = (unsigned int *)archive + archive->headerStart / 4;
        end = (unsigned int *)archive + archive->headerEnd / 4;
        while (src < end) {
            *dest++ = *src++;
            ++words;
        }
        gSoundVabBodies[0] = (unsigned char *)(archive->offsets[6] + (unsigned int)request->cdBuffer);
        gSoundVabBodies[1] = (unsigned char *)(archive->offsets[7] + (unsigned int)request->cdBuffer);
        gSoundVabBodies[2] = (unsigned char *)(archive->offsets[8] + (unsigned int)request->cdBuffer);
        gSoundVabBodies[3] = (unsigned char *)(archive->offsets[9] + (unsigned int)request->cdBuffer);
        gSoundVabBodies[4] = (unsigned char *)(archive->offsets[10] + (unsigned int)request->cdBuffer);
        gSoundVabBodies[5] = (unsigned char *)(archive->offsets[11] + (unsigned int)request->cdBuffer);
        gSoundVabHeaders[5] = (unsigned char *)(archive->offsets[0] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabHeaders[6] = (unsigned char *)(archive->offsets[1] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabHeaders[7] = (unsigned char *)(archive->offsets[2] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabHeaders[8] = (unsigned char *)(archive->offsets[3] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabHeaders[9] = (unsigned char *)(archive->offsets[4] - archive->headerStart +
                                                (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        gSoundVabHeaders[10] = (unsigned char *)(archive->offsets[5] - archive->headerStart +
                                                 (unsigned int)request->headerWork + gSoundHeaderWorkWords * 4U);
        TransferSoundVabSet(gSoundVabHeaders, gSoundVabBodies, SOUND_LOAD_SLASH_MODE);
        break;
    }
    }
    highWater = gSoundHeaderWorkPeakBytes;
    gSoundHeaderWorkWords = words;
    bytes = (unsigned int)gSoundHeaderWorkWords << 2;
    if (highWater < bytes) {
        gSoundHeaderWorkPeakBytes = bytes;
    }
    return bytes;
}

/* returns no value */
int FlushQueuedSounds(void)
{
    short voice;
    int program;
    SoundMapEntry *entry;
    unsigned char selector;
    int bank;
    int tone;

    for (voice = 0; voice < SOUND_VOICE_COUNT; ++voice) {
        program = 0;
        if ((unsigned short)gSoundVoiceQueue[voice].soundId != 0) {
            SsUtKeyOffV(voice);
            entry = &gActiveSoundMap[(unsigned short)gSoundVoiceQueue[voice].soundId];
            selector = entry->bank;
            gCurrentSoundBank = selector;
            gCurrentSoundTone = entry->tone;
            if (selector & SOUND_MAP_PROG_MASK) {
                program = 2;
                if (selector & SOUND_MAP_PROG1_FLAG) {
                    program = 1;
                }
            }
            tone = gCurrentSoundTone;
            bank = gCurrentSoundBank & SOUND_MAP_BANK_MASK;
            gCurrentSoundBank = bank;
            SsUtKeyOnV(voice, gSoundVabIds[bank], program, tone, SOUND_DEFAULT_NOTE, 0,
                       gSoundVoiceQueue[voice].volLeft, gSoundVoiceQueue[voice].volRight);
            gSoundVoiceQueue[voice].soundId = 0;
            gSoundVoiceQueue[voice].volLeft = SOUND_MAX_VOLUME;
            gSoundVoiceQueue[voice].volRight = SOUND_MAX_VOLUME;
        }
    }
}

void TransferSoundVabSet(unsigned char **headers, unsigned char **bodies, int bankSet)
{
    short vabId;
    int voice;

    switch (bankSet) {
    case SOUND_LOAD_WEAPON_PAIR:
        vabId = SsVabOpenHead(headers[0], 0);
        gSoundVabIds[0] = vabId;
        SsVabTransBody(bodies[0], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        vabId = SsVabOpenHead(headers[3], 3);
        gSoundVabIds[3] = vabId;
        SsVabTransBody(bodies[1], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        vabId = SsVabOpenHead(headers[4], 4);
        gSoundVabIds[4] = vabId;
        SsVabTransBody(bodies[2], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        return;
    case SOUND_LOAD_FIRST_FIGHTER:
        vabId = SsVabOpenHead(headers[1], 1);
        gSoundVabIds[1] = vabId;
        SsVabTransBody(bodies[0], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        vabId = SsVabOpenHead(headers[2], 2);
        gSoundVabIds[2] = vabId;
        SsVabTransBody(bodies[1], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        return;
    case SOUND_LOAD_SECOND_FIGHTER:
        vabId = SsVabOpenHead(headers[5], 5);
        gSoundVabIds[5] = vabId;
        SsVabTransBody(bodies[0], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        vabId = SsVabOpenHead(headers[6], 6);
        gSoundVabIds[6] = vabId;
        SsVabTransBody(bodies[1], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        return;
    case SOUND_LOAD_DIALOGUE:
        for (voice = 0; voice < SOUND_VOICE_COUNT; ++voice) {
            SsUtKeyOffV(voice);
        }
        SsVabClose(0);
        SsVabClose(1);
        SsVabClose(2);
        SsVabClose(3);
        SsVabClose(4);
        SsVabClose(5);
        SsVabClose(6);
        vabId = SsVabOpenHead(headers[7], 7);
        gSoundVabIds[7] = vabId;
        SsVabTransBody(bodies[0], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        return;
    case SOUND_LOAD_MENU:
        vabId = SsVabOpenHead(headers[8], 8);
        gSoundVabIds[8] = vabId;
        SsVabTransBody(bodies[0], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        return;
    case SOUND_LOAD_STAGE_AMBIENT_RELOAD:
        for (voice = 0; voice < SOUND_VOICE_COUNT; ++voice) {
            SsUtKeyOffV(voice);
        }
        SsVabClose(9);
        /* fall through */
    case SOUND_LOAD_STAGE_AMBIENT:
        vabId = SsVabOpenHead(headers[9], 9);
        gSoundVabIds[9] = vabId;
        SsVabTransBody(bodies[0], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        return;
    case SOUND_LOAD_LINK_LOBBY:
        vabId = SsVabOpenHead(headers[10], 10);
        gSoundVabIds[10] = vabId;
        SsVabTransBody(bodies[0], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        return;
    case SOUND_LOAD_SLASH_MODE:
        SsVabClose(4);
        vabId = SsVabOpenHead(headers[5], 5);
        gSoundVabIds[5] = vabId;
        SsVabTransBody(bodies[0], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        vabId = SsVabOpenHead(headers[6], 6);
        gSoundVabIds[6] = vabId;
        SsVabTransBody(bodies[1], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        vabId = SsVabOpenHead(headers[7], 7);
        gSoundVabIds[7] = vabId;
        SsVabTransBody(bodies[2], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        vabId = SsVabOpenHead(headers[8], 8);
        gSoundVabIds[8] = vabId;
        SsVabTransBody(bodies[3], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        vabId = SsVabOpenHead(headers[9], 9);
        gSoundVabIds[9] = vabId;
        SsVabTransBody(bodies[4], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
        vabId = SsVabOpenHead(headers[10], 10);
        gSoundVabIds[10] = vabId;
        SsVabTransBody(bodies[5], vabId);
        SsVabTransCompleted(SS_WAIT_COMPLETED);
    default:
        return;
    }
}

void StartSoundSystem(int stageId)
{
    int slot;

    if (stageId == STAGE_ID_UNDERGROUND_GROTTO) {
        SsUtSetReverbDepth(SOUND_REVERB_DEPTH_HALL, SOUND_REVERB_DEPTH_HALL);
    } else if (stageId == STAGE_ID_MEIKYOKAN) {
        SsUtSetReverbDepth(SOUND_REVERB_DEPTH_ROOM, SOUND_REVERB_DEPTH_ROOM);
    }
    SsStart();
    SsSetMVol(SOUND_MAX_VOLUME, SOUND_MAX_VOLUME);
    SsSetStereo();

    for (slot = 0; slot < SOUND_VOICE_COUNT; ++slot) {
        gSoundVoiceQueue[slot].soundId = 0;
        gSoundVoiceQueue[slot].volRight = gSoundVoiceQueue[slot].volLeft = SOUND_MAX_VOLUME;
    }
}
