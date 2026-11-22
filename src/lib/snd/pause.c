/* Pause a sequence slot: silence its active voices and clear its pause flag. */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_int.h"
#include "snd_vm_int.h"
#include "vm_seq.h"

void _SsSndPause(short sep_no, short seq_no)
{
    SndSeqScore *score;

    score = _ss_score[sep_no] + seq_no;
    SpuVmSeqKeyOff(sep_no | (seq_no << 8));
    score->playing = 0;
    _ss_score[sep_no][seq_no].flags &= ~SND_SEQ_FLAG_PAUSE;
}
