/* Unregister the CD-ROM streaming ring buffer and stop sector data transfers. */

#include <sys/types.h>
#include <kernel.h>
#include <libcd.h>

#define CD_STREAM_MODULE
#include "cd_int.h"

void StUnSetRing(void)
{
	EnterCriticalSection();
	CdDataCallback(0);
	CdReadyCallback(0);
	*CD_REG0 = CD_BANK_COMMAND;
	*CD_REG3 = CD_REQ_CLEAR;	/* index 0 request register: clear BFRD to stop sector data transfers */
	ExitCriticalSection();
}
