; _patch_gte: writes .Lpatch_start's fourteen words over the start of the kernel's exception handler.
	opt	c+
#include <hilo.h>
#include <kernel_call.h>

	xref	EnterCriticalSection
	xref	FlushCache
	xref	ExitCriticalSection
	xdef	_patch_gte

	section	.text

_patch_gte:
	sw	ra,save_ra
	jal	EnterCriticalSection
	nop
	KCALL	$b0,GetC0Table
	lui	t2,HI(.Lpatch_start)
	lui	t1,HI(.Lpatch_end)
	lw	v0,C0TBL_EXCEPTION(v0)
	addiu	t2,t2,LO(.Lpatch_start)
	addiu	t1,t1,LO(.Lpatch_end)
.Lcopy_loop:
	lw	v1,0(t2)
	addiu	t2,t2,4
	addiu	v0,v0,4
	bne	t2,t1,.Lcopy_loop
	sw	v1,-4(v0)
	jal	FlushCache
	nop
	jal	ExitCriticalSection
	nop
	lw	ra,save_ra
	nop
	jr	ra
	nop

.Lpatch_start:
	nop
	nop
	addiu	k0,zero,$100
	lw	k0,8(k0)
	nop
	lw	k0,0(k0)
	nop
	addi	k0,k0,8
	sw	at,4(k0)
	sw	v0,8(k0)
	sw	v1,12(k0)
	sw	ra,124(k0)
	mfc0	v0,cause
	nop
.Lpatch_end:

bss	group	bss
	section	.sbss,bss
save_ra:
	dsw	1
unused:			; 12 bytes: nothing reads them
	dsw	3
