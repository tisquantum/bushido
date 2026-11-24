/* Configure SPU reverb mode parameters and hardware reverb registers. */
#include <sys/types.h>
#include <libspu.h>

#include "spu_int.h"

static void _spu_setReverbAttr(SpuReverbPreset *param);

static __inline__ void _memcpy(void *dst, const void *src, long n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--) {
        *d++ = *s++;
    }
}

long SpuSetReverbModeParam(SpuReverbAttr *attr)
{
    unsigned long mask;
    unsigned long mode;
    long all;
    long mode_set;
    long delay_set;
    long feedback_set;
    long clear_wa;
    long rev_on;
    long delay;
    long half_delay;
    long feedback;
    unsigned short spucnt;
    SpuReverbPreset param;

    rev_on = 0;
    mode_set = 0;
    delay_set = 0;
    clear_wa = 0;
    mask = attr->mask;
    feedback_set = 0;
    all = mask == SPU_ALL;
    param.mask = 0;
    if (all || (mask & SPU_REV_MODE)) {
        mode = attr->mode;
        if (mode & SPU_REV_MODE_CLEAR_WA) {
            mode &= ~SPU_REV_MODE_CLEAR_WA;
            clear_wa = 1;
        }
        if (mode >= SPU_REV_MODE_MAX || _SpuIsInAllocateArea_(_spu_rev_startaddr[mode])) {
            return SPU_ERROR;
        }
        mode_set = 1;
        _spu_rev_attr.mode = mode;
        _spu_rev_offsetaddr = _spu_rev_startaddr[_spu_rev_attr.mode];
        _memcpy(&param, &_spu_rev_param[_spu_rev_attr.mode], sizeof(SpuReverbPreset));
        switch (_spu_rev_attr.mode) {
        case SPU_REV_MODE_ECHO:
            _spu_rev_attr.feedback = 0x7f;
            _spu_rev_attr.delay = 0x7f;
            break;
        case SPU_REV_MODE_DELAY:
            _spu_rev_attr.feedback = 0;
            _spu_rev_attr.delay = 0x7f;
            break;
        default:
            _spu_rev_attr.feedback = 0;
            _spu_rev_attr.delay = 0;
            break;
        }
    }
    if (all || (mask & SPU_REV_DELAYTIME)) {
        switch (_spu_rev_attr.mode) {
        case SPU_REV_MODE_ECHO:
        case SPU_REV_MODE_DELAY:
            delay_set = 1;
            if (!mode_set) {
                _memcpy(&param, &_spu_rev_param[_spu_rev_attr.mode], sizeof(SpuReverbPreset));
                param.mask = SPU_REV_MASK_mLSAME | SPU_REV_MASK_mRSAME |
                             SPU_REV_MASK_mLCOMB1 | SPU_REV_MASK_dLSAME |
                             SPU_REV_MASK_mLAPF1 | SPU_REV_MASK_mRAPF1;
            }
            delay = attr->delay;
            _spu_rev_attr.delay = delay;
            param.mLSAME = (delay << 13) / 127 - param.dAPF1;
            half_delay = (delay << 12) / 127;
            param.mRSAME = half_delay - param.dAPF2;
            param.mLCOMB1 = half_delay + param.mRCOMB1;
            param.dLSAME = half_delay + param.dRSAME;
            param.mLAPF1 = half_delay + param.mLAPF2;
            param.mRAPF1 = half_delay + param.mRAPF2;
            break;
        default:
            _spu_rev_attr.delay = 0;
            break;
        }
    }
    if (all || (mask & SPU_REV_FEEDBACK)) {
        switch (_spu_rev_attr.mode) {
        case SPU_REV_MODE_ECHO:
        case SPU_REV_MODE_DELAY:
            feedback_set = 1;
            if (!mode_set) {
                if (!delay_set) {
                    _memcpy(&param, &_spu_rev_param[_spu_rev_attr.mode], sizeof(SpuReverbPreset));
                    param.mask = SPU_REV_MASK_vWALL;
                } else {
                    param.mask |= SPU_REV_MASK_vWALL;
                }
            }
            feedback = attr->feedback;
            _spu_rev_attr.feedback = feedback;
            param.vWALL = (feedback * 0x8100) / 127;
            break;
        default:
            _spu_rev_attr.feedback = 0;
            break;
        }
    }
    if (mode_set) {
        rev_on = (_spu_RXX[SPU_REG_SPUCNT] >> SPUCNT_REVERB_SHIFT) & 1;
        if (rev_on) {
            spucnt = _spu_RXX[SPU_REG_SPUCNT];
            spucnt &= ~SPUCNT_REVERB_ENABLE;
            _spu_RXX[SPU_REG_SPUCNT] = spucnt;
        }
    }
    if (!mode_set) {
        if (all || (mask & SPU_REV_DEPTHL)) {
            _spu_RXX[SPU_REG_RVOL_L] = attr->depth.left;
            _spu_rev_attr.depth.left = attr->depth.left;
        }
        if (all || (mask & SPU_REV_DEPTHR)) {
            _spu_RXX[SPU_REG_RVOL_R] = attr->depth.right;
            _spu_rev_attr.depth.right = attr->depth.right;
        }
    } else {
        _spu_RXX[SPU_REG_RVOL_L] = 0;
        _spu_RXX[SPU_REG_RVOL_R] = 0;
        _spu_rev_attr.depth.left = 0;
        _spu_rev_attr.depth.right = 0;
    }
    if (mode_set || delay_set || feedback_set) {
        _spu_setReverbAttr(&param);
    }
    if (clear_wa) {
        SpuClearReverbWorkArea(_spu_rev_attr.mode);
    }
    if (mode_set) {
        _spu_FsetRXX(SPU_REG_MBASE, _spu_rev_offsetaddr, SPU_FSETRXX_RAW);
        if (rev_on) {
            spucnt = _spu_RXX[SPU_REG_SPUCNT];
            spucnt |= SPUCNT_REVERB_ENABLE;
            _spu_RXX[SPU_REG_SPUCNT] = spucnt;
        }
    }
    return SPU_SUCCESS;
}

