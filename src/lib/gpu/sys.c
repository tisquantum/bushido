/*
 * GPU control, drawing/display environments, command queue, and DMA transfer.
 */
#include <sys/types.h>
#include <stdio.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include "etc_int.h"
#include "gpu_int.h"

/* memory.h would also declare memset, which this module defines static */
extern void *memcpy();

static void memset(char *p, int c, int n);

static int _addque(), _addque2(), _clr(), _cwb(), _drs(), _dws();
static int _exeque(), _getctl(), _otc(), _param(), _reset(), _sync();
static void _ctl(), _cwc();
static u_long _status();

static void checkRECT(char *s, RECT *r);
static void SetDrawEnv2(DR_ENV *dr_env, DRAWENV *de);
static u_long get_mode(int dfe, int dtd, u_short tpage);
static u_long get_cs(short x, short y);
static u_long get_ce(short x, short y);
static u_long get_ofs(short x, short y);
static u_long get_tw(RECT *tw);
static int get_dx(DISPENV *de);
static void set_alarm(void);
static int get_alarm(void);
static int _version(int mode);

/* a queued call: the routine, its parameter or a copy of it in buf, and a value */
typedef struct {
	void	(*func)();
	u_long	*param;
	u_long	value;
	u_long	buf[21];
} GpuQueueEntry;

/* the GPU's jump table */
typedef struct {
	char	*ver;
	int	(*addque)();
	int	(*addque2)();
	int	(*clr)();
	void	(*ctl)();
	int	(*cwb)();
	void	(*cwc)();
	int	(*drs)();
	int	(*dws)();
	int	(*exeque)();
	int	(*getctl)();
	int	(*otc)();
	int	(*param)();
	int	(*reset)();
	u_long	(*status)();
	int	(*sync)();
} GpuJumpTable;

/* "Ps", a byte that differs by library, then five more; nothing reads them */
static u_long gpu_id[2] = { 0x0c077350, 0x00379cda };

static GpuJumpTable jtb = {
	"$Id: sys.c,v 1.129 1996/12/25 03:36:20 noda Exp $",
	_addque, _addque2, _clr, _ctl, _cwb, _cwc, _drs, _dws,
	_exeque, _getctl, _otc, _param, _reset, _status, _sync,
};

static GpuJumpTable *gpu = &jtb;

int (*GPU_printf)() = printf;

static struct {
	u_char	version;
	u_char	que;
	u_char	debug;
	u_char	reverse;
	short	w, h;		/* frame buffer size */
	volatile int syncflag;	/* a command queued since the last draw sync */
	void	(*drawsync)();	/* DrawSyncCallback */
	DRAWENV	draw;
	volatile DISPENV disp;
} env = { 0 };

static int vram_width[] = { 1024, 1024, 1024, 1024, 1024 };
static int vram_height[] = { 512, 1024, 1024, 512, 1024 };

int ResetGraph(int mode)
{
	switch (mode & 7) {
	case 3:
	case 0:
		printf("ResetGraph:jtb=%08x,env=%08x\n", &jtb, &env);
	case 5:
		memset((char *)&env, 0, sizeof(env));
		ResetCallback();
		GPU_cw((u_long)gpu & GPU_ADDR_MASK);
		env.version = _reset(mode);
		env.que = 1;
		env.w = vram_width[env.version];
		env.h = vram_height[env.version];
		memset((char *)&env.draw, -1, sizeof(DRAWENV));
		memset((char *)&env.disp, -1, sizeof(DISPENV));
		return env.version;
	}
	if (env.debug >= 2)
		GPU_printf("ResetGraph(%d)...\n", mode);
	return gpu->reset(1);
}

int SetGraphReverse(int mode)
{
	u_char old = env.reverse;

	if (env.debug >= 2)
		GPU_printf("SetGraphReverse(%d)...\n", mode);
	env.reverse = mode;
	gpu->ctl(gpu->getctl(GP1_CTL_DISP_MODE) |
		(env.reverse ? (GP1_DISP_MODE | GP1_DMODE_REVERSE) : GP1_DISP_MODE));
	if (env.version == 2)
		gpu->ctl(env.reverse ? GP1_2M_REV_ON : GP1_2M_REV_OFF);
	return old;
}

