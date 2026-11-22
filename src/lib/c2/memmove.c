/* Copy n bytes between potentially overlapping memory regions. */
#include <memory.h>

void *memmove(unsigned char *dst, unsigned char *src, int n)
{
    if (dst >= src) {
        while (n-- > 0) {
            dst[n] = src[n];
        }
    } else {
        while (n-- > 0) {
            *dst++ = *src++;
        }
    }
    return dst;
}
