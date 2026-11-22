/* Sequence volume crescendo tick handler. */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_int.h"
#include "snd_vm_int.h"
#include "vm_seq.h"

void _SsSndCrescendo(short sep_no, short seq_no)
{
    SndSeqScore *score;
    unsigned short voll;
    unsigned short volr;

    score = &_ss_score[sep_no][seq_no];
    score->vol_cnt--;

    if (score->vol_step > 0) {
        if (score->vol_cnt % score->vol_step == 0) {
            score->vol_remain--;
            if (score->vol_remain >= 0) {
                SpuVmGetSeqVol(sep_no | (seq_no << 8), &voll, &volr);
                if (voll + 1 <= voll + score->vol_remain) {
                    SpuVmSetSeqVol(sep_no | (seq_no << 8), voll + 1, volr + 1, 1);
                }
            } else {
                SpuVmSetSeqVol(sep_no | (seq_no << 8), 0x7f, 0x7f, 1);
                _ss_score[sep_no][seq_no].flags &= ~SND_SEQ_FLAG_CRES;
            }
            if (score->vol_cnt == 0 || score->vol_remain == 0) {
                _ss_score[sep_no][seq_no].flags &= ~SND_SEQ_FLAG_CRES;
            }
        }
    } else if (score->vol_step < 0) {
        score->vol_remain += score->vol_step;
        if (score->vol_remain >= 0) {
            SpuVmGetSeqVol(sep_no | (seq_no << 8), &voll, &volr);
            if (voll - score->vol_step >= 0x7f && volr - score->vol_step >= 0x7f) {
                SpuVmSetSeqVol(sep_no | (seq_no << 8), 0x7f, 0x7f, 1);
            }
            if ((score->vol_time - score->vol_cnt) * (-score->vol_step) < score->vol_delta) {
                SpuVmSetSeqVol(sep_no | (seq_no << 8), voll - score->vol_step, volr - score->vol_step, 1);
            }
        } else {
            SpuVmSetSeqVol(sep_no | (seq_no << 8), 0x7f, 0x7f, 1);
            _ss_score[sep_no][seq_no].flags &= ~SND_SEQ_FLAG_CRES;
        }
        if (score->vol_cnt == 0 || score->vol_remain == 0) {
            _ss_score[sep_no][seq_no].flags &= ~SND_SEQ_FLAG_CRES;
        }
    }

    SpuVmGetSeqVol(sep_no | (seq_no << 8), &score->saved_voll, &score->saved_volr);
}
