/* Set the maximum number of SPU voices reserved for LIBSND. */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_vm_int.h"

char SsSetReservedVoice(char voices)
{
    if (voices > SPU_VOICE_COUNT || voices == 0) {
        return -1;
    }
    spuVmMaxVoice = voices;
    return spuVmMaxVoice;
}
