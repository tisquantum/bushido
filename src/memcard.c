/*
 * Memory card saves: the options and unlocks packed into one encoded 32-bit word, the save file's
 * header and icon, and the save, load and format machine the auto-save and boot auto-load states drive.
 */
#include <sys/types.h>
#include <rand.h>
#include <strings.h>
#include <kernel.h>
#include <libgte.h>
#include <libgpu.h>
#include "pad.h"
#include "cd.h"
#include "game_state.h"
#include "globals.h"
#include "main.h"
#include "memcard.h"
#include "memcard_io.h"
#include "menu_state.h"
#include "options_ui.h"

enum {
    SAVE_FLAG_DIFFICULTY_BIT = 0,
    SAVE_FLAG_WALL_BUMP_BIT = 1,
    SAVE_FLAG_BGM_BIT = 2,
    SAVE_FLAG_MONOCHROME_BIT = 3,
    SAVE_FLAG_CONTINUE_BIT = 4,
    SAVE_FLAG_DAYTIME_BIT = 5,
    SAVE_FLAG_AUTO_SAVE_BIT = 6,
    SAVE_FLAG_RUMBLE_BIT = 7,
    SAVE_FLAG_KATZE_UNLOCKED_BIT = 8,
    SAVE_FLAG_UNLOCKED_MOVIES_SHIFT = 16
};

unsigned int PackSaveFlags(void)
{
    unsigned int flags;

    flags = gOptionDifficulty & 1;
    flags |= (gOptionWallBump & 1) << SAVE_FLAG_WALL_BUMP_BIT;
    flags |= (gOptionBgmEnabled & 1) << SAVE_FLAG_BGM_BIT;
    flags |= (gOptionMonochrome & 1) << SAVE_FLAG_MONOCHROME_BIT;
    flags |= (gOptionContinueEnabled & 1) << SAVE_FLAG_CONTINUE_BIT;
    flags |= (gOptionDaytime & 1) << SAVE_FLAG_DAYTIME_BIT;
    flags |= (gOptionAutoSave & 1) << SAVE_FLAG_AUTO_SAVE_BIT;
    flags |= (gOptionRumbleEnabled & 1) << SAVE_FLAG_RUMBLE_BIT;
    flags |= (gKatzeUnlocked & 1) << SAVE_FLAG_KATZE_UNLOCKED_BIT;
    flags |= gUnlockedMoviesMask << SAVE_FLAG_UNLOCKED_MOVIES_SHIFT;
    return flags;
}

void UnpackSaveFlags(unsigned int flags)
{
    gOptionDifficulty = flags & 1;
    gOptionWallBump = (flags >> SAVE_FLAG_WALL_BUMP_BIT) & 1;
    gOptionBgmEnabled = (flags >> SAVE_FLAG_BGM_BIT) & 1;
    gOptionMonochrome = (flags >> SAVE_FLAG_MONOCHROME_BIT) & 1;
    gOptionContinueEnabled = (flags >> SAVE_FLAG_CONTINUE_BIT) & 1;
    gOptionDaytime = (flags >> SAVE_FLAG_DAYTIME_BIT) & 1;
    gOptionAutoSave = (flags >> SAVE_FLAG_AUTO_SAVE_BIT) & 1;
    gOptionRumbleEnabled = (flags >> SAVE_FLAG_RUMBLE_BIT) & 1;
    gKatzeUnlocked = (flags >> SAVE_FLAG_KATZE_UNLOCKED_BIT) & 1;
    gUnlockedMoviesMask = flags >> SAVE_FLAG_UNLOCKED_MOVIES_SHIFT;
}



/* The encoded save word, 0xA0 bytes: 32 random key bytes, then 32 copies of the word, each rotated
 * and perhaps inverted by its key; a load accepts a word 17 copies agree on. */
#define SAVE_PAYLOAD_COPIES 32
#define SAVE_PAYLOAD_MAJORITY 16

