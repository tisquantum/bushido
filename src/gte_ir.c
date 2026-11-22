/* Stores the GTE's IR1..IR3 into a VECTOR; nothing in the game calls it. */
#include <sys/types.h>
#include <libgte.h>
#include <inline_c.h>

static void StoreGteIrVector(VECTOR *vector)
{
    gte_stlvl(vector);
}
