/*
 * CD-ROM subsystem initialization and default kernel event callbacks.
 */
#include <sys/types.h>
#include <kernel.h>
#include <stdio.h>
#include <libcd.h>

static void def_cbsync(u_char intr, u_char *result);
static void def_cbready(u_char intr, u_char *result);
static void def_cbread(u_char intr, u_char *result);

int CdInit(void)
{
	int retries = 4;

	while (CdReset(1) != 1) {
		if (retries-- == 0) {
			printf("CdInit: Init failed\n");
			return 0;
		}
	}
	CdSyncCallback(def_cbsync);
	CdReadyCallback(def_cbready);
	CdReadCallback(def_cbread);
	return 1;
}

static void def_cbsync(u_char intr, u_char *result)
{
	DeliverEvent(HwCdRom, EvSpCOMP);
}

static void def_cbready(u_char intr, u_char *result)
{
	DeliverEvent(HwCdRom, EvSpDR);
}

static void def_cbread(u_char intr, u_char *result)
{
	DeliverEvent(HwCdRom, EvSpDR);
}
