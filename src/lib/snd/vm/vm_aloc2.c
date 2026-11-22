/* Set up voice-manager shadow SPU registers for a newly allocated voice. */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_vm_int.h"
#include "vm_aloc2.h"
#include "vm_f.h"

void SpuVmDoAllocate(void)
{
    int i;

    _svm_cur.sreg_offset = _svm_cur.voice * SPU_VREG_COUNT;
    _svm_cur.tone_index = _svm_cur.prog_index * VAB_TONES_PER_PROG + _svm_cur.tone;
    _svm_voice[_svm_cur.voice].envx = SVM_ENVX_MAX;

    for (i = 0; i < SVM_ENVX_HIST_MAX; i++) {
        _svm_envx_hist[i] &= ~(1 << _svm_cur.voice);
    }

    if ((_svm_cur.vag_idx & 1) > 0) {
        _svm_sreg_buf[_svm_cur.sreg_offset + SPU_VREG_START_ADDR] =
            _svm_pg[(_svm_cur.vag_idx - 1) / 2].reserved2 & 0xffff;
        _svm_sreg_dirty[_svm_cur.voice] |= SREG_DIRTY_START_ADDR;
    } else {
        _svm_sreg_buf[_svm_cur.sreg_offset + SPU_VREG_START_ADDR] =
            _svm_pg[(_svm_cur.vag_idx - 1) / 2].reserved2 >> 16;
        _svm_sreg_dirty[_svm_cur.voice] |= SREG_DIRTY_START_ADDR;
    }

    _svm_sreg_buf[_svm_cur.sreg_offset + SPU_VREG_ADSR1] =
        _svm_tn[_svm_cur.prog_index * VAB_TONES_PER_PROG + _svm_cur.tone].adsr1;
    _svm_sreg_buf[_svm_cur.sreg_offset + SPU_VREG_ADSR2] =
        _svm_tn[_svm_cur.prog_index * VAB_TONES_PER_PROG + _svm_cur.tone].adsr2 + _svm_damper;
    _svm_sreg_dirty[_svm_cur.voice] |= SREG_DIRTY_ADSR;
}