static void _spu_setReverbAttr(SpuReverbPreset *param)
{
    unsigned long mask = param->mask;
    long all = mask == 0;

    if (all || (mask & SPU_REV_MASK_dAPF1)) {
        _spu_RXX[SPU_REG_REV_dAPF1] = param->dAPF1;
    }
    if (all || (mask & SPU_REV_MASK_dAPF2)) {
        _spu_RXX[SPU_REG_REV_dAPF2] = param->dAPF2;
    }
    if (all || (mask & SPU_REV_MASK_vIIR)) {
        _spu_RXX[SPU_REG_REV_vIIR] = param->vIIR;
    }
    if (all || (mask & SPU_REV_MASK_vCOMB1)) {
        _spu_RXX[SPU_REG_REV_vCOMB1] = param->vCOMB1;
    }
    if (all || (mask & SPU_REV_MASK_vCOMB2)) {
        _spu_RXX[SPU_REG_REV_vCOMB2] = param->vCOMB2;
    }
    if (all || (mask & SPU_REV_MASK_vCOMB3)) {
        _spu_RXX[SPU_REG_REV_vCOMB3] = param->vCOMB3;
    }
    if (all || (mask & SPU_REV_MASK_vCOMB4)) {
        _spu_RXX[SPU_REG_REV_vCOMB4] = param->vCOMB4;
    }
    if (all || (mask & SPU_REV_MASK_vWALL)) {
        _spu_RXX[SPU_REG_REV_vWALL] = param->vWALL;
    }
    if (all || (mask & SPU_REV_MASK_vAPF1)) {
        _spu_RXX[SPU_REG_REV_vAPF1] = param->vAPF1;
    }
    if (all || (mask & SPU_REV_MASK_vAPF2)) {
        _spu_RXX[SPU_REG_REV_vAPF2] = param->vAPF2;
    }
    if (all || (mask & SPU_REV_MASK_mLSAME)) {
        _spu_RXX[SPU_REG_REV_mLSAME] = param->mLSAME;
    }
    if (all || (mask & SPU_REV_MASK_mRSAME)) {
        _spu_RXX[SPU_REG_REV_mRSAME] = param->mRSAME;
    }
    if (all || (mask & SPU_REV_MASK_mLCOMB1)) {
        _spu_RXX[SPU_REG_REV_mLCOMB1] = param->mLCOMB1;
    }
    if (all || (mask & SPU_REV_MASK_mRCOMB1)) {
        _spu_RXX[SPU_REG_REV_mRCOMB1] = param->mRCOMB1;
    }
    if (all || (mask & SPU_REV_MASK_mLCOMB2)) {
        _spu_RXX[SPU_REG_REV_mLCOMB2] = param->mLCOMB2;
    }
    if (all || (mask & SPU_REV_MASK_mRCOMB2)) {
        _spu_RXX[SPU_REG_REV_mRCOMB2] = param->mRCOMB2;
    }
    if (all || (mask & SPU_REV_MASK_dLSAME)) {
        _spu_RXX[SPU_REG_REV_dLSAME] = param->dLSAME;
    }
    if (all || (mask & SPU_REV_MASK_dRSAME)) {
        _spu_RXX[SPU_REG_REV_dRSAME] = param->dRSAME;
    }
    if (all || (mask & SPU_REV_MASK_mLDIFF)) {
        _spu_RXX[SPU_REG_REV_mLDIFF] = param->mLDIFF;
    }
    if (all || (mask & SPU_REV_MASK_mRDIFF)) {
        _spu_RXX[SPU_REG_REV_mRDIFF] = param->mRDIFF;
    }
    if (all || (mask & SPU_REV_MASK_mLCOMB3)) {
        _spu_RXX[SPU_REG_REV_mLCOMB3] = param->mLCOMB3;
    }
    if (all || (mask & SPU_REV_MASK_mRCOMB3)) {
        _spu_RXX[SPU_REG_REV_mRCOMB3] = param->mRCOMB3;
    }
    if (all || (mask & SPU_REV_MASK_mLCOMB4)) {
        _spu_RXX[SPU_REG_REV_mLCOMB4] = param->mLCOMB4;
    }
    if (all || (mask & SPU_REV_MASK_mRCOMB4)) {
        _spu_RXX[SPU_REG_REV_mRCOMB4] = param->mRCOMB4;
    }
    if (all || (mask & SPU_REV_MASK_dLDIFF)) {
        _spu_RXX[SPU_REG_REV_dLDIFF] = param->dLDIFF;
    }
    if (all || (mask & SPU_REV_MASK_dRDIFF)) {
        _spu_RXX[SPU_REG_REV_dRDIFF] = param->dRDIFF;
    }
    if (all || (mask & SPU_REV_MASK_mLAPF1)) {
        _spu_RXX[SPU_REG_REV_mLAPF1] = param->mLAPF1;
    }
    if (all || (mask & SPU_REV_MASK_mRAPF1)) {
        _spu_RXX[SPU_REG_REV_mRAPF1] = param->mRAPF1;
    }
    if (all || (mask & SPU_REV_MASK_mLAPF2)) {
        _spu_RXX[SPU_REG_REV_mLAPF2] = param->mLAPF2;
    }
    if (all || (mask & SPU_REV_MASK_mRAPF2)) {
        _spu_RXX[SPU_REG_REV_mRAPF2] = param->mRAPF2;
    }
    if (all || (mask & SPU_REV_MASK_vLIN)) {
        _spu_RXX[SPU_REG_REV_vLIN] = param->vLIN;
    }
    if (all || (mask & SPU_REV_MASK_vRIN)) {
        _spu_RXX[SPU_REG_REV_vRIN] = param->vRIN;
    }
}
