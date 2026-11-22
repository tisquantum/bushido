; _patch_card: writes a jump to .Lhook over five words at +$28 of the address the exception handler's
; words at +$70 and +$74 load. _patch_card2: exchanges the four words at ChangeClearPAD's entry + $9c8
; with .Lswap's four, which call .Lswap; a second call exchanges them back.
	opt	c+
#include <hilo.h>
#include <kernel_call.h>

	xref	EnterCriticalSection
	xref	FlushCache
	xdef	_patch_card
	xdef	_patch_card2

	section	.text

.Lhook_jump:
	la	v0,.Lhook
	jr	v0
	nop
	nop
.Lhook:
	lw	v0,$1074(v1)
	nop
	andi	v0,v0,$80
	beq	v0,zero,.Lhook_done
	nop
.Lhook_wait:
	lw	v0,$1044(v1)
	nop
	andi	v0,v0,$80
	bne	v0,zero,.Lhook_wait
	nop
	lw	v0,resume
	nop
	jr	v0
	nop
.Lhook_done:
	jr	ra
	nop

_patch_card:
	sw	ra,saved_ra
	jal	EnterCriticalSection
	nop
	KCALL	$b0,GetC0Table
	lw	v0,C0TBL_EXCEPTION(v0)
	nop
	lw	v1,$70(v0)
	nop
	andi	t1,v1,$ffff
	lw	v1,$74(v0)
	sll	t1,t1,16
	andi	t2,v1,$ffff
	addu	v1,t1,t2
	lui	t2,HI(.Lhook_jump)
	lui	t1,HI(.Lhook)
	addiu	v0,v1,$28
	addiu	t2,t2,LO(.Lhook_jump)
	addiu	t1,t1,LO(.Lhook)
.Lcopy:
	lw	v1,0(t2)
	addiu	t2,t2,4
	addiu	v0,v0,4
	bne	t2,t1,.Lcopy
	sw	v1,-4(v0)
	opt	m-
	lui	at,HI(resume)
	jal	FlushCache
	sw	v0,LO(resume)(at)
	opt	m+
	lw	ra,saved_ra
	nop
	jr	ra
	nop

.Lswap:
	la	t0,.Lswap
	jalr	ra,t0
	nop
	nop
.Lswap_end:
	nop
	addiu	t0,zero,200
.Lswap_delay:
	addiu	t0,t0,-1
	bne	t0,zero,.Lswap_delay
	nop
	jr	ra
	nop

_patch_card2:
	sw	ra,saved_ra
	jal	EnterCriticalSection
	nop
	KCALL	$b0,GetB0Table
	lw	v0,B0TBL_CHANGECLEARPAD(v0)
	lui	t2,HI(.Lswap)
	lui	t1,HI(.Lswap_end)
	lw	v1,$9c8(v0)
	addiu	t2,t2,LO(.Lswap)
	addiu	t1,t1,LO(.Lswap_end)
.Lexchange:
	lw	v1,$9c8(v0)
	lw	t0,0(t2)
	addiu	t2,t2,4
	sw	v1,-4(t2)
	addiu	v0,v0,4
	bne	t2,t1,.Lexchange
	sw	t0,$9c8-4(v0)
	jal	FlushCache
	nop
	lw	ra,saved_ra
	nop
	jr	ra
	nop

; nothing calls .Ldelay
.Ldelay:
	lui	t6,50
	addiu	t6,t6,-1
.Ldelay_loop:
	bne	t6,zero,.Ldelay_loop
	addiu	t6,t6,-1
	jr	ra
	nop
	nop
	nop
	nop

bss	group	bss
	section	.sbss,bss
saved_ra:
	dsw	1
resume:
	dsw	1
unused:			; 8 bytes: nothing reads them
	dsw	2
