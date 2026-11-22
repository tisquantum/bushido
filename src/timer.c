/* Root counter 1, counting horizontal blanks, free running. 0xf2000001 is kernel.h's RCntCNT1 and
 * 0x2000 its RCntMdNOINTR, written as numbers because kernel.h is not included (SetRCnt below). */

#include "timer.h"

long SetRCnt(); /* not kernel.h's prototype: the original passes the target -1 as an int */
long StartRCnt(unsigned long spec);
long GetRCnt(unsigned long spec);

void InitHBlankRootCounter(void)
{
    SetRCnt(0xf2000001, -1, 0x2000);
    StartRCnt(0xf2000001);
}

/* Nothing calls it. */
static int GetHBlankRootCounter(void)
{
    return GetRCnt(0xf2000001);
}
