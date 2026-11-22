/* MIDI sequence playback step and event stream parser. */

#include <sys/types.h>
#include <libsnd.h>

#include "snd_int.h"
#include "snd_vm_int.h"
#include "vm_seq.h"

#define MIDI_STATUS_BIT        0x80
#define MIDI_STATUS_MASK       0xf0
#define MIDI_CHANNEL_MASK      0x0f
#define MIDI_NOTE_ON           0x90
#define MIDI_CONTROL_CHANGE    0xb0
#define MIDI_PROGRAM_CHANGE    0xc0
#define MIDI_PITCH_BEND        0xe0
#define MIDI_SYSTEM            0xf0
#define MIDI_META_EVENT        0xff
#define MIDI_META_END_OF_TRACK 0x2f

void _SsSeqPlay(short sep_no, short seq_no)
{
    SndSeqScore *score;
    short step;
    long left;
    long delta;

    score = _ss_score[sep_no] + seq_no;
    step = score->tick_step;
    left = score->delta - step;
    if (left > 0) {
        if (score->tick_wait > 0) {
            score->tick_wait--;
        } else if (score->tick_wait == 0) {
            score->tick_wait = step;
            score->delta--;
        } else {
            score->delta = left;
        }
    } else if (score->delta <= step) {
        delta = score->delta;
        do {
            do {
                _SsGetSeqData(sep_no, seq_no);
            } while (score->delta == 0);
            delta += score->delta;
        } while (delta < score->tick_step);
        score->delta = delta - score->tick_step;
    }
}

void _SsSeqGetEof(short sep_no, short seq_no, unsigned char ev)
{
    SndSeqScore *score;

    score = _ss_score[sep_no] + seq_no;
    score->played++;
    if (score->play_mode == SSPLAY_INFINITY) {
        score->read_pos = score->top;
        score->delta_value = 0;
        score->loop_start = 0;
        score->delta = 0;
    } else if (score->played < score->play_mode) {
        score->read_pos = score->top;
        score->delta_value = 0;
        score->loop_start = 0;
        score->delta = 0;
        score->loop_pos = score->top;
    } else {
        _ss_score[sep_no][seq_no].flags &= ~SND_SEQ_FLAG_PLAY;
        _ss_score[sep_no][seq_no].flags &= ~SND_SEQ_FLAG_REPLAY;
        _ss_score[sep_no][seq_no].flags &= ~SND_SEQ_FLAG_PAUSE;
        _ss_score[sep_no][seq_no].flags |= SND_SEQ_FLAG_EOS;
        _ss_score[sep_no][seq_no].flags |= SND_SEQ_FLAG_STOP;
        score->playing = 0;
        score->loop_pos = score->top;
        if (score->next_sep_no != SND_SEQ_NO_NEXT) {
            score->playing = 0; /* stored twice in the original */
            _SsSndNextSep(score->next_sep_no, score->next_seq_no);
            SpuVmSeqKeyOff(sep_no | (seq_no << 8));
        }
        SpuVmSeqKeyOff(sep_no | (seq_no << 8));
        score->delta = score->tick_step;
    }
}

void _SsGetSeqData(short sep_no, short seq_no)
{
    SndSeqScore *score;
    unsigned char byte;
    unsigned char note;
    unsigned char vel;
    unsigned char data;

    score = _ss_score[sep_no] + seq_no;
    byte = *score->read_pos++;
    if (byte & MIDI_STATUS_BIT) {
        score->channel = byte & MIDI_CHANNEL_MASK;
        switch (byte & MIDI_STATUS_MASK) {
        case MIDI_NOTE_ON:
            score->status = MIDI_NOTE_ON;
            note = *score->read_pos++;
            vel = *score->read_pos++;
            score->delta = _SsReadDeltaValue(sep_no, seq_no);
            SsFCALL.noteon(sep_no, seq_no, note, vel);
            break;
        case MIDI_CONTROL_CHANGE:
            score->status = MIDI_CONTROL_CHANGE;
            data = *score->read_pos++;
            SsFCALL.control[CC_NUMBER](sep_no, seq_no, data);
            break;
        case MIDI_PROGRAM_CHANGE:
            score->status = MIDI_PROGRAM_CHANGE;
            data = *score->read_pos++;
            SsFCALL.programchange(sep_no, seq_no, data);
            break;
        case MIDI_PITCH_BEND:
            score->status = MIDI_PITCH_BEND;
            score->read_pos++;
            SsFCALL.pitchbend(sep_no, seq_no);
            break;
        case MIDI_SYSTEM:
            score->status = MIDI_META_EVENT;
            score->channel = byte & MIDI_CHANNEL_MASK; /* stored twice in the original */
            data = *score->read_pos++;
            if (data == MIDI_META_END_OF_TRACK) {
                _SsSeqGetEof(sep_no, seq_no, data);
            } else {
                SsFCALL.metaevent(sep_no, seq_no, data);
            }
            break;
        }
    } else {
        switch (score->status) {
        case MIDI_NOTE_ON:
            vel = *score->read_pos++;
            score->delta = _SsReadDeltaValue(sep_no, seq_no);
            SsFCALL.noteon(sep_no, seq_no, byte, vel);
            break;
        case MIDI_CONTROL_CHANGE:
            SsFCALL.control[CC_NUMBER](sep_no, seq_no, byte);
            break;
        case MIDI_PROGRAM_CHANGE:
            SsFCALL.programchange(sep_no, seq_no, byte);
            break;
        case MIDI_PITCH_BEND:
            SsFCALL.pitchbend(sep_no, seq_no);
            break;
        case MIDI_META_EVENT:
            if (byte == MIDI_META_END_OF_TRACK) {
                _SsSeqGetEof(sep_no, seq_no, byte);
            } else {
                SsFCALL.metaevent(sep_no, seq_no, byte);
            }
            break;
        }
    }
}
