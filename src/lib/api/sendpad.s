; _send_pad: writes .patch's four words at ChangeClearPAD's B0 entry + $3d8 and + $4e0, and keeps and
; returns its entry + $7a0, which _SendPAD calls.
	opt	c+
#include <hilo.h>
#include <kernel_call.h>

	xref	EnterCriticalSection
	xref	ExitCriticalSection
	xref	FlushCache
	xdef	_send_pad
	xdef	_SendPAD

	section	.text

_SendPAD:
	lw	t1,send_pad
	addiu	sp,sp,-24
	sw	ra,20(sp)
	jalr	ra,t1
	nop
	lw	ra,20(sp)
	addiu	sp,sp,24
	jr	ra
	nop
_send_pad:
	sw	ra,saved_ra
	jal	EnterCriticalSection
	nop
	KCALL	$b0,GetB0Table
	lw	v0,B0TBL_CHANGECLEARPAD(v0)
	lui	t2,HI(.patch)
	lui	t1,HI(.patch_end)
	opt	m-
	lui	at,HI(send_pad)
	addi	v1,v0,$7a0
	addiu	t2,t2,LO(.patch)
	addiu	t1,t1,LO(.patch_end)
	sw	v1,LO(send_pad)(at)
	opt	m+
.copy:
	lw	v1,0(t2)
	addiu	t2,t2,4
	sw	v1,$3d8(v0)
	addiu	v0,v0,4
	bne	t2,t1,.copy
	sw	v1,$4e0-4(v0)
	jal	FlushCache
	nop
	jal	ExitCriticalSection
	nop
	lw	ra,saved_ra
	lw	v0,send_pad
	jr	ra
	nop
.patch:
	and	v0,v0,s5
	nop
	nop
	nop
.patch_end:
	nop
	nop
	nop

bss	group	bss
	section	.sbss,bss
saved_ra:
	dsw	1
send_pad:
	dsw	1
unused:			; 8 bytes: nothing reads them
	dsw	2
