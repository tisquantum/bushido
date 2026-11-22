/*
 * PlayStation link cable ("sio") serial I/O device driver and control interface.
 */
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <fs.h>
#include <kernel.h>
#include <libgte.h>
#include <libgpu.h>
#include <libcomb.h>
#include "api_int.h"
#include "comb.h"
#include "comb_control.h"
#include "psx_hw.h"

#define SIO_INTR_PRIORITY 3

/* SIO1 baud clock (Hz): divider = SIO_BASE_CLOCK / baud_rate */
#define SIO_BASE_CLOCK    2073600
#define SIO_DEFAULT_BAUD  9600
#define SIO_RX_UNIT_MAX   8

#define COMB_TX_EMPTY     (COMB_TXRDY | COMB_TXU)
#define COMB_ERR_MASK     (COMB_FE | COMB_OE | COMB_PERROR)

#define CR_TXEN           0x001
#define CR_DTR            0x002
#define CR_RXEN           0x004
#define CR_ERRRST         0x010
#define CR_RTS            0x020
#define CR_INTRST         0x040
#define CR_BUFSZ_1        0x000
#define CR_BUFSZ_2        0x100
#define CR_BUFSZ_4        0x200
#define CR_BUFSZ_8        0x300
#define CR_BUFSZ_MASK     0x300
#define CR_BUFSZ_SHIFT    8
#define CR_TXIEN          0x400
#define CR_RXIEN          0x800

#define MR_BR_16          0x02
#define MR_CHLEN_8        0x0c
#define MR_SB_11          0xc0

#define RESET_GRAPH_QUIET      5	/* ResetGraph's mode that resets the GPU without printing */

/* State of one transfer direction (send or receive) */
struct sio_io {
	long busy;
	u_char *buf;
	long count;
	long dsr;
};

/* Kernel file control block passed to a device's strategy routine */
struct FCB {
	long status;
	long diskid;
	u_char *transfer_addr;
	long transfer_length;
	long curpos;
	long devflag;
	long errno;
};

/* Kernel device control block registered with AddDrv */
struct DCB {
	char *name;
	long flags;
	long ssize;
	char *desc;
	long (*init)();
	long (*open)();
	long (*inout)();
	long (*close)();
	long (*ioctl)();
	long (*read)();
	long (*write)();
	long (*erase)();
	long (*undelete)();
	long (*firstfile)();
	long (*nextfile)();
	long (*format)();
	long (*chdir)();
	long (*rename)();
	long (*remove)();
	long (*testdevice)();
};

static long r_sioinit(void);
static long r_sioremove(void);
static long r_sioopen(void);
static long r_sioclose(void);
static long r_siostrategy(struct FCB *fcb, long cmd);
static long __nulldev(void);
static long HandleSio(void);
static long EvalpSio(void);

static volatile struct Sio1Reg *sio = (struct Sio1Reg *)SIO1_REG_BASE;
static volatile u_long *sio_data = (volatile u_long *)SIO1_REG_BASE;
static struct IntrChainEntry sio_intr = { 0, HandleSio, EvalpSio, 0 };
static volatile struct IntrReg *intr = (struct IntrReg *)INTR_STAT_ADDR;

/* Maps RX interrupt threshold byte count (1, 2, 4, 8) to CR_BUFSZ_* bits; -1 if invalid */
static short rx_level[] = {
	-1, CR_BUFSZ_1, CR_BUFSZ_2, -1, CR_BUFSZ_4, -1, -1, -1, CR_BUFSZ_8
};
/* Maps CR_BUFSZ_* field index (0..3) back to byte count */
static short rx_bytes[] = { 1, 2, 4, 8 };

static struct DCB sio_dcb = {
	"sio", DTTYPE_CHAR | DTTYPE_CONS, 1, "SIO console",
	r_sioinit, r_sioopen, r_siostrategy, r_sioclose,
	__nulldev, __nulldev, __nulldev, __nulldev, __nulldev, __nulldev,
	__nulldev, __nulldev, __nulldev, __nulldev, r_sioremove, __nulldev,
};

typedef long (*CombWaitFunc)(long, long);

/* the mode, control and baud values the driver sets and restores the port to */
static volatile struct Sio1Reg regs;
static CombWaitFunc CombWaitCallback;
static volatile struct sio_io sen;
static volatile struct sio_io rec;

