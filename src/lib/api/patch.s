; _patch_pad: keeps ChangeClearPAD's B0 entry + $884 and + $894, which EnablePAD and DesablePAD jump to,
; and writes zero to the eleven words from its entry + $594.
	opt	c+
#include <hilo.h>
#include <kernel_call.h>

	xref	EnterCriticalSection
	xref	FlushCache
	xdef	_patch_pad
	xdef	EnablePAD
	xdef	DesablePAD

	section	.text

EnablePAD:
	lw	t1,enable_pad
	nop
	jr	t1
	nop
DesablePAD:
	lw	t1,disable_pad
	nop
	jr	t1
	nop
_patch_pad:
	sw	ra,saved_ra
	jal	EnterCriticalSection
	nop
	KCALL	$b0,GetB0Table
	lw	v0,B0TBL_CHANGECLEARPAD(v0)
	opt	m-
	lui	at,HI(enable_pad)
	addi	v1,v0,$884
	sw	v1,LO(enable_pad)(at)
	lui	at,HI(disable_pad)
	addi	v1,v0,$894
	addiu	t1,zero,11
	sw	v1,LO(disable_pad)(at)
	opt	m+
.clear:
	addiu	t1,t1,-1
	sw	zero,$594(v0)
	bne	t1,zero,.clear
	addiu	v0,v0,4
	jal	FlushCache
	nop
	lw	ra,saved_ra
	nop
	jr	ra
	nop

bss	group	bss
	section	.sbss,bss
saved_ra:
	dsw	1
unused:			; 4 bytes: nothing reads them
	dsw	1
enable_pad:
	dsw	1
disable_pad:
	dsw	1
