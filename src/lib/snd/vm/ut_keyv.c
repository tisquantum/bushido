/* Direct SPU voice key-on and key-off utilities (SsUtKeyOnV, SsUtKeyOffV). */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_int.h"
#include "snd_vm_int.h"
#include "vm_aloc2.h"
#include "vm_f.h"
#include "vm_n2p.h"
#include "vm_no1.h"
#include "vm_nowof.h"
#include "vm_nowon.h"
#include "vm_vsu.h"

short SsUtKeyOnV(short voice, short vabId, short prog, short tone, short note,
                 short fine, short voll, short volr)
{
    VagAtr *tn;
    ProgAtr *pg;

    if (_snd_ev_flag == 1) {
        return -1;
    }
    _snd_ev_flag = 1;
    if (voice < 0 || voice >= SPU_VOICE_COUNT || SpuVmVSetUp(vabId, prog) != 0) {
        _snd_ev_flag = 0;
        return -1;
    }
    _svm_cur.seq_sep_no = SVM_SEQ_UTILITY;
    _svm_cur.note = note;
    _svm_cur.fine = fine;
    _svm_cur.tone = tone;
    if (voll == volr) {
        _svm_cur.pan = 64;
        _svm_cur.vol = voll;
    } else if (voll > volr) {
        _svm_cur.vol = voll;
        _svm_cur.pan = (volr * 64) / voll;
    } else {
        _svm_cur.vol = volr;
        _svm_cur.pan = 127 - (voll * 64) / volr;
    }
    pg = &_svm_pg[prog];
    _svm_cur.prog_mvol = pg->mvol;
    _svm_cur.prog_mpan = pg->mpan;
    _svm_cur.tones = pg->tones;

    tn = &_svm_tn[_svm_cur.tone + _svm_cur.prog_index * VAB_TONES_PER_PROG];
    _svm_cur.tone_prior = tn->prior;
    _svm_cur.vag_idx = tn->vag;
    _svm_cur.tone_vol = tn->vol;
    _svm_cur.tone_pan = tn->pan;
    _svm_cur.tone_center = tn->center;
    _svm_cur.tone_shift = tn->shift;
    _svm_cur.tone_mode = tn->mode;
    _svm_cur.tone_min = tn->min;
    _svm_cur.tone_max = tn->max;
    if (_svm_cur.vag_idx == 0) {
        _snd_ev_flag = 0;
        return -1;
    }
    _svm_cur.voice = voice;
    _svm_voice[voice].seq_sep_no = SVM_SEQ_UTILITY;
    _svm_voice[voice].vab_id = vabId;
    _svm_voice[voice].prog_index = _svm_cur.prog_index;
    _svm_voice[voice].prog = prog;
    _svm_voice[voice].vag_id = _svm_cur.vag_idx;
    _svm_voice[voice].tone = _svm_cur.tone;
    _svm_voice[voice].note = note;
    _svm_voice[voice].keystat = KEYSTAT_TONE;
    _svm_voice[voice].alloc_state = 0;
    SpuVmDoAllocate();
    if (_svm_cur.vag_idx == SVM_VAG_NOISE) {
        vmNoiseOn(voice);
    } else {
        SpuVmKeyOnNow(1, note2pitch2(note, fine));
    }
    _snd_ev_flag = 0;
    return voice;
}

short SsUtKeyOffV(short voice)
{
    if (_snd_ev_flag == 1) {
        return -1;
    }
    _snd_ev_flag = 1;
    if (voice >= 0 && voice < SPU_VOICE_COUNT) {
        _svm_cur.voice = voice;
        SpuVmKeyOffNow(0);
        _snd_ev_flag = 0;
        return 0;
    }
    _snd_ev_flag = 0;
    return -1;
}
