/* Sequence replay handler for resuming paused playback. */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_int.h"

void _SsSndReplay(short sep_no, short seq_no)
{
    _ss_score[sep_no][seq_no].playing = 1;
    _ss_score[sep_no][seq_no].flags &= ~SND_SEQ_FLAG_REPLAY;
}
