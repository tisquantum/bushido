#ifndef SPU_REVERB_H
#define SPU_REVERB_H

/* Values for the 32 reverb registers (halfword 0xe0 + n from SPU_REG_BASE); mask bit n selects
 * register n, a mask of 0 selects them all. */
typedef struct {
    unsigned long mask;
    unsigned short dAPF1, dAPF2, vIIR, vCOMB1, vCOMB2, vCOMB3, vCOMB4, vWALL;
    unsigned short vAPF1, vAPF2, mLSAME, mRSAME, mLCOMB1, mRCOMB1, mLCOMB2, mRCOMB2;
    unsigned short dLSAME, dRSAME, mLDIFF, mRDIFF, mLCOMB3, mRCOMB3, mLCOMB4, mRCOMB4;
    unsigned short dLDIFF, dRDIFF, mLAPF1, mRAPF1, mLAPF2, mRAPF2, vLIN, vRIN;
} SpuReverbPreset;

#endif