static void EncodeSavePayload(unsigned char *data)
{
    unsigned char keys[SAVE_PAYLOAD_COPIES];
    unsigned int *words;
    unsigned int flags;
    unsigned int word;
    int i;
    int shift;

    flags = PackSaveFlags();
    for (i = 0; i < SAVE_PAYLOAD_COPIES; i++) {
        data[i] = keys[i] = rand();
    }

    words = (unsigned int *)(data + SAVE_PAYLOAD_COPIES);
    for (i = 0; i < SAVE_PAYLOAD_COPIES; i++, words++) {
        shift = (keys[i] + i) & 31;
        /* The processor shifts by the count's low five bits: 32 shifts by 0. */
        word = (flags >> shift) | (flags << (32 - shift));
        *words = word;
        if (keys[i] & i) {
            *words = ~word;
        }
    }
}


static int DecodeSavePayload(unsigned char *data)
{
    unsigned char keys[SAVE_PAYLOAD_COPIES];
    unsigned int decoded[SAVE_PAYLOAD_COPIES];
    const unsigned int *encoded;
    unsigned int word;
    int i, j, count;

    for (i = 0; i < SAVE_PAYLOAD_COPIES; i++) {
        keys[i] = data[i];
    }

    encoded = (const unsigned int *)(data + SAVE_PAYLOAD_COPIES);
    for (i = 0; i < SAVE_PAYLOAD_COPIES; ++i, ++encoded) {
        j = (keys[i] + i) & 31;
        word = *encoded;
        if (keys[i] & i)
            word = ~word;
        decoded[i] = (word >> (32 - j)) | (word << j);
    }

    for (j = 0; j < SAVE_PAYLOAD_MAJORITY; ++j) {
        word = decoded[j];
        count = 0;
        for (i = j + 1; i < SAVE_PAYLOAD_COPIES; i++) {
            if (word == decoded[i])
                ++count;
        }
        if (count >= SAVE_PAYLOAD_MAJORITY)
            break;
    }

    if (j != SAVE_PAYLOAD_MAJORITY) {
        UnpackSaveFlags(word);
        return 1;
    }
    return 0;
}

static void ClearSaveBuffer(void)
{
    int *word;
    int i;

    word = (int *)gSharedWorkBuffer;
    for (i = MEMORY_CARD_SAVE_SIZE / 4 - 1; i >= 0; i--) {
        *word++ = 0;
    }
}

/* The save header takes the first 16; nothing reads the 16 zero entries after them. */
const unsigned short gMemoryCardIconPalette[32] = {
    0x0000, 0x0000, 0x8842, 0x8c63, 0x9084, 0x9ebf, 0xa529, 0xe231, 0xeeb4, 0xfc00, 0xfcc2, 0xfda5,
    0xfecb, 0xff38, 0xfff1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};