int SetGraphDebug(int level)
{
	u_char old = env.debug;

	env.debug = level;
	if (env.debug)
		GPU_printf("SetGraphDebug:level:%d,type:%d reverse:%d\n",
			env.debug, env.version, env.reverse);
	return old;
}

int SetGraphQueue(int mode)
{
	u_char old = env.que;

	if (env.debug >= 2)
		GPU_printf("SetGrapQue(%d)...\n", mode);
	if (mode != env.que) {
		gpu->reset(1);
		env.que = mode;
		DMACallback(DMA_GPU, 0);
	}
	return old;
}

int GetGraphType(void)
{
	return env.version;
}

int GetGraphDebug(void)
{
	return env.debug;
}

u_long DrawSyncCallback(void (*func)())
{
	void (*old)();

	if (env.debug >= 2)
		GPU_printf("DrawSyncCallback(%08x)...\n", func);
	old = env.drawsync;
	env.drawsync = func;
	return (u_long)old;
}

void SetDispMask(int mask)
{
	if (env.debug >= 2)
		GPU_printf("SetDispMask(%d)...\n", mask);
	if (mask == 0)
		memset((char *)&env.disp, -1, sizeof(DISPENV));
	gpu->ctl(mask ? GP1_DISP_ON : GP1_DISP_OFF);
}

int DrawSync(int mode)
{
	if (env.debug >= 2)
		GPU_printf("DrawSync(%d)...\n", mode);
	return gpu->sync(mode);
}

static void checkRECT(char *s, RECT *r)
{
	switch (env.debug) {
	case 1:
		if (r->w > env.w || r->w + r->x > env.w ||
		    r->y > env.h || r->y + r->h > env.h ||
		    r->w <= 0 || r->x < 0 || r->y < 0 || r->h <= 0) {
			GPU_printf("%s:bad RECT", s);
			GPU_printf("(%d,%d)-(%d,%d)\n", r->x, r->y, r->w, r->h);
		}
		break;
	case 2:
		GPU_printf("%s:", s);
		GPU_printf("(%d,%d)-(%d,%d)\n", r->x, r->y, r->w, r->h);
		break;
	}
}

int ClearImage(RECT *rect, u_char r, u_char g, u_char b)
{
	checkRECT("ClearImage", rect);
	return gpu->addque2(gpu->clr, rect, sizeof(RECT), (b << 16) | (g << 8) | r);
}

int ClearImage2(RECT *rect, u_char r, u_char g, u_char b)
{
	checkRECT("ClearImage", rect);
	return gpu->addque2(gpu->clr, rect, sizeof(RECT), CLR_FLAG_DFE | (b << 16) | (g << 8) | r);
}

int LoadImage(RECT *rect, u_long *p)
{
	checkRECT("LoadImage", rect);
	return gpu->addque2(gpu->dws, rect, sizeof(RECT), p);
}

int StoreImage(RECT *rect, u_long *p)
{
	checkRECT("StoreImage", rect);
	return gpu->addque2(gpu->drs, rect, sizeof(RECT), p);
}

int MoveImage(RECT *rect, int x, int y)
{
	static u_long p[5] = { GPU_DMA_TAG(4, GPU_DMA_END), GP0_VRAM_BLIT };

	checkRECT("MoveImage", rect);
	if (rect->w == 0 || rect->h == 0)
		return -1;
	p[2] = *(u_long *)&rect->x;
	p[3] = (y << 16) | (x & 0xffff);
	p[4] = *(u_long *)&rect->w;
	return gpu->addque2(gpu->cwc, p, sizeof(p), 0);
}

static u_long term[5] = { GPU_DMA_TAG(4, GPU_DMA_END), 0, 0, 0, 0 };

u_long *ClearOTag(u_long *ot, int n)
{
	if (env.debug >= 2)
		GPU_printf("ClearOTag(%08x,%d)...\n", ot, n);
	while (--n) {
		setlen(ot, 0);
		setaddr(ot, ot + 1);
		ot++;
	}
	*ot = (u_long)term & GPU_ADDR_MASK;
	return ot;
}

