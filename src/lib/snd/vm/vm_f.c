/* Flush buffered voice-manager state to SPU hardware registers (SpuVmFlush). */

#include <sys/types.h>
#include <libspu.h>
#include <libsnd.h>

#include "snd_vm_int.h"
#include "vm_f.h"

unsigned short *_svm_sreg = (unsigned short *)SPU_REG_BASE;

unsigned short _svm_sreg_buf[SPU_VOICE_COUNT * SPU_VREG_COUNT];
unsigned char _svm_sreg_dirty[SPU_VOICE_COUNT];
unsigned short _svm_okon1;
unsigned short _svm_okon2;
unsigned short _svm_okof1;
unsigned short _svm_okof2;
unsigned short _svm_orev1;
unsigned short _svm_orev2;
SpuVmVoice _svm_voice[SPU_VOICE_COUNT];
long _svm_envx_ptr;
unsigned long _svm_envx_hist[SVM_ENVX_HIST_MAX];
void (*_autovol)(int);
void (*_autopan)(int);

void SpuVmFlush(void)
{
    int i;
    unsigned long envx_hist;

    _svm_envx_ptr = (_svm_envx_ptr + 1) & (SVM_ENVX_HIST_MAX - 1);
    _svm_envx_hist[_svm_envx_ptr] = 0;

    for (i = 0; i < spuVmMaxVoice; i++) {
        _svm_voice[i].envx = _svm_sreg[i * SPU_VREG_COUNT + SPU_VREG_ENVX];
        if (_svm_voice[i].envx == 0) {
            _svm_envx_hist[_svm_envx_ptr] |= (1 << i);
        }
    }

    if (_svm_auto_kof_mode == 0) {
        envx_hist = 0xffffffff;
        for (i = 0; i < SVM_ENVX_HIST_MAX - 1; i++) {
            envx_hist &= _svm_envx_hist[i];
        }
        for (i = 0; i < spuVmMaxVoice; i++) {
            if (envx_hist & (1 << i)) {
                if (_svm_voice[i].keystat == KEYSTAT_NOISE) {
                    SpuSetNoiseVoice(SPU_OFF, SPU_ALLCH);
                }
                _svm_voice[i].keystat = KEYSTAT_OFF;
            }
        }
    }

    _svm_okon1 &= ~_svm_okof1;
    _svm_okon2 &= ~_svm_okof2;

    for (i = 0; i < SPU_VOICE_COUNT; i++) {
        if (_svm_voice[i].auto_vol) {
            (*_autovol)(i);
        }
        if (_svm_voice[i].auto_pan) {
            (*_autopan)(i);
        }
    }

    for (i = 0; i < SPU_VOICE_COUNT; i++) {
        if (_svm_sreg_dirty[i] & SREG_DIRTY_VOL_L) {
            _svm_sreg[i * SPU_VREG_COUNT + SPU_VREG_VOL_L] =
                _svm_sreg_buf[i * SPU_VREG_COUNT + SPU_VREG_VOL_L];
            _svm_sreg[i * SPU_VREG_COUNT + SPU_VREG_VOL_R] =
                _svm_sreg_buf[i * SPU_VREG_COUNT + SPU_VREG_VOL_R];
        }
        if (_svm_sreg_dirty[i] & SREG_DIRTY_PITCH) {
            _svm_sreg[i * SPU_VREG_COUNT + SPU_VREG_PITCH] =
                _svm_sreg_buf[i * SPU_VREG_COUNT + SPU_VREG_PITCH];
        }
        if (_svm_sreg_dirty[i] & SREG_DIRTY_START_ADDR) {
            _svm_sreg[i * SPU_VREG_COUNT + SPU_VREG_START_ADDR] =
                _svm_sreg_buf[i * SPU_VREG_COUNT + SPU_VREG_START_ADDR];
        }
        if (_svm_sreg_dirty[i] & SREG_DIRTY_ADSR1) {
            _svm_sreg[i * SPU_VREG_COUNT + SPU_VREG_ADSR1] =
                _svm_sreg_buf[i * SPU_VREG_COUNT + SPU_VREG_ADSR1];
            _svm_sreg[i * SPU_VREG_COUNT + SPU_VREG_ADSR2] =
                _svm_sreg_buf[i * SPU_VREG_COUNT + SPU_VREG_ADSR2];
        }
        _svm_sreg_dirty[i] = 0;
    }

    _svm_sreg[SPU_REG_KOFF_L] = _svm_okof1;
    _svm_sreg[SPU_REG_KOFF_H] = _svm_okof2;
    _svm_sreg[SPU_REG_KON_L] = _svm_okon1;
    _svm_sreg[SPU_REG_KON_H] = _svm_okon2;
    _svm_sreg[SPU_REG_EON_L] = _svm_orev1;
    _svm_sreg[SPU_REG_EON_H] = _svm_orev2;

    _svm_okof1 = 0;
    _svm_okof2 = 0;
    _svm_okon1 = 0;
    _svm_okon2 = 0;
}
