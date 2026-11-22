/*
 * VBlank interrupt dispatcher and VSync callback table (startIntrVSync).
 */
#include <sys/types.h>
#include <libetc.h>
#include "etc_int.h"

#define VSYNC_CALLBACK_MAX      8
#define RCNT1_MODE_HBLANK_VSYNC (RCNT_MODE_EXT_CLOCK | RCNT_MODE_SYNC_WAIT_VBLANK | RCNT_MODE_SYNC_ENABLE)

static void trapIntrVSync(void);
static IntrCallback setIntrVSync(long ch, IntrCallback func);
static void memclr(long *dst, long nwords);

static IntrCallback vsync_callbacks[VSYNC_CALLBACK_MAX] = {0};
volatile long Vcount = 0;
static volatile u_long *RCNT1_MODE = (volatile u_long *)RCNT1_MODE_ADDR;

IntrCallbackSetter startIntrVSync(void)
{
    *RCNT1_MODE = RCNT1_MODE_HBLANK_VSYNC;
    Vcount = 0;
    memclr((long *)vsync_callbacks, VSYNC_CALLBACK_MAX);
    InterruptCallback(PSX_IRQ_VBLANK, trapIntrVSync);
    return setIntrVSync;
}

static void trapIntrVSync(void)
{
    long ch;

    Vcount++;
    for (ch = 0; ch < VSYNC_CALLBACK_MAX; ch++) {
        if (vsync_callbacks[ch]) {
            (*vsync_callbacks[ch])();
        }
    }
}

static IntrCallback setIntrVSync(long ch, IntrCallback func)
{
    IntrCallback old = vsync_callbacks[ch];

    if (func != old) {
        vsync_callbacks[ch] = func;
    }
    return old;
}

static void memclr(long *dst, long nwords)
{
    while (nwords--) {
        *dst++ = 0;
    }
}
