; The cut prop's fading drawers: lit textured faces scaled by the falling piece's fade.

	opt	c+,at-,m+
#include <gtereg.h>
	include	"INLINE_A.H"
	include	"gte_word.inc"
	include	"libgte.inc"
	include	"libgpu.inc"
	include	"render_scratch.inc"
	include	"scene_node.inc"
	include	"mesh_prims.inc"

	xref	gPacketCursor
	xdef	DrawFadingPolyFt3
	xdef	DrawFadingPolyFt4

	section	.text

; DrawFadingPolyFt3 (dynamic type 42): textured triangles lit by NCS, times the fade.
; In:   a0 the face count (not zero), s0 the DTRI faces, s2 the SceneNode.
; Out:  s0 past them, gPacketCursor a POLY_FT3 further per face.
; Uses: at, v0, v1, a0..a2, t0..t6, the GTE.
DrawFadingPolyFt3:
	addiu	sp,sp,-56
	sw	ra,48(sp)
	sw	fp,44(sp)
	sw	s7,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.ft3_done
	lh	s7,SN_propFade(s2)
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT3_TAG
	li	fp,OT_ADDR_MASK
	lw	s6,gPacketCursor
	lw	v1,SCRATCHPAD+RS_scaleShift
.ft3_next:
	lw	a0,DTRI_vertices(s0)
	lwc2	C2_RGB,DTRI_tpage(s0)
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
	bltz	t6,.ft3_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.ft3_skip
	nop
	bne	t6,zero,.ft3_skip
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
	sw	t0,FT_x0(s6)
	wAVSZ3
	sw	t1,FT_x1(s6)
	mfc2	v0,C2_OTZ
	sw	t2,FT_x2(s6)
	blez	v0,.ft3_skip
	sll	v0,v0,2
	lwc2	C2_VXY0,DTRI_normal(s0)
	lwc2	C2_VZ0,DTRI_normal+4(s0)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	NCS
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	lh	t4,DTRI_clut(s0)
	lh	t5,DTRI_tpage(s0)
	sll	t4,t4,16
	sll	t5,t5,16
	lhu	t0,DTRI_uv0(s0)
	lhu	t1,DTRI_uv1(s0)
	lhu	t2,DTRI_uv2(s0)
	or	t0,t0,t4
	or	t1,t1,t5
	mtc2	s7,C2_IR0
	sw	t0,FT_u0(s6)
	sw	t1,FT_u1(s6)
	sh	t2,FT_u2(s6)
	GPF	1	; the lit colour times the fade
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	swc2	C2_RGB2,FT_r0(s6)
.ft3_skip:
	addiu	s0,s0,DTRI_SIZE
	addiu	s6,s6,FT3_SIZE
	bne	s3,zero,.ft3_next
	addiu	s3,s3,-1
.ft3_done:
	opt	at+
	sw	s6,gPacketCursor
	opt	at-
	lw	ra,48(sp)
	lw	fp,44(sp)
	lw	s7,40(sp)
	lw	s6,36(sp)
	lw	s5,32(sp)
	lw	s4,28(sp)
	lw	s3,24(sp)
	lw	s2,20(sp)
	lw	s1,16(sp)
	jr	ra
	addiu	sp,sp,56

; DrawFadingPolyFt4 (dynamic type 43): textured quads lit by NCS, times the fade.
; In:   a0 the face count (not zero), s0 the DQUAD faces, s2 the SceneNode.
; Out:  s0 past them, gPacketCursor a POLY_FT4 further per face.
; Uses: at, v0, v1, a0..a3, t0..t8, the GTE.
DrawFadingPolyFt4:
	addiu	sp,sp,-56
	sw	ra,48(sp)
	sw	fp,44(sp)
	sw	s7,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.ft4_done
	lh	s7,SN_propFade(s2)
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT4_TAG
	li	fp,OT_ADDR_MASK
	lw	s6,gPacketCursor
	lw	v1,SCRATCHPAD+RS_scaleShift
.ft4_next:
	lhu	a0,DQUAD_vertices(s0)
	lhu	a1,DQUAD_vertices+2(s0)
	lwc2	C2_RGB,DQUAD_tpage(s0)
	sll	a1,a1,16
	or	a0,a0,a1
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
	bltz	t8,.ft4_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.ft4_skip
	nop
	bne	t8,zero,.ft4_skip
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
	blez	v0,.ft4_skip
	nop
	sw	t0,FT_x0(s6)
	sw	t1,FT_x1(s6)
	sw	t2,FT_x2(s6)
	sw	t7,FT_x3(s6)
	lhu	t0,DQUAD_normalZ(s0)
	lwc2	C2_VXY0,DQUAD_normalXY(s0)
	mtc2	t0,C2_VZ0
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	NCS
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	lh	t4,DQUAD_clut(s0)
	lh	t5,DQUAD_tpage(s0)
	sll	t4,t4,16
	sll	t5,t5,16
	lhu	t0,DQUAD_uv0(s0)
	lhu	t1,DQUAD_uv1(s0)
	lhu	t2,DQUAD_uv2(s0)
	lhu	t3,DQUAD_uv3(s0)
	or	t0,t0,t4
	or	t1,t1,t5
	mtc2	s7,C2_IR0
	sw	t0,FT_u0(s6)
	sw	t1,FT_u1(s6)
	sh	t2,FT_u2(s6)
	sh	t3,FT_u3(s6)
	GPF	1
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	swc2	C2_RGB2,FT_r0(s6)
.ft4_skip:
	addiu	s0,s0,DQUAD_SIZE
	addiu	s6,s6,FT4_SIZE
	bne	s3,zero,.ft4_next
	addiu	s3,s3,-1
.ft4_done:
	opt	at+
	sw	s6,gPacketCursor
	opt	at-
	lw	ra,48(sp)
	lw	fp,44(sp)
	lw	s7,40(sp)
	lw	s6,36(sp)
	lw	s5,32(sp)
	lw	s4,28(sp)
	lw	s3,24(sp)
	lw	s2,20(sp)
	lw	s1,16(sp)
	jr	ra
	addiu	sp,sp,56
