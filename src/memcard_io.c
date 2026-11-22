/*
 * Memory card access through the bios: the card events, the directory, file read, write and
 * format, and the probe that finds, a step a call, whether a formatted card is in place.
 */
#include <sys/types.h>
#include <sys/file.h>
#include <kernel.h>
#include <stdio.h>
#include "memcard_io.h"

#define MEMORY_CARD_EVENT_POLL_LIMIT 120

long ev0, ev1, ev2, ev3;     /* SwCARD */
long ev10, ev11, ev12, ev13; /* HwCARD */

void InitMemoryCard(void)
{
    InitCARD(1);
    StartCARD();
    _bu_init();
    ChangeClearPAD(0);

    EnterCriticalSection();
    ev0 = OpenEvent(SwCARD, EvSpIOE, EvMdNOINTR, NULL);
    ev1 = OpenEvent(SwCARD, EvSpERROR, EvMdNOINTR, NULL);
    ev2 = OpenEvent(SwCARD, EvSpTIMOUT, EvMdNOINTR, NULL);
    ev3 = OpenEvent(SwCARD, EvSpNEW, EvMdNOINTR, NULL);
    ev10 = OpenEvent(HwCARD, EvSpIOE, EvMdNOINTR, NULL);
    ev11 = OpenEvent(HwCARD, EvSpERROR, EvMdNOINTR, NULL);
    ev12 = OpenEvent(HwCARD, EvSpTIMOUT, EvMdNOINTR, NULL);
    ev13 = OpenEvent(HwCARD, EvSpNEW, EvMdNOINTR, NULL);
    ExitCriticalSection();

    EnableEvent(ev0);
    EnableEvent(ev1);
    EnableEvent(ev2);
    EnableEvent(ev3);
    EnableEvent(ev10);
    EnableEvent(ev11);
    EnableEvent(ev12);
    EnableEvent(ev13);
}

int event_time_out;

static int PollSwCardEventWithTimeout(void)
{
    int event = MEMORY_CARD_EVENT_NONE;

    if (TestEvent(ev0) == 1) {
        event = MEMORY_CARD_EVENT_IOE;
    }
    if (TestEvent(ev1) == 1) {
        event = MEMORY_CARD_EVENT_ERROR;
    }
    if (TestEvent(ev2) == 1) {
        event = MEMORY_CARD_EVENT_TIMEOUT;
    }
    if (TestEvent(ev3) == 1) {
        event = MEMORY_CARD_EVENT_NEW;
    }
    if (event_time_out++ >= MEMORY_CARD_EVENT_POLL_LIMIT) {
        event = MEMORY_CARD_EVENT_ERROR;
    }
    return event;
}

int PollSwCardEvent(void)
{
    if (TestEvent(ev0) == 1) {
        return MEMORY_CARD_EVENT_IOE;
    }
    if (TestEvent(ev1) == 1) {
        return MEMORY_CARD_EVENT_ERROR;
    }
    if (TestEvent(ev2) == 1) {
        return MEMORY_CARD_EVENT_TIMEOUT;
    }
    if (TestEvent(ev3) == 1) {
        return MEMORY_CARD_EVENT_NEW;
    }
    return MEMORY_CARD_EVENT_NONE;
}

static void ClearSwCardEvents(void)
{
    TestEvent(ev0);
    TestEvent(ev1);
    TestEvent(ev2);
    TestEvent(ev3);
}

static int WaitHwCardEvent(void)
{
    for (;;) {
        if (TestEvent(ev10) == 1) {
            return MEMORY_CARD_EVENT_IOE;
        }
        if (TestEvent(ev11) == 1) {
            return MEMORY_CARD_EVENT_ERROR;
        }
        if (TestEvent(ev12) == 1) {
            return MEMORY_CARD_EVENT_TIMEOUT;
        }
        if (TestEvent(ev13) == 1) {
            return MEMORY_CARD_EVENT_NEW;
        }
    }
}

static void ClearHwCardEvents(void)
{
    TestEvent(ev10);
    TestEvent(ev11);
    TestEvent(ev12);
    TestEvent(ev13);
}

struct DIRENTRY slot_dir[MEMORY_CARD_TOTAL_BLOCKS];

int gMemoryCardDirEntryCount;

int ReadMemoryCardDirectory(int port, int slot)
{
    char path[32];
    struct DIRENTRY *entry = slot_dir;
    int count;

    sprintf(path, "bu%1d%1d:*", port, slot);
    count = 0;
    if (firstfile(path, entry) != NULL) {
        do {
            count++;
            entry++;
        } while (nextfile(entry) != NULL);
    }
    gMemoryCardDirEntryCount = count;
    return count;
}

int GetMemoryCardFreeBlocks(void)
{
    struct DIRENTRY *entries = slot_dir;
    int i;
    int used = 0;

    for (i = 0; i < gMemoryCardDirEntryCount; i++) {
        used += entries[i].size;
    }
    used /= MEMORY_CARD_BLOCK_SIZE;
    return MEMORY_CARD_TOTAL_BLOCKS - used;
}

int MemoryCardHasFile(char *filename)
{
    struct DIRENTRY *entries = slot_dir;
    int i, j;
    int mismatch;

    for (i = 0; i < gMemoryCardDirEntryCount; i++) {
        mismatch = 0;
        /* A 21st name byte that is not the end is compared with the low byte of attr. */
        for (j = 0; j < MEMORY_CARD_NAME_MAX + 1; j++) {
            if (filename[j] == '\0') {
                break;
            }
            if (filename[j] != entries[i].name[j]) {
                mismatch = 1;
                break;
            }
        }
        if (!mismatch) {
            return 1;
        }
    }
    return 0;
}

