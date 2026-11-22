/* Voice-manager sequence volume control and key-off routines. */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_int.h"
#include "snd_vm_int.h"
#include "vm_f.h"
#include "vm_nowof.h"
#include "vm_seq.h"

short SpuVmSetSeqVol(short seq_sep_no, unsigned short voll, unsigned short volr, short update)
{
    SndSeqScore *score;
    short voice;
    long voice_vol;
    unsigned long lvol;
    unsigned long rvol;
    unsigned char pan;

    score = &_ss_score[seq_sep_no & 0xff][(seq_sep_no & 0xff00) >> 8];
    _svm_cur.seq_sep_no = seq_sep_no;
    if (voll == 0) {
        voll = 1;
    }
    score->voll = voll;
    if (volr == 0) {
        volr = 1;
    }
    score->volr = volr;
    if (score->voll > 127) {
        score->voll = 127;
    }
    if (score->volr > 127) {
        score->volr = 127;
    }
    if (update == 1) {
        for (voice = 0; voice < spuVmMaxVoice; voice++) {
            if ((unsigned short)_svm_voice[voice].seq_sep_no == (unsigned short)seq_sep_no) {
                voice_vol = ((_svm_voice[voice].vol * score->vol[score->channel]) / 127) * 0x3fff;
                lvol = (_svm_vh->mvol * voice_vol) / (127 * 127);
                rvol = (lvol * _svm_pg[_svm_voice[voice].prog].mvol *
                        _svm_tn[_svm_voice[voice].prog_index * VAB_TONES_PER_PROG +
                                _svm_voice[voice].tone].vol) /
                       (127 * 127);
                lvol = (rvol * score->voll) / 127;
                rvol = (rvol * score->volr) / 127;

                pan = _svm_tn[_svm_voice[voice].prog_index * VAB_TONES_PER_PROG +
                              _svm_voice[voice].tone].pan;
                if (pan < 64) {
                    voll = lvol;
                    volr = (rvol * pan) / 63;
                } else {
                    volr = rvol;
                    voll = (lvol * (127 - pan)) / 63;
                }

                pan = _svm_pg[_svm_voice[voice].prog].mpan;
                if (pan < 64) {
                    volr = (volr * pan) / 63;
                } else {
                    voll = (voll * (127 - pan)) / 63;
                }

                pan = _svm_voice[voice].pan;
                if (pan < 64) {
                    volr = (volr * pan) / 63;
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

                voll = (voll * voll) / 0x3fff;
                volr = (volr * volr) / 0x3fff;
                _svm_sreg_buf[voice * SPU_VREG_COUNT + SPU_VREG_VOL_L] = voll;
                _svm_sreg_buf[voice * SPU_VREG_COUNT + SPU_VREG_VOL_R] = volr;
                _svm_sreg_dirty[voice] |= SREG_DIRTY_VOL;
            }
        }
    }
    return _svm_cur.seq_sep_no;
}

short SpuVmGetSeqVol(short seq_sep_no, unsigned short *voll, unsigned short *volr)
{
    SndSeqScore *score;

    score = &_ss_score[seq_sep_no & 0xff][(seq_sep_no & 0xff00) >> 8];
    _svm_cur.seq_sep_no = seq_sep_no;
    *voll = score->voll;
    *volr = score->volr;
    return _svm_cur.seq_sep_no;
}

short SpuVmGetSeqLVol(short seq_sep_no)
{
    SndSeqScore *score;

    score = &_ss_score[seq_sep_no & 0xff][(seq_sep_no & 0xff00) >> 8];
    _svm_cur.seq_sep_no = seq_sep_no;
    return score->voll;
}

short SpuVmGetSeqRVol(short seq_sep_no)
{
    SndSeqScore *score;

    score = &_ss_score[seq_sep_no & 0xff][(seq_sep_no & 0xff00) >> 8];
    _svm_cur.seq_sep_no = seq_sep_no;
    return score->volr;
}

void SpuVmSeqKeyOff(short seq_sep_no)
{
    unsigned char voice;

    for (voice = 0; voice < spuVmMaxVoice; voice++) {
        if (_svm_voice[voice].seq_sep_no == seq_sep_no) {
            _svm_cur.voice = voice;
            SpuVmKeyOffNow(0);
        }
    }
}
