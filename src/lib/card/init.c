/*
 * Memory card initialization, start, and stop routines.
 */
#include <sys/types.h>
#include <kernel.h>
#include "card_int.h"

/* "Ps", a byte that differs by library, then five more; nothing reads them */
static long card_id[2] = {0x0a037350, 0x1036d31a};

void InitCARD(long val)
{
    ChangeClearPAD(0);
    EnterCriticalSection();
    InitCARD2(val);
    _patch_card();
    _patch_card2();
    ExitCriticalSection();
}

/* returns no value */
long StartCARD(void)
{
    EnterCriticalSection();
    StartCARD2();
    ChangeClearPAD(0);
    ExitCriticalSection();
}

/* returns no value */
long StopCARD(void)
{
    StopCARD2();
    _patch_card2();
    _ExitCard();
}