static long r_sioinit(void)
{
	EnterCriticalSection();
	regs.control = CR_TXEN | CR_RXEN;
	regs.mode = MR_SB_11 | MR_CHLEN_8 | MR_BR_16;
	regs.baud = SIO_BASE_CLOCK / SIO_DEFAULT_BAUD;
	sio->control = CR_INTRST | CR_ERRRST;
	sio->mode = regs.mode;
	sio->baud = regs.baud;
	sio->control |= CR_ERRRST;
	sio->control = regs.control;
	SysDeqIntRP(SIO_INTR_PRIORITY, &sio_intr);
	SysEnqIntRP(SIO_INTR_PRIORITY, &sio_intr);
	intr->mask |= INTR_MASK_SIO;
	sen.busy = rec.busy = 0;
	sen.buf = rec.buf = 0;
	CombWaitCallback = 0;
	sen.count = rec.count = 0;
	ExitCriticalSection();
	return 0;
}

static long r_sioremove(void)
{
	EnterCriticalSection();
	intr->mask &= ~INTR_MASK_SIO;
	sio->control |= CR_INTRST | CR_ERRRST;
	sio->control = regs.control;
	SysDeqIntRP(SIO_INTR_PRIORITY, &sio_intr);
	sen.busy = rec.busy = 0;
	sen.count = rec.count = 0;
	ExitCriticalSection();
	return 0;
}

static long r_sioopen(void)
{
	return 0;
}

static long r_sioclose(void)
{
	return 0;
}

static long SioAnsyncRead(u_char *buf, long count);
static long SioSyncroRead(u_char *buf, long count);
static long SioAnsyncWrite(u_char *buf, long count);
static long SioSyncroWrite(u_char *buf, long count);

static long r_siostrategy(struct FCB *fcb, long cmd)
{
	long transferred;

	fcb->errno = 0;
	if (cmd == READ) {
		if (fcb->status & FASYNC) {
			if (SioAnsyncRead(fcb->transfer_addr, fcb->transfer_length)) {
				fcb->errno = EBUSY;
				return -1;
			}
			return 0;
		}
		transferred = SioSyncroRead(fcb->transfer_addr, fcb->transfer_length);
		if (transferred < 0)
			fcb->errno = EIO;
		return transferred;
	}
	if (cmd == WRITE) {
		if (fcb->status & FASYNC) {
			if (SioAnsyncWrite(fcb->transfer_addr, fcb->transfer_length)) {
				fcb->errno = EBUSY;
				return -1;
			}
			return 0;
		}
		transferred = SioSyncroWrite(fcb->transfer_addr, fcb->transfer_length);
		if (transferred < 0)
			fcb->errno = EIO;
		return transferred;
	}
	fcb->errno = EINVAL;
	return -1;
}

static long __nulldev(void)
{
	return 0;
}

void AddCOMB(void)
{
	AddDrv(&sio_dcb);
}

void DelCOMB(void)
{
	DelDrv("sio");
	FlushCache();
}

/* returns no value */
int ChangeClearSIO(long val)
{
}

static long SioAnsyncRead(u_char *buf, long count)
{
	if (rec.busy)
		return -1;
	rec.count = count;
	rec.buf = buf;
	rec.busy = 1;
	sio->control |= CR_RXIEN;
	sio->control |= CR_RTS;
	return 0;
}

static long SioSyncroRead(u_char *buf, long count)
{
	long chunk_bytes, wait_count, step;
	u_short saved_control;

	wait_count = 0;
	if (rec.busy)
		return -1;
	step = rx_bytes[(regs.control & CR_BUFSZ_MASK) >> CR_BUFSZ_SHIFT];
	rec.count = count;
	rec.buf = buf;
	rec.busy = 0;
	sio->control |= CR_RTS;
	chunk_bytes = 0;
	while (rec.count) {
		if (sio->status & COMB_ERR_MASK) {
			saved_control = sio->control;
			sio->control = CR_INTRST | CR_ERRRST;
			sio->mode = regs.mode;
			sio->baud = regs.baud;
			sio->control |= CR_ERRRST;
			sio->control = saved_control;
			sio->control &= ~CR_RTS;
			DeliverEvent(HwSIO, EvSpERROR);
			return count - rec.count;
		}
		while (!(sio->status & COMB_RXRDY)) {
			if (CombWaitCallback && !CombWaitCallback(READ, wait_count++)) {
				sio->control &= ~CR_RTS;
				DeliverEvent(HwSIO, EvSpTIMOUT);
				return count - rec.count;
			}
		}
		*rec.buf = sio->data;
		rec.buf++;
		rec.count--;
		if (++chunk_bytes == step) {
			sio->control ^= CR_DTR;
			chunk_bytes = 0;
		}
	}
	sio->control &= ~CR_RTS;
	return count - rec.count;
}