const unsigned short gMemoryCardIconFrames[3][64] = {
    {
        0xc17b, 0x9caa, 0x11a1, 0x1111, 0x178b, 0xabce, 0xc119, 0x1111,
        0x78db, 0xbbe1, 0x111a, 0x11a1, 0x8db1, 0xcc17, 0x1a9b, 0x1191,
        0xdba1, 0xe178, 0xabbc, 0x1111, 0xba91, 0x178d, 0xbcce, 0xc111,
        0x1111, 0x78db, 0xace1, 0x1a11, 0x1911, 0x8dba, 0xbe17, 0x111a,
        0x1111, 0xdba1, 0xe178, 0x19ab, 0x1a11, 0xba91, 0x178d, 0x1abc,
        0x1111, 0x1111, 0x78db, 0x13b1, 0x1111, 0x9111, 0x8db1, 0x1138,
        0x1111, 0x1111, 0xdba1, 0x1653, 0x1111, 0x1111, 0x3a11, 0x6544,
        0x1111, 0x1111, 0x131a, 0x5441, 0x1111, 0x1111, 0x1111, 0x4411,
    },
    {
        0xc17c, 0xabbc, 0x1111, 0x1111, 0x178c, 0xecee, 0x919b, 0x11c1,
        0x78db, 0xeee1, 0x9abc, 0x11a1, 0x8deb, 0xec17, 0xabce, 0x1111,
        0xdeba, 0xc178, 0xbcee, 0x119a, 0xcba9, 0x178d, 0xeeec, 0xc11b,
        0xba11, 0x78dc, 0xcec1, 0x119b, 0xb111, 0x8dec, 0xec17, 0x11ac,
        0xab11, 0xdeeb, 0xe178, 0x19bc, 0x1111, 0xeeca, 0x178d, 0x9ace,
        0x111b, 0xecb1, 0x78de, 0x13e1, 0x1111, 0xcbaa, 0x8dee, 0x1138,
        0x9111, 0xba11, 0xdecc, 0x1653, 0x1111, 0xa111, 0x39bb, 0x6544,
        0x1111, 0x1b11, 0x1391, 0x5441, 0x1111, 0x1111, 0x1111, 0x4411,
    },
    {
        0x9179, 0x1119, 0x111b, 0x1111, 0x1789, 0x1c9b, 0xba11, 0x11a1,
        0x78d1, 0xc1b1, 0xccc1, 0xe1ac, 0x8d91, 0xe917, 0xcbec, 0x11cb,
        0xd911, 0x9178, 0xecec, 0x111c, 0x9ee1, 0x178d, 0xeecb, 0x11ce,
        0xcecb, 0x78d9, 0xecb1, 0x11be, 0x1c11, 0x8de1, 0xeb17, 0xabcc,
        0xc111, 0xdcc1, 0xc178, 0x1a1a, 0xeba1, 0xceee, 0x178d, 0x111c,
        0xecba, 0xeecc, 0x78de, 0x1391, 0x1b11, 0xce11, 0x8dee, 0x1138,
        0x1111, 0x11ba, 0xdcec, 0x1653, 0xc11e, 0x11be, 0x3bec, 0x6544,
        0xb111, 0x111c, 0x13cb, 0x5441, 0x1e11, 0xa111, 0x1111, 0x4411,
    },
};

/* The save file's title, Shift JIS full-width: the game's name padded with ideographic spaces,
 * then the name with the versus mode's Katze: "[+Katze (VS mode)]", in Japan "[Katze joins
 * the versus mode!]" after the name in katakana. */
char *gMemoryCardTitles[] = {
#ifdef VERSION_JP
    "\203u\203V\203h\201[\203u\203\214\201[\203h\201@\201@\201@\201@\201@\201@\201@\201@"
    "\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@",
    "\203u\203V\203h\201[\203u\203\214\201[\203h\201@\201m\221\316\220\355\203\202\201[\203h\202\311"
    "\203J\203b\203c\203F\214\251\216Q\201I\201n\201@\201@\201@\201@\201@\201@\201@\201@",
#else
    "\202a\202\225\202\223\202\210\202\211\202\204\202\217\201@\202a\202\214\202\201\202\204\202\205\201@\201@\201@"
    "\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@\201@",
    "\202a\202\225\202\223\202\210\202\211\202\204\202\217\201@\202a\202\214\202\201\202\204\202\205\201@\201m\201{"
    "\202j\202\201\202\224\202\232\202\205\201i\202u\202r\201@\202\215\202\217\202\204\202\205\201j\201n\201@",
#endif
};

/* 255: none. */
unsigned char gMenuSoundIds[36] = {
    114, 114, 115, 118, 114, 114, 115, 114, 114, 113, 120, 119, 119, 119, 119, 255, 117, 255, 114,
    114, 115, 114, 114, 113, 115, 120, 114, 114, 115, 117, 120, 130, 128, 129, 131, 132,
};

/* 0x11 shows the first icon frame alone, 0x13 cycles through three. */
#ifdef VERSION_JP
#define MEMORY_CARD_ICON_FORMAT 0x13
#else
#define MEMORY_CARD_ICON_FORMAT 0x11
#endif