u_long *ClearOTagR(u_long *ot, int n)
{
	if (env.debug >= 2)
		GPU_printf("ClearOTagR(%08x,%d)...\n", ot, n);
	gpu->otc(ot, n);
	*ot = (u_long)term & GPU_ADDR_MASK;
	return ot;
}

void DrawPrim(void *p)
{
	int len = getlen(p);

	gpu->sync(0);
	gpu->cwb((u_long *)p + 1, len);
}

void DrawOTag(u_long *p)
{
	if (env.debug >= 2)
		GPU_printf("DrawOTag(%08x)...\n", p);
	gpu->addque2(gpu->cwc, p, 0, 0);
}

DRAWENV *PutDrawEnv(DRAWENV *de)
{
	if (env.debug >= 2)
		GPU_printf("PutDrawEnv(%08x)...\n", de);
	SetDrawEnv2(&de->dr_env, de);
	de->dr_env.tag |= GPU_DMA_END;
	gpu->addque2(gpu->cwc, &de->dr_env, sizeof(DR_ENV), 0);
	env.draw = *de;
	return de;
}

void DrawOTagEnv(u_long *p, DRAWENV *de)
{
	if (env.debug >= 2)
		GPU_printf("DrawOTagEnv(%08x,&08x)...\n", p, de);
	SetDrawEnv2(&de->dr_env, de);
	setaddr(&de->dr_env, p);
	gpu->addque2(gpu->cwc, &de->dr_env, sizeof(DR_ENV), 0);
	env.draw = *de;
}

DRAWENV *GetDrawEnv(DRAWENV *de)
{
	memcpy(de, &env.draw, sizeof(DRAWENV));
	return de;
}

DISPENV *PutDispEnv(DISPENV *de)
{
	u_long mode = GP1_DISP_MODE;
	int hs, he, vs, ve;
	int fits;

	if (env.debug >= 2)
		GPU_printf("PutDispEnv(%08x)...\n", de);
	gpu->ctl(env.version == 1 || env.version == 2 ?
		GP1_DISP_START | ((de->disp.y & 0xfff) << 12 | get_dx(de) & 0xfff) :
		GP1_DISP_START | ((de->disp.y & 0x3ff) << 10 | de->disp.x & 0x3ff));

	if (env.disp.screen.x != de->screen.x || env.disp.screen.y != de->screen.y ||
	    env.disp.screen.w != de->screen.w || env.disp.screen.h != de->screen.h) {
		de->pad0 = GetVideoMode();	/* pad0 caches MODE_NTSC / MODE_PAL */
		hs = de->screen.x * 10 + 608;
		vs = de->screen.y + (de->pad0 ? 19 : 16);
		he = de->screen.w ? hs + de->screen.w * 10 : hs + 2560;
		ve = de->screen.h ? vs + de->screen.h : vs + 240;
		limitRange(hs, 500, 3290);
		limitRange(he, hs + 80, 3290);
		limitRange(vs, 16, de->pad0 ? 310 : 256);
		limitRange(ve, vs + 2, de->pad0 ? 312 : 258);
		gpu->ctl(GP1_DISP_HRANGE | (he & 0xfff) << 12 | hs & 0xfff);
		gpu->ctl(GP1_DISP_VRANGE | (ve & 0x3ff) << 10 | vs & 0x3ff);
	}

	if (*(u_long *)&env.disp.isinter != *(u_long *)&de->isinter ||
	    env.disp.disp.x != de->disp.x || env.disp.disp.y != de->disp.y ||
	    env.disp.disp.w != de->disp.w || env.disp.disp.h != de->disp.h) {
		de->pad0 = GetVideoMode();
		if (de->pad0 == MODE_PAL)
			mode |= GP1_DMODE_PAL;
		if (de->isrgb24)
			mode |= GP1_DMODE_RGB24;
		if (de->isinter)
			mode |= GP1_DMODE_INTERLACE;
		if (env.reverse)
			mode |= GP1_DMODE_REVERSE;
		if (de->disp.w > 280) {
			if (de->disp.w <= 352)
				mode |= GP1_DMODE_HRES_320;
			else if (de->disp.w <= 400)
				mode |= GP1_DMODE_HRES_368;
			else if (de->disp.w <= 560)
				mode |= GP1_DMODE_HRES_512;
			else
				mode |= GP1_DMODE_HRES_640;
		}
		fits = de->disp.h <= (de->pad0 ? 288 : 256);
		if (!fits)
			mode |= GP1_DMODE_INTERLACE | GP1_DMODE_VRES_480;
		gpu->ctl(mode);
	}
	memcpy(&env.disp, de, sizeof(DISPENV));
	return de;
}

