/*
 * Primitive initialization, linking, attribute helpers, and environment dumps.
 */
#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include "gpu_int.h"

#define DR_LOAD_MAX_WORDS	12	/* DR_LOAD.p[13] holds up to 12 pixel words + 1 cache flush */
#define GPU_PRIM_MAX_WORDS	16

u_short GetTPage(int tp, int abr, int x, int y)
{
	return getTPage(tp, abr, x, y);
}

u_short GetClut(int x, int y)
{
	return getClut(x, y);
}

void DumpTPage(u_short tpage)
{
	dumpTPage(tpage);
}

void DumpClut(u_short clut)
{
	dumpClut(clut);
}

void *NextPrim(void *p)
{
	return nextPrim(p);
}

int IsEndPrim(void *p)
{
	return isendprim(p);
}

void AddPrim(void *ot, void *p)
{
	addPrim(ot, p);
}

void AddPrims(void *ot, void *p0, void *p1)
{
	addPrims(ot, p0, p1);
}

void CatPrim(void *p0, void *p1)
{
	catPrim(p0, p1);
}

void TermPrim(void *p)
{
	termPrim(p);
}

void SetSemiTrans(void *p, int abe)
{
	setSemiTrans(p, abe);
}

void SetShadeTex(void *p, int tge)
{
	setShadeTex(p, tge);
}

void SetPolyF3(POLY_F3 *p)
{
	setPolyF3(p);
}

void SetPolyFT3(POLY_FT3 *p)
{
	setPolyFT3(p);
}

void SetPolyG3(POLY_G3 *p)
{
	setPolyG3(p);
}

void SetPolyGT3(POLY_GT3 *p)
{
	setPolyGT3(p);
}

void SetPolyF4(POLY_F4 *p)
{
	setPolyF4(p);
}

void SetPolyFT4(POLY_FT4 *p)
{
	setPolyFT4(p);
}

void SetPolyG4(POLY_G4 *p)
{
	setPolyG4(p);
}

void SetPolyGT4(POLY_GT4 *p)
{
	setPolyGT4(p);
}

void SetSprt8(SPRT_8 *p)
{
	setSprt8(p);
}

void SetSprt16(SPRT_16 *p)
{
	setSprt16(p);
}

void SetSprt(SPRT *p)
{
	setSprt(p);
}

void SetTile1(TILE_1 *p)
{
	setTile1(p);
}

void SetTile8(TILE_8 *p)
{
	setTile8(p);
}

void SetTile16(TILE_16 *p)
{
	setTile16(p);
}

void SetTile(TILE *p)
{
	setTile(p);
}

void SetLineF2(LINE_F2 *p)
{
	setLineF2(p);
}

void SetLineG2(LINE_G2 *p)
{
	setLineG2(p);
}

void SetLineF3(LINE_F3 *p)
{
	setLineF3(p);
}

void SetLineG3(LINE_G3 *p)
{
	setLineG3(p);
}

void SetLineF4(LINE_F4 *p)
{
	setLineF4(p);
}

void SetLineG4(LINE_G4 *p)
{
	setLineG4(p);
}

void SetDrawTPage(DR_TPAGE *p, int dfe, int dtd, int tpage)
{
	setDrawTPage(p, dfe, dtd, tpage);
}

void SetDrawMove(DR_MOVE *p, RECT *rect, int x, int y)
{
	int len = 5;

	if (rect->w == 0 || rect->h == 0) {
		len = 0;
	}
	setlen(p, len);
	p->code[0] = GP0_CLEAR_CACHE;
	p->code[1] = GP0_VRAM_BLIT;
	p->code[2] = *(u_long *)&rect->x;
	p->code[3] = (y << 16) | (x & 0xffff);
	p->code[4] = *(u_long *)&rect->w;
}

void SetDrawLoad(DR_LOAD *p, RECT *rect)
{
	int pixel_words = (rect->w * rect->h + 1) / 2;
	int len = pixel_words + 4;

	if (pixel_words < 0 || pixel_words > DR_LOAD_MAX_WORDS) {
		len = 0;
	}
	setlen(p, len);
	p->code[0] = GP0_CPU_TO_VRAM;
	p->code[1] = *(u_long *)&rect->x;
	p->code[2] = *(u_long *)&rect->w;
	*(u_long *)((u_char *)p + (len << 2)) = GP0_CLEAR_CACHE;
}

int MargePrim(void *p0, void *p1)
{
	int len = getlen(p0) + getlen(p1) + 1;

	if (len > GPU_PRIM_MAX_WORDS) {
		return -1;
	}
	setlen(p0, len);
	*(u_long *)p1 = GP0_NOP;
	return 0;
}

void DumpDrawEnv(DRAWENV *env)
{
	GPU_printf("clip (%3d,%3d)-(%d,%d)\n", env->clip.x, env->clip.y, env->clip.w, env->clip.h);
	GPU_printf("ofs  (%3d,%3d)\n", env->ofs[0], env->ofs[1]);
	GPU_printf("tw   (%d,%d)-(%d,%d)\n", env->tw.x, env->tw.y, env->tw.w, env->tw.h);
	GPU_printf("dtd   %d\n", env->dtd);
	GPU_printf("dfe   %d\n", env->dfe);
	dumpTPage(env->tpage);
}

void DumpDispEnv(DISPENV *env)
{
	GPU_printf("disp   (%3d,%3d)-(%d,%d)\n", env->disp.x, env->disp.y, env->disp.w, env->disp.h);
	GPU_printf("screen (%3d,%3d)-(%d,%d)\n", env->screen.x, env->screen.y, env->screen.w, env->screen.h);
	GPU_printf("isinter %d\n", env->isinter);
	GPU_printf("isrgb24 %d\n", env->isrgb24);
}