static void BuildMemoryCardHeader(MemoryCardSaveHeader *header)
{
    int title = gKatzeUnlocked;
    int i, j;

    header->magic[0] = 'S';
    header->magic[1] = 'C';
    header->iconFormat = MEMORY_CARD_ICON_FORMAT;
    header->blockCount = MEMORY_CARD_SAVE_BLOCKS;
    for (i = 0; i < 64; i++) {
        header->title[i] = 0;
    }
    strcpy(header->title, gMemoryCardTitles[title]); /* past: every title is 64 bytes; its NUL lands in unused[0] */
    for (i = 0; i < 28; i++) {
        header->unused[i] = 0;
    }
    for (i = 0; i < 16; i++) {
        header->palette[i] = gMemoryCardIconPalette[i];
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 64; j++) {
            header->iconFrames[i][j] = gMemoryCardIconFrames[i][j];
        }
    }
}

/* A save file's name: the region's prefix, the disc's product code, the game's own tag. */
#if defined(VERSION_JP)
char *gMemoryCardFilename = "BISLPM-86020BUSHIDO1";
#elif defined(VERSION_EU)
char *gMemoryCardFilename = "BESCES-00899BUSHIDO1";
#else
char *gMemoryCardFilename = "BASCUS-94180BUSHIDO1";
#endif

int gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
short gMemoryCardStatus;

int gMemoryCardProbeResult = 0;
short gMemoryCardCommand;
short gMemoryCardNoCardRetries;

static void StepMemoryCardCommand(void)
{
    int freeBlocks;
    int create;

    if (gMemoryCardPhase == MEMORY_CARD_PHASE_PROBE) {
        switch (gMemoryCardProbeResult) {
        case MEMORY_CARD_PROBE_OK:
        case MEMORY_CARD_PROBE_NEW_CARD:
            if (gMemoryCardCommand == MEMORY_CARD_COMMAND_LOAD) {
                gMemoryCardPhase = MEMORY_CARD_PHASE_LOAD;
            } else if (gMemoryCardCommand == MEMORY_CARD_COMMAND_FORMAT) {
                gMemoryCardPhase = MEMORY_CARD_PHASE_FORMAT;
            } else {
                gMemoryCardPhase = MEMORY_CARD_PHASE_SAVE;
            }
            break;
        case MEMORY_CARD_PROBE_UNFORMATTED:
            if (gMemoryCardCommand == MEMORY_CARD_COMMAND_FORMAT) {
                gMemoryCardPhase = MEMORY_CARD_PHASE_FORMAT;
            } else {
                gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
                gMemoryCardStatus = MEMORY_CARD_STATUS_UNFORMATTED;
            }
            break;
        case MEMORY_CARD_PROBE_NO_CARD:
            if (++gMemoryCardNoCardRetries >= MEMORY_CARD_NO_CARD_RETRY_LIMIT) {
                gMemoryCardStatus = MEMORY_CARD_STATUS_NO_CARD;
                gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
            }
            break;
        }
        return;
    }

    switch (gMemoryCardPhase) {
    case MEMORY_CARD_PHASE_SAVE:
        gMemoryCardStatus = MEMORY_CARD_STATUS_SAVING;
        ReadMemoryCardDirectory(MEMORY_CARD_PORT, MEMORY_CARD_SLOT);
        freeBlocks = GetMemoryCardFreeBlocks();
        if (MemoryCardHasFile(gMemoryCardFilename)) {
            create = 0;
            if (gMemoryCardCommand == MEMORY_CARD_COMMAND_SAVE) {
                gMemoryCardStatus = MEMORY_CARD_STATUS_EXISTS;
                gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
                break;
            }
        } else {
            create = 1;
            if (freeBlocks == 0) {
                gMemoryCardStatus = MEMORY_CARD_STATUS_NO_FREE_BLOCKS;
                gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
                break;
            }
        }
        ClearSaveBuffer();
        BuildMemoryCardHeader((MemoryCardSaveHeader *)gSharedWorkBuffer);
        EncodeSavePayload(gSharedWorkBuffer + MEMORY_CARD_SAVE_HEADER_SIZE);
        if (StartMemoryCardWrite(MEMORY_CARD_PORT, MEMORY_CARD_SLOT, gMemoryCardFilename, gSharedWorkBuffer,
                                 MEMORY_CARD_SAVE_BLOCKS, MEMORY_CARD_SAVE_SIZE, create)) {
            close(fd);
            gMemoryCardStatus = MEMORY_CARD_STATUS_SAVE_ERROR;
            gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
            break;
        }
        gMemoryCardPhase = MEMORY_CARD_PHASE_WAIT_WRITE;
        break;
    case MEMORY_CARD_PHASE_LOAD:
        ReadMemoryCardDirectory(MEMORY_CARD_PORT, MEMORY_CARD_SLOT);
        GetMemoryCardFreeBlocks();
        if (!MemoryCardHasFile(gMemoryCardFilename)) {
            gMemoryCardStatus = MEMORY_CARD_STATUS_NO_DATA;
            gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
            break;
        }
        gMemoryCardStatus = MEMORY_CARD_STATUS_LOADING;
        if (StartMemoryCardRead(MEMORY_CARD_PORT, MEMORY_CARD_SLOT, gMemoryCardFilename, gSharedWorkBuffer,
                                MEMORY_CARD_SAVE_SIZE)) {
            close(fd);
            gMemoryCardStatus = MEMORY_CARD_STATUS_LOAD_ERROR;
            gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
            break;
        }
        gMemoryCardPhase = MEMORY_CARD_PHASE_WAIT_READ;
        break;
    case MEMORY_CARD_PHASE_FORMAT:
        FormatMemoryCard(MEMORY_CARD_PORT, MEMORY_CARD_SLOT); /* its result is not read:
                                                                 a format always reports success */
        gMemoryCardStatus = MEMORY_CARD_STATUS_FORMAT_OK;
        gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
        break;
    }
}