DISPENV *GetDispEnv(DISPENV *de)
{
	memcpy(de, &env.disp, sizeof(DISPENV));
	return de;
}

int GetODE(void)
{
	return gpu->status() >> 31;
}

void SetTexWindow(DR_TWIN *p, RECT *tw)
{
	setlen(p, 2);
	p->code[0] = get_tw(tw);
	p->code[1] = 0;
}

void SetDrawArea(DR_AREA *p, RECT *r)
{
	setlen(p, 2);
	p->code[0] = get_cs(r->x, r->y);
	p->code[1] = get_ce(r->x + r->w - 1, r->y + r->h - 1);
}

void SetDrawOffset(DR_OFFSET *p, u_short *ofs)
{
	setlen(p, 2);
	p->code[0] = get_ofs(ofs[0], ofs[1]);
	p->code[1] = 0;
}

void SetPriority(DR_PRIO *p, int pbc, int pbw)
{
	setlen(p, 2);
	p->code[0] = GP0_MASK_BIT | (pbc ? GP0_MASK_CHECK : 0) | (pbw ? GP0_MASK_SET : 0);
	p->code[1] = 0;
}

void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw)
{
	setlen(p, 2);
	p->code[0] = get_mode(dfe, dtd, tpage);
	p->code[1] = get_tw(tw);
}

void SetDrawEnv(DR_ENV *dr_env, DRAWENV *de)
{
	u_long *p = (u_long *)dr_env;
	RECT clip;
	int n;

	p[1] = get_cs(de->clip.x, de->clip.y);
	p[2] = get_ce(de->clip.w + de->clip.x - 1, de->clip.y + de->clip.h - 1);
	p[3] = get_ofs(de->ofs[0], de->ofs[1]);
	p[4] = get_mode(de->dfe, de->dtd, de->tpage);
	p[5] = get_tw(&de->tw);
	p[6] = GP0_MASK_BIT;
	n = 7;
	if (de->isbg) {
		clip.x = de->clip.x;
		clip.y = de->clip.y;
		clip.w = de->clip.w;
		clip.h = de->clip.h;
		limitRange(clip.w, 0, env.w - 1);
		limitRange(clip.h, 0, env.h - 1);
		clip.x -= de->ofs[0];
		clip.y -= de->ofs[1];
		p[n++] = GP0_TILE | de->b0 << 16 | de->g0 << 8 | de->r0;
		p[n++] = *(u_long *)&clip.x;
		p[n++] = *(u_long *)&clip.w;
		clip.x += de->ofs[0];
		clip.y += de->ofs[1];
	}
	setlen(p, n - 1);
}

/* Like SetDrawEnv, but uses GP0_FILL_RECT when clip x and w are 64-pixel aligned. */
static void SetDrawEnv2(DR_ENV *dr_env, DRAWENV *de)
{
	u_long *p = (u_long *)dr_env;
	RECT clip;
	int n;

	p[1] = get_cs(de->clip.x, de->clip.y);
	p[2] = get_ce(de->clip.w + de->clip.x - 1, de->clip.y + de->clip.h - 1);
	p[3] = get_ofs(de->ofs[0], de->ofs[1]);
	p[4] = get_mode(de->dfe, de->dtd, de->tpage);
	p[5] = get_tw(&de->tw);
	p[6] = GP0_MASK_BIT;
	n = 7;
	if (de->isbg) {
		clip.x = de->clip.x;
		clip.y = de->clip.y;
		clip.w = de->clip.w;
		clip.h = de->clip.h;
		limitRange(clip.w, 0, env.w - 1);
		limitRange(clip.h, 0, env.h - 1);
		if ((clip.x & 0x3f) || (clip.w & 0x3f)) {
			clip.x -= de->ofs[0];
			clip.y -= de->ofs[1];
			p[n++] = GP0_TILE | de->b0 << 16 | de->g0 << 8 | de->r0;
			p[n++] = *(u_long *)&clip.x;
			p[n++] = *(u_long *)&clip.w;
			clip.x += de->ofs[0];
			clip.y += de->ofs[1];
		} else {
			p[n++] = GP0_FILL_RECT | de->b0 << 16 | de->g0 << 8 | de->r0;
			p[n++] = *(u_long *)&clip.x;
			p[n++] = *(u_long *)&clip.w;
		}
	}
	setlen(p, n - 1);
}

