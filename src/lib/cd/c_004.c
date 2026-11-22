/* CD-ROM stream ring frame-completion callback and back-location query. */

#include <sys/types.h>
#include <libcd.h>

#define CD_STREAM_MODULE
#include "cd_int.h"

static CdlLOC previous_frame_location;
static u_long next_frameid;

void data_ready_callback(void)
{
	StHEADER *header;

	header = (StHEADER *)(StRingAddr + StRingIdx2 * HEADER_SIZE);
	((u_short *)header)[StSTATUS] = StCOMPLETE;
	previous_frame_location = header->loc;
	next_frameid = header->frameCount;
	StRingIdx2 = StRingIdx1;
	if (StFunc1)
		(*StFunc1)();
	StFinalSector = 0;
}

int StGetBackloc(CdlLOC *loc)
{
	if (StMode != ST_MODE_2340)
		return -1;
	CdIntToPos(CdPosToInt(&previous_frame_location) + 1, loc);
	return next_frameid;
}
