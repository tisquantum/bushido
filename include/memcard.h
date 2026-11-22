#ifndef MEMCARD_H
#define MEMCARD_H

#define MEMORY_CARD_PORT 0
#define MEMORY_CARD_SLOT 0

typedef enum MemoryCardPhase {
    MEMORY_CARD_PHASE_IDLE = 0,
    MEMORY_CARD_PHASE_PROBE = 1,
    MEMORY_CARD_PHASE_SAVE = 3,
    MEMORY_CARD_PHASE_WAIT_WRITE = 4,
    MEMORY_CARD_PHASE_LOAD = 5,
    MEMORY_CARD_PHASE_WAIT_READ = 6,
    MEMORY_CARD_PHASE_FORMAT = 7
} MemoryCardPhase;

/* The header a save file starts with, which the card's directory screen shows. */
typedef struct MemoryCardSaveHeader {
    char magic[2]; /* 'S', 'C' */
    char iconFormat;
    char blockCount;
    char title[64]; /* Shift JIS */
    char unused[28];
    unsigned short palette[16];
    unsigned short iconFrames[3][64]; /* 16x16, 4 bits a pixel */
} MemoryCardSaveHeader;

#define MEMORY_CARD_SAVE_BLOCKS 1
#define MEMORY_CARD_SAVE_HEADER_SIZE 0x200
#define MEMORY_CARD_SAVE_SIZE 0x2a0        /* the header and 0xa0 bytes of encoded save word */
#define MEMORY_CARD_NO_CARD_RETRY_LIMIT 4

typedef enum MemoryCardCommand {
    MEMORY_CARD_COMMAND_LOAD = 0,
    MEMORY_CARD_COMMAND_SAVE = 1,
    MEMORY_CARD_COMMAND_OVERWRITE_SAVE = 2,
    MEMORY_CARD_COMMAND_FORMAT = 3
} MemoryCardCommand;

/* What UpdateMemoryCard returns. */
typedef enum MemoryCardStatus {
    MEMORY_CARD_STATUS_IDLE = 0,
    MEMORY_CARD_STATUS_SAVING = 1,
    MEMORY_CARD_STATUS_SAVE_OK = 2,
    MEMORY_CARD_STATUS_SAVE_ERROR = 3,
    MEMORY_CARD_STATUS_LOADING = 4,
    MEMORY_CARD_STATUS_LOAD_OK = 5,
    MEMORY_CARD_STATUS_LOAD_ERROR = 6,
    MEMORY_CARD_STATUS_NO_FREE_BLOCKS = 7,
    MEMORY_CARD_STATUS_NO_CARD = 8,
    MEMORY_CARD_STATUS_FORMATTING = 9,
    MEMORY_CARD_STATUS_UNFORMATTED = 10,
    MEMORY_CARD_STATUS_FORMAT_OK = 11,
    MEMORY_CARD_STATUS_FORMAT_ERROR = 12, /* handled by the menus; nothing returns it */
    MEMORY_CARD_STATUS_EXISTS = 13,
    MEMORY_CARD_STATUS_NO_DATA = 14,
    MEMORY_CARD_STATUS_CORRUPT_DATA = 15,
    MEMORY_CARD_STATUS_COUNT = 16
} MemoryCardStatus;

unsigned int PackSaveFlags(void);
void UnpackSaveFlags(unsigned int flags);
void BeginMemoryCardLoad(void);
void BeginMemoryCardSave(void);
void BeginMemoryCardOverwriteSave(void);
void BeginMemoryCardFormat(void);
int UpdateMemoryCard(void);
void InitAutoSaveState(void);
void UpdateAutoSaveState(void);
void InitBootAutoLoadState(void);
void UpdateBootAutoLoadState(void);

#endif