static long SioAnsyncWrite(u_char *buf, long count)
{
	if (sen.busy)
		return -1;
	sen.count = count;
	sen.buf = buf;
	sen.busy = 1;
	sen.dsr = sio->status & COMB_DSR;
	sio->control |= CR_TXIEN;
	return 0;
}

static long SioSyncroWrite(u_char *buf, long count)
{
	long chunk_bytes, wait_count, step;

	wait_count = 0;
	if (sen.busy)
		return -1;
	step = rx_bytes[(regs.control & CR_BUFSZ_MASK) >> CR_BUFSZ_SHIFT];
	sen.count = count;
	sen.buf = buf;
	chunk_bytes = 0;
	while (sen.count) {
		while ((sio->status & COMB_TX_EMPTY) != COMB_TX_EMPTY) {
			if (CombWaitCallback && !CombWaitCallback(WRITE, wait_count++)) {
				DeliverEvent(HwSIO, EvSpTIMOUT);
				return count - sen.count;
			}
		}
		if (chunk_bytes == 0)
			sen.dsr = sio->status & COMB_DSR;
		sio->data = *sen.buf;
		sen.buf++;
		sen.count--;
		if (++chunk_bytes == step) {
			while ((sio->status & COMB_DSR) == sen.dsr) {
				if (CombWaitCallback && !CombWaitCallback(WRITE, wait_count++)) {
					DeliverEvent(HwSIO, EvSpTIMOUT);
					return count - sen.count - 1;
				}
			}
			chunk_bytes = 0;
		}
	}
	return count - sen.count;
}

static void reset_graph(void)
{
	ResetGraph(RESET_GRAPH_QUIET);
}

int _comb_control(u_long cmd, u_long arg, u_long param)
{
	int ret;

	ret = 0;
	switch (cmd) {
	case COMB_CTRL_GET:
		switch (arg) {
		case COMB_PARAM_STATUS:
			ret = sio->status;
			break;
		case COMB_PARAM_CONTROL:
			ret = (sio->control >> 1) & COMB_BIT_DTR;
			if (sio->control & CR_RTS)
				ret |= COMB_BIT_RTS;
			break;
		case COMB_PARAM_MODE:
			ret = regs.mode;
			break;
		case COMB_PARAM_BAUD:
			ret = SIO_BASE_CLOCK / regs.baud;
			break;
		case COMB_PARAM_RX_UNIT:
			ret = 1;
			break;
		case COMB_PARAM_ASYNC_COUNT:
			if (param == COMB_ASYNC_WRITE)
				ret = sen.count;
			else
				ret = rec.count;
			break;
		case COMB_PARAM_ASYNC_BUSY:
			ret = (param == COMB_ASYNC_WRITE ? sen.busy : rec.busy) != 0;
			break;
		}
		break;
	case COMB_CTRL_SET:
		switch (arg) {
		case COMB_PARAM_STATUS:
			break;
		case COMB_PARAM_CONTROL:
			regs.control = sio->control & ~(CR_RTS | CR_DTR);
			regs.control |= (param & COMB_BIT_DTR ? CR_DTR : 0) |
			                (param & COMB_BIT_RTS ? CR_RTS : 0);
			sio->control = regs.control;
			break;
		case COMB_PARAM_MODE:
			regs.mode = param;
			sio->mode = regs.mode;
			break;
		case COMB_PARAM_BAUD:
			if (SIO_BASE_CLOCK % param) {
				ret = -1;
				break;
			}
			regs.baud = SIO_BASE_CLOCK / param;
			sio->baud = regs.baud;
			sio->control |= CR_ERRRST;
			break;
		case COMB_PARAM_RX_UNIT:
			if (param == 0 || param > SIO_RX_UNIT_MAX)
				break;
			ret = rx_level[param];
			if (ret < 0)
				break;
			regs.control &= ~CR_BUFSZ_MASK;
			regs.control |= ret;
			sio->control = regs.control;
			break;
		}
		break;
	case COMB_CTRL_RESET:
		switch (arg) {
		case COMB_RESET_ALL:
			EnterCriticalSection();
			sio->control |= CR_INTRST | CR_ERRRST;
			sio->mode = regs.mode;
			sio->control = regs.control;
			sio->baud = regs.baud;
			ret = 0;
			sio->control |= CR_ERRRST;
			ExitCriticalSection();
			break;
		case COMB_RESET_ERROR:
			ret = 0;
			sio->control |= CR_ERRRST;
			break;
		case COMB_CANCEL_WRITE:
			EnterCriticalSection();
			sio->control &= ~CR_TXIEN;
			sen.busy = 0;
			sen.count = 0;
			ExitCriticalSection();
			break;
		case COMB_CANCEL_READ:
			EnterCriticalSection();
			sio->control &= ~(CR_RXIEN | CR_RTS);
			rec.busy = 0;
			rec.count = 0;
			ExitCriticalSection();
			break;
		}
		break;
	case COMB_CTRL_HANDSHAKE:
		switch (arg) {
		case COMB_HANDSHAKE_SET_RTS:
			if (param)
				sio->control |= CR_RTS;
			else
				sio->control &= ~CR_RTS;
			break;
		case COMB_HANDSHAKE_GET_CTS:
			if (sio->status & COMB_CTS)
				ret = 1;
			break;
		}
		break;
	case COMB_CTRL_SET_CALLBACK:
		if (arg == 0) {
			/* kept: the plain form compiles differently; cause not established */
			CombWaitFunc *cb = &CombWaitCallback;
			ret = (int)*cb;
			*cb = (CombWaitFunc)param;
		}
		break;
	case COMB_CTRL_RESET_GRAPH:
		if (arg == 0)
			reset_graph();
		break;
	}
	return ret;
}

