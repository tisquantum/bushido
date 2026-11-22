/* Key on an SPU voice as a noise generator using current voice-manager state. */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_int.h"
#include "snd_vm_int.h"
#include "vm_f.h"
#include "vm_no1.h"

void vmNoiseOn(unsigned char voice)
{
    unsigned long lvol;
    unsigned long rvol;
    unsigned long voll;
    unsigned long volr;
    unsigned short spucnt;
    unsigned short mask_lo;
    unsigned short mask_hi;
    unsigned short seq_sep_no;
    unsigned long pan;
    short i;
    SndSeqScore *score;

    seq_sep_no = _svm_cur.seq_sep_no;
    /* SsUtKeyOnV's voices (33) read _ss_score[33], past its 32 entries, and use that record */
    score = &_ss_score[seq_sep_no & 0xff][seq_sep_no >> 8];
    lvol = score->voll * 129;
    lvol = (lvol * _svm_cur.prog_mvol) / 127;
    rvol = score->volr * 129;
    rvol = (rvol * _svm_cur.prog_mvol) / 127;
    lvol = (lvol * _svm_cur.tone_vol) / 127;
    rvol = (rvol * _svm_cur.tone_vol) / 127;

    spucnt = _svm_sreg[SPU_REG_SPUCNT];

    pan = _svm_cur.tone_pan;
    if (pan < 64) {
        voll = lvol;
        volr = (rvol * pan) / 63;
    } else {
        volr = rvol;
        voll = (lvol * (127 - pan)) / 63;
    }

    pan = _svm_cur.prog_mpan;
    if (pan < 64) {
        volr = (volr * pan) / 63;
    } else {
        voll = (voll * (127 - pan)) / 63;
    }

    pan = _svm_cur.pan;
    if (pan < 64) {
        volr = (pan * volr) / 63;
    } else {
        voll = (voll * (127 - pan)) / 63;
    }

    if (_svm_stereo_mono == SVM_MODE_MONO) {
        if (voll < volr) {
            voll = volr;
        } else {
            volr = voll;
        }
    }

    spucnt = (spucnt & ~SPUCNT_NOISE_CLK_BITS) |
             (((_svm_cur.note - _svm_cur.tone_center) & SPUCNT_NOISE_CLK_MASK) << SPUCNT_NOISE_CLK_SHIFT);
    _svm_sreg[SPU_REG_SPUCNT] = spucnt;

    _svm_sreg_buf[voice * SPU_VREG_COUNT + SPU_VREG_VOL_R] = volr;
    _svm_sreg_buf[voice * SPU_VREG_COUNT + SPU_VREG_VOL_L] = voll;
    _svm_sreg_dirty[voice] |= SREG_DIRTY_VOL;

    if (voice < 16) {
        mask_lo = 1 << voice;
        mask_hi = 0;
    } else {
        mask_lo = 0;
        mask_hi = 1 << (voice - 16);
    }

    _svm_voice[voice].pitch = 10;
    for (i = 0; i < spuVmMaxVoice; i++) {
        _svm_voice[i].keystat &= KEYSTAT_TONE;
    }
    _svm_voice[voice].keystat = KEYSTAT_NOISE;

    _svm_okon1 |= mask_lo;
    _svm_okon2 |= mask_hi;
    _svm_okof1 &= ~_svm_okon1;
    _svm_okof2 &= ~_svm_okon2;

    if (_svm_cur.tone_mode & TONE_MODE_REVERB) {
        _svm_orev1 |= mask_lo;
        _svm_orev2 |= mask_hi;
    } else {
        _svm_orev1 &= ~mask_lo;
        _svm_orev2 &= ~mask_hi;
    }

    _svm_sreg[SPU_REG_NON_L] = mask_lo;
    _svm_sreg[SPU_REG_NON_H] = mask_hi;
}
