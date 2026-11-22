; The POV overlay mannequin's flat drawers, into the overlay's own ordering table.

	opt	c+,at-,m+
#include <gtereg.h>
	include	"INLINE_A.H"
	include	"gte_word.inc"
	include	"libgpu.inc"
	include	"render_scratch.inc"
	include	"mesh_prims.inc"
	include	"pov_overlay.inc"

OVERLAY_FLAT_DEPTH_BIAS	equ	1000

	xref	gOverlayOt
	xdef	DrawOverlayPolyF3
	xdef	DrawOverlayPolyF4

	section	.text

; DrawOverlayPolyF3 (POV type 0): flat triangles lit by NCCS, into gOverlayOt.
; In:   a0 the face count (not zero), s0 the MF3 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a2, t0..t6, t9, the GTE.
DrawOverlayPolyF3:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.f3_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,gOverlayOt
	li	s5,F3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,F3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MF3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,F_r0(s6)		; replaced before NCCS reads it
.f3_next:
	lw	a0,0(s0)
	nop
	sll	a1,a0,2
	andi	t3,a1,$ff<<2
	addu	a1,t3,s1
	lw	t0,0(a1)
	srl	a1,a0,6
	andi	t4,a1,$ff<<2
	addu	a1,t4,s1
	lw	t1,0(a1)
	srl	a1,a0,14
	andi	t5,a1,$ff<<2
	addu	a1,t5,s1
	lw	t2,0(a1)
	mtc2	t0,C2_SXY0
	mtc2	t1,C2_SXY1
	mtc2	t2,C2_SXY2
	and	t6,t0,t1
	and	t6,t6,t2
	NCLIP
	bltz	t6,.f3_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.f3_skip
	nop
	bne	t6,zero,.f3_skip
	srl	v0,t3,1
	addu	v0,v0,s2
	lh	a0,0(v0)
	srl	v0,t4,1
	addu	v0,v0,s2
	lh	a1,0(v0)
	srl	v0,t5,1
	addu	v0,v0,s2
	lh	a2,0(v0)
	mtc2	a0,C2_SZ1
	mtc2	a1,C2_SZ2
	mtc2	a2,C2_SZ3
	sw	t0,F_x0(s6)
	wAVSZ3
	sw	t1,F_x1(s6)
	mfc2	v0,C2_OTZ
	sw	t2,F_x2(s6)
	blez	v0,.f3_skip
	sll	v0,v0,2
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.f3_skip
	nop
	addi	v0,v0,-OVERLAY_FLAT_DEPTH_BIAS
	lwc2	C2_VXY0,MF3_normal-MF3_vertices(s0)
	lwc2	C2_VZ0,MF3_normal+4-MF3_vertices(s0)
	lwc2	C2_RGB,MF3_color-MF3_vertices(s0)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	NCCS
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	srl	v0,v0,4
	andi	v0,v0,POV_OVERLAY_OT_LEN-1
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,F_tag(s6)
	swc2	C2_RGB2,F_r0(s6)
.f3_skip:
	addiu	s0,s0,MF3_SIZE
	addiu	s6,s6,MF3_SIZE
	bne	s3,zero,.f3_next
	addiu	s3,s3,-1
.f3_done:
	addi	s0,s0,-MF3_vertices
	lw	ra,44(sp)
	lw	fp,40(sp)
	lw	s6,36(sp)
	lw	s5,32(sp)
	lw	s4,28(sp)
	lw	s3,24(sp)
	lw	s2,20(sp)
	lw	s1,16(sp)
	jr	ra
	addiu	sp,sp,56

; DrawOverlayPolyF4 (POV type 8): flat quads lit by NCCS, into gOverlayOt.
; In:   a0 the face count (not zero), s0 the MF4 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawOverlayPolyF4:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.f4_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,gOverlayOt
	li	s5,F4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,F4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MF4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,F_r0(s6)		; replaced before NCCS reads it
.f4_next:
	lw	a0,0(s0)
	nop
	sll	a1,a0,2
	andi	t3,a1,$ff<<2
	addu	a1,t3,s1
	lw	t0,0(a1)
	srl	a1,a0,6
	andi	t4,a1,$ff<<2
	addu	a1,t4,s1
	lw	t1,0(a1)
	srl	a1,a0,14
	andi	t5,a1,$ff<<2
	addu	a1,t5,s1
	lw	t2,0(a1)
	srl	a1,a0,22
	andi	t6,a1,$ff<<2
	addu	a1,t6,s1
	lw	t7,0(a1)
	mtc2	t0,C2_SXY0
	mtc2	t1,C2_SXY1
	mtc2	t2,C2_SXY2
	and	t8,t0,t1
	and	t8,t8,t2
	and	t8,t8,t7
	NCLIP
	bltz	t8,.f4_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.f4_skip
	nop
	bne	t8,zero,.f4_skip
	srl	v0,t3,1
	addu	v0,v0,s2
	lh	a0,0(v0)
	srl	v0,t4,1
	addu	v0,v0,s2
	lh	a1,0(v0)
	srl	v0,t5,1
	addu	v0,v0,s2
	lh	a2,0(v0)
	srl	v0,t6,1
	addu	v0,v0,s2
	lh	a3,0(v0)
	add	a0,a0,a1
	add	a0,a0,a2
	add	a0,a0,a3
	sra	v0,a0,2
	blez	v0,.f4_skip
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.f4_skip
	nop
	sw	t0,F_x0(s6)
	sw	t1,F_x1(s6)
	sw	t2,F_x2(s6)
	sw	t7,F_x3(s6)
	addi	v0,v0,-OVERLAY_FLAT_DEPTH_BIAS
	lwc2	C2_VXY0,MF4_normal-MF4_vertices(s0)
	lwc2	C2_VZ0,MF4_normal+4-MF4_vertices(s0)
	lwc2	C2_RGB,MF4_color-MF4_vertices(s0)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	NCCS
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	srl	v0,v0,4
	andi	v0,v0,POV_OVERLAY_OT_LEN-1
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,F_tag(s6)
	swc2	C2_RGB2,F_r0(s6)
.f4_skip:
	addiu	s0,s0,MF4_SIZE
	addiu	s6,s6,MF4_SIZE
	bne	s3,zero,.f4_next
	addiu	s3,s3,-1
.f4_done:
	addi	s0,s0,-MF4_vertices
	lw	ra,44(sp)
	lw	fp,40(sp)
	lw	s6,36(sp)
	lw	s5,32(sp)
	lw	s4,28(sp)
	lw	s3,24(sp)
	lw	s2,20(sp)
	lw	s1,16(sp)
	jr	ra
	addiu	sp,sp,56
