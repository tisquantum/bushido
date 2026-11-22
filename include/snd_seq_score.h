#ifndef SND_SEQ_SCORE_H
#define SND_SEQ_SCORE_H

/* SndSeqScore.flags */
#define SND_SEQ_FLAG_PLAY    0x001
#define SND_SEQ_FLAG_PAUSE   0x002
#define SND_SEQ_FLAG_STOP    0x004
#define SND_SEQ_FLAG_REPLAY  0x008
#define SND_SEQ_FLAG_CRES    0x010
#define SND_SEQ_FLAG_DECRES  0x020
#define SND_SEQ_FLAG_ACCELE  0x040
#define SND_SEQ_FLAG_RIT     0x080
#define SND_SEQ_FLAG_NEXT    0x100
#define SND_SEQ_FLAG_EOS     0x200

/* next_sep_no when no score follows. */
#define SND_SEQ_NO_NEXT      0xff

/* One score of a separated sequence, SS_SEQ_TABSIZ (0xAC) bytes. Delta times count tenths of a
 * MIDI tick; each call of SsSeqCalledTbyT consumes tick_step of them. */
typedef struct SndSeqScore {
    unsigned char next_seq_no;   /* with next_sep_no, the score _SsSeqGetEof starts next */
    unsigned char *read_pos;
    unsigned char *top;
    unsigned char *loop_pos;
    unsigned char loop_active;
    unsigned char status;        /* running MIDI status */
    unsigned char channel;
    unsigned char rpn_lsb;
    unsigned char rpn_msb;
    unsigned char nrpn_lsb;
    unsigned char nrpn_msb;
    unsigned char panpot[16];
    unsigned char loop_start;
    unsigned char loop_count;
    unsigned char rpn_count;
    unsigned char nrpn_count;
    unsigned char playing;       /* 1 after a replay or a move to the next score;
                                    0 after pause, stop or the end; never read */
    unsigned char programs[16];
    unsigned char next_sep_no;
    short vol_delta;
    short vol_remain;
    short vol_step;              /* > 0: calls per volume step of 1; otherwise minus the step per call */
    short tempo_step;            /* > 0: calls per tempo step of 1; otherwise minus the step per call */
    short play_mode;             /* times to play, SSPLAY_INFINITY for ever */
    short played;
    short resolution;
    short vab_id;
    short vol[16];
    short tick_wait;
    short tick_step;
    short init_tick_step;
    unsigned short voll;
    unsigned short volr;
    unsigned short saved_voll;
    unsigned short saved_volr;
    long init_delta;
    long delta_value;
    unsigned long init_tempo;
    long delta;
    unsigned long tempo;
    long flags;
    unsigned long vol_time;
    unsigned long vol_cnt;
    unsigned long tempo_time;
    unsigned long tempo_cnt;
    unsigned long tempo_target;
    char unused[4];              /* nothing reads them */
} SndSeqScore;

#endif
