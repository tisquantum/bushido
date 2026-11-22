/*
 * Controller send handler setup and transmission (SendPAD).
 */
#include <sys/types.h>
#include <kernel.h>
#include "api_int.h"

long SendPAD(char *buf1, long len1, char *buf2, long len2)
{
    _send_pad();
    return _SendPAD(buf1, len1, buf2, len2);
}