static u_long get_mode(int dfe, int dtd, u_short tpage)
{
	if (env.version == 1 || env.version == 2)
		return GP0_DRAW_MODE | (dtd ? GP0_MODE_2M_DTD : 0) |
			(dfe ? GP0_MODE_2M_DFE : 0) | (tpage & GP0_MODE_2M_TPAGE_MASK);
	else
		return GP0_DRAW_MODE | (dtd ? GP0_MODE_DTD : 0) |
			(dfe ? GP0_MODE_DFE : 0) | (tpage & GP0_MODE_TPAGE_MASK);
}

static u_long get_cs(short x, short y)
{
	limitRange(x, 0, env.w - 1);
	limitRange(y, 0, env.h - 1);
	if (env.version == 1 || env.version == 2)
		return GP0_DRAW_AREA_TL | (y & 0xfff) << 12 | x & 0xfff;
	else
		return GP0_DRAW_AREA_TL | (y & 0x3ff) << 10 | x & 0x3ff;
}

static u_long get_ce(short x, short y)
{
	limitRange(x, 0, env.w - 1);
	limitRange(y, 0, env.h - 1);
	if (env.version == 1 || env.version == 2)
		return GP0_DRAW_AREA_BR | (y & 0xfff) << 12 | x & 0xfff;
	else
		return GP0_DRAW_AREA_BR | (y & 0x3ff) << 10 | x & 0x3ff;
}

static u_long get_ofs(short x, short y)
{
	if (env.version == 1 || env.version == 2)
		return GP0_DRAW_OFFSET | (y & 0xfff) << 12 | x & 0xfff;
	else
		return GP0_DRAW_OFFSET | (y & 0x7ff) << 11 | x & 0x7ff;
}

static u_long get_tw(RECT *tw)
{
	u_long pat[4];

	if (tw) {
		pat[0] = (tw->x & 0xff) >> 3;
		pat[2] = (-tw->w & 0xff) >> 3;
		pat[1] = (tw->y & 0xff) >> 3;
		pat[3] = (-tw->h & 0xff) >> 3;
		return GP0_TEX_WINDOW | pat[1] << 15 | pat[0] << 10 | pat[3] << 5 | pat[2];
	}
	return 0;
}

static int get_dx(DISPENV *de)
{
	switch (env.version) {
	case 1:
		return env.reverse ? 1024 - de->disp.x - de->disp.w : de->disp.x;
	case 2:
		return env.reverse ? 1024 - de->disp.x - de->disp.w / 2 : de->disp.x / 2;
	default:
		return de->disp.x;
	}
}

static volatile u_long *GPU_DATA = (u_long *)GPU_DATA_ADDR;
static volatile u_long *GPU_STATUS = (u_long *)GPU_STATUS_ADDR;
static volatile u_long *DMA2_MADR = (u_long *)DMA2_MADR_ADDR;
static volatile u_long *DMA2_BCR = (u_long *)DMA2_BCR_ADDR;
static volatile u_long *DMA2_CHCR = (u_long *)DMA2_CHCR_ADDR;
static volatile u_long *DMA6_MADR = (u_long *)DMA6_MADR_ADDR;
static volatile u_long *DMA6_BCR = (u_long *)DMA6_BCR_ADDR;
static volatile u_long *DMA6_CHCR = (u_long *)DMA6_CHCR_ADDR;
static volatile u_long *DMA_DPCR = (u_long *)DMA_DPCR_ADDR;

volatile u_long _qlog[4] = { 0 };
volatile int _qin = 0;
volatile int _qout = 0;
static int que_mask = 0;
static int exe_mask = 0;
static int reset_mask = 0;
static int alarm_time = 0;
static int alarm_count = 0;

