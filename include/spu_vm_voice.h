#ifndef SPU_VM_VOICE_H
#define SPU_VM_VOICE_H

/* _svm_sreg_dirty[] bits: the shadow registers SpuVmFlush copies to the SPU. */
#define SREG_DIRTY_VOL_L        0x01
#define SREG_DIRTY_VOL          0x03
#define SREG_DIRTY_PITCH        0x04
#define SREG_DIRTY_START_ADDR   0x08
#define SREG_DIRTY_ADSR1        0x10
#define SREG_DIRTY_ADSR         0x30

/* SpuVmVoice.keystat */
#define KEYSTAT_OFF             0
#define KEYSTAT_TONE            1
#define KEYSTAT_NOISE           2

/* One voice's state (_svm_voice[24], 0x34 bytes each). */
typedef struct {
    short vag_id;
    short alloc_state;          /* 24 after SpuVmInit, 0 once SsUtKeyOnV takes the voice; never read */
    short pitch;
    unsigned short envx;
    short vol;
    unsigned char pan;          /* 0x40 = center */
    short note;
    short seq_sep_no;           /* the owner's SpuVmCur.seq_sep_no, -1 for none */
    short prog_index;
    short prog;
    short tone;
    short vab_id;
    short prior;                /* nothing reads or writes it */
    unsigned char unused;       /* nothing reads it */
    unsigned char keystat;      /* KEYSTAT_* */
    short auto_vol;             /* nonzero: SpuVmFlush calls _autovol for the voice */
    short auto_vol_state[5];
    short auto_pan;             /* nonzero: SpuVmFlush calls _autopan for the voice */
    short auto_pan_state[5];
} SpuVmVoice;

#endif
