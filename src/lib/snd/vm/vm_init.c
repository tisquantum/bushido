/* Initialize LIBSND voice-manager state, SPU allocator, and voice registers. */

#include <sys/types.h>
#include <libspu.h>
#include <libsnd.h>

#include "snd_vm_int.h"
#include "spu_int.h"
#include "vm_f.h"
#include "vm_init.h"
#include "vm_nowof.h"

/* SpuInitMalloc gets 32 records starting at the second of these 18 (SpuMalloc writes up to 33): only 17 lie in this table */
static char _ss_spu_vm_rec[SPU_MALLOC_RECSIZ * 18];

void SpuVmInit(unsigned char MaxVoice)
{
    unsigned short i;
    unsigned short sreg_offset;
    unsigned short max_voices;

    _spu_setInTransfer(0);
    _svm_damper = 0;
    SpuInitMalloc(32, &_ss_spu_vm_rec[SPU_MALLOC_RECSIZ]);

    for (i = 0; i < SPU_VOICE_COUNT * SPU_VREG_COUNT; i++) {
        _svm_sreg_buf[i] = 0;
    }
    for (i = 0; i < SPU_VOICE_COUNT; i++) {
        _svm_sreg_dirty[i] = 0;
    }
    _svm_vab_count = 0;
    for (i = 0; i < SVM_VAB_MAX; i++) {
        _svm_vab_used[i] = SVM_VAB_FREE;
    }

    max_voices = MaxVoice;
    if (max_voices >= SPU_VOICE_COUNT) {
        spuVmMaxVoice = SPU_VOICE_COUNT;
    } else {
        spuVmMaxVoice = max_voices;
    }

    for (i = 0; i < spuVmMaxVoice; i++) {
        _svm_voice[i].alloc_state = 24;
        _svm_voice[i].seq_sep_no = -1;
        _svm_voice[i].vag_id = 0xff;
        _svm_voice[i].keystat = KEYSTAT_OFF;
        _svm_voice[i].pitch = 0;
        _svm_voice[i].envx = 0;
        _svm_voice[i].prog_index = 0;
        _svm_voice[i].prog = 0;
        _svm_voice[i].tone = 0xff;
        _svm_voice[i].vol = 0;
        _svm_voice[i].pan = 0x40;
        _svm_voice[i].auto_vol = 0;
        _svm_voice[i].auto_vol_state[0] = 0;
        _svm_voice[i].auto_vol_state[1] = 0;
        _svm_voice[i].auto_vol_state[2] = 0;
        _svm_voice[i].auto_pan = 0;
        _svm_voice[i].auto_pan_state[0] = 0;
        _svm_voice[i].auto_pan_state[1] = 0;
        _svm_voice[i].auto_pan_state[2] = 0;
        _svm_voice[i].auto_pan_state[3] = 0;
        _svm_voice[i].auto_vol_state[3] = 0;

        sreg_offset = i * SPU_VREG_COUNT;
        _svm_sreg[sreg_offset + SPU_VREG_START_ADDR] = 0x200;
        _svm_sreg[sreg_offset + SPU_VREG_PITCH] = 0x1000;
        _svm_sreg[sreg_offset + SPU_VREG_ADSR1] = 0x80ff;
        _svm_sreg[sreg_offset + SPU_VREG_VOL_L] = 0;
        _svm_sreg[sreg_offset + SPU_VREG_VOL_R] = 0;
        _svm_sreg[sreg_offset + SPU_VREG_ADSR2] = 0x4000;

        _svm_cur.voice = i;
        SpuVmKeyOffNow(1);
    }

    _svm_rattr.depth.left = 0x3fff;
    _svm_rattr.depth.right = 0x3fff;
    _svm_okon1 = 0;
    _svm_okon2 = 0;
    _svm_okof1 = 0;
    _svm_orev1 = 0;
    _svm_orev2 = 0;
    _svm_rattr.mask = 0;
    _svm_rattr.mode = SPU_REV_MODE_OFF;
    _svm_auto_kof_mode = 0;
    _svm_stereo_mono = SVM_MODE_STEREO;
    kMaxPrograms = 128;
    SpuVmFlush();
}
