/* Set LIBSND voice-manager stereo/mono output mode (SsSetMono, SsSetStereo). */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_vm_int.h"

void SsSetMono(void)
{
    _svm_stereo_mono = SVM_MODE_MONO;
}

void SsSetStereo(void)
{
    _svm_stereo_mono = SVM_MODE_STEREO;
}
