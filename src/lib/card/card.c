#include <sys/types.h>
#include <kernel.h>

long _card_clear(long chan)
{
    _new_card();
    return _card_write(chan, 63, NULL);
}
