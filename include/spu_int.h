/*
 * Internal definitions and interfaces for LIBSPU.
 */
#ifndef _SPU_INT_H_
#define _SPU_INT_H_

#include <sys/types.h>
#include <libspu.h>
#include "psx_hw.h"
#include "spu_malloc.h"
#include "spu_reverb.h"

/* _SpuInit() / _spu_init() mode: cold init resets voices and centerNote. */
#define SPU_INIT_COLD           0
#define SPU_INIT_HOT            1

#define SPU_RQ_COUNT            10      /* mirrors KON..EON (0xc4..0xcd) */

#define SPU_TCTRL_NORMAL        0x0004
#define SPU_WAIT_TIMEOUT        0x0f00

/* Volume register (MVOL / voice VOL) mode bits and value limits:
 * bit 15 = 0: direct 15-bit signed volume;
 * bit 15 = 1: sweep (bits 14..12 = curve/direction/phase, bits 6..0 = rate). */
#define SPU_VOL_DIRECT_MASK         0x7fff
#define SPU_VOL_SWEEP_RATE_MAX      0x7f
#define SPU_VOL_MODE_DIRECT         0x0000
#define SPU_VOL_SWEEP_LINEAR_INC_N  0x8000
#define SPU_VOL_SWEEP_LINEAR_INC_R  0x9000
#define SPU_VOL_SWEEP_LINEAR_DEC_N  0xa000
#define SPU_VOL_SWEEP_LINEAR_DEC_R  0xb000
#define SPU_VOL_SWEEP_EXP_INC_N     0xc000
#define SPU_VOL_SWEEP_EXP_INC_R     0xd000
#define SPU_VOL_SWEEP_EXP_DEC       0xe000

/* _spu_t() transfer sub-commands and return status. */
#define SPU_TRANS_READ          0
#define SPU_TRANS_WRITE         1
#define SPU_TRANS_ADDR          2
#define SPU_TRANS_START         3
#define SPU_TRANS_TIMEOUT       (-2)

/* _spu_FsetRXX() mode: write raw 16-bit value vs. scaled byte address. */
#define SPU_FSETRXX_RAW         0

/* Sound RAM extent in scaled units (512 KB = 0x10000 << _spu_mem_mode_plus) and zero buffer size. */
#define SPU_RAM_UNITS           0x10000
#define SPU_ZEROBUF_SIZE        0x400

/* SPU reverb registers (halfword offsets from 0x1f801c00: 0x1f801dc0..0x1f801dfe). */
#define SPU_REG_REV_dAPF1       0xe0
#define SPU_REG_REV_dAPF2       0xe1
#define SPU_REG_REV_vIIR        0xe2
#define SPU_REG_REV_vCOMB1      0xe3
#define SPU_REG_REV_vCOMB2      0xe4
#define SPU_REG_REV_vCOMB3      0xe5
#define SPU_REG_REV_vCOMB4      0xe6
#define SPU_REG_REV_vWALL       0xe7
#define SPU_REG_REV_vAPF1       0xe8
#define SPU_REG_REV_vAPF2       0xe9
#define SPU_REG_REV_mLSAME      0xea
#define SPU_REG_REV_mRSAME      0xeb
#define SPU_REG_REV_mLCOMB1     0xec
#define SPU_REG_REV_mRCOMB1     0xed
#define SPU_REG_REV_mLCOMB2     0xee
#define SPU_REG_REV_mRCOMB2     0xef
#define SPU_REG_REV_dLSAME      0xf0
#define SPU_REG_REV_dRSAME      0xf1
#define SPU_REG_REV_mLDIFF      0xf2
#define SPU_REG_REV_mRDIFF      0xf3
#define SPU_REG_REV_mLCOMB3     0xf4
#define SPU_REG_REV_mRCOMB3     0xf5
#define SPU_REG_REV_mLCOMB4     0xf6
#define SPU_REG_REV_mRCOMB4     0xf7
#define SPU_REG_REV_dLDIFF      0xf8
#define SPU_REG_REV_dRDIFF      0xf9
#define SPU_REG_REV_mLAPF1      0xfa
#define SPU_REG_REV_mRAPF1      0xfb
#define SPU_REG_REV_mLAPF2      0xfc
#define SPU_REG_REV_mRAPF2      0xfd
#define SPU_REG_REV_vLIN        0xfe
#define SPU_REG_REV_vRIN        0xff

