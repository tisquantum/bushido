; A call thunk.

	opt	c+,at-,m+

	xdef	CallWithPreservedReturn

	section	.text

; CallWithPreservedReturn(value, fn, arg) -> value: unused; calls fn(arg), returns value.
; In:   a0 the value, a1 fn, a2 arg.
; Out:  v0 the value.
; Uses: a0, and what fn uses.
CallWithPreservedReturn:
	addiu	sp,sp,-24
	sw	ra,20(sp)
	sw	s0,16(sp)
	addu	s0,a0,zero
	jalr	ra,a1
	addu	a0,a2,zero
	addu	v0,s0,zero
	lw	ra,20(sp)
	lw	s0,16(sp)
	jr	ra
	addi	sp,sp,24