static void PollMemoryCardTransfer(void)
{
    int event;

    switch (gMemoryCardPhase) {
    case MEMORY_CARD_PHASE_WAIT_WRITE:
        event = PollSwCardEvent();
        if (event != MEMORY_CARD_EVENT_NONE) {
            close(fd);
            if (event == MEMORY_CARD_EVENT_IOE) {
                gMemoryCardStatus = MEMORY_CARD_STATUS_SAVE_OK;
            } else {
                gMemoryCardStatus = MEMORY_CARD_STATUS_SAVE_ERROR;
            }
            gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
        } else {
            gMemoryCardStatus = MEMORY_CARD_STATUS_SAVING;
        }
        break;
    case MEMORY_CARD_PHASE_WAIT_READ:
        event = PollSwCardEvent();
        if (event != MEMORY_CARD_EVENT_NONE) {
            close(fd);
            if (event == MEMORY_CARD_EVENT_IOE) {
                gMemoryCardStatus = MEMORY_CARD_STATUS_LOAD_OK;
                if (DecodeSavePayload(gSharedWorkBuffer + MEMORY_CARD_SAVE_HEADER_SIZE) == 0) {
                    gMemoryCardStatus = MEMORY_CARD_STATUS_CORRUPT_DATA;
                }
            } else {
                gMemoryCardStatus = MEMORY_CARD_STATUS_LOAD_ERROR;
            }
            gMemoryCardPhase = MEMORY_CARD_PHASE_IDLE;
        } else {
            gMemoryCardStatus = MEMORY_CARD_STATUS_LOADING;
        }
        break;
    }
}


void BeginMemoryCardLoad(void)
{
    gMemoryCardStatus = MEMORY_CARD_STATUS_LOADING;
    gMemoryCardNoCardRetries = 0;
    gMemoryCardCommand = MEMORY_CARD_COMMAND_LOAD;
    gMemoryCardPhase = MEMORY_CARD_PHASE_PROBE;
}

void BeginMemoryCardSave(void)
{
    gMemoryCardStatus = MEMORY_CARD_STATUS_SAVING;
    gMemoryCardCommand = MEMORY_CARD_COMMAND_SAVE;
    gMemoryCardNoCardRetries = 0;
    gMemoryCardPhase = MEMORY_CARD_PHASE_PROBE;
}