#define SPU_REV_MASK_dAPF1      0x00000001
#define SPU_REV_MASK_dAPF2      0x00000002
#define SPU_REV_MASK_vIIR       0x00000004
#define SPU_REV_MASK_vCOMB1     0x00000008
#define SPU_REV_MASK_vCOMB2     0x00000010
#define SPU_REV_MASK_vCOMB3     0x00000020
#define SPU_REV_MASK_vCOMB4     0x00000040
#define SPU_REV_MASK_vWALL      0x00000080
#define SPU_REV_MASK_vAPF1      0x00000100
#define SPU_REV_MASK_vAPF2      0x00000200
#define SPU_REV_MASK_mLSAME     0x00000400
#define SPU_REV_MASK_mRSAME     0x00000800
#define SPU_REV_MASK_mLCOMB1    0x00001000
#define SPU_REV_MASK_mRCOMB1    0x00002000
#define SPU_REV_MASK_mLCOMB2    0x00004000
#define SPU_REV_MASK_mRCOMB2    0x00008000
#define SPU_REV_MASK_dLSAME     0x00010000
#define SPU_REV_MASK_dRSAME     0x00020000
#define SPU_REV_MASK_mLDIFF     0x00040000
#define SPU_REV_MASK_mRDIFF     0x00080000
#define SPU_REV_MASK_mLCOMB3    0x00100000
#define SPU_REV_MASK_mRCOMB3    0x00200000
#define SPU_REV_MASK_mLCOMB4    0x00400000
#define SPU_REV_MASK_mRCOMB4    0x00800000
#define SPU_REV_MASK_dLDIFF     0x01000000
#define SPU_REV_MASK_dRDIFF     0x02000000
#define SPU_REV_MASK_mLAPF1     0x04000000
#define SPU_REV_MASK_mRAPF1     0x08000000
#define SPU_REV_MASK_mLAPF2     0x10000000
#define SPU_REV_MASK_mRAPF2     0x20000000
#define SPU_REV_MASK_vLIN       0x40000000
#define SPU_REV_MASK_vRIN       0x80000000

extern volatile unsigned short *_spu_RXX;
extern unsigned short _spu_tsa;
extern long _spu_transMode;
extern long _spu_trans_mode;
extern long _spu_addrMode;
extern long _spu_mem_mode;
extern long _spu_mem_mode_plus;
extern long _spu_mem_mode_unit;
extern long _spu_mem_mode_unitM;
extern long _spu_inTransfer;          /* 0 while a transfer runs, 1 when none does */
extern volatile SpuTransferCallbackProc _spu_transferCallback;
extern volatile SpuIRQCallbackProc _spu_IRQCallback;
extern volatile unsigned short _spu_RQ[SPU_RQ_COUNT];

extern long _spu_EVdma;
extern unsigned long _spu_keystat;
extern long _spu_rev_flag;
extern long _spu_rev_reserve_wa;
extern long _spu_rev_offsetaddr;
extern SpuReverbAttr _spu_rev_attr;
extern unsigned long _spu_RQvoice;
extern volatile unsigned long _spu_RQmask;
extern unsigned short _spu_voice_centerNote[SPU_VOICE_COUNT];
extern unsigned char _spu_zerobuf[SPU_ZEROBUF_SIZE];
extern unsigned long _spu_env;
extern long _spu_isCalled;

extern long _spu_rev_startaddr[SPU_REV_MODE_MAX];
extern long _spu_rev_workareasize[SPU_REV_MODE_MAX];
extern SpuReverbPreset _spu_rev_param[SPU_REV_MODE_MAX];

extern SpuMemBlock *_spu_memList;
extern long _spu_AllocBlockNum;
extern long _spu_AllocLastNum;

extern long _spu_init(long mode);
extern void _SpuInit(long mode);
extern void _spu_FiDMA(void);
extern void _spu_r_(unsigned long addr, unsigned short tsa, unsigned long blocks);
extern long _spu_t(long mode, ...);
extern unsigned long _spu_write(unsigned char *addr, unsigned long size);
extern unsigned long _spu_read(unsigned char *addr, unsigned long size);
extern void _spu_FsetRXX(long reg, unsigned long value, long mode);
extern unsigned long _spu_FsetRXXa(long reg, unsigned long value);
extern unsigned long _spu_FgetRXXa(long reg, long mode);
extern void _spu_FsetPCR(long high_priority);
extern void _SpuDataCallback(void (*func)(void));
extern unsigned long _SpuSetAnyVoice(long on_off, unsigned long voice_bit, long reg_lo, long reg_hi);
extern long _SpuIsInAllocateArea(unsigned long addr);
extern long _SpuIsInAllocateArea_(unsigned long addr);
extern void _spu_gcSPU(void);
extern void _spu_setInTransfer(int flag);
extern int _spu_getInTransfer(void);

#endif /* _SPU_INT_H_ */
