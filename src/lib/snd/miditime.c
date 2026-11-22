/* MIDI variable-length delta-time reader for sequence playback. */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_int.h"

long _SsReadDeltaValue(short sep_no, short seq_no)
{
    SndSeqScore *score;
    unsigned char byte;
    long value;

    score = _ss_score[sep_no] + seq_no;
    value = *score->read_pos++;
    if (value == 0) {
        return 0;
    }
    if (value & 0x80) {
        value &= 0x7f;
        do {
            byte = *score->read_pos++;
            value = (value << 7) + (byte & 0x7f);
        } while (byte & 0x80);
    }
    score->delta_value += value * 10;
    return value * 10;
}
