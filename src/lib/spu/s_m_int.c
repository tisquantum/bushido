/*
 * LIBSPU sound RAM allocator garbage collection.
 */
#include <sys/types.h>
#include <libspu.h>

#include "spu_int.h"

void _spu_gcSPU(void)
{
    long i;
    long j;
    unsigned long swap_addr;
    unsigned long swap_size;

    for (i = 0; i <= _spu_AllocLastNum; ) {
        if (_spu_memList[i].addr & SPU_MALLOC_FREE) {
            for (j = i + 1; ; j++) {
                if (_spu_memList[j].addr == SPU_MALLOC_EMPTY) {
                    continue;
                }
                break;
            }
            if ((_spu_memList[j].addr & SPU_MALLOC_FREE) &&
                ((_spu_memList[j].addr & SPU_MALLOC_ADDR) ==
                 ((_spu_memList[i].addr & SPU_MALLOC_ADDR) + _spu_memList[i].size))) {
                _spu_memList[j].addr = SPU_MALLOC_EMPTY;
                _spu_memList[i].size += _spu_memList[j].size;
                continue;
            }
        }
        i++;
    }

    for (i = 0; i <= _spu_AllocLastNum; i++) {
        if (_spu_memList[i].size == 0) {
            _spu_memList[i].addr = SPU_MALLOC_EMPTY;
        }
    }

    for (i = 0; i <= _spu_AllocLastNum; i++) {
        if (_spu_memList[i].addr & SPU_MALLOC_END) {
            break;
        }
        for (j = i + 1; j <= _spu_AllocLastNum; j++) {
            if (_spu_memList[j].addr & SPU_MALLOC_END) {
                break;
            }
            if ((_spu_memList[j].addr & SPU_MALLOC_ADDR) <
                (_spu_memList[i].addr & SPU_MALLOC_ADDR)) {
                swap_addr = _spu_memList[i].addr;
                swap_size = _spu_memList[i].size;
                _spu_memList[i].addr = _spu_memList[j].addr;
                _spu_memList[i].size = _spu_memList[j].size;
                _spu_memList[j].addr = swap_addr;
                _spu_memList[j].size = swap_size;
            }
        }
    }

    for (i = 0; i <= _spu_AllocLastNum; i++) {
        if (_spu_memList[i].addr & SPU_MALLOC_END) {
            break;
        }
        if (_spu_memList[i].addr == SPU_MALLOC_EMPTY) {
            _spu_memList[i].addr = _spu_memList[_spu_AllocLastNum].addr;
            _spu_memList[i].size = _spu_memList[_spu_AllocLastNum].size;
            _spu_AllocLastNum = i;
            break;
        }
    }

    for (i = _spu_AllocLastNum - 1; i >= 0; i--) {
        if (_spu_memList[i].addr & SPU_MALLOC_FREE) {
            _spu_memList[i].addr = (_spu_memList[i].addr & SPU_MALLOC_ADDR) | SPU_MALLOC_END;
            _spu_memList[i].size += _spu_memList[_spu_AllocLastNum].size;
            _spu_AllocLastNum = i;
        } else {
            break;
        }
    }
}
