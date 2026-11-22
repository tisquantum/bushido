/* The controller driver's start and stop, and the interrupt entry that clears the controller
 * port's control register on each vertical blank. */
#include <sys/types.h>
#include <kernel.h>
#include "api_int.h"
#include "psx_hw.h"

#define PAD_RESET_WAIT 10

static long InitPadFlag = 0;
static volatile struct Sio0Reg *pad = (struct Sio0Reg *)SIO0_REG_BASE;
static volatile struct IntrReg *intr = (struct IntrReg *)INTR_STAT_ADDR;

static struct IntrChainEntry patch0;

static long SetPatchPad(void);
static long RemovePatchPad(void);

void SetInitPadFlag(long flag)
{
    InitPadFlag = flag;
}

long ReadInitPadFlag(void)
{
    return InitPadFlag;
}

long PAD_init(long mode, long *tbl, long arg3, long arg4)
{
    EnterCriticalSection();
    _patch_pad();
    ExitCriticalSection();
    ChangeClearPAD(0);
    SetPatchPad();
    PAD_init2(mode, tbl, arg3, arg4);
    InitPadFlag = 1;
    return 1;
}

long InitPAD(char *bufA, long lenA, char *bufB, long lenB)
{
    EnterCriticalSection();
    _patch_pad();
    ExitCriticalSection();
    ChangeClearPAD(0);
    SetPatchPad();
    InitPAD2(bufA, lenA, bufB, lenB);
    InitPadFlag = 1;
    return 1;
}

long StartPAD(void)
{
    StartPAD2();
    ChangeClearPAD(0);
    return EnablePAD();
}

void StopPAD(void)
{
    DesablePAD();
    StopPAD2();
    RemovePatchPad();
}

static long _Pad1(void);
static long _IsVSync(void);

static inline void setup_patch_rp(long (*handler)(void), long (*check)(void))
{
    patch0.handler = handler;
    patch0.check = check;
    patch0.next = 0;
    patch0.unused = 0;
}

static long SetPatchPad(void)
{
    EnterCriticalSection();
    setup_patch_rp(_Pad1, _IsVSync);
    SysDeqIntRP(PAD_INTR_PRIORITY, &patch0);
    SysEnqIntRP(PAD_INTR_PRIORITY, &patch0);
    ExitCriticalSection();
    return 1;
}

static long RemovePatchPad(void)
{
    EnterCriticalSection();
    SysDeqIntRP(PAD_INTR_PRIORITY, &patch0);
    ExitCriticalSection();
    return 1;
}

static inline void wait(volatile long count)
{
    while (count--) {
    }
}

static long _Pad1(void)
{
    pad->control = 0;
    wait(PAD_RESET_WAIT);
    return 0;
}

static long _IsVSync(void)
{
    if (!(intr->mask & INTR_MASK_VBLANK) || !(intr->stat & INTR_MASK_VBLANK)) {
        return 0;
    }
    return 1;
}