static u_char ctlbuf[256];
volatile GpuQueueEntry _que[GPU_QUE_SIZE];

static u_long _status(void)
{
	return *GPU_STATUS;
}

static int _otc(u_long *ot, int n)
{
	*DMA_DPCR |= DMA_DPCR_OTC_EN;
	*DMA6_CHCR = 0;
	*DMA6_MADR = (u_long)&ot[n - 1];
	*DMA6_BCR = n;
	*DMA6_CHCR = DMA_CHCR_OTC_CLEAR;
	set_alarm();
	while (*DMA6_CHCR & DMA_CHCR_BUSY) {
		if (get_alarm())
			return -1;
	}
	return n;
}

static int _clr(RECT *rect, u_long color)
{
	static u_long p0[9];
	static u_long p1[8];

	limitRange(rect->w, 0, env.w - 1);
	limitRange(rect->h, 0, env.h - 1);
	if ((rect->x & 0x3f) || (rect->w & 0x3f)) {
		p0[0] = GPU_DMA_TAG(8, p1);
		p0[1] = GP0_DRAW_AREA_TL;
		p0[2] = GP0_DRAW_AREA_BR | GPU_INFO_MASK;
		p0[3] = GP0_DRAW_OFFSET;
		p0[4] = GP0_MASK_BIT;
		p0[5] = GP0_DRAW_MODE | (*GPU_STATUS & GPUSTAT_DRAW_MODE_MASK) | ((color >> 31) << 10);
		p0[6] = GP0_TILE | (color & GPU_COLOR_MASK);
		p0[7] = *(u_long *)&rect->x;
		p0[8] = *(u_long *)&rect->w;
		p1[0] = GPU_DMA_TAG(3, GPU_DMA_END);
		p1[1] = GP0_DRAW_AREA_TL | _param(GP1_INFO_DRAW_AREA_TL);
		p1[2] = GP0_DRAW_AREA_BR | _param(GP1_INFO_DRAW_AREA_BR);
		p1[3] = GP0_DRAW_OFFSET | _param(GP1_INFO_DRAW_OFFSET);
	} else {
		p0[0] = GPU_DMA_TAG(5, GPU_DMA_END);
		p0[1] = GP0_MASK_BIT;
		p0[2] = GP0_DRAW_MODE | (*GPU_STATUS & GPUSTAT_DRAW_MODE_MASK) | ((color >> 31) << 10);
		p0[3] = GP0_FILL_RECT | (color & GPU_COLOR_MASK);
		p0[4] = *(u_long *)&rect->x;
		p0[5] = *(u_long *)&rect->w;
	}
	_cwc(p0);
	return 0;
}

static int _dws(RECT *rect, u_long *p)
{
	int size, n;
	int blocks;
	/* always 0; the original still tests it */
	int mode = 0;

	set_alarm();
	limitRange(rect->w, 0, env.w);
	limitRange(rect->h, 0, env.h);
	size = (rect->w * rect->h + 1) / 2;
	if (size <= 0)
		return -1;
	n = size % GPU_DMA_BLOCK_WORDS;
	blocks = size / GPU_DMA_BLOCK_WORDS;
	while (!(*GPU_STATUS & GPUSTAT_CMD_READY)) {
		if (get_alarm())
			return -1;
	}
	*GPU_STATUS = GP1_DMA_OFF;
	*GPU_DATA = GP0_CLEAR_CACHE;
	*GPU_DATA = mode ? GP0_CPU_TO_VRAM_ALT : GP0_CPU_TO_VRAM;
	*GPU_DATA = *(u_long *)&rect->x;
	*GPU_DATA = *(u_long *)&rect->w;
	while (n--)
		*GPU_DATA = *p++;
	if (blocks) {
		*GPU_STATUS = GP1_DMA_CPU_TO_GP0;
		*DMA2_MADR = (u_long)p;
		*DMA2_BCR = blocks << 16 | GPU_DMA_BLOCK_WORDS;
		*DMA2_CHCR = DMA_CHCR_GPU_WRITE;
	}
	return 0;
}