void BeginMemoryCardOverwriteSave(void)
{
    gMemoryCardStatus = MEMORY_CARD_STATUS_SAVING;
    gMemoryCardCommand = MEMORY_CARD_COMMAND_OVERWRITE_SAVE;
    gMemoryCardNoCardRetries = 0;
    gMemoryCardPhase = MEMORY_CARD_PHASE_PROBE;
}

void BeginMemoryCardFormat(void)
{
    gMemoryCardStatus = MEMORY_CARD_STATUS_FORMATTING;
    gMemoryCardCommand = MEMORY_CARD_COMMAND_FORMAT;
    gMemoryCardNoCardRetries = 0;
    gMemoryCardPhase = MEMORY_CARD_PHASE_PROBE;
}

int UpdateMemoryCard(void)
{
    if (gMemoryCardPhase == MEMORY_CARD_PHASE_IDLE || gMemoryCardPhase == MEMORY_CARD_PHASE_PROBE) {
        gMemoryCardProbeResult = StepMemoryCardProbe(MEMORY_CARD_PORT, MEMORY_CARD_SLOT);
    }
    StepMemoryCardCommand();
    PollMemoryCardTransfer();
    return gMemoryCardStatus;
}

enum {
    AUTO_SAVE_SUBSTATE_SAVING = 0,
    AUTO_SAVE_SUBSTATE_LOAD_ERROR_UI = 1,
    AUTO_SAVE_SUBSTATE_SHOW_ERROR = 2
};

short gAutoSaveUiSubState;

void InitAutoSaveState(void)
{
    gAutoSaveUiSubState = AUTO_SAVE_SUBSTATE_SAVING;
    BeginMemoryCardOverwriteSave();
    gGameState = GAME_STATE_UPDATE_AUTO_SAVE;
}


void UpdateAutoSaveState(void)
{
    if (gAutoSaveUiSubState == AUTO_SAVE_SUBSTATE_SAVING) {
        switch (UpdateMemoryCard()) {
        case MEMORY_CARD_STATUS_SAVE_ERROR:
        case MEMORY_CARD_STATUS_LOAD_ERROR:
        case MEMORY_CARD_STATUS_NO_FREE_BLOCKS:
        case MEMORY_CARD_STATUS_NO_CARD:
        case MEMORY_CARD_STATUS_UNFORMATTED:
        case MEMORY_CARD_STATUS_FORMAT_ERROR:
        case MEMORY_CARD_STATUS_NO_DATA:
            gAutoSaveUiSubState = AUTO_SAVE_SUBSTATE_LOAD_ERROR_UI;
            return;

        case MEMORY_CARD_STATUS_SAVING:
        case MEMORY_CARD_STATUS_LOADING:
        case MEMORY_CARD_STATUS_FORMATTING:
            return;
        }
    } else if (gAutoSaveUiSubState == AUTO_SAVE_SUBSTATE_LOAD_ERROR_UI) {
        StopAndWaitCdIdle();
        DrawSync(0);
        LoadOptionsMenuAssets();
        gDisplayPresentEnabled = 1;
        gAutoSaveUiSubState = AUTO_SAVE_SUBSTATE_SHOW_ERROR;
        return;
    } else {
        DrawAutoSaveErrorOverlay();
        /* Cross on every build, not the region's MENU_CONFIRM. */
        if (!(gPadState.pressed & (_PAD(0, PADRdown) | _PAD(1, PADRdown))))
            return;
    }

    gGameState = gAutoSaveReturnState;
}

void InitBootAutoLoadState(void)
{
    BeginMemoryCardLoad();
    gGameState = GAME_STATE_UPDATE_BOOT_AUTO_LOAD;
}

void UpdateBootAutoLoadState(void)
{
    switch (UpdateMemoryCard()) {
    case MEMORY_CARD_STATUS_SAVING:
    case MEMORY_CARD_STATUS_LOADING:
    case MEMORY_CARD_STATUS_FORMATTING:
        break;
    default:
        gGameState = GAME_STATE_INIT_OPENING;
        break;
    }
}
