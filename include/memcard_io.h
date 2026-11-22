#ifndef MEMCARD_IO_H
#define MEMCARD_IO_H

#define MEMORY_CARD_BLOCK_SIZE 0x2000
#define MEMORY_CARD_TOTAL_BLOCKS 15
#define MEMORY_CARD_NAME_MAX 20 /* DIRENTRY.name */

/* The bios event that ended a card operation. */
typedef enum MemoryCardEvent {
    MEMORY_CARD_EVENT_NONE = 0,
    MEMORY_CARD_EVENT_IOE = 1,
    MEMORY_CARD_EVENT_ERROR = 2,
    MEMORY_CARD_EVENT_TIMEOUT = 3, /* no card */
    MEMORY_CARD_EVENT_NEW = 4      /* a card inserted since the last access, or not formatted */
} MemoryCardEvent;

typedef enum MemoryCardProbeStep {
    MEMORY_CARD_PROBE_STEP_INFO = 0,
    MEMORY_CARD_PROBE_STEP_WAIT_INFO = 1,
    MEMORY_CARD_PROBE_STEP_LOAD = 2,
    MEMORY_CARD_PROBE_STEP_WAIT_LOAD = 3,
    MEMORY_CARD_PROBE_STEP_PUBLISH = 4
} MemoryCardProbeStep;

typedef enum MemoryCardProbeResult {
    MEMORY_CARD_PROBE_ERROR = -3,
    MEMORY_CARD_PROBE_UNFORMATTED = -2,
    MEMORY_CARD_PROBE_NO_CARD = -1,
    MEMORY_CARD_PROBE_BUSY = 0,
    MEMORY_CARD_PROBE_OK = 1,
    MEMORY_CARD_PROBE_NEW_CARD = 2
} MemoryCardProbeResult;

extern long fd;

void InitMemoryCard(void);
int PollSwCardEvent(void);
int ReadMemoryCardDirectory(int port, int slot);
int GetMemoryCardFreeBlocks(void);
int MemoryCardHasFile(char *filename);
int StartMemoryCardRead(int port, int slot, char *filename, char *buffer, long size);
int StartMemoryCardWrite(int port, int slot, char *filename, char *data, long blocks, long size, int create);
int StepMemoryCardProbe(int port, int slot);
int FormatMemoryCard(int port, int slot);

#endif