static int _drs(RECT *rect, u_long *p)
{
	int size, n;
	int blocks;

	set_alarm();
	limitRange(rect->w, 0, env.w);
	limitRange(rect->h, 0, env.h);
	size = (rect->w * rect->h + 1) / 2;
	if (size <= 0)
		return -1;
	n = size % GPU_DMA_BLOCK_WORDS;
	blocks = size / GPU_DMA_BLOCK_WORDS;
	while (!(*GPU_STATUS & GPUSTAT_CMD_READY)) {
		if (get_alarm())
			return -1;
	}
	*GPU_STATUS = GP1_DMA_OFF;
	*GPU_DATA = GP0_CLEAR_CACHE;
	*GPU_DATA = GP0_VRAM_TO_CPU;
	*GPU_DATA = *(u_long *)&rect->x;
	*GPU_DATA = *(u_long *)&rect->w;
	while (!(*GPU_STATUS & GPUSTAT_VRAM_READY)) {
		if (get_alarm())
			return -1;
	}
	while (n--)
		*p++ = *GPU_DATA;
	if (blocks) {
		*GPU_STATUS = GP1_DMA_GPU_TO_CPU;
		*DMA2_MADR = (u_long)p;
		*DMA2_BCR = blocks << 16 | GPU_DMA_BLOCK_WORDS;
		*DMA2_CHCR = DMA_CHCR_GPU_READ;
	}
	return 0;
}

static void _ctl(u_long cmd)
{
	*GPU_STATUS = cmd;
	ctlbuf[cmd >> 24] = cmd;
}

static int _getctl(int n)
{
	return ctlbuf[n];
}

static int _cwb(u_long *p, int n)
{
	*GPU_STATUS = GP1_DMA_OFF;
	while (n--)
		*GPU_DATA = *p++;
	return 0;
}

static void _cwc(u_long *p)
{
	*GPU_STATUS = GP1_DMA_CPU_TO_GP0;
	*DMA2_MADR = (u_long)p;
	*DMA2_BCR = 0;
	*DMA2_CHCR = DMA_CHCR_GPU_LIST;
}

static int _param(u_long n)
{
	*GPU_STATUS = GP1_GET_INFO | n;
	return *GPU_DATA & GPU_INFO_MASK;
}

static int _addque(void (*func)(), u_long *param, u_long value)
{
	return _addque2(func, param, 0, value);
}

static int _addque2(void (*func)(), u_long *param, int size, u_long value)
{
	int i;

	set_alarm();
	while (((_qin + 1) & GPU_QUE_MASK) == _qout) {
		if (get_alarm())
			return -1;
		_exeque();
	}
	que_mask = SetIntrMask(0);
	env.syncflag = 1;
	if (env.que == 0 ||
	    (_qin == _qout && !(*DMA2_CHCR & DMA_CHCR_BUSY) && env.drawsync == 0)) {
		while (!(*GPU_STATUS & GPUSTAT_CMD_READY))
			;
		(*func)(param, value);
		_qlog[0] = (u_long)func;
		_qlog[1] = (u_long)param;
		_qlog[2] = value;
		SetIntrMask(que_mask);
		return 0;
	}
	DMACallback(DMA_GPU, (void (*)())_exeque);
	if (size) {
		for (i = 0; i < size / 4; i++)
			_que[_qin].buf[i] = param[i];
		_que[_qin].param = _que[_qin].buf;
	} else {
		_que[_qin].param = param;
	}
	_que[_qin].value = value;
	_que[_qin].func = func;
	_qin = (_qin + 1) & GPU_QUE_MASK;
	SetIntrMask(que_mask);
	_exeque();
	return (_qin - _qout) & GPU_QUE_MASK;
}

