/* Key on an SPU tone voice using current voice-manager state (SpuVmKeyOnNow). */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_int.h"
#include "snd_vm_int.h"
#include "vm_f.h"
#include "vm_nowon.h"

void SpuVmKeyOnNow(short flag, unsigned short pitch)
{
    long vab_vol;
    unsigned long lvol;
    unsigned long rvol;
    unsigned long voll;
    unsigned long volr;
    unsigned short sreg_offset;
    SndSeqScore *score;
    unsigned short mask_lo;
    unsigned short mask_hi;

    vab_vol = _svm_vh->mvol * 0x3fff;
    lvol = (_svm_cur.vol * vab_vol) / (127 * 127);
    rvol = (lvol * _svm_cur.prog_mvol * _svm_cur.tone_vol) / (127 * 127);
    sreg_offset = _svm_cur.voice * SPU_VREG_COUNT;

    /* SsUtKeyOnV's voices (33) read _ss_score[33], past its 32 entries; score goes unused */
    score = &_ss_score[_svm_cur.seq_sep_no & 0xff][(_svm_cur.seq_sep_no & 0xff00) >> 8];

    lvol = rvol;
    if (_svm_cur.seq_sep_no != SVM_SEQ_UTILITY) {
        lvol = (rvol * score->voll) / 127;
        rvol = (rvol * score->volr) / 127;
    }

    if (_svm_cur.tone_pan < 64) {
        voll = lvol;
        volr = (rvol * _svm_cur.tone_pan) / 63;
    } else {
        voll = (lvol * (127 - _svm_cur.tone_pan)) / 63;
        volr = rvol;
    }

    if (_svm_cur.prog_mpan < 64) {
        volr = (volr * _svm_cur.prog_mpan) / 63;
    } else {
        voll = (voll * (127 - _svm_cur.prog_mpan)) / 63;
    }

    if (_svm_cur.pan < 64) {
        volr = (volr * _svm_cur.pan) / 63;
    } else {
        voll = (voll * (127 - _svm_cur.pan)) / 63;
    }

    if (_svm_stereo_mono == SVM_MODE_MONO) {
        if (voll < volr) {
            voll = volr;
        } else {
            volr = voll;
        }
    }

    voll = (voll * voll) / 0x3fff;
    volr = (volr * volr) / 0x3fff;

    _svm_sreg_buf[sreg_offset + SPU_VREG_PITCH] = pitch;
    _svm_sreg_buf[sreg_offset + SPU_VREG_VOL_L] = voll;
    _svm_sreg_buf[sreg_offset + SPU_VREG_VOL_R] = volr;

    _svm_sreg_dirty[_svm_cur.voice] |= SREG_DIRTY_VOL | SREG_DIRTY_PITCH;
    _svm_voice[_svm_cur.voice].pitch = pitch;
    _svm_voice[_svm_cur.voice].keystat = KEYSTAT_TONE;

    if (_svm_cur.voice < 16) {
        mask_lo = 1 << _svm_cur.voice;
        mask_hi = 0;
    } else {
        mask_lo = 0;
        mask_hi = 1 << (_svm_cur.voice - 16);
    }

    if (_svm_cur.tone_mode & TONE_MODE_REVERB) {
        _svm_orev1 |= mask_lo;
        _svm_orev2 |= mask_hi;
    } else {
        _svm_orev1 &= ~mask_lo;
        _svm_orev2 &= ~mask_hi;
    }

    _svm_okon1 |= mask_lo;
    _svm_okon2 |= mask_hi;
    _svm_okof1 &= ~_svm_okon1;
    _svm_okof2 &= ~_svm_okon2;
}