long fd;

int StartMemoryCardRead(int port, int slot, char *filename, char *buffer, long size)
{
    char path[32];
    long file;

    sprintf(path, "bu%1d%1d:%s", port, slot, filename);
    file = open(path, O_RDONLY | O_NOWAIT);
    if (file == -1) {
        return -1;
    }
    fd = file;
    ClearSwCardEvents();
    ClearHwCardEvents();
    read(file, buffer, size);
    if (WaitHwCardEvent() != MEMORY_CARD_EVENT_IOE) {
        return -1;
    }
    return 0;
}

int StartMemoryCardWrite(int port, int slot, char *filename, char *data, long blocks, long size, int create)
{
    char path[32];
    long file;

    sprintf(path, "bu%1d%1d:%s", port, slot, filename);
    if (create) {
        file = open(path, O_CREAT | (blocks << 16));
        if (file == -1) {
            return -1;
        }
        close(file);
    }
    file = open(path, O_WRONLY | O_NOWAIT);
    if (file == -1) {
        return -1;
    }
    fd = file;
    ClearSwCardEvents();
    ClearHwCardEvents();
    write(file, data, size);
    if (WaitHwCardEvent() != MEMORY_CARD_EVENT_IOE) {
        return -1;
    }
    return 0;
}

int gMemoryCardLoaded = 0;
int gMemoryCardProbeStep = MEMORY_CARD_PROBE_STEP_INFO;
int card_sts_tmp;
int card_sts;

int StepMemoryCardProbe(int port, int slot)
{
    long channel;
    int event;

    channel = (port << 4) + slot;
    switch (gMemoryCardProbeStep) {
    case MEMORY_CARD_PROBE_STEP_INFO:
        _card_info(channel);
        gMemoryCardProbeStep = MEMORY_CARD_PROBE_STEP_WAIT_INFO;
        event_time_out = 0;
        card_sts = MEMORY_CARD_PROBE_BUSY;
        break;
    case MEMORY_CARD_PROBE_STEP_WAIT_INFO:
        event = PollSwCardEventWithTimeout();
        if (event == MEMORY_CARD_EVENT_NONE) {
            break;
        }
        switch (event) {
        case MEMORY_CARD_EVENT_IOE:
            card_sts_tmp = MEMORY_CARD_PROBE_OK;
            if (gMemoryCardLoaded != 0) {
                gMemoryCardProbeStep = MEMORY_CARD_PROBE_STEP_PUBLISH;
            } else {
                gMemoryCardProbeStep = MEMORY_CARD_PROBE_STEP_LOAD;
            }
            break;
        case MEMORY_CARD_EVENT_TIMEOUT:
            card_sts_tmp = MEMORY_CARD_PROBE_NO_CARD;
            gMemoryCardProbeStep = MEMORY_CARD_PROBE_STEP_PUBLISH;
            gMemoryCardLoaded = 0;
            break;
        case MEMORY_CARD_EVENT_NEW:
            card_sts_tmp = MEMORY_CARD_PROBE_NEW_CARD;
            ClearHwCardEvents();
            _card_clear(channel);
            WaitHwCardEvent();
            gMemoryCardProbeStep = MEMORY_CARD_PROBE_STEP_LOAD;
            gMemoryCardLoaded = 0;
            break;
        case MEMORY_CARD_EVENT_ERROR:
        default:
            card_sts_tmp = MEMORY_CARD_PROBE_ERROR;
            gMemoryCardProbeStep = MEMORY_CARD_PROBE_STEP_PUBLISH;
            gMemoryCardLoaded = 0;
            break;
        }
        break;
    case MEMORY_CARD_PROBE_STEP_LOAD:
        ClearSwCardEvents();
        _card_load(channel);
        gMemoryCardProbeStep = MEMORY_CARD_PROBE_STEP_WAIT_LOAD;
        event_time_out = 0;
        break;
    case MEMORY_CARD_PROBE_STEP_WAIT_LOAD:
        event = PollSwCardEventWithTimeout();
        if (event == MEMORY_CARD_EVENT_NONE) {
            break;
        }
        gMemoryCardProbeStep = MEMORY_CARD_PROBE_STEP_PUBLISH;
        switch (event) {
        case MEMORY_CARD_EVENT_IOE:
            gMemoryCardLoaded = 1;
            break;
        case MEMORY_CARD_EVENT_TIMEOUT:
            card_sts_tmp = MEMORY_CARD_PROBE_NO_CARD;
            gMemoryCardLoaded = 0;
            break;
        case MEMORY_CARD_EVENT_NEW:
            card_sts_tmp = MEMORY_CARD_PROBE_UNFORMATTED;
            gMemoryCardLoaded = 0;
            break;
        case MEMORY_CARD_EVENT_ERROR:
        default:
            card_sts_tmp = MEMORY_CARD_PROBE_ERROR;
            gMemoryCardLoaded = 0;
            break;
        }
        break;
    case MEMORY_CARD_PROBE_STEP_PUBLISH:
        card_sts = card_sts_tmp;
        gMemoryCardProbeStep = MEMORY_CARD_PROBE_STEP_INFO;
        break;
    }
    return card_sts;
}

int FormatMemoryCard(int port, int slot)
{
    char path[8];

    sprintf(path, "bu%1d%1d:", port, slot);
    if (format(path) == 0) {
        return -1;
    }
    return 0;
}