static int _exeque(void)
{
	if (*DMA2_CHCR & DMA_CHCR_BUSY)
		return 1;
	exe_mask = SetIntrMask(0);
	while (_qin != _qout && !(*DMA2_CHCR & DMA_CHCR_BUSY)) {
		if (((_qout + 1) & GPU_QUE_MASK) == _qin && env.drawsync == 0)
			DMACallback(DMA_GPU, 0);
		while (!(*GPU_STATUS & GPUSTAT_CMD_READY))
			;
		(*_que[_qout].func)(_que[_qout].param, _que[_qout].value);
		_qlog[0] = (u_long)_que[_qout].func;
		_qlog[1] = (u_long)_que[_qout].param;
		_qlog[2] = _que[_qout].value;
		_qout = (_qout + 1) & GPU_QUE_MASK;
	}
	SetIntrMask(exe_mask);
	if (_qin == _qout && !(*DMA2_CHCR & DMA_CHCR_BUSY) && env.syncflag && env.drawsync) {
		env.syncflag = 0;
		(*env.drawsync)();
	}
	return (_qin - _qout) & GPU_QUE_MASK;
}

static int _reset(int mode)
{
	reset_mask = SetIntrMask(0);
	_qin = _qout = 0;
	switch (mode & 7) {
	case 0:
	case 5:
		*DMA2_CHCR = DMA_CHCR_GPU_IDLE;
		*DMA_DPCR |= DMA_DPCR_GPU_EN;
		*GPU_STATUS = GP1_RESET;
		memset((char *)ctlbuf, 0, sizeof(ctlbuf));
		memset((char *)_que, 0, sizeof(_que));
		break;
	case 1:
	case 3:
		*DMA2_CHCR = DMA_CHCR_GPU_IDLE;
		*DMA_DPCR |= DMA_DPCR_GPU_EN;
		*GPU_STATUS = GP1_ACK_IRQ;
		*GPU_STATUS = GP1_RESET_FIFO;
		break;
	}
	SetIntrMask(reset_mask);
	return (mode & 7) ? 0 : _version(mode);
}

static int _sync(int mode)
{
	int n;

	if (mode == 0) {
		set_alarm();
		while (_qin != _qout) {
			_exeque();
			if (get_alarm())
				return -1;
		}
		while ((*DMA2_CHCR & DMA_CHCR_BUSY) || !(*GPU_STATUS & GPUSTAT_CMD_READY)) {
			if (get_alarm())
				return -1;
		}
		return 0;
	}
	n = (_qin - _qout) & GPU_QUE_MASK;
	if (n)
		_exeque();
	if ((*DMA2_CHCR & DMA_CHCR_BUSY) || !(*GPU_STATUS & GPUSTAT_CMD_READY))
		return n ? n : 1;
	return n;
}

static void set_alarm(void)
{
	alarm_time = VSync(-1) + GPU_ALARM_VSYNC;
	alarm_count = 0;
}

static int get_alarm(void)
{
	if (alarm_time < VSync(-1) || alarm_count++ > GPU_ALARM_LIMIT) {
		*GPU_STATUS;
		printf("GPU timeout:que=%d,stat=%08x,chcr=%08x,madr=%08x,",
			(_qin - _qout) & GPU_QUE_MASK, *GPU_STATUS, *DMA2_CHCR, *DMA2_MADR);
		printf("func=(%08x)(%08x,%08x)\n", _qlog[0], _qlog[1], _qlog[2]);
		reset_mask = SetIntrMask(0);
		_qin = _qout = 0;
		*DMA2_CHCR = DMA_CHCR_GPU_IDLE;
		*DMA_DPCR |= DMA_DPCR_GPU_EN;
		*GPU_STATUS = GP1_ACK_IRQ;
		*GPU_STATUS = GP1_RESET_FIFO;
		SetIntrMask(reset_mask);
		return -1;
	}
	return 0;
}

static int _version(int mode)
{
	*GPU_STATUS = GP1_GET_INFO | GP1_INFO_GPU_TYPE;
	if ((*GPU_DATA & GPU_INFO_MASK) != 2) {
		*GPU_DATA = GP0_DRAW_MODE | GP0_MODE_2M_DFE | (*GPU_STATUS & GPUSTAT_2M_DRAW_MODE_MASK);
		*GPU_DATA;
		if (!(*GPU_STATUS & GPUSTAT_2M_DFE))
			return 0;
		if (!(mode & 8))
			return 1;
		*GPU_STATUS = GP1_2M_REV_OFF;
		return 2;
	}
	if (!(mode & 8))
		return 3;
	*GPU_STATUS = GP1_EXT_VRAM;
	return 4;
}

static void memset(char *p, int c, int n)
{
	while (n--)
		*p++ = c;
}
