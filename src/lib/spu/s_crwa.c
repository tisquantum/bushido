/* Clear the SPU reverb work area in SPU RAM. */
#include <sys/types.h>
#include <kernel.h>
#include <libspu.h>

#include "spu_int.h"

/* SPU_REV_MODE_OFF clears the final 0x10 units (128 bytes) at 0xfff0. */
#define SPU_REV_OFF_CLEAR_ADDR  0xfff0
#define SPU_REV_OFF_CLEAR_SIZE  0x10

long SpuClearReverbWorkArea(long mode)
{
    unsigned long size, addr, chunk_size;
    long more;
    long prev_trans_mode;
    long restore_trans_mode = 0;
    volatile SpuTransferCallbackProc saved_callback = NULL;

    if (mode < SPU_REV_MODE_OFF || mode >= SPU_REV_MODE_MAX ||
        _SpuIsInAllocateArea_(_spu_rev_startaddr[mode])) {
        return SPU_ERROR;
    }
    if (mode == SPU_REV_MODE_OFF) {
        size = SPU_REV_OFF_CLEAR_SIZE << _spu_mem_mode_plus;
        addr = SPU_REV_OFF_CLEAR_ADDR << _spu_mem_mode_plus;
    } else {
        size = (SPU_RAM_UNITS - _spu_rev_startaddr[mode]) << _spu_mem_mode_plus;
        addr = _spu_rev_startaddr[mode] << _spu_mem_mode_plus;
    }
    prev_trans_mode = _spu_transMode;
    if (prev_trans_mode == SPU_TRANSFER_BY_IO) {
        _spu_transMode = SPU_TRANSFER_BY_DMA;
        restore_trans_mode = 1;
    }
    more = 1;
    if (_spu_transferCallback) {
        saved_callback = _spu_transferCallback;
        _spu_transferCallback = NULL;
    }
    do {
        if (size > SPU_ZEROBUF_SIZE) {
            chunk_size = SPU_ZEROBUF_SIZE;
        } else {
            chunk_size = size;
            more = 0;
        }
        _spu_t(SPU_TRANS_ADDR, addr);
        _spu_t(SPU_TRANS_WRITE);
        _spu_t(SPU_TRANS_START, _spu_zerobuf, chunk_size);
        WaitEvent(_spu_EVdma);
        size -= SPU_ZEROBUF_SIZE;
        addr += SPU_ZEROBUF_SIZE;
    } while (more);
    if (restore_trans_mode) {
        _spu_transMode = prev_trans_mode;
    }
    if (saved_callback) {
        _spu_transferCallback = saved_callback;
    }
    return SPU_SUCCESS;
}
