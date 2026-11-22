/* Cold-start initialization for the sound library. */

#include <sys/types.h>
#include <libetc.h>
#include <libspu.h>
#include <libsnd.h>

#include "snd_int.h"

void SsInit(void)
{
    ResetCallback();
    SpuInit();
    _SsInit();
}