static long EvalpSio(void)
{
	if (!(intr->stat & intr->mask & INTR_MASK_SIO))
		return 0;
	if (sen.busy && (sio->status & COMB_TXRDY))
		sio->control &= ~CR_TXIEN;
	return 1;
}

static long HandleSio(void)
{
	u_long stat;
	long step;

	stat = intr->stat;
	if (stat & INTR_MASK_SIO)
		intr->stat = stat & ~INTR_MASK_SIO;
	if (sio->status & COMB_ERR_MASK) {
		sio->control &= ~(CR_RXIEN | CR_RTS);
		rec.count = 0;
		rec.busy = 0;
		(void)sio->control;
		sio->control = CR_INTRST | CR_ERRRST;
		sio->mode = regs.mode;
		sio->baud = regs.baud;
		sio->control |= CR_ERRRST;
		DeliverEvent(HwSIO, EvSpERROR);
		return 0;
	}
	if (rec.busy && rec.count && (sio->status & COMB_RXRDY)) {
		step = rx_bytes[(regs.control & CR_BUFSZ_MASK) >> CR_BUFSZ_SHIFT];
		switch (step) {
		case 1:
			*rec.buf = sio->data;
			break;
		case 2:
			rec.buf[0] = sio->data;
			rec.buf[1] = sio->data;
			break;
		case 4:
			*(u_long *)rec.buf = *sio_data;
			break;
		case 8:
			((u_long *)rec.buf)[0] = *sio_data;
			((u_long *)rec.buf)[1] = *sio_data;
			break;
		}
		rec.buf += step;
		rec.count -= step;
		if (rec.count == 0) {
			rec.busy = 0;
			sio->control &= ~(CR_RXIEN | CR_RTS);
			DeliverEvent(HwSIO, EvSpIOER);
		}
		sio->control ^= CR_DTR;
	}
	if (sen.busy && (sio->status & COMB_TXRDY) && (sio->status & COMB_DSR) == sen.dsr) {
		if (sen.count == 0) {
			sen.busy = 0;
			DeliverEvent(HwSIO, EvSpIOEW);
		} else {
			sen.dsr ^= COMB_DSR;
			sio->data = *sen.buf;
			sen.buf++;
			sen.count--;
			sio->control |= CR_TXIEN;
		}
	}
	sio->control |= CR_ERRRST;
	return 0;
}
