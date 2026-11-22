/*
 * Waiting for vertical blank, and the vertical and horizontal blank counts.
 */
#include <sys/types.h>
#include <stdio.h>
#include <kernel.h>
#include <libetc.h>
#include "etc_int.h"

static void v_wait(long target_vcount, long timeout);

/* "Ps", a byte that differs by library, then five more; nothing reads them */
static long etc_id[2] = {0x0a067350, 0x1036d51a};
static volatile u_long *GPU_STATUS = (volatile u_long *)GPU_STATUS_ADDR;
static volatile u_long *RCNT1_COUNT = (volatile u_long *)RCNT1_COUNT_ADDR;
long Hcount = 0;
static long last_vcount = 0;

int VSync(int mode)
{
    /* this first read is not used */
    u_long status = *GPU_STATUS;
    long hcount = (*RCNT1_COUNT - Hcount) & 0xffff;

    if (mode < 0) {
        return Vcount;
    }
    if (mode == 1) {
        return hcount;
    }
    v_wait(mode > 0 ? last_vcount + (mode - 1) : last_vcount, mode > 0 ? mode - 1 : 0);
    status = *GPU_STATUS;
    v_wait(Vcount + 1, 1);
    if (status & GPUSTAT_VRES_480) {
        while (!((status ^ *GPU_STATUS) & GPUSTAT_INTERLACE_FIELD)) {
        }
    }
    last_vcount = Vcount;
    Hcount = *RCNT1_COUNT;
    return hcount;
}

static void v_wait(long target_vcount, long timeout)
{
    volatile long left;

    left = timeout << 15;
    while (Vcount < target_vcount) {
        if (--left == -1) {
            puts("VSync: timeout\n");
            ChangeClearPAD(0);
            ChangeClearRCnt(RCNT_VBLANK, 0);
            return;
        }
    }
}
