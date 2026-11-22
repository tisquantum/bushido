	opt	c+
#include <gtereg.h>
#include <r3000.h>

	xref	_patch_gte
	xdef	InitGeom

	section	.text
	dw	$0a097350		; "Ps", a byte that differs by library, then five more; nothing reads them
	dw	$1036e91a

InitGeom:
	sw	ra,saved_ra
	jal	_patch_gte
	nop
	lw	ra,saved_ra
	nop
	mfc0	v0,status
	lui	v1,SR_CU2>>16
	or	v0,v0,v1
	mtc0	v0,status
	nop
	addiu	t0,zero,341
	ctc2	t0,C2_ZSF3
	nop
	addiu	t0,zero,256
	ctc2	t0,C2_ZSF4
	nop
	addiu	t0,zero,1000
	ctc2	t0,C2_H
	nop
	addiu	t0,zero,-4194
	ctc2	t0,C2_DQA
	nop
	lui	t0,$140
	ctc2	t0,C2_DQB
	nop
	ctc2	zero,C2_OFX
	ctc2	zero,C2_OFY
	nop
	jr	ra
	nop
	nop
	nop

	section	.data
saved_ra:
	dsw	1
unused:			; 12 bytes: nothing reads them
	dsw	3
