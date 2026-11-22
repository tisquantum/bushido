/*
 * LIBCD streaming ring release (StFreeRing): frees a locked frame's sectors
 * in the ring buffer and advances the consumer read index.
 */
#include <sys/types.h>
#include <libcd.h>

#define CD_STREAM_MODULE
#include "cd_int.h"

u_long StFreeRing(u_long *base)
{
	int sector_index, i;
	short sector_count;
	short *header;

	sector_index = (base - (StRingAddr + StRingSize * HEADER_SIZE)) / (SECTOR_SIZE - HEADER_SIZE);
	header = (short *)(StRingAddr + sector_index * HEADER_SIZE);
	sector_count = ((StHEADER *)header)->nSectors;
	if (header[StSTATUS] != StLOCK)
		return 1;
	for (i = 0; i < sector_count; i++)
		((short *)(StRingAddr + (i + sector_index) * HEADER_SIZE))[StSTATUS] = StFREE;
	StRingIdx3 = i + sector_index;
	return 0;
}
