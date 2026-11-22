; _ExitCard: writes three nops over the kernel exception handler's words at +$70..+$7b.
	opt	c+
#include <hilo.h>
#include <kernel_call.h>

	xref	EnterCriticalSection
	xref	ExitCriticalSection
	xref	FlushCache
	xdef	_ExitCard

	section	.text

_ExitCard:
	sw	ra,saved_ra
	jal	EnterCriticalSection
	nop
	KCALL	$b0,GetC0Table
	lui	t2,HI(.Lpatch)
	lui	t1,HI(.Lpatch_end)
	lw	v0,C0TBL_EXCEPTION(v0)
	addiu	t2,t2,LO(.Lpatch)
	addiu	t1,t1,LO(.Lpatch_end)
.Lcopy:
	lw	v1,0(t2)
	addiu	t2,t2,4
	addiu	v0,v0,4
	bne	t2,t1,.Lcopy
	sw	v1,$70-4(v0)
	jal	FlushCache
	nop
	jal	ExitCriticalSection
	nop
	lw	ra,saved_ra
	nop
	jr	ra
	nop
.Lpatch:
	nop
	nop
	nop
.Lpatch_end:
	nop
	nop
	nop

bss	group	bss
	section	.sbss,bss
saved_ra:
	dsw	1
unused:			; 12 bytes: nothing reads them
	dsw	3
