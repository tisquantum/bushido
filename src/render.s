; The 3D renderer: RenderSceneDisplayList walks gRenderQueue through gSceneNodeRenderDispatch, mesh batches
; through gMeshBatchDispatch, primitive groups through the scratchpad's primitive table. Held across the walk:
; s0 mesh stream, s1 SCRATCHPAD, s2 node, s3 its LightSlot, s4 primitive table, s5 shift, s6 view, s7 queue.

	opt	c+,at-,m+
#include <gtereg.h>
	include	"INLINE_A.H"
	include	"gte_word.inc"
	include	"libgte.inc"
	include	"libgpu.inc"
	include	"render_scratch.inc"
	include	"scene_node.inc"
	include	"mesh_prims.inc"

; LightSlot (light.h).
		rsreset
LS_directions		rh	4*3
LS_lightMatrix		rb	MATRIX_SIZE
LS_colorMatrix		rb	MATRIX_SIZE
LS_ambientRgb		rb	4
LS_scale		rh	1
			rh	1
LIGHT_SLOT_SIZE		rb	0
LIGHT_SLOT_STAGE	equ	4

CLOTH_SIDE_DEPTH_BIAS	equ	80
Q12_THIRD		equ	$555
Q12_QUARTER		equ	$400

; SceneTile (stage_cull.h).
		rsreset
ST_renderType	rh	1
ST_modelSlot	rh	1
ST_meshIndex	rh	1
ST_lodLevel	rb	1
ST_flags	rb	1
ST_worldX	rw	1
ST_worldZ	rw	1
SCENE_NODE_TILE	equ	12

; The tile batch's scratchpad: RenderScratch with 60-vertex buffers, then its own words.
		rsset	RS_screenXy
TS_screenXy		rw	60
TS_screenZ		rh	60
TS_viewRotation		rh	9
			rh	1
TS_tileRotation		rh	9
			rh	1
TS_vertices		rw	1
TS_cameraPosition	rw	3
TS_stageFlags		rw	1
TS_faceCode		rh	1
			rh	1
TS_clut			rh	1
TS_tpage		rh	1
TS_color		rw	1
TS_faceCount		rw	1
TS_tileVertices		rw	1
TS_otEntry		rw	1
TS_lodLevel		rb	1
TS_tileFlags		rb	1
TS_uv0			rh	1
TS_uv1			rh	1
TS_uv2			rh	1
TS_uv3			rh	1

; SceneTileFlag and gStageRenderFlags (stage_cull.h).
SCENE_TILE_FLAG_SEMI_TRANS	equ	$01
STAGE_RENDER_MIP_MODE_MASK	equ	$03
STAGE_RENDER_MIP_MODE_LOD	equ	$01
STAGE_RENDER_MIP_MODE_AREA	equ	$02
STAGE_RENDER_DEBUG_MIP_TINT	equ	$04
STAGE_RENDER_TRI_MIPMAP		equ	$20
SEMI_TRANS_SHIFT		equ	25
SCREEN_HEIGHT			equ	240
DQUAD_FLAG_DEEPER		equ	$01
DQUAD_DEEPER_BIAS		equ	1000
TRI_LEVEL1_AREA		equ	512
TRI_LEVEL0_AREA		equ	1408
QUAD_LEVEL1_AREA	equ	1024
QUAD_LEVEL0_AREA	equ	2816
MIP_TINT_WHITE		equ	$00ffffff
TRI_MIP_TINT_YELLOW	equ	$ff00ffff
TRI_MIP_TINT_ORANGE	equ	$ff0040ff
QUAD_TINT_CYAN		equ	-256
QUAD_TINT_BLUE		equ	$ffff4000

; A subdivision corner, and the recursion's frame on the scratchpad, a child's after its parent's.
		rsreset
SV_x		rh	1
SV_y		rh	1
SV_z		rh	1
SV_uv		rh	1
SV_SIZE		rb	0
		rsreset
SUB_return	rw	1
SUB_depth	rh	1
		rh	1
SUB_v0		rb	SV_SIZE
SUB_v1		rb	SV_SIZE
SUB_v2		rb	SV_SIZE
SUB_v3		rb	SV_SIZE
SUB_FRAME_SIZE	rb	0

; The node handlers' scratchpad words, over the vertex buffers until a batch is transformed.
			rsset	RS_screenXy
NS_cameraOffset		rh	4
NS_rotation		rb	MATRIX_SIZE
NS_lightRotation	rh	9
			rh	1

; A node handler's coordinate shift (s5): thresholds the OR of |dx|, |dy| and |dz| reaches.
NODE_SHIFT1_AT		equ	$4a00
NODE_SHIFT2_AT		equ	$a500
WORLD_SHIFT1_AT		equ	$2d00
WORLD_SHIFT2_AT		equ	$5a80
WORLD_SHIFT3_AT		equ	$b504
SKINNED_SHIFT1_AT	equ	$5a00
SKINNED_SHIFT2_AT	equ	$b500
SCREEN_CENTRE_X		equ	320
SCREEN_CENTRE_Y		equ	120
POV_OVERLAY_CENTRE_X	equ	560
POV_OVERLAY_CENTRE_Y	equ	60

	xref	MulMatrix2
	xref	gDynamicPrimRenderDispatch
	xref	gPovPrimRenderDispatch
	xref	gSinLutQ12
	xref	gCameraNodePtr
	xref	gStageRenderFlags
	xref	gRenderQueueCursor
	xref	gSkinnedLightSlot
	xref	gPacketCursor
	xref	gSkinnedPrelitColors
	xref	gLightSlots
	xref	gPovOverlayCameraMatrix
	xref	gCombinedViewMatrix
	xref	gRenderQueue
	xref	gModelTables
	xref	DrawSkinnedFighterModel
	xref	ProjectMatrixToShadowPlane
	xref	DrawOceanWaveMesh
	xdef	BuildRotationMatrixFast
	xdef	RenderSceneDisplayList
	xdef	RenderMeshBatchAlignedVerts
	xdef	TransformVertices8ByteRtpt
	xdef	TransformVertices6ByteRtpt
	xdef	DispatchMeshPrimitiveGroups
	xdef	RenderStandardSceneNode
	xdef	DrawPolyFt3DoubleLit
	xdef	DrawPolyFt4DoubleLit
	xdef	DrawPolyGt3DoubleLit
	xdef	DrawPolyGt4DoubleLit
	xdef	DrawSubdivPolyFt3Dynamic
	xdef	SubdividePolyFt3Recursive
	xdef	DrawSubdivPolyFt4Dynamic
	xdef	SubdividePolyFt4Recursive
	xdef	AverageVertexAndUv
	xdef	RenderStageTileBatch
	xdef	DrawPolyF3LitFarClip
	xdef	DrawPolyF4UnlitFarClip
	xdef	DrawClothPolyFt3DoubleLit
	xdef	DrawClothPolyFt4DoubleLit
	xdef	DrawPolyF3Lit
	xdef	DrawPolyF4Lit
	xdef	RenderClothSoftbodyNode
	xdef	DrawPolyFt3Unlit
	xdef	DrawPolyFt4Unlit
	xdef	DrawPolyFt3Lit
	xdef	DrawPolyFt4Lit
	xdef	DrawPolyG3Lit
	xdef	DrawPolyG4Lit
	xdef	DrawPolyGt3Lit
	xdef	DrawPolyGt4Lit
	xdef	RenderSkinnedSkeletalMesh
	xdef	DrawSkinnedPolyFt3Prelit
	xdef	DrawSkinnedPolyFt4Prelit
	xdef	DrawSkinnedPolyGt3Prelit
	xdef	DrawSkinnedPolyGt4Prelit
	xdef	AverageThreePrelitColors
	xdef	AverageFourPrelitColors
	xdef	FetchThreePrelitGt3Colors
	xdef	FetchFourPrelitGt4Colors
	xdef	PrelightSkinnedMeshNormals
	xdef	IgnoreSkinnedPolyF3
	xdef	IgnoreSkinnedPolyF4
	xdef	DrawPolyFt3DoubleLitDynamic
	xdef	DrawPolyFt4DoubleLitDynamic
	xdef	DrawPolyFt3LitDynamic
	xdef	DrawPolyFt4LitDynamic
	xdef	DrawPolyGt3LitDynamic
	xdef	DrawPolyGt4LitDynamic
	xdef	DrawShadowPolyF3Dynamic
	xdef	DrawShadowPolyF4Dynamic
	xdef	DrawPolyF3LitCopy
	xdef	DrawPolyF4WithDrawMode
	xdef	CopyWordsFast
	xdef	InterpolateOrthonormalMatrix
	xdef	NormalizeGteIrVector
	xdef	ComputeVectorLength3D
	xdef	ComputeVectorLengthSq3D
	xdef	LerpSvector
	xdef	LerpMatrix3x3
	xdef	MulMatrix3x3Fast
	xdef	TransformSvectorByMatrix
	xdef	SetGteRotAndTransMatrix
	xdef	SetGteRotMatrixZeroTrans
	xdef	TransformSvectorWithShortTrans
	xdef	GetGteFarColor
	xdef	TriggerDebugTrap
	xdef	gSceneNodeRenderDispatch
	xdef	gMeshBatchDispatch
	xdef	gTilePrimRenderDispatch
	xdef	gClothPrimRenderDispatch
	xdef	gRecipSqrtLutQ12
	xdef	gSqrtLutQ9

	section	.text

; BuildRotationMatrixFast(angles, out): the rotation of three 12-bit angles.
; In:   a0 the SVECTOR of angles, a1 the MATRIX.
; Out:  its rotation; its translation is not written.
; Uses: v0, a2, a3, t0..t9, hi/lo, the GTE.
BuildRotationMatrixFast:
	addiu	sp,sp,-56
	sw	ra,52(sp)
	sw	fp,48(sp)
	sw	s7,44(sp)
	sw	s6,40(sp)
	sw	s5,36(sp)
	sw	s4,32(sp)
	sw	s3,28(sp)
	sw	s2,24(sp)
	sw	s1,20(sp)
	sw	s0,16(sp)
	la	v0,gSinLutQ12
	lhu	t0,SVEC_vx(a0)
	lhu	t1,SVEC_vy(a0)
	lhu	t2,SVEC_vz(a0)
	andi	t0,t0,$fff
	andi	t1,t1,$fff
	andi	t2,t2,$fff
	add	t0,t0,t0
	add	t1,t1,t1
	add	t2,t2,t2
	add	t3,v0,t0
	add	t4,v0,t1
	add	t5,v0,t2
	lh	s0,0(t3)
	lh	s1,0(t4)
	lh	s2,0(t5)
	addi	t0,t0,2048
	addi	t1,t1,2048
	addi	t2,t2,2048
	andi	t0,t0,$1ffe
	andi	t1,t1,$1ffe
	andi	t2,t2,$1ffe
	add	t0,v0,t0
	add	t1,v0,t1
	add	t2,v0,t2
	lh	t5,0(t2)
	lh	s4,0(t1)
	lh	s3,0(t0)
	mtc2	t5,C2_IR0
	mtc2	s0,C2_IR1
	mtc2	s3,C2_IR2
	mtc2	s4,C2_IR3
	mfc2	s0,C2_IR1		; reads back what was just written
	wGPF	1
	mult	s0,s4
	sub	t7,zero,s1
	sh	t7,M_m2(a1)
	mfc2	t6,C2_IR1
	mfc2	t7,C2_IR2
	mfc2	t8,C2_IR3
	mtc2	s2,C2_IR0
	mtc2	s0,C2_IR1
	mtc2	s3,C2_IR2
	mtc2	s4,C2_IR3
	sh	t8,M_m0(a1)
	mflo	t9
	GPF	1
	sra	t9,t9,12
	mult	s3,s4
	mtc2	s1,C2_IR0
	sh	t9,M_m2+2(a1)
	mfc2	t8,C2_IR1
	mfc2	t9,C2_IR2
	mfc2	a2,C2_IR3
	mfc2	s1,C2_IR0		; reads back what was just written
	mtc2	t7,C2_MAC1
	sub	a3,zero,t6
	mtc2	a3,C2_MAC2
	mtc2	t8,C2_MAC3
	mtc2	t7,C2_IR3
	mflo	a3
	wGPL	1
	sh	a2,M_m1(a1)
	mult	s1,t6
	sra	a3,a3,12
	sh	a3,M_m2+4(a1)
	mfc2	t6,C2_IR1
	mfc2	t7,C2_IR2
	mfc2	t8,C2_IR3
	sh	t6,M_m1+2(a1)
	sh	t7,M_m1+4(a1)
	mflo	a2
	sh	t8,M_m0+4(a1)
	sra	a2,a2,12
	sub	a2,a2,t9
	sh	a2,M_m0+2(a1)
	lw	ra,52(sp)
	lw	fp,48(sp)
	lw	s7,44(sp)
	lw	s6,40(sp)
	lw	s5,36(sp)
	lw	s4,32(sp)
	lw	s3,28(sp)
	lw	s2,24(sp)
	lw	s1,20(sp)
	lw	s0,16(sp)
	jr	ra
	addiu	sp,sp,56

; RenderSceneDisplayList(): draws every SceneNode queued in gRenderQueue.
; In:   the scratchpad header (RenderScratch) filled.
; Out:  the frame's packets linked into RS_orderingTable.
; Uses: every temporary and argument register, the GTE.
RenderSceneDisplayList:
	addiu	sp,sp,-56
	sw	ra,52(sp)
	sw	fp,48(sp)
	sw	s7,44(sp)
	sw	s6,40(sp)
	sw	s5,36(sp)
	sw	s4,32(sp)
	sw	s3,28(sp)
	sw	s2,24(sp)
	sw	s1,20(sp)
	sw	s0,16(sp)
	lw	v0,gRenderQueueCursor
	la	s7,gRenderQueue
	beq	v0,s7,.scene_node_done
	lui	s1,SCRATCHPAD>>16
	addu	s5,zero,zero
	la	fp,gCombinedViewMatrix
	lw	s4,RS_primTable(s1)
.scene_node_next:
	lw	s2,0(s7)
	addu	s6,fp,zero
	lhu	t0,SN_renderType(s2)
	lhu	t1,SN_flags(s2)
	sll	t0,t0,2
	la	at,gSceneNodeRenderDispatch
	addu	at,at,t0
	lw	t0,0(at)
	beq	t1,zero,.dispatch_scene_node	; no effect: both paths dispatch
	nop
.dispatch_scene_node:
	jr	t0
	addiu	s7,s7,4
.walk_node_mesh:
	lh	t0,SN_modelSlot(s2)
	lh	t1,SN_meshIndex(s2)
	sll	t0,t0,2
	la	t2,gModelTables
	addu	t0,t0,t2
	lw	t0,0(t0)
	sll	t1,t1,2
	addu	t0,t0,t1
	lw	s0,0(t0)
	nop
.mesh_batch_next:
	lw	a1,0(s0)
	nop
	andi	a0,a1,$ffff
	srl	a1,a1,16
	beq	a0,zero,.scene_node_done
	addi	s0,s0,4
	sll	a1,a1,2
	la	at,gMeshBatchDispatch
	addu	at,at,a1
	lw	a1,0(at)
	nop
	jr	a1
	nop
.mesh_batch_done:
	lhu	t0,0(s0)
	addiu	s0,s0,2
	bne	t0,zero,.mesh_batch_next
	nop
.scene_node_done:
	lw	v0,gRenderQueueCursor
	nop
	bne	s7,v0,.scene_node_next
	nop
	lw	ra,52(sp)
	lw	fp,48(sp)
	lw	s7,44(sp)
	lw	s6,40(sp)
	lw	s5,36(sp)
	lw	s4,32(sp)
	lw	s3,28(sp)
	lw	s2,24(sp)
	lw	s1,20(sp)
	lw	s0,16(sp)
	jr	ra
	addiu	sp,sp,56

; RenderMeshBatchAlignedVerts (batch kind 0): projects 8-byte vertices, draws the groups.
; In:   a0 the count of vertex triples, s0 the vertices, s1 SCRATCHPAD, s4 the primitive table, s5 the shift.
; Out:  s0 past the groups, at .mesh_batch_done.
; Uses: at, a1, a2, t0, t1, ra and what the drawers use.
RenderMeshBatchAlignedVerts:
	ori	t0,zero,$3
	subu	t0,t0,s5
	lui	at,SCRATCHPAD>>16
	sw	t0,RS_scaleShift(at)
	addiu	a1,s1,RS_screenXy
	addiu	a2,s1,RS_screenZ
	jal	TransformVertices8ByteRtpt
	nop
	jal	DispatchMeshPrimitiveGroups
	nop
	j	.mesh_batch_done
	nop
; kind 1: 6-byte vertices (a0 counts vertices), drawn through gDynamicPrimRenderDispatch.
.mesh_batch_packed_verts:
	ori	t0,zero,$3
	subu	t0,t0,s5
	lui	at,SCRATCHPAD>>16
	sw	t0,RS_scaleShift(at)
	addiu	a1,s1,RS_screenXy
	addiu	a2,s1,RS_screenZ
	jal	TransformVertices6ByteRtpt
	nop
	la	s4,gDynamicPrimRenderDispatch
	nop
	jal	DispatchMeshPrimitiveGroups
	nop
	lw	s4,RS_primTable(s1)
	j	.mesh_batch_done
	nop
; kind 2: projected into the POV overlay's viewport, drawn through gPovPrimRenderDispatch.
.mesh_batch_pov_overlay:
	ori	t0,zero,$3
	subu	t0,t0,s5
	lui	at,SCRATCHPAD>>16
	sw	t0,RS_scaleShift(at)
	addiu	a1,s1,RS_screenXy
	addiu	a2,s1,RS_screenZ
	lui	t0,POV_OVERLAY_CENTRE_X
	lui	t1,POV_OVERLAY_CENTRE_Y
	ctc2	t0,C2_OFX
	ctc2	t1,C2_OFY
	jal	TransformVertices8ByteRtpt
	nop
	lui	t0,SCREEN_CENTRE_X
	lui	t1,SCREEN_CENTRE_Y
	ctc2	t0,C2_OFX
	ctc2	t1,C2_OFY
	la	s4,gPovPrimRenderDispatch
	jal	DispatchMeshPrimitiveGroups
	nop
	lw	s4,RS_primTable(s1)
	j	.mesh_batch_done
	nop

; TransformVertices8ByteRtpt: projects vertex triples into screen points and depths.
; In:   a0 the triples (at least one), s0 the SVECTORs, a1 the points, a2 the depths.
; Out:  s0 past the vertices.
; Uses: a0..a2, t0..t2, the GTE.
TransformVertices8ByteRtpt:
	addiu	a0,a0,-1
	lwc2	C2_VXY0,0(s0)
	lwc2	C2_VZ0,4(s0)
	lwc2	C2_VXY1,8(s0)
	lwc2	C2_VZ1,12(s0)
	lwc2	C2_VXY2,16(s0)
	lwc2	C2_VZ2,20(s0)
	nRTPT
	j	.verts8_store_three
	addiu	s0,s0,24
; pipelined: a triple's depths are stored while RTPT projects the next
.verts8_next_three:
	RTPT
	addiu	a2,a2,6
	sh	t0,-6(a2)
	sh	t1,-4(a2)
	sh	t2,-2(a2)
.verts8_store_three:
	swc2	C2_SXY0,0(a1)
	swc2	C2_SXY1,4(a1)
	swc2	C2_SXY2,8(a1)
	mfc2	t0,C2_SZ1
	mfc2	t1,C2_SZ2
	mfc2	t2,C2_SZ3
	addiu	a1,a1,12
	beq	a0,zero,.verts8_store_last
	addiu	a0,a0,-1
	lwc2	C2_VXY0,0(s0)
	lwc2	C2_VZ0,4(s0)
	lwc2	C2_VXY1,8(s0)
	lwc2	C2_VZ1,12(s0)
	lwc2	C2_VXY2,16(s0)
	lwc2	C2_VZ2,20(s0)
	j	.verts8_next_three
	addiu	s0,s0,24
.verts8_store_last:
	sh	t0,0(a2)
	sh	t1,2(a2)
	sh	t2,4(a2)
	jr	ra
	nop

; TransformVertices6ByteRtpt: the same for packed 6-byte vertices, three at a time.
; In:   a0 the vertex count; s0, a1, a2 as TransformVertices8ByteRtpt.
; Out:  s0 past the vertices, word-aligned.
; Uses: a0..a2, t0..t2, t6..t8, the GTE.
TransformVertices6ByteRtpt:
	addiu	a0,a0,-3
	lhu	t6,0(s0)
	lhu	t7,2(s0)
	lhu	t8,4(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY0
	mtc2	t8,C2_VZ0
	lhu	t6,6(s0)
	lhu	t7,8(s0)
	lhu	t8,10(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY1
	mtc2	t8,C2_VZ1
	lhu	t6,12(s0)
	lhu	t7,14(s0)
	lhu	t8,16(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY2
	mtc2	t8,C2_VZ2
	nRTPT
	j	.verts6_store_three
	addiu	s0,s0,18
.verts6_next_three:
	RTPT
	addiu	a2,a2,6
	sh	t0,-6(a2)
	sh	t1,-4(a2)
	sh	t2,-2(a2)
.verts6_store_three:
	swc2	C2_SXY0,0(a1)
	swc2	C2_SXY1,4(a1)
	swc2	C2_SXY2,8(a1)
	mfc2	t0,C2_SZ1
	mfc2	t1,C2_SZ2
	mfc2	t2,C2_SZ3
	addiu	a1,a1,12
	blez	a0,.verts6_done
	nop
	addiu	a0,a0,-3
	lhu	t6,0(s0)
	lhu	t7,2(s0)
	lhu	t8,4(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY0
	mtc2	t8,C2_VZ0
	lhu	t6,6(s0)
	lhu	t7,8(s0)
	lhu	t8,10(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY1
	mtc2	t8,C2_VZ1
	lhu	t6,12(s0)
	lhu	t7,14(s0)
	lhu	t8,16(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY2
	mtc2	t8,C2_VZ2
	j	.verts6_next_three
	addiu	s0,s0,18
.verts6_done:
	sh	t0,0(a2)
	sh	t1,2(a2)
	sh	t2,4(a2)
	add	a0,a0,a0
	add	t0,a0,a0
	add	a0,t0,a0
	add	s0,s0,a0
	andi	t0,s0,$3
	add	s0,s0,t0
	jr	ra
	nop

; DispatchMeshPrimitiveGroups: calls the table's drawer for each primitive group of a batch.
; In:   s0 the groups (count, type, records; a count of 0 ends them), s4 the table, s1 SCRATCHPAD.
; Out:  s0 past the ending count.
; Uses: a0, t0 and what the drawers use.
DispatchMeshPrimitiveGroups:
	sw	ra,RS_dispatchReturn(s1)
	lhu	a0,0(s0)
	addiu	s0,s0,2
	beq	a0,zero,.prim_groups_done
.prim_group_next:
	lhu	t0,0(s0)
	addiu	s0,s0,2
	sll	t0,t0,2
	addu	t0,t0,s4
	lw	t0,0(t0)
	nop
	jalr	ra,t0
	nop
	lhu	a0,0(s0)
	addiu	s0,s0,2
	bne	a0,zero,.prim_group_next
	nop
.prim_groups_done:
	lw	ra,RS_dispatchReturn(s1)
	nop
	jr	ra
	nop

; RenderStandardSceneNode (renderType 0): sets up the GTE for a lit mesh in the node's orientation.
; In:   s2 the node, s6 the view matrix, s1 SCRATCHPAD.
; Out:  to .walk_node_mesh, with s3 the light slot and s5 the shift.
; Uses: at, v0, v1, a0..a2, t0..t9, ra, the GTE.
RenderStandardSceneNode:
	lw	v0,gCameraNodePtr
	lw	t0,SN_worldMatrix+M_t(s2)
	lw	t1,SN_worldMatrix+M_t+4(s2)
	lw	t2,SN_worldMatrix+M_t+8(s2)
	lw	t3,SN_worldMatrix+M_t(v0)
	lw	t4,SN_worldMatrix+M_t+4(v0)
	lw	t5,SN_worldMatrix+M_t+8(v0)
	subu	t0,t0,t3
	subu	t1,t1,t4
	subu	t2,t2,t5
	slt	t6,t0,zero
	sub	t6,zero,t6
	xor	t3,t0,t6
	sub	t3,t3,t6
	slt	t6,t1,zero
	sub	t6,zero,t6
	xor	t4,t1,t6
	sub	t4,t4,t6
	slt	t6,t2,zero
	sub	t6,zero,t6
	xor	t5,t2,t6
	sub	t5,t5,t6
	or	t3,t3,t4
	or	t3,t3,t5
	ori	s5,zero,$2
	slti	t4,t3,NODE_SHIFT1_AT
	ori	at,zero,NODE_SHIFT2_AT
	slt	t5,t3,at
	sub	s5,s5,t4
	sub	s5,s5,t5
	nop
	srav	t0,t0,s5
	sh	t0,NS_cameraOffset(s1)
	srav	t1,t1,s5
	sh	t1,NS_cameraOffset+2(s1)
	srav	t2,t2,s5
	sh	t2,NS_cameraOffset+4(s1)
	lh	t0,SN_lightSlot(s2)
	la	t2,gLightSlots
	sll	t1,t0,1
	add	t1,t1,t0
	sll	t1,t1,5
	addu	s3,t1,t2
	addiu	a0,s3,LS_lightMatrix
	addiu	a1,s2,SN_worldMatrix
	jal	MulMatrix3x3Fast
	addiu	a2,s1,NS_lightRotation
	lw	t0,NS_lightRotation+M_r11r12(s1)
	lw	t1,NS_lightRotation+M_r13r21(s1)
	lw	t2,NS_lightRotation+M_r22r23(s1)
	lw	t3,NS_lightRotation+M_r31r32(s1)
	lw	t4,NS_lightRotation+M_r33(s1)
	ctc2	t0,C2_L11L12
	ctc2	t1,C2_L13L21
	ctc2	t2,C2_L22L23
	ctc2	t3,C2_L31L32
	ctc2	t4,C2_L33
	lw	t0,LS_colorMatrix+M_r11r12(s3)
	lw	t1,LS_colorMatrix+M_r13r21(s3)
	lw	t2,LS_colorMatrix+M_r22r23(s3)
	lw	t3,LS_colorMatrix+M_r31r32(s3)
	lw	t4,LS_colorMatrix+M_r33(s3)
	ctc2	t0,C2_LR1LR2
	ctc2	t1,C2_LR3LG1
	ctc2	t2,C2_LG2LG3
	ctc2	t3,C2_LB1LB2
	ctc2	t4,C2_LB3
	addu	a0,s6,zero
	addiu	a1,s2,SN_worldMatrix
	jal	MulMatrix3x3Fast
	addiu	a2,s1,NS_rotation
	lw	t0,M_r11r12(s6)
	lw	t1,M_r13r21(s6)
	lw	t2,M_r22r23(s6)
	lw	t3,M_r31r32(s6)
	lw	t4,M_r33(s6)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	lwc2	C2_VXY0,NS_cameraOffset(s1)
	lwc2	C2_VZ0,NS_cameraOffset+4(s1)
	lw	a0,LS_ambientRgb(s3)
	ori	t1,zero,$ff0
	MVMVA	1,0,0,0,0
	srl	a1,a0,4
	srl	a2,a0,12
	sll	a0,a0,4
	and	a0,a0,t1
	and	a1,a1,t1
	and	a2,a2,t1
	ctc2	a0,C2_RBK
	ctc2	a1,C2_GBK
	ctc2	a2,C2_BBK
	mfc2	t0,C2_IR1
	mfc2	t1,C2_IR2
	mfc2	t2,C2_IR3
	sra	t1,t1,1
	ctc2	t0,C2_TRX
	ctc2	t1,C2_TRY
	ctc2	t2,C2_TRZ
	lh	t0,NS_rotation+M_m0(s1)
	lh	t1,NS_rotation+M_m0+2(s1)
	lh	t2,NS_rotation+M_m0+4(s1)
	lh	t3,NS_rotation+M_m1(s1)
	lh	t4,NS_rotation+M_m1+2(s1)
	lh	t5,NS_rotation+M_m1+4(s1)
	lh	t6,NS_rotation+M_m2(s1)
	lh	t7,NS_rotation+M_m2+2(s1)
	lh	t8,NS_rotation+M_m2+4(s1)
	srav	t0,t0,s5
	srav	t1,t1,s5
	srav	t2,t2,s5
	addi	s5,s5,1
	srav	t3,t3,s5
	srav	t4,t4,s5
	srav	t5,t5,s5
	addi	s5,s5,-1
	srav	t6,t6,s5
	srav	t7,t7,s5
	srav	t8,t8,s5
	andi	t0,t0,$ffff
	sll	t1,t1,16
	andi	t2,t2,$ffff
	sll	t3,t3,16
	andi	t4,t4,$ffff
	sll	t5,t5,16
	andi	t6,t6,$ffff
	sll	t7,t7,16
	or	t0,t0,t1
	or	t2,t2,t3
	or	t4,t4,t5
	or	t6,t6,t7
	ctc2	t0,C2_R11R12
	ctc2	t2,C2_R13R21
	ctc2	t4,C2_R22R23
	ctc2	t6,C2_R31R32
	ctc2	t8,C2_R33
	j	.walk_node_mesh
	nop
; renderType 5: the mesh under the GTE matrices the previous node left.
.render_reused_matrix_node:
	j	.walk_node_mesh
	nop
; renderType 1: a world-aligned mesh: the view's rotation, the slot's light matrix, shift 2.
.render_world_scene_node:
	lw	v0,gCameraNodePtr
	lw	t0,SN_worldMatrix+M_t(s2)
	lw	t1,SN_worldMatrix+M_t+4(s2)
	lw	t2,SN_worldMatrix+M_t+8(s2)
	lw	t3,SN_worldMatrix+M_t(v0)
	lw	t4,SN_worldMatrix+M_t+4(v0)
	lw	t5,SN_worldMatrix+M_t+8(v0)
	subu	t0,t0,t3
	subu	t1,t1,t4
	subu	t2,t2,t5
	slt	t6,t0,zero
	sub	t6,zero,t6
	xor	t3,t0,t6
	sub	t3,t3,t6
	slt	t6,t1,zero
	sub	t6,zero,t6
	xor	t4,t1,t6
	sub	t4,t4,t6
	slt	t6,t2,zero
	sub	t6,zero,t6
	xor	t5,t2,t6
	sub	t5,t5,t6
	or	t3,t3,t4
	or	t3,t3,t5
	ori	s5,zero,$3		; replaced before it is read, with the next seven
	slti	t4,t3,WORLD_SHIFT1_AT
	slti	t5,t3,WORLD_SHIFT2_AT
	ori	at,zero,WORLD_SHIFT3_AT
	slt	t6,t3,at
	sub	s5,s5,t6
	sub	s5,s5,t5
	sub	s5,s5,t4
	ori	s5,zero,$2
	srav	t0,t0,s5
	sh	t0,NS_cameraOffset(s1)
	srav	t1,t1,s5
	sh	t1,NS_cameraOffset+2(s1)
	srav	t2,t2,s5
	sh	t2,NS_cameraOffset+4(s1)
	lw	t0,M_r11r12(s6)
	lw	t1,M_r13r21(s6)
	lw	t2,M_r22r23(s6)
	lw	t3,M_r31r32(s6)
	lw	t4,M_r33(s6)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	lwc2	C2_VXY0,NS_cameraOffset(s1)
	lwc2	C2_VZ0,NS_cameraOffset+4(s1)
	lh	t0,SN_lightSlot(s2)
	wMVMVA	1,0,0,0,0
	sll	t1,t0,1
	add	t1,t1,t0
	sll	t1,t1,5
	la	t0,gLightSlots
	addu	s3,t1,t0
	mfc2	t0,C2_IR1
	mfc2	t1,C2_IR2
	mfc2	t2,C2_IR3
	sra	t1,t1,1
	ctc2	t0,C2_TRX
	ctc2	t1,C2_TRY
	ctc2	t2,C2_TRZ
	lw	t0,LS_lightMatrix+M_r11r12(s3)
	lw	t1,LS_lightMatrix+M_r13r21(s3)
	lw	t2,LS_lightMatrix+M_r22r23(s3)
	lw	t3,LS_lightMatrix+M_r31r32(s3)
	lw	t4,LS_lightMatrix+M_r33(s3)
	ctc2	t0,C2_L11L12
	ctc2	t1,C2_L13L21
	ctc2	t2,C2_L22L23
	ctc2	t3,C2_L31L32
	ctc2	t4,C2_L33
	lw	t0,LS_colorMatrix+M_r11r12(s3)
	lw	t1,LS_colorMatrix+M_r13r21(s3)
	lw	t2,LS_colorMatrix+M_r22r23(s3)
	lw	t3,LS_colorMatrix+M_r31r32(s3)
	lw	t4,LS_colorMatrix+M_r33(s3)
	ctc2	t0,C2_LR1LR2
	ctc2	t1,C2_LR3LG1
	ctc2	t2,C2_LG2LG3
	ctc2	t3,C2_LB1LB2
	ctc2	t4,C2_LB3
	lw	a0,LS_ambientRgb(s3)
	ori	t1,zero,$ff0
	srl	a1,a0,4
	srl	a2,a0,12
	sll	a0,a0,4
	and	a0,a0,t1
	and	a1,a1,t1
	and	a2,a2,t1
	ctc2	a0,C2_RBK
	ctc2	a1,C2_GBK
	ctc2	a2,C2_BBK
	lh	t0,M_m0(s6)
	lh	t1,M_m0+2(s6)
	lh	t2,M_m0+4(s6)
	lh	t3,M_m1(s6)
	lh	t4,M_m1+2(s6)
	lh	t5,M_m1+4(s6)
	lh	t6,M_m2(s6)
	lh	t7,M_m2+2(s6)
	lh	t8,M_m2+4(s6)
	srav	t0,t0,s5
	srav	t1,t1,s5
	srav	t2,t2,s5
	addi	s5,s5,1
	srav	t3,t3,s5
	srav	t4,t4,s5
	srav	t5,t5,s5
	addi	s5,s5,-1
	srav	t6,t6,s5
	srav	t7,t7,s5
	srav	t8,t8,s5
	andi	t0,t0,$ffff
	sll	t1,t1,16
	andi	t2,t2,$ffff
	sll	t3,t3,16
	andi	t4,t4,$ffff
	sll	t5,t5,16
	andi	t6,t6,$ffff
	sll	t7,t7,16
	or	t0,t0,t1
	or	t2,t2,t3
	or	t4,t4,t5
	or	t6,t6,t7
	ctc2	t0,C2_R11R12
	ctc2	t2,C2_R13R21
	ctc2	t4,C2_R22R23
	ctc2	t6,C2_R31R32
	ctc2	t8,C2_R33
	j	.walk_node_mesh
	nop
; renderType 3: a planar shadow, its parent's matrix flattened by ProjectMatrixToShadowPlane.
.render_shadow_scene_node:
	lw	a0,SN_parent(s2)
	addiu	a1,s1,NS_rotation
	lw	a2,SN_param(s2)
	jal	ProjectMatrixToShadowPlane
	addiu	a0,a0,SN_worldMatrix
	lw	v0,gCameraNodePtr
	lw	t0,NS_rotation+M_t(s1)
	lw	t1,NS_rotation+M_t+4(s1)
	lw	t2,NS_rotation+M_t+8(s1)
	lw	t3,SN_worldMatrix+M_t(v0)
	lw	t4,SN_worldMatrix+M_t+4(v0)
	lw	t5,SN_worldMatrix+M_t+8(v0)
	subu	t0,t0,t3
	subu	t1,t1,t4
	subu	t2,t2,t5
	sra	t0,t0,2
	sra	t1,t1,2
	sra	t2,t2,2
	sh	t0,NS_cameraOffset(s1)
	sh	t1,NS_cameraOffset+2(s1)
	sh	t2,NS_cameraOffset+4(s1)
	lw	t0,M_r11r12(s6)
	lw	t1,M_r13r21(s6)
	lw	t2,M_r22r23(s6)
	lw	t3,M_r31r32(s6)
	lw	t4,M_r33(s6)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	lw	t0,NS_cameraOffset(s1)
	lw	t1,NS_cameraOffset+4(s1)
	mtc2	t0,C2_VXY0
	mtc2	t1,C2_VZ0
	nop
	wMVMVA	1,0,0,0,0
	nop
	nop
	swc2	C2_IR1,M_t(s6)
	swc2	C2_IR2,M_t+4(s6)
	swc2	C2_IR3,M_t+8(s6)
	addu	a0,s6,zero
	jal	MulMatrix2
	addiu	a1,s1,NS_rotation
	lw	t0,M_t(s6)
	lw	t1,M_t+4(s6)
	lw	t2,M_t+8(s6)
	sra	t1,t1,1
	ctc2	t0,C2_TRX
	ctc2	t1,C2_TRY
	ctc2	t2,C2_TRZ
	lh	t0,NS_rotation+M_m0(s1)
	lh	t1,NS_rotation+M_m0+2(s1)
	lh	t2,NS_rotation+M_m0+4(s1)
	lh	t3,NS_rotation+M_m1(s1)
	lh	t4,NS_rotation+M_m1+2(s1)
	lh	t5,NS_rotation+M_m1+4(s1)
	lh	t6,NS_rotation+M_m2(s1)
	lh	t7,NS_rotation+M_m2+2(s1)
	lh	t8,NS_rotation+M_m2+4(s1)
	sra	t0,t0,2
	sra	t1,t1,2
	sra	t2,t2,2
	sra	t3,t3,3
	sra	t4,t4,3
	sra	t5,t5,3
	sra	t6,t6,2
	sra	t7,t7,2
	sra	t8,t8,2
	andi	t0,t0,$ffff
	sll	t1,t1,16
	andi	t2,t2,$ffff
	sll	t3,t3,16
	andi	t4,t4,$ffff
	sll	t5,t5,16
	andi	t6,t6,$ffff
	sll	t7,t7,16
	or	t0,t0,t1
	or	t2,t2,t3
	or	t4,t4,t5
	or	t6,t6,t7
	ctc2	t0,C2_R11R12
	ctc2	t2,C2_R13R21
	ctc2	t4,C2_R22R23
	ctc2	t6,C2_R31R32
	ctc2	t8,C2_R33
	j	.walk_node_mesh
	ori	s5,zero,$2
; renderType 10: a skinned fighter, by DrawSkinnedFighterModel.
.render_skinned_scene_node:
	lw	v0,gCameraNodePtr
	lw	t0,SN_worldMatrix+M_t(s2)
	lw	t1,SN_worldMatrix+M_t+4(s2)
	lw	t2,SN_worldMatrix+M_t+8(s2)
	lw	t3,SN_worldMatrix+M_t(v0)
	lw	t4,SN_worldMatrix+M_t+4(v0)
	lw	t5,SN_worldMatrix+M_t+8(v0)
	subu	t0,t0,t3
	subu	t1,t1,t4
	subu	t2,t2,t5
	slt	t6,t0,zero
	sub	t6,zero,t6
	xor	t3,t0,t6
	sub	t3,t3,t6
	slt	t6,t1,zero
	sub	t6,zero,t6
	xor	t4,t1,t6
	sub	t4,t4,t6
	slt	t6,t2,zero
	sub	t6,zero,t6
	xor	t5,t2,t6
	sub	t5,t5,t6
	or	t3,t3,t4
	or	t3,t3,t5
	ori	s5,zero,$2
	slti	t4,t3,SKINNED_SHIFT1_AT
	ori	at,zero,SKINNED_SHIFT2_AT
	slt	t5,t3,at
	sub	s5,s5,t4
	sub	s5,s5,t5
	nop
	lh	t0,SN_lightSlot(s2)
	nop
	sll	t1,t0,1
	add	t1,t1,t0
	sll	t1,t1,5
	la	t0,gLightSlots
	addu	s3,t1,t0
	lw	t0,LS_colorMatrix+M_r11r12(s3)
	lw	t1,LS_colorMatrix+M_r13r21(s3)
	lw	t2,LS_colorMatrix+M_r22r23(s3)
	lw	t3,LS_colorMatrix+M_r31r32(s3)
	lw	t4,LS_colorMatrix+M_r33(s3)
	ctc2	t0,C2_LR1LR2
	ctc2	t1,C2_LR3LG1
	ctc2	t2,C2_LG2LG3
	ctc2	t3,C2_LB1LB2
	ctc2	t4,C2_LB3
	lw	a0,LS_ambientRgb(s3)
	ori	t1,zero,$ff0
	srl	a1,a0,4
	srl	a2,a0,12
	sll	a0,a0,4
	and	a0,a0,t1
	and	a1,a1,t1
	and	a2,a2,t1
	ctc2	a0,C2_RBK
	ctc2	a1,C2_GBK
	ctc2	a2,C2_BBK
	lui	at,SCRATCHPAD>>16
	sw	s5,RS_scaleShift(at)
	nop
	lh	t0,SN_modelSlot(s2)
	lh	t1,SN_meshIndex(s2)
	sll	t0,t0,2
	la	t2,gModelTables
	addu	t0,t0,t2
	lw	t0,0(t0)
	sll	t1,t1,2
	addu	t0,t0,t1
	lw	a1,0(t0)
	lw	a0,SN_param(s2)
	lh	a2,SN_lightSlot(s2)
	jal	DrawSkinnedFighterModel
	nop
	addu	s0,v0,zero		; replaced before it is read
	j	.scene_node_done
	nop
	j	.scene_node_done		; unreached
	nop
	j	.scene_node_done		; unreached
	nop
; renderType 12: a run of stage tiles, by RenderStageTileBatch.
.render_tile_scene_node:
	addu	a0,s7,zero
	addu	a1,s2,zero
	jal	RenderStageTileBatch
	addu	a2,s6,zero
	addu	s7,v0,zero
	j	.scene_node_done
	nop
; renderType 13: a cloth, by RenderClothSoftbodyNode.
.render_cloth_scene_node:
	lw	v0,gCameraNodePtr
	lw	t0,SN_worldMatrix+M_t(s2)
	lw	t1,SN_worldMatrix+M_t+4(s2)
	lw	t2,SN_worldMatrix+M_t+8(s2)
	lw	t3,SN_worldMatrix+M_t(v0)
	lw	t4,SN_worldMatrix+M_t+4(v0)
	lw	t5,SN_worldMatrix+M_t+8(v0)
	subu	t0,t0,t3
	subu	t1,t1,t4
	subu	t2,t2,t5
	slt	t6,t0,zero
	sub	t6,zero,t6
	xor	t3,t0,t6
	sub	t3,t3,t6
	slt	t6,t1,zero
	sub	t6,zero,t6
	xor	t4,t1,t6
	sub	t4,t4,t6
	slt	t6,t2,zero
	sub	t6,zero,t6
	xor	t5,t2,t6
	sub	t5,t5,t6
	or	t3,t3,t4
	or	t3,t3,t5
	ori	s5,zero,$2
	slti	t4,t3,NODE_SHIFT1_AT
	ori	at,zero,NODE_SHIFT2_AT
	slt	t5,t3,at
	sub	s5,s5,t4
	sub	s5,s5,t5
	nop
	lh	t0,SN_lightSlot(s2)
	la	t2,gLightSlots
	sll	t1,t0,1
	add	t1,t1,t0
	sll	t1,t1,5
	addu	s3,t1,t2
	addiu	a0,s3,LS_lightMatrix
	addiu	a1,s2,SN_worldMatrix
	jal	MulMatrix3x3Fast
	addiu	a2,s1,NS_lightRotation
	lw	t0,NS_lightRotation+M_r11r12(s1)
	lw	t1,NS_lightRotation+M_r13r21(s1)
	lw	t2,NS_lightRotation+M_r22r23(s1)
	lw	t3,NS_lightRotation+M_r31r32(s1)
	lw	t4,NS_lightRotation+M_r33(s1)
	ctc2	t0,C2_L11L12
	ctc2	t1,C2_L13L21
	ctc2	t2,C2_L22L23
	ctc2	t3,C2_L31L32
	ctc2	t4,C2_L33
	lw	t0,LS_colorMatrix+M_r11r12(s3)
	lw	t1,LS_colorMatrix+M_r13r21(s3)
	lw	t2,LS_colorMatrix+M_r22r23(s3)
	lw	t3,LS_colorMatrix+M_r31r32(s3)
	lw	t4,LS_colorMatrix+M_r33(s3)
	ctc2	t0,C2_LR1LR2
	ctc2	t1,C2_LR3LG1
	ctc2	t2,C2_LG2LG3
	ctc2	t3,C2_LB1LB2
	ctc2	t4,C2_LB3
	lw	a0,LS_ambientRgb(s3)
	ori	t1,zero,$ff0
	srl	a1,a0,4
	srl	a2,a0,12
	sll	a0,a0,4
	and	a0,a0,t1
	and	a1,a1,t1
	and	a2,a2,t1
	ctc2	a0,C2_RBK
	ctc2	a1,C2_GBK
	ctc2	a2,C2_BBK
	lhu	t2,M_m0+4(s6)
	lh	t3,M_m1(s6)
	lh	t4,M_m1+2(s6)
	lh	t5,M_m1+4(s6)
	sra	t3,t3,1
	sra	t4,t4,1
	sra	t5,t5,1
	lw	t0,M_r11r12(s6)
	sll	t3,t3,16
	andi	t4,t4,$ffff
	sll	t5,t5,16
	or	t2,t2,t3
	or	t4,t4,t5
	lw	t6,M_r31r32(s6)
	lw	t8,M_r33(s6)
	ctc2	t0,C2_R11R12
	ctc2	t2,C2_R13R21
	ctc2	t4,C2_R22R23
	ctc2	t6,C2_R31R32
	ctc2	t8,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	addu	a1,s5,zero
	jal	RenderClothSoftbodyNode
	addu	a0,s2,zero
	j	.scene_node_done
	nop
; renderType 15: a POV overlay part, relative to gPovOverlayCameraMatrix.
.render_pov_overlay_node:
	la	v0,gPovOverlayCameraMatrix
	lw	t0,SN_worldMatrix+M_t(s2)
	lw	t1,SN_worldMatrix+M_t+4(s2)
	lw	t2,SN_worldMatrix+M_t+8(s2)
	lw	t3,M_t(v0)
	lw	t4,M_t+4(v0)
	lw	t5,M_t+8(v0)
	subu	t0,t0,t3
	subu	t1,t1,t4
	subu	t2,t2,t5
	slt	t6,t0,zero
	sub	t6,zero,t6
	xor	t3,t0,t6
	sub	t3,t3,t6
	slt	t6,t1,zero
	sub	t6,zero,t6
	xor	t4,t1,t6
	sub	t4,t4,t6
	slt	t6,t2,zero
	sub	t6,zero,t6
	xor	t5,t2,t6
	sub	t5,t5,t6
	or	t3,t3,t4
	or	t3,t3,t5
	ori	s5,zero,$2		; replaced before it is read, with the next five
	slti	t4,t3,NODE_SHIFT1_AT
	ori	at,zero,NODE_SHIFT2_AT
	slt	t5,t3,at
	sub	s5,s5,t4
	sub	s5,s5,t5
	nop
	ori	s5,zero,$1
	srav	t0,t0,s5
	sh	t0,NS_cameraOffset(s1)
	srav	t1,t1,s5
	sh	t1,NS_cameraOffset+2(s1)
	srav	t2,t2,s5
	sh	t2,NS_cameraOffset+4(s1)
	lh	t0,SN_lightSlot(s2)
	la	t2,gLightSlots
	sll	t1,t0,1
	add	t1,t1,t0
	sll	t1,t1,5
	addu	s3,t1,t2
	addiu	a0,s3,LS_lightMatrix
	addiu	a1,s2,SN_worldMatrix
	jal	MulMatrix3x3Fast
	addiu	a2,s1,NS_lightRotation
	lw	t0,NS_lightRotation+M_r11r12(s1)
	lw	t1,NS_lightRotation+M_r13r21(s1)
	lw	t2,NS_lightRotation+M_r22r23(s1)
	lw	t3,NS_lightRotation+M_r31r32(s1)
	lw	t4,NS_lightRotation+M_r33(s1)
	ctc2	t0,C2_L11L12
	ctc2	t1,C2_L13L21
	ctc2	t2,C2_L22L23
	ctc2	t3,C2_L31L32
	ctc2	t4,C2_L33
	lw	t0,LS_colorMatrix+M_r11r12(s3)
	lw	t1,LS_colorMatrix+M_r13r21(s3)
	lw	t2,LS_colorMatrix+M_r22r23(s3)
	lw	t3,LS_colorMatrix+M_r31r32(s3)
	lw	t4,LS_colorMatrix+M_r33(s3)
	ctc2	t0,C2_LR1LR2
	ctc2	t1,C2_LR3LG1
	ctc2	t2,C2_LG2LG3
	ctc2	t3,C2_LB1LB2
	ctc2	t4,C2_LB3
	la	a0,gPovOverlayCameraMatrix
	addiu	a1,s2,SN_worldMatrix
	jal	MulMatrix3x3Fast
	addiu	a2,s1,NS_rotation
	la	v0,gPovOverlayCameraMatrix
	nop
	lw	t0,M_r11r12(v0)
	lw	t1,M_r13r21(v0)
	lw	t2,M_r22r23(v0)
	lw	t3,M_r31r32(v0)
	lw	t4,M_r33(v0)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	lwc2	C2_VXY0,NS_cameraOffset(s1)
	lwc2	C2_VZ0,NS_cameraOffset+4(s1)
	lw	a0,LS_ambientRgb(s3)
	ori	t1,zero,$ff0
	MVMVA	1,0,0,0,0
	srl	a1,a0,4
	srl	a2,a0,12
	sll	a0,a0,4
	and	a0,a0,t1
	and	a1,a1,t1
	and	a2,a2,t1
	ctc2	a0,C2_RBK
	ctc2	a1,C2_GBK
	ctc2	a2,C2_BBK
	mfc2	t0,C2_IR1
	mfc2	t1,C2_IR2
	mfc2	t2,C2_IR3
	sra	t1,t1,1
	ctc2	t0,C2_TRX
	ctc2	t1,C2_TRY
	ctc2	t2,C2_TRZ
	lh	t0,NS_rotation+M_m0(s1)
	lh	t1,NS_rotation+M_m0+2(s1)
	lh	t2,NS_rotation+M_m0+4(s1)
	lh	t3,NS_rotation+M_m1(s1)
	lh	t4,NS_rotation+M_m1+2(s1)
	lh	t5,NS_rotation+M_m1+4(s1)
	lh	t6,NS_rotation+M_m2(s1)
	lh	t7,NS_rotation+M_m2+2(s1)
	lh	t8,NS_rotation+M_m2+4(s1)
	srav	t0,t0,s5
	srav	t1,t1,s5
	srav	t2,t2,s5
	addi	s5,s5,1
	srav	t3,t3,s5
	srav	t4,t4,s5
	srav	t5,t5,s5
	addi	s5,s5,-1
	srav	t6,t6,s5
	srav	t7,t7,s5
	srav	t8,t8,s5
	andi	t0,t0,$ffff
	sll	t1,t1,16
	andi	t2,t2,$ffff
	sll	t3,t3,16
	andi	t4,t4,$ffff
	sll	t5,t5,16
	andi	t6,t6,$ffff
	sll	t7,t7,16
	or	t0,t0,t1
	or	t2,t2,t3
	or	t4,t4,t5
	or	t6,t6,t7
	ctc2	t0,C2_R11R12
	ctc2	t2,C2_R13R21
	ctc2	t4,C2_R22R23
	ctc2	t6,C2_R31R32
	ctc2	t8,C2_R33
	j	.walk_node_mesh
	nop
; renderType 14: the ocean, by DrawOceanWaveMesh (ocean.c).
.render_ocean_scene_node:
	lw	v0,gCameraNodePtr
	lw	t0,SN_worldMatrix+M_t(s2)
	lw	t1,SN_worldMatrix+M_t+4(s2)
	lw	t2,SN_worldMatrix+M_t+8(s2)
	lw	t3,SN_worldMatrix+M_t(v0)
	lw	t4,SN_worldMatrix+M_t+4(v0)
	lw	t5,SN_worldMatrix+M_t+8(v0)
	subu	t0,t0,t3
	subu	t1,t1,t4
	subu	t2,t2,t5
	ori	s5,zero,$1
	srav	t0,t0,s5
	sh	t0,NS_cameraOffset(s1)
	srav	t1,t1,s5
	sh	t1,NS_cameraOffset+2(s1)
	srav	t2,t2,s5
	sh	t2,NS_cameraOffset+4(s1)
	lw	t0,M_r11r12(s6)
	lw	t1,M_r13r21(s6)
	lw	t2,M_r22r23(s6)
	lw	t3,M_r31r32(s6)
	lw	t4,M_r33(s6)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	lwc2	C2_VXY0,NS_cameraOffset(s1)
	lwc2	C2_VZ0,NS_cameraOffset+4(s1)
	lh	t0,SN_lightSlot(s2)
	wMVMVA	1,0,0,0,0
	sll	t1,t0,1
	add	t1,t1,t0
	sll	t1,t1,5
	la	t2,gLightSlots
	addu	s3,t1,t2
	mfc2	t0,C2_IR1
	mfc2	t1,C2_IR2
	mfc2	t2,C2_IR3
	sra	t1,t1,1
	ctc2	t0,C2_TRX
	ctc2	t1,C2_TRY
	ctc2	t2,C2_TRZ
	lw	t0,LS_lightMatrix+M_r11r12(s3)
	lw	t1,LS_lightMatrix+M_r13r21(s3)
	lw	t2,LS_lightMatrix+M_r22r23(s3)
	lw	t3,LS_lightMatrix+M_r31r32(s3)
	lw	t4,LS_lightMatrix+M_r33(s3)
	ctc2	t0,C2_L11L12
	ctc2	t1,C2_L13L21
	ctc2	t2,C2_L22L23
	ctc2	t3,C2_L31L32
	ctc2	t4,C2_L33
	lw	t0,LS_colorMatrix+M_r11r12(s3)
	lw	t1,LS_colorMatrix+M_r13r21(s3)
	lw	t2,LS_colorMatrix+M_r22r23(s3)
	lw	t3,LS_colorMatrix+M_r31r32(s3)
	lw	t4,LS_colorMatrix+M_r33(s3)
	ctc2	t0,C2_LR1LR2
	ctc2	t1,C2_LR3LG1
	ctc2	t2,C2_LG2LG3
	ctc2	t3,C2_LB1LB2
	ctc2	t4,C2_LB3
	lw	a0,LS_ambientRgb(s3)
	ori	t1,zero,$ff0
	srl	a1,a0,4
	srl	a2,a0,12
	sll	a0,a0,4
	and	a0,a0,t1
	and	a1,a1,t1
	and	a2,a2,t1
	ctc2	a0,C2_RBK
	ctc2	a1,C2_GBK
	ctc2	a2,C2_BBK
	lh	t0,M_m0(s6)
	lh	t1,M_m0+2(s6)
	lh	t2,M_m0+4(s6)
	lh	t3,M_m1(s6)
	lh	t4,M_m1+2(s6)
	lh	t5,M_m1+4(s6)
	lh	t6,M_m2(s6)
	lh	t7,M_m2+2(s6)
	lh	t8,M_m2+4(s6)
	srav	t0,t0,s5
	srav	t1,t1,s5
	srav	t2,t2,s5
	addi	s5,s5,1
	srav	t3,t3,s5
	srav	t4,t4,s5
	srav	t5,t5,s5
	addi	s5,s5,-1
	srav	t6,t6,s5
	srav	t7,t7,s5
	srav	t8,t8,s5
	sh	t0,NS_rotation+M_m0(s1)
	sh	t1,NS_rotation+M_m0+2(s1)
	sh	t2,NS_rotation+M_m0+4(s1)
	sh	t3,NS_rotation+M_m1(s1)
	sh	t4,NS_rotation+M_m1+2(s1)
	sh	t5,NS_rotation+M_m1+4(s1)
	sh	t6,NS_rotation+M_m2(s1)
	sh	t7,NS_rotation+M_m2+2(s1)
	sh	t8,NS_rotation+M_m2+4(s1)
	lw	t0,NS_rotation+M_r11r12(s1)
	lw	t1,NS_rotation+M_r13r21(s1)
	lw	t2,NS_rotation+M_r22r23(s1)
	lw	t3,NS_rotation+M_r31r32(s1)
	lw	t4,NS_rotation+M_r33(s1)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	lw	a1,gStageRenderFlags
	nop
	sw	a1,TS_stageFlags(s1)
	jal	DrawOceanWaveMesh
	addu	a0,s2,zero		; not read: DrawOceanWaveMesh takes no arguments
	j	.scene_node_done
	nop

; DrawPolyFt3DoubleLit (type 6): textured triangles lit on both sides.
; In:   a0 the face count (not zero), s0 the MFT3 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a3, t0..t6, t9, the GTE.
DrawPolyFt3DoubleLit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_ft3_double_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT3_TAG
	li	fp,OT_ADDR_MASK
	lw	v1,SCRATCHPAD+RS_scaleShift
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,FT3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MFT3_vertices
	lwc2	C2_RGB,FT_r0(s6)
.poly_ft3_double_lit_next:
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
	bltz	t6,.poly_ft3_double_lit_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bne	t6,zero,.poly_ft3_double_lit_skip
	slt	a3,v0,zero
	addi	a3,a3,-1
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
	blez	v0,.poly_ft3_double_lit_skip
	sll	v0,v0,2
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_ft3_double_lit_skip
	nop
	lw	t0,MFT3_normal-MFT3_vertices(s0)
	lw	t1,MFT3_normal+4-MFT3_vertices(s0)
	xor	t0,t0,a3
	mtc2	t0,C2_VXY0
	xor	t1,t1,a3
	mtc2	t1,C2_VZ0
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCS
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	swc2	C2_RGB2,FT_r0(s6)
.poly_ft3_double_lit_skip:
	addiu	s0,s0,MFT3_SIZE
	addiu	s6,s6,MFT3_SIZE
	bne	s3,zero,.poly_ft3_double_lit_next
	addiu	s3,s3,-1
.poly_ft3_double_lit_done:
	addi	s0,s0,-MFT3_vertices
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

; DrawPolyFt4DoubleLit (type 14): textured quads lit on both sides.
; In:   a0 the face count (not zero), s0 the MFT4 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawPolyFt4DoubleLit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_ft4_double_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,FT4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MFT4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,FT_r0(s6)
.poly_ft4_double_lit_next:
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
	bltz	t8,.poly_ft4_double_lit_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bne	t8,zero,.poly_ft4_double_lit_skip
	slt	a3,v0,zero
	addi	a3,a3,-1
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
	lh	t3,0(v0)
	add	a0,a0,a1
	add	a0,a0,a2
	add	a0,a0,t3
	sra	v0,a0,2
	blez	v0,.poly_ft4_double_lit_skip
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_ft4_double_lit_skip
	nop
	sw	t0,FT_x0(s6)
	sw	t1,FT_x1(s6)
	sw	t2,FT_x2(s6)
	sw	t7,FT_x3(s6)
	lw	t0,MFT4_normal-MFT4_vertices(s0)
	lw	t1,MFT4_normal+4-MFT4_vertices(s0)
	xor	t0,t0,a3
	mtc2	t0,C2_VXY0
	xor	t1,t1,a3
	mtc2	t1,C2_VZ0
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCS
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	swc2	C2_RGB2,FT_r0(s6)
.poly_ft4_double_lit_skip:
	addiu	s0,s0,MFT4_SIZE
	addiu	s6,s6,MFT4_SIZE
	bne	s3,zero,.poly_ft4_double_lit_next
	addiu	s3,s3,-1
.poly_ft4_double_lit_done:
	addi	s0,s0,-MFT4_vertices
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

; DrawPolyGt3DoubleLit (type 22; cloth entry 2): Gouraud textured triangles lit on both sides.
; In:   a0 the face count (not zero), s0 the MGT3 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a3, t0..t6, t9, the GTE.
DrawPolyGt3DoubleLit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_gt3_double_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,GT3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,GT3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MGT3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,GT_r0(s6)
.poly_gt3_double_lit_next:
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
	bltz	t6,.poly_gt3_double_lit_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bne	t6,zero,.poly_gt3_double_lit_skip
	slt	a3,v0,zero
	addi	a3,a3,-1
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
	sw	t0,GT_x0(s6)
	wAVSZ3
	sw	t1,GT_x1(s6)
	mfc2	v0,C2_OTZ
	sw	t2,GT_x2(s6)
	blez	v0,.poly_gt3_double_lit_skip
	sll	v0,v0,2
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_gt3_double_lit_skip
	nop
	lw	t0,MGT3_normalXy0-MGT3_vertices(s0)
	lw	t1,MGT3_normalZ0-MGT3_vertices(s0)
	lw	t2,MGT3_normalXy1-MGT3_vertices(s0)
	xor	t0,t0,a3
	mtc2	t0,C2_VXY0
	xor	t1,t1,a3
	mtc2	t1,C2_VZ0
	xor	t2,t2,a3
	mtc2	t2,C2_VXY1
	srl	t1,t1,16
	mtc2	t1,C2_VZ1
	lw	t0,MGT3_normalXy2-MGT3_vertices(s0)
	lw	t2,MGT3_normalZ2-MGT3_vertices(s0)
	xor	t0,t0,a3
	mtc2	t0,C2_VXY2
	xor	t2,t2,a3
	mtc2	t2,C2_VZ2
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCT
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,GT_tag(s6)
	swc2	C2_RGB0,GT_r0(s6)
	swc2	C2_RGB1,GT_r1(s6)
	swc2	C2_RGB2,GT_r2(s6)
.poly_gt3_double_lit_skip:
	addiu	s0,s0,MGT3_SIZE
	addiu	s6,s6,MGT3_SIZE
	bne	s3,zero,.poly_gt3_double_lit_next
	addiu	s3,s3,-1
.poly_gt3_double_lit_done:
	addi	s0,s0,-MGT3_vertices
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

; DrawPolyGt4DoubleLit (type 30; cloth entry 3): Gouraud textured quads lit on both sides.
; In:   a0 the face count (not zero), s0 the MGT4 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawPolyGt4DoubleLit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_gt4_double_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,GT4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,GT4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MGT4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,GT_r0(s6)
.poly_gt4_double_lit_next:
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
	bltz	t8,.poly_gt4_double_lit_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bne	t8,zero,.poly_gt4_double_lit_skip
	slt	a3,v0,zero
	addi	a3,a3,-1
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
	lh	t3,0(v0)
	add	a0,a0,a1
	add	a0,a0,a2
	add	a0,a0,t3
	sra	v0,a0,2
	blez	v0,.poly_gt4_double_lit_skip
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_gt4_double_lit_skip
	nop
	sw	t0,GT_x0(s6)
	sw	t1,GT_x1(s6)
	sw	t2,GT_x2(s6)
	sw	t7,GT_x3(s6)
	lw	t0,MGT4_normalXy0-MGT4_vertices(s0)
	lw	t5,MGT4_normalZ0-MGT4_vertices(s0)
	xor	t0,t0,a3
	mtc2	t0,C2_VXY0
	xor	t5,t5,a3
	mtc2	t5,C2_VZ0
	lw	t1,MGT4_normalXy1-MGT4_vertices(s0)
	lw	t2,MGT4_normalXy2-MGT4_vertices(s0)
	NCS
	lw	t3,MGT4_normalZ2-MGT4_vertices(s0)
	lw	t4,MGT4_normalXy3-MGT4_vertices(s0)
	xor	t1,t1,a3
	xor	t2,t2,a3		; normalZ2, normalZ3 and normalXy3 are not inverted for a back face
	srl	t5,t5,16
	swc2	C2_RGB2,GT_r0(s6)
	mtc2	t1,C2_VXY0
	mtc2	t5,C2_VZ0
	mtc2	t2,C2_VXY1
	mtc2	t3,C2_VZ1
	mtc2	t4,C2_VXY2
	srl	t1,t3,16
	mtc2	t1,C2_VZ2
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	NCT
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,GT_tag(s6)
	swc2	C2_RGB0,GT_r1(s6)
	swc2	C2_RGB1,GT_r2(s6)
	swc2	C2_RGB2,GT_r3(s6)
.poly_gt4_double_lit_skip:
	addiu	s0,s0,MGT4_SIZE
	addiu	s6,s6,MGT4_SIZE
	bne	s3,zero,.poly_gt4_double_lit_next
	addiu	s3,s3,-1
.poly_gt4_double_lit_done:
	addi	s0,s0,-MGT4_vertices
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

; DrawSubdivPolyFt3Dynamic(count, vertices, faces, lod) -> past them: a tile's triangles.
; In:   a0 the count (not zero), a1 the tile's 6-byte vertices, a2 the DTRI faces, a3 its lodLevel and flags, t3 as the caller left it.
; Out:  v0 past the faces, gPacketCursor past the packets.
; Uses: at, v0, v1, a0..a3, t0..t6, t8, t9, the GTE.
DrawSubdivPolyFt3Dynamic:
	addiu	sp,sp,-56
	sw	ra,52(sp)
	sw	fp,48(sp)
	sw	s7,44(sp)
	sw	s6,40(sp)
	sw	s5,36(sp)
	sw	s4,32(sp)
	sw	s3,28(sp)
	sw	s2,24(sp)
	sw	s1,20(sp)
	sw	s0,16(sp)
	lui	s7,SCRATCHPAD>>16
	sh	a3,TS_lodLevel(s7)
	sw	a1,TS_tileVertices(s7)
	addu	s0,a2,zero
	lw	s2,RS_orderingTable(s7)
	lw	s4,gPacketCursor
	addi	s5,s7,TS_screenXy
	addi	s6,s7,TS_screenZ
	addi	s3,s7,TS_uv3
	li	fp,OT_ADDR_MASK
	ori	t1,zero,$4
	andi	t0,s3,$3
	sub	t0,t1,t0
	andi	t0,t0,$3
	add	s3,s3,t0
	addi	a0,a0,-1
.subdiv_tri_next:
	lw	v0,DTRI_vertices(s0)
	lb	t1,TS_tileFlags(s7)
	lw	t0,DTRI_tpage(s0)
	andi	t1,t1,SCENE_TILE_FLAG_SEMI_TRANS
	sll	t1,t1,SEMI_TRANS_SHIFT
	or	t0,t0,t1
	mtc2	t0,C2_RGB
	sw	a0,TS_faceCount(s7)
	sll	t4,v0,2
	andi	t4,t4,$ff<<2
	addu	t0,t4,s5
	lw	t0,0(t0)
	srl	t5,v0,6
	andi	t5,t5,$ff<<2
	addu	t1,t5,s5
	lw	t1,0(t1)
	srl	t6,v0,14
	andi	t6,t6,$ff<<2
	addu	t2,t6,s5
	lw	t2,0(t2)
	mtc2	t0,C2_SXY0
	mtc2	t1,C2_SXY1
	mtc2	t2,C2_SXY2
	and	v1,t0,t1
	and	v1,v1,t2
	and	v1,v1,t3		; t3: left by the last subdivided face (a uv), else the caller's value
	NCLIP
	bltz	v1,.subdiv_tri_skip
	nop
	andi	v1,v1,$8000
	mfc2	v0,C2_MAC0
	bne	v1,zero,.subdiv_tri_skip
	nop
	bgez	v0,.subdiv_tri_skip
	sub	v1,zero,v0
	srl	t4,t4,1
	add	a0,t4,s6
	lh	a0,0(a0)
	srl	t5,t5,1
	add	a1,t5,s6
	lh	a1,0(a1)
	srl	t6,t6,1
	add	a2,t6,s6
	lh	a2,0(a2)
	slt	v0,a0,a1
	beq	v0,zero,.subdiv_tri_max_z2
	slt	v0,a0,a2
	addu	a0,a1,zero
	slt	v0,a0,a2
.subdiv_tri_max_z2:
	beq	v0,zero,.subdiv_tri_max_z
	nop
	addu	a0,a2,zero
.subdiv_tri_max_z:
	addu	v0,a0,zero
	addu	a3,v0,zero
	lw	a1,RS_fogFarOtz(s7)
	lw	a2,RS_stageTileMinOtz(s7)
	srl	v0,v0,2
	srl	t8,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t8
	andi	v0,v0,$fff
	slt	at,a1,v0
	bne	at,zero,.subdiv_tri_skip
	nop
	slt	at,v0,a2
	bne	at,zero,.subdiv_tri_skip
	nop
	lwc2	C2_VXY0,DTRI_normal(s0)
	lwc2	C2_VZ0,DTRI_normal+4(s0)
	lh	a0,DTRI_clut(s0)
	lh	a1,DTRI_tpage(s0)
	sh	a0,TS_clut(s7)
	sh	a1,TS_tpage(s7)
	NCS
	addi	v1,v1,-TRI_LEVEL1_AREA
	bltz	v1,.subdiv_tri_area_level
	ori	a0,zero,$2
	addi	v1,v1,-(TRI_LEVEL0_AREA-TRI_LEVEL1_AREA)
	bltz	v1,.subdiv_tri_area_level
	ori	a0,zero,$1
	ori	a0,zero,$0
.subdiv_tri_area_level:
	addu	v1,a0,zero
	lw	a1,TS_stageFlags(s7)
	nop
	andi	a1,a1,STAGE_RENDER_MIP_MODE_MASK
	beq	a1,zero,.subdiv_tri_set_level
	addu	a0,zero,zero
	addi	a2,a1,-STAGE_RENDER_MIP_MODE_AREA
	beq	a2,zero,.subdiv_tri_level
	nop
	lb	a0,TS_lodLevel(s7)
	nop
	addi	a0,a0,-2
	sub	a0,zero,a0
	addi	a2,a1,-STAGE_RENDER_MIP_MODE_LOD
	beq	a2,zero,.subdiv_tri_set_level
	nop
	lb	a0,TS_lodLevel(s7)
	nop
	sub	a0,v1,a0
	bgtz	a0,.subdiv_tri_set_level
	nop
	ori	a0,zero,$0
.subdiv_tri_set_level:
	addu	v1,a0,zero
.subdiv_tri_level:
	lw	a0,TS_stageFlags(s7)
	nop
	andi	a0,a0,STAGE_RENDER_DEBUG_MIP_TINT
	beq	a0,zero,.subdiv_tri_no_tint
	nop
	addu	a0,v1,zero
	lw	a1,DTRI_tpage(s0)
	nop
	li	at,MIP_TINT_WHITE
	or	a1,a1,at
	beq	a0,zero,.subdiv_tri_tint
	nop
	li	at,TRI_MIP_TINT_YELLOW
	and	a1,a1,at
	addi	a0,a0,-1
	beq	a0,zero,.subdiv_tri_tint
	nop
	li	at,TRI_MIP_TINT_ORANGE
	and	a1,a1,at
.subdiv_tri_tint:
	mtc2	a1,C2_RGB2
	nop
	nop
.subdiv_tri_no_tint:
	lw	a0,TS_stageFlags(s7)
	nop
	andi	a0,a0,STAGE_RENDER_TRI_MIPMAP
	bne	a0,zero,.subdiv_tri_uv
	nop
	addu	v1,zero,zero
.subdiv_tri_uv:
	lhu	a0,DTRI_uv0(s0)
	lhu	a1,DTRI_uv1(s0)
	lhu	a2,DTRI_uv2(s0)
	beq	v1,zero,.subdiv_tri_uv_full
	ori	t8,zero,$ff
	srlv	t8,t8,v1
	sll	t9,t8,8
	or	t8,t8,t9
	srlv	a0,a0,v1
	and	a0,a0,t8
	sh	a0,TS_uv0(s7)
	srlv	a1,a1,v1
	and	a1,a1,t8
	sh	a1,TS_uv1(s7)
	srlv	a2,a2,v1
	and	a2,a2,t8
	sh	a2,TS_uv2(s7)
	j	.subdiv_tri_uv_done
	nop
.subdiv_tri_uv_full:
	sh	a0,TS_uv0(s7)
	sh	a1,TS_uv1(s7)
	sh	a2,TS_uv2(s7)
.subdiv_tri_uv_done:
	lb	a3,TS_lodLevel(s7)
	nop
	bne	a3,zero,.subdiv_tri_split
	nop
	lh	t4,TS_clut(s7)
	lh	t5,TS_tpage(s7)
	sw	t0,FT_x0(s4)
	sw	t1,FT_x1(s4)
	sw	t2,FT_x2(s4)
	sll	t4,t4,16
	sll	t5,t5,16
	lhu	t0,TS_uv0(s7)
	lhu	t1,TS_uv1(s7)
	lhu	t2,TS_uv2(s7)
	or	t0,t0,t4
	or	t1,t1,t5
	sw	t0,FT_u0(s4)
	sw	t1,FT_u1(s4)
	sh	t2,FT_u2(s4)
	swc2	C2_RGB2,FT_r0(s4)
	sll	v0,v0,2
	addu	v0,v0,s2
	lw	t1,0(v0)
	and	t0,s4,fp
	sw	t0,0(v0)
	lui	t2,FT3_TAG>>16
	or	t1,t1,t2
	sw	t1,FT_tag(s4)
	lw	a0,TS_faceCount(s7)
	addi	s0,s0,DTRI_SIZE
	addi	s4,s4,FT3_SIZE
	bne	a0,zero,.subdiv_tri_next
	addi	a0,a0,-1
	j	.subdiv_tri_done
	nop
.subdiv_tri_split:
	sll	v0,v0,2
	addu	v0,v0,s2
	lw	s1,0(v0)
	sw	v0,TS_otEntry(s7)
	lui	t0,FT3_TAG>>16
	or	s1,s1,t0
	lw	a1,TS_tileVertices(s7)
	swc2	C2_RGB2,TS_color(s7)
	add	t0,t4,t4
	add	t4,t4,t0
	add	t4,t4,a1
	lh	t0,0(t4)
	lh	t1,2(t4)
	lh	t2,4(t4)
	lh	t3,TS_uv0(s7)
	sh	t0,SUB_v0+SV_x(s3)
	sh	t1,SUB_v0+SV_y(s3)
	sh	t2,SUB_v0+SV_z(s3)
	sh	t3,SUB_v0+SV_uv(s3)
	add	t0,t5,t5
	add	t5,t5,t0
	add	t5,t5,a1
	lh	t0,0(t5)
	lh	t1,2(t5)
	lh	t2,4(t5)
	lh	t3,TS_uv1(s7)
	sh	t0,SUB_v1+SV_x(s3)
	sh	t1,SUB_v1+SV_y(s3)
	sh	t2,SUB_v1+SV_z(s3)
	sh	t3,SUB_v1+SV_uv(s3)
	add	t0,t6,t6
	add	t6,t6,t0
	add	t6,t6,a1
	lh	t0,0(t6)
	lh	t1,2(t6)
	lh	t2,4(t6)
	lh	t3,TS_uv2(s7)
	sh	t0,SUB_v2+SV_x(s3)
	sh	t1,SUB_v2+SV_y(s3)
	sh	t2,SUB_v2+SV_z(s3)
	sh	t3,SUB_v2+SV_uv(s3)
	jal	SubdividePolyFt3Recursive
	addu	a0,a3,zero
	lw	t0,TS_otEntry(s7)
	and	t1,s1,fp
	sw	t1,0(t0)
	lw	a0,TS_faceCount(s7)
	addi	s0,s0,DTRI_SIZE
	bne	a0,zero,.subdiv_tri_next
	addi	a0,a0,-1
	j	.subdiv_tri_done
	nop
.subdiv_tri_skip:
	lw	a0,TS_faceCount(s7)
	addi	s0,s0,DTRI_SIZE
	bne	a0,zero,.subdiv_tri_next
	addi	a0,a0,-1
.subdiv_tri_done:
	opt	at+
	sw	s4,gPacketCursor
	opt	at-
	addu	v0,s0,zero
	lw	ra,52(sp)
	lw	fp,48(sp)
	lw	s7,44(sp)
	lw	s6,40(sp)
	lw	s5,36(sp)
	lw	s4,32(sp)
	lw	s3,28(sp)
	lw	s2,24(sp)
	lw	s1,20(sp)
	lw	s0,16(sp)
	jr	ra
	addiu	sp,sp,56

; SubdividePolyFt3Recursive: draws the triangle of frame s3, split a0 times into four.
; In:   a0 the depth, s3 the frame, s4 the next packet, s1 the tag to link, s7 SCRATCHPAD, fp OT_ADDR_MASK.
; Out:  s4 past the packets, s1 the last one's tag.
; Uses: a0..a2, t0..t5, the GTE.
SubdividePolyFt3Recursive:
	bne	a0,zero,.split_ft3
	nop
	lwc2	C2_VXY0,SUB_v0(s3)
	lwc2	C2_VZ0,SUB_v0+SV_z(s3)
	lwc2	C2_VXY1,SUB_v1(s3)
	lwc2	C2_VZ1,SUB_v1+SV_z(s3)
	lwc2	C2_VXY2,SUB_v2(s3)
	lwc2	C2_VZ2,SUB_v2+SV_z(s3)
	nRTPT
	lhu	t4,TS_clut(s7)
	lhu	t5,TS_tpage(s7)
	lhu	t0,SUB_v0+SV_uv(s3)
	lhu	t1,SUB_v1+SV_uv(s3)
	lhu	t2,SUB_v2+SV_uv(s3)
	sll	t4,t4,16
	sll	t5,t5,16
	or	t0,t0,t4
	or	t1,t1,t5
	sw	t0,FT_u0(s4)
	sw	t1,FT_u1(s4)
	sh	t2,FT_u2(s4)
	swc2	C2_SXY0,FT_x0(s4)
	swc2	C2_SXY1,FT_x1(s4)
	swc2	C2_SXY2,FT_x2(s4)
	lw	t0,TS_color(s7)
	sw	s1,FT_tag(s4)
	sw	t0,FT_r0(s4)
	lui	t1,FT3_TAG>>16
	and	s1,s4,fp
	or	s1,s1,t1
	jr	ra
	addi	s4,s4,FT3_SIZE
.split_ft3:
	addi	s3,s3,SUB_FRAME_SIZE
	addi	a0,a0,-1
	sw	ra,SUB_return(s3)
	sh	a0,SUB_depth(s3)
	lw	t0,SUB_v0-SUB_FRAME_SIZE(s3)
	lw	t1,SUB_v0+SV_z-SUB_FRAME_SIZE(s3)
	sw	t0,SUB_v0(s3)
	sw	t1,SUB_v0+SV_z(s3)
	addi	a0,s3,SUB_v0-SUB_FRAME_SIZE
	addi	a1,s3,SUB_v1-SUB_FRAME_SIZE
	jal	AverageVertexAndUv
	addi	a2,s3,SUB_v1
	addi	a0,s3,SUB_v0-SUB_FRAME_SIZE
	addi	a1,s3,SUB_v2-SUB_FRAME_SIZE
	jal	AverageVertexAndUv
	addi	a2,s3,SUB_v2
	lh	a0,SUB_depth(s3)
	jal	SubdividePolyFt3Recursive
	nop
	addi	a0,s3,SUB_v1-SUB_FRAME_SIZE
	addi	a1,s3,SUB_v2-SUB_FRAME_SIZE
	jal	AverageVertexAndUv
	addi	a2,s3,SUB_v0
	lh	a0,SUB_depth(s3)
	jal	SubdividePolyFt3Recursive
	nop
	lw	t0,SUB_v1(s3)
	lw	t1,SUB_v1+SV_z(s3)
	sw	t0,SUB_v3(s3)
	sw	t1,SUB_v3+SV_z(s3)
	lw	t0,SUB_v2-SUB_FRAME_SIZE(s3)
	lw	t1,SUB_v2+SV_z-SUB_FRAME_SIZE(s3)
	sw	t0,SUB_v1(s3)
	sw	t1,SUB_v1+SV_z(s3)
	lh	a0,SUB_depth(s3)
	jal	SubdividePolyFt3Recursive
	nop
	lw	t0,SUB_v3(s3)
	lw	t1,SUB_v3+SV_z(s3)
	sw	t0,SUB_v1(s3)
	sw	t1,SUB_v1+SV_z(s3)
	lw	t0,SUB_v1-SUB_FRAME_SIZE(s3)
	lw	t1,SUB_v1+SV_z-SUB_FRAME_SIZE(s3)
	sw	t0,SUB_v2(s3)
	sw	t1,SUB_v2+SV_z(s3)
	lh	a0,SUB_depth(s3)
	jal	SubdividePolyFt3Recursive
	nop
	lw	ra,SUB_return(s3)
	nop
	jr	ra
	addi	s3,s3,-SUB_FRAME_SIZE

; DrawSubdivPolyFt4Dynamic(count, vertices, faces, lod) -> past them: a tile's quads.
; In:   a0 the count (not zero), a1 the tile's 6-byte vertices, a2 the DQUAD faces, a3 its lodLevel and flags.
; Out:  v0 past the faces, gPacketCursor past the packets.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawSubdivPolyFt4Dynamic:
	addiu	sp,sp,-56
	sw	ra,52(sp)
	sw	fp,48(sp)
	sw	s7,44(sp)
	sw	s6,40(sp)
	sw	s5,36(sp)
	sw	s4,32(sp)
	sw	s3,28(sp)
	sw	s2,24(sp)
	sw	s1,20(sp)
	sw	s0,16(sp)
	lui	s7,SCRATCHPAD>>16
	sh	a3,TS_lodLevel(s7)
	sw	a1,TS_tileVertices(s7)
	addu	s0,a2,zero
	lw	s2,RS_orderingTable(s7)
	lw	s4,gPacketCursor
	addi	s5,s7,TS_screenXy
	addi	s6,s7,TS_screenZ
	addi	s3,s7,TS_uv3+4
	li	fp,OT_ADDR_MASK
	addi	a0,a0,-1
.subdiv_quad_next:
	lb	t1,TS_tileFlags(s7)
	lw	t0,DQUAD_tpage(s0)
	andi	t1,t1,SCENE_TILE_FLAG_SEMI_TRANS
	sll	t1,t1,SEMI_TRANS_SHIFT
	or	t0,t0,t1
	mtc2	t0,C2_RGB
	srl	t0,t0,16
	sh	t0,TS_faceCode(s7)
	lhu	v1,DQUAD_vertices+2(s0)
	lhu	v0,DQUAD_vertices(s0)
	sll	v1,v1,16
	or	v0,v0,v1
	sw	a0,TS_faceCount(s7)
	sll	t4,v0,2
	andi	t4,t4,$ff<<2
	addu	t0,t4,s5
	lw	t0,0(t0)
	srl	t5,v0,6
	andi	t5,t5,$ff<<2
	addu	t1,t5,s5
	lw	t1,0(t1)
	srl	t6,v0,14
	andi	t6,t6,$ff<<2
	addu	t2,t6,s5
	lw	t2,0(t2)
	srl	t7,v0,22
	andi	t7,t7,$ff<<2
	addu	t3,t7,s5
	lw	t3,0(t3)
	mtc2	t0,C2_SXY0
	mtc2	t1,C2_SXY1
	mtc2	t2,C2_SXY2
	and	v1,t0,t1
	and	v1,v1,t2
	and	v1,v1,t3
	NCLIP
	bltz	v1,.subdiv_quad_skip
	nop
	andi	v1,v1,$8000
	mfc2	v0,C2_MAC0
	bne	v1,zero,.subdiv_quad_skip
	nop
	bgez	v0,.subdiv_quad_skip
	sub	v1,zero,v0
	sra	a0,t0,16
	addi	a0,a0,-SCREEN_HEIGHT
	sra	a1,t1,16
	addi	a1,a1,-SCREEN_HEIGHT
	or	a0,a0,a1
	sra	a1,t2,16
	addi	a1,a1,-SCREEN_HEIGHT
	or	a0,a0,a1
	sra	a1,t3,16
	addi	a1,a1,-SCREEN_HEIGHT
	or	a0,a0,a1
	andi	a0,a0,$8000
	beq	a0,zero,.subdiv_quad_skip
	nop
	srl	t4,t4,1
	add	a0,t4,s6
	lh	a0,0(a0)
	srl	t5,t5,1
	add	a1,t5,s6
	lh	a1,0(a1)
	srl	t6,t6,1
	add	a2,t6,s6
	lh	a2,0(a2)
	srl	t7,t7,1
	add	a3,t7,s6
	lh	a3,0(a3)
	slt	v0,a0,a1
	beq	v0,zero,.subdiv_quad_max_z2
	slt	v0,a0,a2
	addu	a0,a1,zero
	slt	v0,a0,a2
.subdiv_quad_max_z2:
	beq	v0,zero,.subdiv_quad_max_z3
	slt	v0,a0,a3
	addu	a0,a2,zero
	slt	v0,a0,a3
.subdiv_quad_max_z3:
	beq	v0,zero,.subdiv_quad_max_z
	nop
	addu	a0,a3,zero
.subdiv_quad_max_z:
	addu	v0,a0,zero
	addu	a3,v0,zero
	lbu	t8,DQUAD_flags(s0)
	nop
	andi	t8,t8,DQUAD_FLAG_DEEPER
	beq	t8,zero,.subdiv_quad_depth
	nop
	addi	v0,v0,DQUAD_DEEPER_BIAS
.subdiv_quad_depth:
	lw	a1,RS_fogFarOtz(s7)
	lw	a2,RS_stageTileMinOtz(s7)
	srl	v0,v0,2
	srl	t8,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t8
	andi	v0,v0,$fff
	slt	at,a1,v0
	bne	at,zero,.subdiv_quad_skip
	nop
	slt	at,v0,a2
	bne	at,zero,.subdiv_quad_skip
	nop
	lhu	a1,DQUAD_normalZ(s0)
	lwc2	C2_VXY0,DQUAD_normalXY(s0)
	mtc2	a1,C2_VZ0
	lh	a0,DQUAD_clut(s0)
	lh	a1,DQUAD_tpage(s0)
	sh	a0,TS_clut(s7)
	sh	a1,TS_tpage(s7)
	NCS
	addi	v1,v1,-QUAD_LEVEL1_AREA
	bltz	v1,.subdiv_quad_area_level
	ori	a0,zero,$2
	addi	v1,v1,-(QUAD_LEVEL0_AREA-QUAD_LEVEL1_AREA)
	bltz	v1,.subdiv_quad_area_level
	ori	a0,zero,$1
	ori	a0,zero,$0
.subdiv_quad_area_level:
	addu	v1,a0,zero
	lw	a1,TS_stageFlags(s7)
	nop
	andi	a1,a1,STAGE_RENDER_MIP_MODE_MASK
	beq	a1,zero,.subdiv_quad_set_level
	addu	a0,zero,zero
	addi	a2,a1,-STAGE_RENDER_MIP_MODE_AREA
	beq	a2,zero,.subdiv_quad_level
	nop
	lb	a0,TS_lodLevel(s7)
	nop
	addi	a0,a0,-2
	sub	a0,zero,a0
	addi	a2,a1,-STAGE_RENDER_MIP_MODE_LOD
	beq	a2,zero,.subdiv_quad_set_level
	nop
	lb	a0,TS_lodLevel(s7)
	nop
	sub	a0,v1,a0
	bgtz	a0,.subdiv_quad_set_level
	nop
	ori	a0,zero,$0
.subdiv_quad_set_level:
	addu	v1,a0,zero
.subdiv_quad_level:
	lhu	a0,DQUAD_uv0(s0)
	lhu	a1,DQUAD_uv1(s0)
	lhu	a2,DQUAD_uv2(s0)
	lhu	a3,DQUAD_uv3(s0)
	beq	v1,zero,.subdiv_quad_uv_done
	ori	t8,zero,$ff
	srlv	t8,t8,v1
	sll	t9,t8,8
	or	t8,t8,t9
	srlv	a0,a0,v1
	and	a0,a0,t8
	srlv	a1,a1,v1
	and	a1,a1,t8
	srlv	a2,a2,v1
	and	a2,a2,t8
	srlv	a3,a3,v1
	and	a3,a3,t8
.subdiv_quad_uv_done:
	sh	a0,TS_uv0(s7)
	sh	a1,TS_uv1(s7)
	sh	a2,TS_uv2(s7)
	sh	a3,TS_uv3(s7)
	lbu	t8,DQUAD_flags(s0)
	lb	a3,TS_lodLevel(s7)
	srl	t8,t8,1
	andi	t9,t8,$3
	sub	a3,a3,t9
	bgtz	a3,.subdiv_quad_split
	nop
	lh	t4,TS_clut(s7)
	lh	t5,TS_tpage(s7)
	sw	t0,FT_x0(s4)
	sw	t1,FT_x1(s4)
	sw	t2,FT_x2(s4)
	sw	t3,FT_x3(s4)
	sll	t4,t4,16
	sll	t5,t5,16
	lhu	t0,TS_uv0(s7)
	lhu	t1,TS_uv1(s7)
	lhu	t2,TS_uv2(s7)
	lhu	t3,TS_uv3(s7)
	or	t0,t0,t4
	or	t1,t1,t5
	sw	t0,FT_u0(s4)
	sw	t1,FT_u1(s4)
	sh	t2,FT_u2(s4)
	sh	t3,FT_u3(s4)
	swc2	C2_RGB2,FT_r0(s4)
	sll	v0,v0,2
	addu	v0,v0,s2
	lw	t1,0(v0)
	and	t0,s4,fp
	sw	t0,0(v0)
	lui	t2,FT4_TAG>>16
	or	t1,t1,t2
	sw	t1,FT_tag(s4)
	lw	a0,TS_faceCount(s7)
	addi	s0,s0,DQUAD_SIZE
	addi	s4,s4,FT4_SIZE
	bne	a0,zero,.subdiv_quad_next
	addi	a0,a0,-1
	j	.subdiv_quad_done
	nop
.subdiv_quad_split:
	lw	a0,TS_stageFlags(s7)
	nop
	andi	a0,a0,STAGE_RENDER_DEBUG_MIP_TINT
	beq	a0,zero,.subdiv_quad_no_tint
	nop
	addu	a0,a3,zero
	lw	a1,DQUAD_tpage(s0)
	nop
	li	at,MIP_TINT_WHITE
	or	a1,a1,at
	beq	a0,zero,.subdiv_quad_tint
	nop
	addiu	at,zero,QUAD_TINT_CYAN
	and	a1,a1,at
	addi	a0,a0,-1
	beq	a0,zero,.subdiv_quad_tint
	nop
	li	at,QUAD_TINT_BLUE
	and	a1,a1,at
.subdiv_quad_tint:
	mtc2	a1,C2_RGB2
	nop
	nop
.subdiv_quad_no_tint:
	sll	v0,v0,2
	addu	v0,v0,s2
	lw	s1,0(v0)
	sw	v0,TS_otEntry(s7)
	lui	t0,FT4_TAG>>16
	or	s1,s1,t0
	lw	a1,TS_tileVertices(s7)
	swc2	C2_RGB2,TS_color(s7)
	add	t0,t4,t4
	add	t4,t4,t0
	add	t4,t4,a1
	lh	t0,0(t4)
	lh	t1,2(t4)
	lh	t2,4(t4)
	lh	t3,TS_uv0(s7)
	sh	t0,SUB_v0+SV_x(s3)
	sh	t1,SUB_v0+SV_y(s3)
	sh	t2,SUB_v0+SV_z(s3)
	sh	t3,SUB_v0+SV_uv(s3)
	add	t0,t5,t5
	add	t5,t5,t0
	add	t5,t5,a1
	lh	t0,0(t5)
	lh	t1,2(t5)
	lh	t2,4(t5)
	lh	t3,TS_uv1(s7)
	sh	t0,SUB_v1+SV_x(s3)
	sh	t1,SUB_v1+SV_y(s3)
	sh	t2,SUB_v1+SV_z(s3)
	sh	t3,SUB_v1+SV_uv(s3)
	add	t0,t6,t6
	add	t6,t6,t0
	add	t6,t6,a1
	lh	t0,0(t6)
	lh	t1,2(t6)
	lh	t2,4(t6)
	lh	t3,TS_uv2(s7)
	sh	t0,SUB_v2+SV_x(s3)
	sh	t1,SUB_v2+SV_y(s3)
	sh	t2,SUB_v2+SV_z(s3)
	sh	t3,SUB_v2+SV_uv(s3)
	add	t0,t7,t7
	add	t7,t7,t0
	add	t7,t7,a1
	lh	t0,0(t7)
	lh	t1,2(t7)
	lh	t2,4(t7)
	lh	t3,TS_uv3(s7)
	sh	t0,SUB_v3+SV_x(s3)
	sh	t1,SUB_v3+SV_y(s3)
	sh	t2,SUB_v3+SV_z(s3)
	sh	t3,SUB_v3+SV_uv(s3)
	jal	SubdividePolyFt4Recursive
	addu	a0,a3,zero
	lw	t0,TS_otEntry(s7)
	and	t1,s1,fp
	sw	t1,0(t0)
	lw	a0,TS_faceCount(s7)
	addi	s0,s0,DQUAD_SIZE
	bne	a0,zero,.subdiv_quad_next
	addi	a0,a0,-1
	j	.subdiv_quad_done
	nop
.subdiv_quad_skip:
	lw	a0,TS_faceCount(s7)
	addi	s0,s0,DQUAD_SIZE
	bne	a0,zero,.subdiv_quad_next
	addi	a0,a0,-1
.subdiv_quad_done:
	opt	at+
	sw	s4,gPacketCursor
	opt	at-
	addu	v0,s0,zero
	lw	ra,52(sp)
	lw	fp,48(sp)
	lw	s7,44(sp)
	lw	s6,40(sp)
	lw	s5,36(sp)
	lw	s4,32(sp)
	lw	s3,28(sp)
	lw	s2,24(sp)
	lw	s1,20(sp)
	lw	s0,16(sp)
	jr	ra
	addiu	sp,sp,56

; SubdividePolyFt4Recursive: draws the quad of frame s3, split a0 times into four.
; In:   a0 the depth, s3 the frame; s4, s1, s7, fp as SubdividePolyFt3Recursive.
; Out:  s4 past the packets, s1 the last one's tag.
; Uses: a0..a2, t0..t5, the GTE.
SubdividePolyFt4Recursive:
	bne	a0,zero,.split_ft4
	nop
	lwc2	C2_VXY0,SUB_v0(s3)
	lwc2	C2_VZ0,SUB_v0+SV_z(s3)
	lwc2	C2_VXY1,SUB_v1(s3)
	lwc2	C2_VZ1,SUB_v1+SV_z(s3)
	lwc2	C2_VXY2,SUB_v2(s3)
	lwc2	C2_VZ2,SUB_v2+SV_z(s3)
	nop
	wRTPT
	lhu	t4,TS_clut(s7)
	lhu	t5,TS_tpage(s7)
	lhu	t0,SUB_v0+SV_uv(s3)
	lhu	t1,SUB_v1+SV_uv(s3)
	lhu	t2,SUB_v2+SV_uv(s3)
	lhu	t3,SUB_v3+SV_uv(s3)
	sll	t4,t4,16
	sll	t5,t5,16
	or	t0,t0,t4
	or	t1,t1,t5
	sw	t0,FT_u0(s4)
	sw	t1,FT_u1(s4)
	sh	t2,FT_u2(s4)
	sh	t3,FT_u3(s4)
	swc2	C2_SXY0,FT_x0(s4)
	swc2	C2_SXY1,FT_x1(s4)
	swc2	C2_SXY2,FT_x2(s4)
	lwc2	C2_VXY0,SUB_v3(s3)
	lwc2	C2_VZ0,SUB_v3+SV_z(s3)
	lw	t0,TS_color(s7)
	wRTPS
	sw	t0,FT_r0(s4)
	sw	s1,FT_tag(s4)
	swc2	C2_SXY2,FT_x3(s4)
	lui	t1,FT4_TAG>>16
	and	s1,s4,fp
	or	s1,s1,t1
	jr	ra
	addi	s4,s4,FT4_SIZE
.split_ft4:
	addi	s3,s3,SUB_FRAME_SIZE
	addi	a0,a0,-1
	sw	ra,SUB_return(s3)
	sh	a0,SUB_depth(s3)
	lw	t0,SUB_v0-SUB_FRAME_SIZE(s3)
	lw	t1,SUB_v0+SV_z-SUB_FRAME_SIZE(s3)
	sw	t0,SUB_v0(s3)
	sw	t1,SUB_v0+SV_z(s3)
	addi	a0,s3,SUB_v0-SUB_FRAME_SIZE
	addi	a1,s3,SUB_v1-SUB_FRAME_SIZE
	jal	AverageVertexAndUv
	addi	a2,s3,SUB_v1
	addi	a0,s3,SUB_v0-SUB_FRAME_SIZE
	addi	a1,s3,SUB_v2-SUB_FRAME_SIZE
	jal	AverageVertexAndUv
	addi	a2,s3,SUB_v2
	addi	a0,s3,SUB_v0-SUB_FRAME_SIZE
	addi	a1,s3,SUB_v3-SUB_FRAME_SIZE
	jal	AverageVertexAndUv
	addi	a2,s3,SUB_v3
	lh	a0,SUB_depth(s3)
	jal	SubdividePolyFt4Recursive
	nop
	lw	t0,SUB_v1-SUB_FRAME_SIZE(s3)
	lw	t1,SUB_v1+SV_z-SUB_FRAME_SIZE(s3)
	sw	t0,SUB_v0(s3)
	sw	t1,SUB_v0+SV_z(s3)
	addi	a0,s3,SUB_v1-SUB_FRAME_SIZE
	addi	a1,s3,SUB_v3-SUB_FRAME_SIZE
	jal	AverageVertexAndUv
	addi	a2,s3,SUB_v2
	lh	a0,SUB_depth(s3)
	jal	SubdividePolyFt4Recursive
	nop
	lw	t0,SUB_v3-SUB_FRAME_SIZE(s3)
	lw	t1,SUB_v3+SV_z-SUB_FRAME_SIZE(s3)
	sw	t0,SUB_v0(s3)
	sw	t1,SUB_v0+SV_z(s3)
	addi	a0,s3,SUB_v2-SUB_FRAME_SIZE
	addi	a1,s3,SUB_v3-SUB_FRAME_SIZE
	jal	AverageVertexAndUv
	addi	a2,s3,SUB_v1
	lh	a0,SUB_depth(s3)
	jal	SubdividePolyFt4Recursive
	nop
	lw	t0,SUB_v2-SUB_FRAME_SIZE(s3)
	lw	t1,SUB_v2+SV_z-SUB_FRAME_SIZE(s3)
	sw	t0,SUB_v0(s3)
	sw	t1,SUB_v0+SV_z(s3)
	addi	a0,s3,SUB_v0-SUB_FRAME_SIZE
	addi	a1,s3,SUB_v2-SUB_FRAME_SIZE
	jal	AverageVertexAndUv
	addi	a2,s3,SUB_v2
	lh	a0,SUB_depth(s3)
	jal	SubdividePolyFt4Recursive
	nop
	lw	ra,SUB_return(s3)
	nop
	jr	ra
	addi	s3,s3,-SUB_FRAME_SIZE

; AverageVertexAndUv(a, b, out): the midpoint of two subdivision corners.
; In:   a0, a1 the corners, a2 the midpoint.
; Out:  the midpoint at a2.
; Uses: t0..t5.
AverageVertexAndUv:
	lh	t0,SV_x(a0)
	lh	t1,SV_y(a0)
	lh	t2,SV_z(a0)
	lh	t3,SV_x(a1)
	lh	t4,SV_y(a1)
	lh	t5,SV_z(a1)
	add	t0,t0,t3
	add	t1,t1,t4
	add	t2,t2,t5
	sra	t0,t0,1
	sra	t1,t1,1
	sra	t2,t2,1
	sh	t0,SV_x(a2)
	sh	t1,SV_y(a2)
	sh	t2,SV_z(a2)
	lhu	t3,SV_uv(a0)
	lhu	t4,SV_uv(a1)
	andi	t0,t3,$ff00
	andi	t1,t4,$ff00
	andi	t3,t3,$ff
	andi	t4,t4,$ff
	add	t3,t3,t4
	add	t0,t0,t1
	srl	t3,t3,1
	srl	t0,t0,1
	andi	t3,t3,$ff
	andi	t0,t0,$ff00
	or	t3,t3,t0
	jr	ra
	sh	t3,SV_uv(a2)
	nop		; unreached

; RenderStageTileBatch(cursor, tile, view) -> cursor: draws the tiles queued from this one on.
; In:   a0 the gRenderQueue cursor past the tile, a1 the SceneTile, a2 the view matrix.
; Out:  v0 the cursor past the last tile drawn.
; Uses: s0, every temporary and argument register, the GTE.
RenderStageTileBatch:
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
	addu	s2,a0,zero
	addu	s1,a1,zero
	lui	s3,SCRATCHPAD>>16
	ori	s4,zero,$1
	lw	a0,gStageRenderFlags
	nop
	sw	a0,TS_stageFlags(s3)
	lw	t0,M_r11r12(a2)
	lw	t1,M_r13r21(a2)
	lw	t2,M_r22r23(a2)
	lw	t3,M_r31r32(a2)
	lh	t4,M_r33(a2)
	sw	t0,TS_viewRotation+M_r11r12(s3)
	sw	t1,TS_viewRotation+M_r13r21(s3)
	sw	t2,TS_viewRotation+M_r22r23(s3)
	sw	t3,TS_viewRotation+M_r31r32(s3)
	sh	t4,TS_viewRotation+M_r33(s3)
	addi	s4,s4,16
	srav	t0,t0,s4
	srav	t1,t1,s4
	srav	t2,t2,s4
	srav	t3,t3,s4
	addi	s4,s4,-16
	lh	t5,TS_viewRotation+M_r11r12(s3)
	lh	t6,TS_viewRotation+M_r13r21(s3)
	lh	t7,TS_viewRotation+M_r22r23(s3)
	lh	t8,TS_viewRotation+M_r31r32(s3)
	srav	t4,t4,s4
	srav	t5,t5,s4
	srav	t6,t6,s4
	srav	t7,t7,s4
	srav	t8,t8,s4
	sra	t1,t1,1
	sra	t7,t7,1
	sra	t2,t2,1
	sh	t5,TS_tileRotation+M_m0(s3)
	sh	t0,TS_tileRotation+M_m0+2(s3)
	sh	t6,TS_tileRotation+M_m0+4(s3)
	sh	t1,TS_tileRotation+M_m1(s3)
	sh	t7,TS_tileRotation+M_m1+2(s3)
	sh	t2,TS_tileRotation+M_m1+4(s3)
	sh	t8,TS_tileRotation+M_m2(s3)
	sh	t3,TS_tileRotation+M_m2+2(s3)
	sh	t4,TS_tileRotation+M_m2+4(s3)
	lw	t0,gCameraNodePtr
	nop
	lw	t1,SN_worldMatrix+M_t(t0)
	lw	t2,SN_worldMatrix+M_t+4(t0)
	lw	t3,SN_worldMatrix+M_t+8(t0)
	sw	t1,TS_cameraPosition(s3)
	sw	t2,TS_cameraPosition+4(s3)
	sw	t3,TS_cameraPosition+8(s3)
	ori	t0,zero,LIGHT_SLOT_STAGE
	la	t2,gLightSlots
	sll	t1,t0,1
	add	t1,t1,t0
	sll	t1,t1,5
	addu	t0,t1,t2
	lw	t1,LS_lightMatrix+M_r11r12(t0)
	lw	t2,LS_lightMatrix+M_r13r21(t0)
	lw	t3,LS_lightMatrix+M_r22r23(t0)
	lw	t4,LS_lightMatrix+M_r31r32(t0)
	lw	t5,LS_lightMatrix+M_r33(t0)
	ctc2	t1,C2_L11L12
	ctc2	t2,C2_L13L21
	ctc2	t3,C2_L22L23
	ctc2	t4,C2_L31L32
	ctc2	t5,C2_L33
	lw	t6,LS_ambientRgb(t0)
	lw	t1,LS_colorMatrix+M_r11r12(t0)
	lw	t2,LS_colorMatrix+M_r13r21(t0)
	lw	t3,LS_colorMatrix+M_r22r23(t0)
	lw	t4,LS_colorMatrix+M_r31r32(t0)
	lw	t5,LS_colorMatrix+M_r33(t0)
	ctc2	t1,C2_LR1LR2
	ctc2	t2,C2_LR3LG1
	ctc2	t3,C2_LG2LG3
	ctc2	t4,C2_LB1LB2
	ctc2	t5,C2_LB3
	ori	t0,zero,$ff0
	sll	t1,t6,4
	srl	t2,t6,4
	srl	t3,t6,12
	and	t1,t1,t0
	and	t2,t2,t0
	and	t3,t3,t0
	ctc2	t1,C2_RBK
	ctc2	t2,C2_GBK
	ctc2	t3,C2_BBK
.stage_tile_next:
	lw	t0,TS_cameraPosition(s3)
	lw	t1,TS_cameraPosition+4(s3)
	lw	t2,TS_cameraPosition+8(s3)
	lw	t3,ST_worldX(s1)
	addu	t4,zero,zero
	lw	t5,ST_worldZ(s1)
	sub	t3,t3,t0
	sub	t4,t4,t1
	sub	t5,t5,t2
	srav	t3,t3,s4
	srav	t4,t4,s4
	srav	t5,t5,s4
	andi	t3,t3,$ffff
	sll	t4,t4,16
	or	t3,t3,t4
	mtc2	t3,C2_VXY0
	mtc2	t5,C2_VZ0
	lw	t0,TS_viewRotation+M_r11r12(s3)
	lw	t1,TS_viewRotation+M_r13r21(s3)
	lw	t2,TS_viewRotation+M_r22r23(s3)
	lw	t3,TS_viewRotation+M_r31r32(s3)
	lw	t4,TS_viewRotation+M_r33(s3)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	nop
	wMVMVA	1,0,0,0,0
	nop
	nop
	mfc2	t0,C2_IR1
	mfc2	t1,C2_IR2
	mfc2	t2,C2_IR3
	sra	t1,t1,1
	ctc2	t0,C2_TRX
	ctc2	t1,C2_TRY
	ctc2	t2,C2_TRZ
	lw	t0,TS_tileRotation+M_r11r12(s3)
	lw	t1,TS_tileRotation+M_r13r21(s3)
	lw	t2,TS_tileRotation+M_r22r23(s3)
	lw	t3,TS_tileRotation+M_r31r32(s3)
	lw	t4,TS_tileRotation+M_r33(s3)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	lh	t0,ST_modelSlot(s1)
	lh	t1,ST_meshIndex(s1)
	sll	t0,t0,2
	la	t2,gModelTables
	addu	t0,t0,t2
	lw	t0,0(t0)
	sll	t1,t1,2
	addu	t0,t0,t1
	lw	s0,0(t0)
	lh	s7,ST_lodLevel(s1)
	nop
	lw	a1,0(s0)
	nop
	andi	a0,a1,$ffff
	srl	a1,a1,16
	beq	a0,zero,.stage_tile_done
	addi	s0,s0,4
	sw	s0,TS_vertices(s3)
	ori	t0,zero,$4
	bne	t0,a0,.tile_verts_by_three
	nop
	lhu	t6,0(s0)
	lhu	t7,2(s0)
	lhu	t8,4(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY0
	mtc2	t8,C2_VZ0
	lhu	t6,6(s0)
	lhu	t7,8(s0)
	lhu	t8,10(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY1
	mtc2	t8,C2_VZ1
	lhu	t6,12(s0)
	lhu	t7,14(s0)
	lhu	t8,16(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY2
	mtc2	t8,C2_VZ2
	lhu	t6,18(s0)
	lhu	t7,20(s0)
	RTPT
	lhu	t8,22(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	addi	s0,s0,24
	addiu	s5,s3,TS_screenXy
	addiu	s6,s3,TS_screenZ
	swc2	C2_SXY0,0(s5)
	swc2	C2_SXY1,4(s5)
	swc2	C2_SXY2,8(s5)
	mfc2	t0,C2_SZ1
	mfc2	t1,C2_SZ2
	mfc2	t2,C2_SZ3
	mtc2	t6,C2_VXY0
	mtc2	t8,C2_VZ0
	nRTPS
	sh	t0,0(s6)
	sh	t1,2(s6)
	sh	t2,4(s6)
	mfc2	t0,C2_SZ3
	swc2	C2_SXY2,12(s5)
	sh	t0,6(s6)
	b	.tile_prim_groups
	nop
.tile_verts_by_three:
	addiu	a0,a0,-3
	lhu	t6,0(s0)
	lhu	t7,2(s0)
	lhu	t8,4(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY0
	mtc2	t8,C2_VZ0
	lhu	t6,6(s0)
	lhu	t7,8(s0)
	lhu	t8,10(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY1
	mtc2	t8,C2_VZ1
	lhu	t6,12(s0)
	lhu	t7,14(s0)
	lhu	t8,16(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY2
	mtc2	t8,C2_VZ2
	addiu	s5,s3,TS_screenXy
	addiu	s6,s3,TS_screenZ
	RTPT
	j	.tile_store_three
	addiu	s0,s0,18
.tile_next_three:
	RTPT
	addiu	s6,s6,6
	sh	t0,-6(s6)
	sh	t1,-4(s6)
	sh	t2,-2(s6)
.tile_store_three:
	swc2	C2_SXY0,0(s5)
	swc2	C2_SXY1,4(s5)
	swc2	C2_SXY2,8(s5)
	mfc2	t0,C2_SZ1
	mfc2	t1,C2_SZ2
	mfc2	t2,C2_SZ3
	addiu	s5,s5,12
	blez	a0,.tile_verts_done
	nop
	addiu	a0,a0,-3
	lhu	t6,0(s0)
	lhu	t7,2(s0)
	lhu	t8,4(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY0
	mtc2	t8,C2_VZ0
	lhu	t6,6(s0)
	lhu	t7,8(s0)
	lhu	t8,10(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY1
	mtc2	t8,C2_VZ1
	lhu	t6,12(s0)
	lhu	t7,14(s0)
	lhu	t8,16(s0)
	sll	t7,t7,16
	or	t6,t6,t7
	mtc2	t6,C2_VXY2
	mtc2	t8,C2_VZ2
	j	.tile_next_three
	addiu	s0,s0,18
.tile_verts_done:
	sh	t0,0(s6)
	sh	t1,2(s6)
	sh	t2,4(s6)
	add	a0,a0,a0
	add	t0,a0,a0
	add	a0,t0,a0
	add	s0,s0,a0
	andi	t0,s0,$3
	add	s0,s0,t0
.tile_prim_groups:
	lhu	a0,0(s0)
	addiu	s0,s0,2
.tile_prim_group_next:
	lhu	t0,0(s0)
	lw	a1,TS_vertices(s3)
	addiu	s0,s0,2
	srl	t0,t0,3
	sll	t0,t0,2
	la	at,gTilePrimRenderDispatch
	addu	at,at,t0
	lw	t0,0(at)
	addu	a2,s0,zero
	jalr	ra,t0
	addu	a3,s7,zero
	addu	s0,v0,zero
	lhu	a0,0(s0)
	addiu	s0,s0,2
	bne	a0,zero,.tile_prim_group_next
	nop
	lhu	t0,0(s0)
	addiu	s0,s0,2
	beq	t0,zero,.stage_tile_done
	nop
	j	TriggerDebugTrap
	nop
.stage_tile_done:
	lw	v0,gRenderQueueCursor
	nop
	beq	v0,s2,.stage_tiles_end
	nop
	lw	s1,0(s2)
	nop
	lh	v0,ST_renderType(s1)
	nop
	ori	at,zero,SCENE_NODE_TILE
	beq	v0,at,.stage_tile_next
	addi	s2,s2,4
	addi	s2,s2,-4
.stage_tiles_end:
	addu	v0,s2,zero
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

; DrawPolyF3LitFarClip: unused; DrawPolyF3Lit to DEPTH_LIMIT_FAR_CLIP.
; In:   a0 the face count (not zero), s0 the MF3 records.
; Out:  s0 past them.
; Uses: v0, v1, a0..a2, t0..t6, t9, the GTE.
DrawPolyF3LitFarClip:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_f3_lit_far_clip_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,F3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,F3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MF3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,F_r0(s6)		; replaced before NCCS reads it
.poly_f3_lit_far_clip_next:
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
	bltz	t6,.poly_f3_lit_far_clip_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.poly_f3_lit_far_clip_skip
	nop
	bne	t6,zero,.poly_f3_lit_far_clip_skip
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
	blez	v0,.poly_f3_lit_far_clip_skip
	sll	v0,v0,2
	slti	t9,v0,DEPTH_LIMIT_FAR_CLIP
	beq	t9,zero,.poly_f3_lit_far_clip_skip
	lwc2	C2_VXY0,MF3_normal-MF3_vertices(s0)
	lwc2	C2_VZ0,MF3_normal+4-MF3_vertices(s0)
	lwc2	C2_RGB,MF3_color-MF3_vertices(s0)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCCS
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,F_tag(s6)
	swc2	C2_RGB2,F_r0(s6)
.poly_f3_lit_far_clip_skip:
	addiu	s0,s0,MF3_SIZE
	addiu	s6,s6,MF3_SIZE
	bne	s3,zero,.poly_f3_lit_far_clip_next
	addiu	s3,s3,-1
.poly_f3_lit_far_clip_done:
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

; DrawPolyF4UnlitFarClip: unused; flat quads in their packets' colour to DEPTH_LIMIT_FAR_CLIP.
; In:   a0 the face count (not zero), s0 the MF4 (no normal) records.
; Out:  s0 past them.
; Uses: v0, v1, a0..a3, t0..t9, the GTE.
DrawPolyF4UnlitFarClip:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_f4_unlit_far_clip_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,F4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,F4_SIZE
	and	t1,t0,t1
	add	s6,s0,t1
	addi	s0,s0,MF4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_f4_unlit_far_clip_next:
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
	bltz	t8,.poly_f4_unlit_far_clip_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.poly_f4_unlit_far_clip_skip
	nop
	bne	t8,zero,.poly_f4_unlit_far_clip_skip
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
	blez	v0,.poly_f4_unlit_far_clip_skip
	slti	t9,v0,DEPTH_LIMIT_FAR_CLIP
	beq	t9,zero,.poly_f4_unlit_far_clip_skip
	nop
	sw	t0,F_x0(s6)
	sw	t1,F_x1(s6)
	sw	t2,F_x2(s6)
	sw	t7,F_x3(s6)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,F_tag(s6)
.poly_f4_unlit_far_clip_skip:
	addiu	s0,s0,MF4_NO_NORMAL_SIZE
	addiu	s6,s6,MF4_NO_NORMAL_SIZE
	bne	s3,zero,.poly_f4_unlit_far_clip_next
	addiu	s3,s3,-1
.poly_f4_unlit_far_clip_done:
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

; DrawClothPolyFt3DoubleLit (cloth entry 0): DrawPolyFt3DoubleLit, depth biased by side.
; In:   a0 the face count (not zero), a1 the MFT3 records.
; Out:  s0 and v0 past them.
; Uses: at, v0, v1, a0..a3, t0..t6, t9, the GTE.
DrawClothPolyFt3DoubleLit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.cloth_poly_ft3_double_lit_done
	addu	s0,a1,zero
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT3_TAG
	li	fp,OT_ADDR_MASK
	lw	v1,SCRATCHPAD+RS_scaleShift
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,FT3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MFT3_vertices
	lwc2	C2_RGB,FT_r0(s6)
.cloth_poly_ft3_double_lit_next:
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
	bltz	t6,.cloth_poly_ft3_double_lit_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bne	t6,zero,.cloth_poly_ft3_double_lit_skip
	slt	a3,v0,zero
	addi	a3,a3,-1
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
	blez	v0,.cloth_poly_ft3_double_lit_skip
	sll	v0,v0,2
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.cloth_poly_ft3_double_lit_skip
	nop
	lw	t0,MFT3_normal-MFT3_vertices(s0)
	lw	t1,MFT3_normal+4-MFT3_vertices(s0)
	xor	t0,t0,a3
	mtc2	t0,C2_VXY0
	xor	t1,t1,a3
	mtc2	t1,C2_VZ0
	addiu	at,zero,-CLOTH_SIDE_DEPTH_BIAS
	xor	t0,a3,at
	add	v0,v0,t0
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCS
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	swc2	C2_RGB2,FT_r0(s6)
.cloth_poly_ft3_double_lit_skip:
	addiu	s0,s0,MFT3_SIZE
	addiu	s6,s6,MFT3_SIZE
	bne	s3,zero,.cloth_poly_ft3_double_lit_next
	addiu	s3,s3,-1
.cloth_poly_ft3_double_lit_done:
	addi	s0,s0,-MFT3_vertices
	addu	v0,s0,zero
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

; DrawClothPolyFt4DoubleLit (cloth entry 1): the quads.
; In:   a0 the face count (not zero), a1 the MFT4 records.
; Out:  s0 and v0 past them.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawClothPolyFt4DoubleLit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.cloth_poly_ft4_double_lit_done
	addu	s0,a1,zero
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,FT4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MFT4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,FT_r0(s6)
.cloth_poly_ft4_double_lit_next:
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
	bltz	t8,.cloth_poly_ft4_double_lit_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bne	t8,zero,.cloth_poly_ft4_double_lit_skip
	slt	a3,v0,zero
	addi	a3,a3,-1
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
	lh	t3,0(v0)
	add	a0,a0,a1
	add	a0,a0,a2
	add	a0,a0,t3
	sra	v0,a0,2
	blez	v0,.cloth_poly_ft4_double_lit_skip
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.cloth_poly_ft4_double_lit_skip
	nop
	sw	t0,FT_x0(s6)
	sw	t1,FT_x1(s6)
	sw	t2,FT_x2(s6)
	sw	t7,FT_x3(s6)
	lw	t0,MFT4_normal-MFT4_vertices(s0)
	lw	t1,MFT4_normal+4-MFT4_vertices(s0)
	xor	t0,t0,a3
	mtc2	t0,C2_VXY0
	xor	t1,t1,a3
	mtc2	t1,C2_VZ0
	addiu	at,zero,-CLOTH_SIDE_DEPTH_BIAS
	xor	t0,a3,at
	add	v0,v0,t0
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCS
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	swc2	C2_RGB2,FT_r0(s6)
.cloth_poly_ft4_double_lit_skip:
	addiu	s0,s0,MFT4_SIZE
	addiu	s6,s6,MFT4_SIZE
	bne	s3,zero,.cloth_poly_ft4_double_lit_next
	addiu	s3,s3,-1
.cloth_poly_ft4_double_lit_done:
	addi	s0,s0,-MFT4_vertices
	addu	v0,s0,zero
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

; DrawPolyF3Lit (type 0): flat triangles lit by NCCS.
; In:   a0 the face count (not zero), s0 the MF3 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a2, t0..t6, t9, the GTE.
DrawPolyF3Lit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_f3_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,F3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,F3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MF3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,F_r0(s6)		; replaced before NCCS reads it
.poly_f3_lit_next:
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
	bltz	t6,.poly_f3_lit_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.poly_f3_lit_skip
	nop
	bne	t6,zero,.poly_f3_lit_skip
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
	blez	v0,.poly_f3_lit_skip
	sll	v0,v0,2
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_f3_lit_skip
	nop
	lwc2	C2_VXY0,MF3_normal-MF3_vertices(s0)
	lwc2	C2_VZ0,MF3_normal+4-MF3_vertices(s0)
	lwc2	C2_RGB,MF3_color-MF3_vertices(s0)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCCS
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,F_tag(s6)
	swc2	C2_RGB2,F_r0(s6)
.poly_f3_lit_skip:
	addiu	s0,s0,MF3_SIZE
	addiu	s6,s6,MF3_SIZE
	bne	s3,zero,.poly_f3_lit_next
	addiu	s3,s3,-1
.poly_f3_lit_done:
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

; DrawPolyF4Lit (type 8): flat quads lit by NCCS.
; In:   a0 the face count (not zero), s0 the MF4 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawPolyF4Lit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_f4_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,F4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,F4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MF4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,F_r0(s6)		; replaced before NCCS reads it
.poly_f4_lit_next:
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
	bltz	t8,.poly_f4_lit_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.poly_f4_lit_skip
	nop
	bne	t8,zero,.poly_f4_lit_skip
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
	blez	v0,.poly_f4_lit_skip
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_f4_lit_skip
	nop
	sw	t0,F_x0(s6)
	sw	t1,F_x1(s6)
	sw	t2,F_x2(s6)
	sw	t7,F_x3(s6)
	lwc2	C2_VXY0,MF4_normal-MF4_vertices(s0)
	lwc2	C2_VZ0,MF4_normal+4-MF4_vertices(s0)
	lwc2	C2_RGB,MF4_color-MF4_vertices(s0)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCCS
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,F_tag(s6)
	swc2	C2_RGB2,F_r0(s6)
.poly_f4_lit_skip:
	addiu	s0,s0,MF4_SIZE
	addiu	s6,s6,MF4_SIZE
	bne	s3,zero,.poly_f4_lit_next
	addiu	s3,s3,-1
.poly_f4_lit_done:
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

; ClothPoint (cloth.h).
		rsreset
CP_x			rw	1
CP_y			rw	1
CP_z			rw	1
CLOTH_POINT_SIZE	equ	64
CLOTH_POSITION_SHIFT	equ	7

; RenderClothSoftbodyNode(node, shift) -> node: draws a cloth's mesh at its simulated points.
; In:   a0 the SceneNode, its mesh holding a primitive group (the first count is not tested), a1 its coordinate shift.
; Out:  v0 the node, s0 past the mesh's primitive groups.
; Uses: every temporary and argument register, the GTE.
RenderClothSoftbodyNode:
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
	addu	s1,a0,zero
	addu	s5,a1,zero
	lh	t0,SN_modelSlot(s1)
	lh	t1,SN_meshIndex(s1)
	sll	t0,t0,2
	la	t2,gModelTables
	addu	t0,t0,t2
	lw	t0,0(t0)
	sll	t1,t1,2
	addu	t0,t0,t1
	lw	s0,0(t0)
	nop
	lw	a1,0(s0)
	nop
	andi	a0,a1,$ffff
	srl	a1,a1,16
	beq	a0,zero,.cloth_node_done
	addi	s0,s0,4
	add	t0,a0,a0
	add	t0,t0,a0
	sll	t0,t0,3
	add	s0,s0,t0
	lw	s2,SN_clothWorkOrFade(s1)
	lw	v0,gCameraNodePtr
	addiu	a0,a0,-1
	lw	t3,SN_worldMatrix+M_t(v0)
	lw	t4,SN_worldMatrix+M_t+4(v0)
	lw	t5,SN_worldMatrix+M_t+8(v0)
	lw	t0,CP_x(s2)
	lw	t1,CP_y(s2)
	lw	t2,CP_z(s2)
	sra	t0,t0,CLOTH_POSITION_SHIFT
	sra	t1,t1,CLOTH_POSITION_SHIFT
	sra	t2,t2,CLOTH_POSITION_SHIFT
	sub	t0,t0,t3
	sub	t1,t1,t4
	sub	t2,t2,t5
	srav	t0,t0,s5
	srav	t1,t1,s5
	srav	t2,t2,s5
	sll	t1,t1,16
	andi	t0,t0,$ffff
	or	t0,t0,t1
	mtc2	t0,C2_VXY0
	mtc2	t2,C2_VZ0
	lw	t0,CLOTH_POINT_SIZE+CP_x(s2)
	lw	t1,CLOTH_POINT_SIZE+CP_y(s2)
	lw	t2,CLOTH_POINT_SIZE+CP_z(s2)
	sra	t0,t0,CLOTH_POSITION_SHIFT
	sra	t1,t1,CLOTH_POSITION_SHIFT
	sra	t2,t2,CLOTH_POSITION_SHIFT
	sub	t0,t0,t3
	sub	t1,t1,t4
	sub	t2,t2,t5
	srav	t0,t0,s5
	srav	t1,t1,s5
	srav	t2,t2,s5
	sll	t1,t1,16
	andi	t0,t0,$ffff
	or	t0,t0,t1
	mtc2	t0,C2_VXY1
	mtc2	t2,C2_VZ1
	lw	t0,2*CLOTH_POINT_SIZE+CP_x(s2)
	lw	t1,2*CLOTH_POINT_SIZE+CP_y(s2)
	lw	t2,2*CLOTH_POINT_SIZE+CP_z(s2)
	sra	t0,t0,CLOTH_POSITION_SHIFT
	sra	t1,t1,CLOTH_POSITION_SHIFT
	sra	t2,t2,CLOTH_POSITION_SHIFT
	sub	t0,t0,t3
	sub	t1,t1,t4
	sub	t2,t2,t5
	srav	t0,t0,s5
	srav	t1,t1,s5
	srav	t2,t2,s5
	sll	t1,t1,16
	andi	t0,t0,$ffff
	or	t0,t0,t1
	mtc2	t0,C2_VXY2
	mtc2	t2,C2_VZ2
	li	s3,SCRATCHPAD+RS_screenXy
	li	s4,SCRATCHPAD+RS_screenZ
	RTPT
	j	.cloth_store_three
	addi	s2,s2,3*CLOTH_POINT_SIZE
.cloth_next_three:
	RTPT
	addiu	s4,s4,6
	sh	t6,-6(s4)
	sh	t7,-4(s4)
	sh	t8,-2(s4)
.cloth_store_three:
	swc2	C2_SXY0,0(s3)
	swc2	C2_SXY1,4(s3)
	swc2	C2_SXY2,8(s3)
	mfc2	t6,C2_SZ1
	mfc2	t7,C2_SZ2
	mfc2	t8,C2_SZ3
	addiu	s3,s3,12
	beq	a0,zero,.cloth_verts_done
	addiu	a0,a0,-1
	lw	t0,CP_x(s2)
	lw	t1,CP_y(s2)
	lw	t2,CP_z(s2)
	sra	t0,t0,CLOTH_POSITION_SHIFT
	sra	t1,t1,CLOTH_POSITION_SHIFT
	sra	t2,t2,CLOTH_POSITION_SHIFT
	sub	t0,t0,t3
	sub	t1,t1,t4
	sub	t2,t2,t5
	srav	t0,t0,s5
	srav	t1,t1,s5
	srav	t2,t2,s5
	sll	t1,t1,16
	andi	t0,t0,$ffff
	or	t0,t0,t1
	mtc2	t0,C2_VXY0
	mtc2	t2,C2_VZ0
	lw	t0,CLOTH_POINT_SIZE+CP_x(s2)
	lw	t1,CLOTH_POINT_SIZE+CP_y(s2)
	lw	t2,CLOTH_POINT_SIZE+CP_z(s2)
	sra	t0,t0,CLOTH_POSITION_SHIFT
	sra	t1,t1,CLOTH_POSITION_SHIFT
	sra	t2,t2,CLOTH_POSITION_SHIFT
	sub	t0,t0,t3
	sub	t1,t1,t4
	sub	t2,t2,t5
	srav	t0,t0,s5
	srav	t1,t1,s5
	srav	t2,t2,s5
	sll	t1,t1,16
	andi	t0,t0,$ffff
	or	t0,t0,t1
	mtc2	t0,C2_VXY1
	mtc2	t2,C2_VZ1
	lw	t0,2*CLOTH_POINT_SIZE+CP_x(s2)
	lw	t1,2*CLOTH_POINT_SIZE+CP_y(s2)
	lw	t2,2*CLOTH_POINT_SIZE+CP_z(s2)
	sra	t0,t0,CLOTH_POSITION_SHIFT
	sra	t1,t1,CLOTH_POSITION_SHIFT
	sra	t2,t2,CLOTH_POSITION_SHIFT
	sub	t0,t0,t3
	sub	t1,t1,t4
	sub	t2,t2,t5
	srav	t0,t0,s5
	srav	t1,t1,s5
	srav	t2,t2,s5
	sll	t1,t1,16
	andi	t0,t0,$ffff
	or	t0,t0,t1
	mtc2	t0,C2_VXY2
	mtc2	t2,C2_VZ2
	j	.cloth_next_three
	addiu	s2,s2,3*CLOTH_POINT_SIZE
.cloth_verts_done:
	sh	t6,0(s4)
	sh	t7,2(s4)
	sh	t8,4(s4)
	ori	t0,zero,$3
	subu	t0,t0,s5
	lui	at,SCRATCHPAD>>16
	sw	t0,RS_scaleShift(at)
	lhu	a0,0(s0)
	addiu	s0,s0,2
.cloth_prim_group_next:
	lhu	t0,0(s0)
	addiu	s0,s0,2
	srl	t0,t0,3
	sll	t0,t0,2
	la	at,gClothPrimRenderDispatch
	addu	at,at,t0
	lw	t0,0(at)
	addu	a1,s0,zero
	jalr	ra,t0
	nop
	lhu	a0,0(s0)
	addiu	s0,s0,2
	bne	a0,zero,.cloth_prim_group_next
	nop
.cloth_node_done:
	addu	v0,s1,zero
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

; DrawPolyFt3Unlit (type 5): textured triangles in their packets' colour.
; In:   a0 the face count (not zero), s0 the MFT3 (no normal) records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a2, t0..t6, t9, the GTE.
DrawPolyFt3Unlit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_ft3_unlit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,FT3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MFT3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_ft3_unlit_next:
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
	bltz	t6,.poly_ft3_unlit_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bne	t6,zero,.poly_ft3_unlit_skip
	nop
	bgez	v0,.poly_ft3_unlit_skip
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
	blez	v0,.poly_ft3_unlit_skip
	sll	v0,v0,2
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_ft3_unlit_skip
	nop
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	nop
.poly_ft3_unlit_skip:
	addiu	s0,s0,MFT3_NO_NORMAL_SIZE
	addiu	s6,s6,MFT3_NO_NORMAL_SIZE
	bne	s3,zero,.poly_ft3_unlit_next
	addiu	s3,s3,-1
.poly_ft3_unlit_done:
	addi	s0,s0,-MFT3_vertices
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

; DrawPolyFt4Unlit (type 13): textured quads in their packets' colour.
; In:   a0 the face count (not zero), s0 the MFT4 (no normal) records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawPolyFt4Unlit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_ft4_unlit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,FT4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MFT4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_ft4_unlit_next:
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
	and	t8,t8,t3		; t3, not t7: vertex 0's index times 4, so neither reject below can fire
	NCLIP
	bltz	t8,.poly_ft4_unlit_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bne	t8,zero,.poly_ft4_unlit_skip
	nop
	bgez	v0,.poly_ft4_unlit_skip
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
	blez	v0,.poly_ft4_unlit_skip
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_ft4_unlit_skip
	nop
	sw	t0,FT_x0(s6)
	sw	t1,FT_x1(s6)
	sw	t2,FT_x2(s6)
	sw	t7,FT_x3(s6)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	nop
.poly_ft4_unlit_skip:
	addiu	s0,s0,MFT4_NO_NORMAL_SIZE
	addiu	s6,s6,MFT4_NO_NORMAL_SIZE
	bne	s3,zero,.poly_ft4_unlit_next
	addiu	s3,s3,-1
.poly_ft4_unlit_done:
	addi	s0,s0,-MFT4_vertices
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

; DrawPolyFt3Lit (type 4): textured triangles lit by NCS.
; In:   a0 the face count (not zero), s0 the MFT3 records.
; Out:  s0 past them.
; Uses: v0, v1, a0..a2, t0..t6, the GTE.
DrawPolyFt3Lit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_ft3_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,FT3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MFT3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_ft3_lit_next:
	lw	a0,0(s0)
	lwc2	C2_RGB,FT_r0(s6)
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
	bltz	t6,.poly_ft3_lit_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.poly_ft3_lit_skip
	nop
	bne	t6,zero,.poly_ft3_lit_skip
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
	blez	v0,.poly_ft3_lit_skip
	sll	v0,v0,2
	lwc2	C2_VXY0,MFT3_normal-MFT3_vertices(s0)
	lwc2	C2_VZ0,MFT3_normal+4-MFT3_vertices(s0)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	NCS
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	swc2	C2_RGB2,FT_r0(s6)
.poly_ft3_lit_skip:
	addiu	s0,s0,MFT3_SIZE
	addiu	s6,s6,MFT3_SIZE
	bne	s3,zero,.poly_ft3_lit_next
	addiu	s3,s3,-1
.poly_ft3_lit_done:
	addi	s0,s0,-MFT3_vertices
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

; DrawPolyFt4Lit (type 12): textured quads lit by NCS.
; In:   a0 the face count (not zero), s0 the MFT4 records.
; Out:  s0 past them.
; Uses: v0, v1, a0..a3, t0..t8, the GTE.
DrawPolyFt4Lit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_ft4_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,FT4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MFT4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_ft4_lit_next:
	lw	a0,0(s0)
	lwc2	C2_RGB,FT_r0(s6)
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
	bltz	t8,.poly_ft4_lit_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.poly_ft4_lit_skip
	nop
	bne	t8,zero,.poly_ft4_lit_skip
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
	blez	v0,.poly_ft4_lit_skip
	nop
	sw	t0,FT_x0(s6)
	sw	t1,FT_x1(s6)
	sw	t2,FT_x2(s6)
	sw	t7,FT_x3(s6)
	lwc2	C2_VXY0,MFT4_normal-MFT4_vertices(s0)
	lwc2	C2_VZ0,MFT4_normal+4-MFT4_vertices(s0)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	NCS
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	swc2	C2_RGB2,FT_r0(s6)
.poly_ft4_lit_skip:
	addiu	s0,s0,MFT4_SIZE
	addiu	s6,s6,MFT4_SIZE
	bne	s3,zero,.poly_ft4_lit_next
	addiu	s3,s3,-1
.poly_ft4_lit_done:
	addi	s0,s0,-MFT4_vertices
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

; DrawPolyG3Lit (type 16, skinned too): Gouraud triangles lit by NCCT from the first colour.
; In:   a0 the face count (not zero), s0 the MG3 records.
; Out:  s0 past them.
; Uses: v0, v1, a0..a2, t0..t6, t9, the GTE.
DrawPolyG3Lit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_g3_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,G3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,G3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MG3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_g3_lit_next:
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
	bltz	t6,.poly_g3_lit_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.poly_g3_lit_skip
	nop
	bne	t6,zero,.poly_g3_lit_skip
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
	sw	t0,G_x0(s6)
	wAVSZ3
	sw	t1,G_x1(s6)
	mfc2	v0,C2_OTZ
	sw	t2,G_x2(s6)
	blez	v0,.poly_g3_lit_skip
	sll	v0,v0,2
	slti	t9,v0,DEPTH_LIMIT_G
	beq	t9,zero,.poly_g3_lit_skip
	nop
	lwc2	C2_VXY0,MG3_normal0-MG3_vertices(s0)
	lwc2	C2_VZ0,MG3_normal0+4-MG3_vertices(s0)
	lwc2	C2_VXY1,MG3_normal1-MG3_vertices(s0)
	lwc2	C2_VZ1,MG3_normal1+4-MG3_vertices(s0)
	lwc2	C2_VXY2,MG3_normal2-MG3_vertices(s0)
	lwc2	C2_VZ2,MG3_normal2+4-MG3_vertices(s0)
	lwc2	C2_RGB,MG3_color0-MG3_vertices(s0)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCCT
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,G_tag(s6)
	swc2	C2_RGB0,G_r0(s6)
	swc2	C2_RGB1,G_r1(s6)
	swc2	C2_RGB2,G_r2(s6)
.poly_g3_lit_skip:
	addiu	s0,s0,MG3_SIZE
	addiu	s6,s6,MG3_SIZE
	bne	s3,zero,.poly_g3_lit_next
	addiu	s3,s3,-1
.poly_g3_lit_done:
	addi	s0,s0,-MG3_vertices
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

; DrawPolyG4Lit (type 24, skinned too): Gouraud quads lit from the first colour.
; In:   a0 the face count (not zero), s0 the MG4 records.
; Out:  s0 past them.
; Uses: v0, v1, a0..a3, t0..t9, the GTE.
DrawPolyG4Lit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_g4_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,G4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,G4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MG4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_g4_lit_next:
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
	bltz	t8,.poly_g4_lit_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.poly_g4_lit_skip
	nop
	bne	t8,zero,.poly_g4_lit_skip
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
	blez	v0,.poly_g4_lit_skip
	slti	t9,v0,DEPTH_LIMIT_G
	beq	t9,zero,.poly_g4_lit_skip
	nop
	sw	t0,G_x0(s6)
	sw	t1,G_x1(s6)
	sw	t2,G_x2(s6)
	sw	t7,G_x3(s6)
	lwc2	C2_VXY0,MG4_normal0-MG4_vertices(s0)
	lwc2	C2_VZ0,MG4_normal0+4-MG4_vertices(s0)
	lwc2	C2_RGB,MG4_color0-MG4_vertices(s0)
	nNCCS
	nop
	nop
	swc2	C2_RGB2,G_r0(s6)
	lwc2	C2_VXY0,MG4_normal1-MG4_vertices(s0)
	lwc2	C2_VZ0,MG4_normal1+4-MG4_vertices(s0)
	lwc2	C2_VXY1,MG4_normal2-MG4_vertices(s0)
	lwc2	C2_VZ1,MG4_normal2+4-MG4_vertices(s0)
	lwc2	C2_VXY2,MG4_normal3-MG4_vertices(s0)
	lwc2	C2_VZ2,MG4_normal3+4-MG4_vertices(s0)
	lwc2	C2_RGB,MG4_color0-MG4_vertices(s0)	; no effect: C2_RGB already holds it
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCCT
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,G_tag(s6)
	swc2	C2_RGB0,G_r1(s6)
	swc2	C2_RGB1,G_r2(s6)
	swc2	C2_RGB2,G_r3(s6)
.poly_g4_lit_skip:
	addiu	s0,s0,MG4_SIZE
	addiu	s6,s6,MG4_SIZE
	bne	s3,zero,.poly_g4_lit_next
	addiu	s3,s3,-1
.poly_g4_lit_done:
	addi	s0,s0,-MG4_vertices
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

; DrawPolyGt3Lit (type 20): Gouraud textured triangles lit by NCT.
; In:   a0 the face count (not zero), s0 the MGT3 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a2, t0..t6, t9, the GTE.
DrawPolyGt3Lit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_gt3_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,GT3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,GT3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MGT3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,GT_r0(s6)
.poly_gt3_lit_next:
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
	bltz	t6,.poly_gt3_lit_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.poly_gt3_lit_skip
	nop
	bne	t6,zero,.poly_gt3_lit_skip
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
	sw	t0,GT_x0(s6)
	wAVSZ3
	sw	t1,GT_x1(s6)
	mfc2	v0,C2_OTZ
	sw	t2,GT_x2(s6)
	blez	v0,.poly_gt3_lit_skip
	sll	v0,v0,2
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_gt3_lit_skip
	nop
	lwc2	C2_VXY0,MGT3_normalXy0-MGT3_vertices(s0)
	lw	t0,MGT3_normalZ0-MGT3_vertices(s0)
	lwc2	C2_VXY1,MGT3_normalXy1-MGT3_vertices(s0)
	mtc2	t0,C2_VZ0
	lwc2	C2_VXY2,MGT3_normalXy2-MGT3_vertices(s0)
	lwc2	C2_VZ2,MGT3_normalZ2-MGT3_vertices(s0)
	srl	t0,t0,16
	mtc2	t0,C2_VZ1
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCT
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,GT_tag(s6)
	swc2	C2_RGB0,GT_r0(s6)
	swc2	C2_RGB1,GT_r1(s6)
	swc2	C2_RGB2,GT_r2(s6)
.poly_gt3_lit_skip:
	addiu	s0,s0,MGT3_SIZE
	addiu	s6,s6,MGT3_SIZE
	bne	s3,zero,.poly_gt3_lit_next
	addiu	s3,s3,-1
.poly_gt3_lit_done:
	addi	s0,s0,-MGT3_vertices
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

; DrawPolyGt4Lit (type 28): Gouraud textured quads lit by NCS and NCT.
; In:   a0 the face count (not zero), s0 the MGT4 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawPolyGt4Lit:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_gt4_lit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,GT4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,GT4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MGT4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,GT_r0(s6)
.poly_gt4_lit_next:
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
	bltz	t8,.poly_gt4_lit_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.poly_gt4_lit_skip
	nop
	bne	t8,zero,.poly_gt4_lit_skip
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
	blez	v0,.poly_gt4_lit_skip
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_gt4_lit_skip
	nop
	sw	t0,GT_x0(s6)
	sw	t1,GT_x1(s6)
	sw	t2,GT_x2(s6)
	sw	t7,GT_x3(s6)
	lw	t9,MGT4_normalZ0-MGT4_vertices(s0)
	lwc2	C2_VXY0,MGT4_normalXy0-MGT4_vertices(s0)
	mtc2	t9,C2_VZ0
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	NCS
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	swc2	C2_RGB2,GT_r0(s6)
	lwc2	C2_VXY0,MGT4_normalXy1-MGT4_vertices(s0)
	srl	t9,t9,16
	mtc2	t9,C2_VZ0
	lwc2	C2_VXY1,MGT4_normalXy2-MGT4_vertices(s0)
	lw	t9,MGT4_normalZ2-MGT4_vertices(s0)
	lwc2	C2_VXY2,MGT4_normalXy3-MGT4_vertices(s0)
	mtc2	t9,C2_VZ1
	srl	t8,t9,16
	mtc2	t8,C2_VZ2
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	NCT
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,GT_tag(s6)
	swc2	C2_RGB0,GT_r1(s6)
	swc2	C2_RGB1,GT_r2(s6)
	swc2	C2_RGB2,GT_r3(s6)
.poly_gt4_lit_skip:
	addiu	s0,s0,MGT4_SIZE
	addiu	s6,s6,MGT4_SIZE
	bne	s3,zero,.poly_gt4_lit_next
	addiu	s3,s3,-1
.poly_gt4_lit_done:
	addi	s0,s0,-MGT4_vertices
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

; RenderSkinnedSkeletalMesh's stack frame.
		rsset	16
SKIN_boneRotation	rh	9
			rh	1
SKIN_viewBone		rb	MATRIX_SIZE
			rh	4
SKIN_offset		rw	3
SKIN_translation	rw	3
SKIN_BONES		equ	13

; RenderSkinnedSkeletalMesh(bones, mesh, view, shift) -> past the mesh: a skinned mesh.
; In:   a0 the bone matrices, a1 the mesh, a2 the view matrix, a3 the shift.
; Out:  v0 past the mesh.
; Uses: every temporary and argument register, the GTE.
RenderSkinnedSkeletalMesh:
	addiu	sp,sp,-140
	sw	ra,136(sp)
	sw	fp,132(sp)
	sw	s7,128(sp)
	sw	s6,124(sp)
	sw	s5,120(sp)
	sw	s4,116(sp)
	sw	s3,112(sp)
	sw	s2,108(sp)
	sw	s1,104(sp)
	sw	s0,100(sp)
	addu	fp,a0,zero
	addu	s0,a1,zero
	addu	s7,a2,zero
	addu	s1,a3,zero
	ori	s5,zero,SKIN_BONES-1
	li	s6,SCRATCHPAD+RS_screenXy
	addu	s3,s6,zero
	addi	s4,s6,RS_screenZ-RS_screenXy
	addi	a0,s0,2
	jal	PrelightSkinnedMeshNormals
	addu	a1,fp,zero
	addi	s0,v0,-2
.skin_next_bone:
	lw	a0,gCameraNodePtr
	lw	s2,0(fp)
	lw	t0,SN_worldMatrix+M_t(a0)
	lw	t1,SN_worldMatrix+M_t+4(a0)
	lw	t2,SN_worldMatrix+M_t+8(a0)
	lw	t3,M_t(s2)
	lw	t4,M_t+4(s2)
	lw	t5,M_t+8(s2)
	sub	t3,t3,t0
	sub	t4,t4,t1
	sub	t5,t5,t2
	srav	t3,t3,s1
	srav	t4,t4,s1
	srav	t5,t5,s1
	sw	t3,SKIN_offset(sp)
	sw	t4,SKIN_offset+4(sp)
	sw	t5,SKIN_offset+8(sp)
	addi	v0,sp,SKIN_boneRotation
	lh	t0,M_m0(s2)
	lh	t1,M_m0+2(s2)
	lh	t2,M_m0+4(s2)
	lh	t3,M_m1(s2)
	lh	t4,M_m1+2(s2)
	lh	t5,M_m1+4(s2)
	lh	t6,M_m2(s2)
	lh	t7,M_m2+2(s2)
	lh	t8,M_m2+4(s2)
	srav	t0,t0,s1
	srav	t1,t1,s1
	srav	t2,t2,s1
	srav	t3,t3,s1
	srav	t4,t4,s1
	srav	t5,t5,s1
	srav	t6,t6,s1
	srav	t7,t7,s1
	srav	t8,t8,s1
	sh	t0,M_m0(v0)
	sh	t1,M_m0+2(v0)
	sh	t2,M_m0+4(v0)
	sh	t3,M_m1(v0)
	sh	t4,M_m1+2(v0)
	sh	t5,M_m1+4(v0)
	sh	t6,M_m2(v0)
	sh	t7,M_m2+2(v0)
	sh	t8,M_m2+4(v0)
	addu	s2,v0,zero
	addi	fp,fp,4
	addu	a0,s7,zero
	addu	a1,s2,zero
	jal	MulMatrix3x3Fast
	addi	a2,sp,SKIN_viewBone
	lw	t0,M_r11r12(s7)
	lw	t1,M_r13r21(s7)
	lw	t2,M_r22r23(s7)
	lw	t3,M_r31r32(s7)
	lw	t4,M_r33(s7)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	lw	t0,SKIN_offset(sp)
	lw	t1,SKIN_offset+4(sp)
	lw	t2,SKIN_offset+8(sp)
	andi	t0,t0,$ffff
	sll	t1,t1,16
	or	t1,t1,t0
	mtc2	t1,C2_VXY0
	mtc2	t2,C2_VZ0
	nMVMVA	1,0,0,0,0
	nop
	nop
	swc2	C2_MAC1,SKIN_translation(sp)
	swc2	C2_MAC2,SKIN_translation+4(sp)
	swc2	C2_MAC3,SKIN_translation+8(sp)
	nop
	addi	s0,s0,2
	lh	v1,0(s0)
	nop
	beq	v1,zero,.skin_from_kept_offset_list
	addi	s0,s0,2
	addi	v1,v1,-1
	lw	t0,M_r11r12(s2)
	lw	t1,M_r13r21(s2)
	lw	t2,M_r22r23(s2)
	lw	t3,M_r31r32(s2)
	lw	t4,M_r33(s2)
	lw	t5,SKIN_offset(sp)
	lw	t6,SKIN_offset+4(sp)
	lw	t7,SKIN_offset+8(sp)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	t5,C2_TRX
	ctc2	t6,C2_TRY
	ctc2	t7,C2_TRZ
.skin_keep_offset_loop:
	lhu	t0,SKVO_x(s0)
	lh	t1,SKVO_y(s0)
	lh	t2,SKVO_z(s0)
	sll	t1,t1,16
	or	t1,t1,t0
	mtc2	t1,C2_VXY0
	mtc2	t2,C2_VZ0
	lh	t0,SKVO_dx(s0)
	wMVMVA	1,0,0,0,0
	lh	t1,SKVO_dy(s0)
	lh	t2,SKVO_dz(s0)
	addi	s0,s0,SKVO_SIZE
	addi	s3,s3,4
	addi	s4,s4,2
	mfc2	t3,C2_MAC1
	mfc2	t4,C2_MAC2
	mfc2	t5,C2_MAC3
	add	t0,t0,t3
	add	t1,t1,t4
	add	t2,t2,t5
	sh	t0,-4(s3)
	sh	t1,-2(s3)
	sh	t2,-2(s4)
	bne	v1,zero,.skin_keep_offset_loop
	addi	v1,v1,-1
.skin_from_kept_offset_list:
	lh	v1,0(s0)
	nop
	beq	v1,zero,.skin_keep_list
	addi	s0,s0,2
	addi	v1,v1,-1
.skin_from_kept_offset_loop:
	lhu	t0,SKFO_x(s0)
	lh	t1,SKFO_y(s0)
	lh	t2,SKFO_z(s0)
	sll	t1,t1,16
	or	t1,t1,t0
	mtc2	t1,C2_VXY0
	mtc2	t2,C2_VZ0
	lh	t0,SKFO_base(s0)
	lh	t3,SKFO_dx(s0)
	lh	t4,SKFO_dy(s0)
	lh	t5,SKFO_dz(s0)
	add	t0,t0,t0
	addi	t9,s6,RS_screenZ-RS_screenXy
	add	t9,t9,t0
	add	t0,t0,t0
	add	t8,s6,t0
	lh	t0,0(t8)
	lh	t1,2(t8)
	lh	t2,0(t9)
	add	t0,t0,t3
	add	t1,t1,t4
	add	t2,t2,t5
	lw	t3,M_r11r12(s2)
	lw	t4,M_r13r21(s2)
	lw	t5,M_r22r23(s2)
	lw	t6,M_r31r32(s2)
	lw	t7,M_r33(s2)
	ctc2	t3,C2_R11R12
	ctc2	t4,C2_R13R21
	ctc2	t5,C2_R22R23
	ctc2	t6,C2_R31R32
	ctc2	t7,C2_R33
	ctc2	t0,C2_TRX
	ctc2	t1,C2_TRY
	ctc2	t2,C2_TRZ
	nop
	wMVMVA	1,0,0,0,0
	lw	t0,M_r11r12(s7)
	lw	t1,M_r13r21(s7)
	lw	t2,M_r22r23(s7)
	lw	t3,M_r31r32(s7)
	lw	t4,M_r33(s7)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	mfc2	t0,C2_MAC1
	mfc2	t1,C2_MAC2
	mfc2	t2,C2_MAC3
	andi	t0,t0,$ffff
	sll	t1,t1,16
	or	t1,t1,t0
	mtc2	t1,C2_VXY0
	mtc2	t2,C2_VZ0
	nop
	wRTPS
	addi	s0,s0,SKFO_SIZE
	mfc2	t0,C2_SZ3
	swc2	C2_SXY2,0(t8)
	sh	t0,0(t9)
	bne	v1,zero,.skin_from_kept_offset_loop
	addi	v1,v1,-1
.skin_keep_list:
	lh	v1,0(s0)
	nop
	beq	v1,zero,.skin_from_kept_list
	addi	s0,s0,2
	addi	v1,v1,-1
	lw	t0,SKIN_viewBone+M_r11r12(sp)
	lw	t1,SKIN_viewBone+M_r13r21(sp)
	lw	t2,SKIN_viewBone+M_r22r23(sp)
	lw	t3,SKIN_viewBone+M_r31r32(sp)
	lw	t4,SKIN_viewBone+M_r33(sp)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
.skin_keep_loop:
	lhu	t0,SKV_x(s0)
	lh	t1,SKV_y(s0)
	lh	t2,SKV_z(s0)
	sll	t1,t1,16
	or	t1,t1,t0
	mtc2	t1,C2_VXY0
	mtc2	t2,C2_VZ0
	nop
	wMVMVA	1,0,0,0,0
	addi	s0,s0,SKV_SIZE
	addi	s3,s3,4
	addi	s4,s4,2
	mfc2	t0,C2_MAC1
	mfc2	t1,C2_MAC2
	mfc2	t2,C2_MAC3
	sh	t0,-4(s3)
	sh	t1,-2(s3)
	sh	t2,-2(s4)
	bne	v1,zero,.skin_keep_loop
	addi	v1,v1,-1
.skin_from_kept_list:
	lh	v1,0(s0)
	nop
	beq	v1,zero,.skin_three_list
	addi	s0,s0,2
	lw	t0,SKIN_viewBone+M_r11r12(sp)
	lw	t1,SKIN_viewBone+M_r13r21(sp)
	lw	t2,SKIN_viewBone+M_r22r23(sp)
	lw	t3,SKIN_viewBone+M_r31r32(sp)
	lw	t4,SKIN_viewBone+M_r33(sp)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	lw	t3,SKIN_translation(sp)
	lw	t4,SKIN_translation+4(sp)
	lw	t5,SKIN_translation+8(sp)
	addi	v1,v1,-1
.skin_from_kept_loop:
	lhu	t0,SKF_x(s0)
	lh	t1,SKF_y(s0)
	lh	t2,SKF_z(s0)
	sll	t1,t1,16
	or	t1,t1,t0
	lh	t0,SKF_base(s0)
	mtc2	t1,C2_VXY0
	mtc2	t2,C2_VZ0
	add	t0,t0,t0
	addi	t9,s6,RS_screenZ-RS_screenXy
	add	t9,t9,t0
	add	t0,t0,t0
	add	t8,s6,t0
	lh	t0,0(t8)
	lh	t1,2(t8)
	lh	t2,0(t9)
	add	t0,t0,t3
	add	t1,t1,t4
	add	t2,t2,t5
	ctc2	t0,C2_TRX
	ctc2	t1,C2_TRY
	ctc2	t2,C2_TRZ
	nRTPS
	addi	s0,s0,SKF_SIZE
	mfc2	t0,C2_SZ3
	swc2	C2_SXY2,0(t8)
	sh	t0,0(t9)
	bne	v1,zero,.skin_from_kept_loop
	addi	v1,v1,-1
.skin_three_list:
	lh	v1,0(s0)
	nop
	beq	v1,zero,.skin_prim_groups
	addi	s0,s0,2
	lw	t0,SKIN_viewBone+M_r11r12(sp)
	lw	t1,SKIN_viewBone+M_r13r21(sp)
	lw	t2,SKIN_viewBone+M_r22r23(sp)
	lw	t3,SKIN_viewBone+M_r31r32(sp)
	lw	t4,SKIN_viewBone+M_r33(sp)
	lw	t5,SKIN_translation(sp)
	lw	t6,SKIN_translation+4(sp)
	lw	t7,SKIN_translation+8(sp)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	t5,C2_TRX
	ctc2	t6,C2_TRY
	ctc2	t7,C2_TRZ
	addiu	v1,v1,-3
	lhu	t0,SKV_x(s0)
	lh	t1,SKV_y(s0)
	lh	t2,SKV_z(s0)
	sll	t1,t1,16
	or	t1,t1,t0
	mtc2	t1,C2_VXY0
	mtc2	t2,C2_VZ0
	lhu	t0,SKV_x+SKV_SIZE(s0)
	lh	t1,SKV_y+SKV_SIZE(s0)
	lh	t2,SKV_z+SKV_SIZE(s0)
	sll	t1,t1,16
	or	t1,t1,t0
	mtc2	t1,C2_VXY1
	mtc2	t2,C2_VZ1
	lhu	t0,SKV_x+2*SKV_SIZE(s0)
	lh	t1,SKV_y+2*SKV_SIZE(s0)
	lh	t2,SKV_z+2*SKV_SIZE(s0)
	sll	t1,t1,16
	or	t1,t1,t0
	mtc2	t1,C2_VXY2
	mtc2	t2,C2_VZ2
	nRTPT
	j	.skin_store_three
	addiu	s0,s0,3*SKV_SIZE
.skin_next_three:
	RTPT
	sh	t0,0(s4)
	sh	t1,2(s4)
	sh	t2,4(s4)
	addiu	s4,s4,6
.skin_store_three:
	blez	v1,.skin_three_done
	nop
	addiu	v1,v1,-3
	lhu	t0,SKV_x(s0)
	lh	t2,SKV_y(s0)
	lh	t1,SKV_z(s0)
	sll	t2,t2,16
	or	t0,t0,t2
	lhu	t2,SKV_x+SKV_SIZE(s0)
	lh	t4,SKV_y+SKV_SIZE(s0)
	lh	t3,SKV_z+SKV_SIZE(s0)
	sll	t4,t4,16
	or	t2,t2,t4
	lhu	t4,SKV_x+2*SKV_SIZE(s0)
	lh	t6,SKV_y+2*SKV_SIZE(s0)
	lh	t5,SKV_z+2*SKV_SIZE(s0)
	sll	t6,t6,16
	or	t4,t4,t6
	swc2	C2_SXY0,0(s3)
	swc2	C2_SXY1,4(s3)
	swc2	C2_SXY2,8(s3)
	addiu	s3,s3,12
	mtc2	t0,C2_VXY0
	mtc2	t1,C2_VZ0
	mtc2	t2,C2_VXY1
	mtc2	t3,C2_VZ1
	mtc2	t4,C2_VXY2
	mtc2	t5,C2_VZ2
	mfc2	t0,C2_SZ1
	mfc2	t1,C2_SZ2
	mfc2	t2,C2_SZ3
	j	.skin_next_three
	addiu	s0,s0,3*SKV_SIZE
.skin_three_done:
	swc2	C2_SXY0,0(s3)
	swc2	C2_SXY1,4(s3)
	swc2	C2_SXY2,8(s3)
	mfc2	t0,C2_SZ1
	mfc2	t1,C2_SZ2
	mfc2	t2,C2_SZ3
	sh	t0,0(s4)
	sh	t1,2(s4)
	sh	t2,4(s4)
	add	v1,v1,v1
	add	t0,v1,v1
	add	v1,t0,v1
	add	s0,s0,v1
.skin_prim_groups:
	addi	s0,s0,2
	ori	t1,zero,$4		; replaced before it is read, with the next two
	andi	t0,s0,$3
	sub	t0,t1,t0
	andi	t0,s0,$3
	add	s0,s0,t0
	addi	s4,s6,-RS_screenXy
	lw	s3,RS_primTable(s4)
.skin_next_prim_group:
	lh	a0,0(s0)
	addi	s0,s0,2
	beq	a0,zero,.skin_seams
	lhu	t0,0(s0)
	addi	s0,s0,2
	sll	t0,t0,2
	add	t0,t0,s3
	lw	t0,0(t0)
	nop
	jalr	ra,t0
	nop
	nop
	j	.skin_next_prim_group
	nop
.skin_seams:
	beq	s5,zero,.skin_done
	ori	s2,zero,$ffff
.skin_next_seam:
	lhu	t0,SEAM_source(s0)
	lhu	t1,SEAM_dest(s0)
	beq	t0,s2,.skin_seams_done
	addu	t4,s6,zero
	addi	t6,s6,RS_screenZ-RS_screenXy
	lh	t3,SEAM_count(s0)
	addu	t5,t4,zero
	addu	t7,t6,zero
	add	t0,t0,t0
	add	t1,t1,t1
	add	t6,t6,t0
	add	t7,t7,t1
	add	t0,t0,t0
	add	t1,t1,t1
	add	t4,t4,t0
	add	t5,t5,t1
	addi	s0,s0,SEAM_SIZE
	addi	t3,t3,-1
.skin_copy_seam_vertex:
	lw	t0,0(t4)
	lh	t1,0(t6)
	sw	t0,0(t5)
	sh	t1,0(t7)
	addi	t4,t4,-4
	addi	t5,t5,4
	addi	t6,t6,-2
	addi	t7,t7,2
	bne	t3,zero,.skin_copy_seam_vertex
	addi	t3,t3,-1
	j	.skin_next_seam
	nop
.skin_seams_done:
	addi	s0,s0,2
	lh	t0,0(s0)
	addu	s3,s6,zero
	addi	s4,s6,RS_screenZ-RS_screenXy
	add	t0,t0,t0
	add	s4,s4,t0
	add	t0,t0,t0
	add	s3,s3,t0
	addi	s0,s0,2
	b	.skin_next_bone
	addi	s5,s5,-1
.skin_done:
	addu	v0,s0,zero
	lw	ra,136(sp)
	lw	fp,132(sp)
	lw	s7,128(sp)
	lw	s6,124(sp)
	lw	s5,120(sp)
	lw	s4,116(sp)
	lw	s3,112(sp)
	lw	s2,108(sp)
	lw	s1,104(sp)
	lw	s0,100(sp)
	jr	ra
	addiu	sp,sp,140
	nop		; unreached

; DrawSkinnedPolyFt3Prelit (skinned type 38): textured triangles, prelit colours averaged.
; In:   a0 the face count (not zero), s0 the MFT3 (no normal) records.
; Out:  s0 past them.
; Uses: v0, v1, a0..a2, t0..t6, the GTE.
DrawSkinnedPolyFt3Prelit:
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
	beq	a0,zero,.skinned_poly_ft3_prelit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,FT3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MFT3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,FT_r0(s6)
.skinned_poly_ft3_prelit_next:
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
	bltz	t6,.skinned_poly_ft3_prelit_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.skinned_poly_ft3_prelit_skip
	nop
	bne	t6,zero,.skinned_poly_ft3_prelit_skip
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
	blez	v0,.skinned_poly_ft3_prelit_skip
	sll	v0,v0,2
	lhu	a0,MFT3_prelitHigh-MFT3_vertices(s0)
	lhu	a1,MFT3_prelitLow-MFT3_vertices(s0)
	sll	a0,a0,16
	or	a0,a0,a1
	ori	t0,zero,Q12_THIRD
	mtc2	t0,C2_IR0
	la	t0,gSkinnedPrelitColors
	srl	t4,a0,18
	andi	t4,t4,$ffc
	add	t4,t4,t0
	lw	t4,0(t4)
	nop
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	srl	t4,a0,8
	andi	t4,t4,$ffc
	add	t4,t4,t0
	lw	t4,0(t4)
	GPF	1
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	sll	t4,a0,2
	andi	t4,t4,$ffc
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	add	t4,t4,t0
	lw	t4,0(t4)
	GPL	1
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	nGPL	1
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	swc2	C2_RGB2,FT_r0(s6)
.skinned_poly_ft3_prelit_skip:
	addiu	s0,s0,MFT3_NO_NORMAL_SIZE
	addiu	s6,s6,MFT3_NO_NORMAL_SIZE
	bne	s3,zero,.skinned_poly_ft3_prelit_next
	addiu	s3,s3,-1
.skinned_poly_ft3_prelit_done:
	addi	s0,s0,-MFT3_vertices
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

; DrawSkinnedPolyFt4Prelit (skinned type 39): textured quads, prelit colours averaged.
; In:   a0 the face count (not zero), s0 the MFT4 (no normal) records.
; Out:  s0 past them.
; Uses: v0, v1, a0..a3, t0..t8, the GTE.
DrawSkinnedPolyFt4Prelit:
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
	beq	a0,zero,.skinned_poly_ft4_prelit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,FT4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MFT4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,FT_r0(s6)
.skinned_poly_ft4_prelit_next:
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
	bltz	t8,.skinned_poly_ft4_prelit_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.skinned_poly_ft4_prelit_skip
	nop
	bne	t8,zero,.skinned_poly_ft4_prelit_skip
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
	blez	v0,.skinned_poly_ft4_prelit_skip
	nop
	sw	t0,FT_x0(s6)
	sw	t1,FT_x1(s6)
	sw	t2,FT_x2(s6)
	sw	t7,FT_x3(s6)
	lhu	a0,MFT4_prelit0-MFT4_vertices(s0)
	lhu	a1,MFT4_prelit1-MFT4_vertices(s0)
	lhu	a2,MFT4_prelit2-MFT4_vertices(s0)
	lhu	a3,MFT4_prelit3-MFT4_vertices(s0)
	ori	t0,zero,Q12_QUARTER
	mtc2	t0,C2_IR0
	la	t0,gSkinnedPrelitColors
	sll	a0,a0,2
	add	a0,a0,t0
	lw	t4,0(a0)
	nop
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	sll	a1,a1,2
	add	a1,a1,t0
	lw	t4,0(a1)
	GPF	1
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	sll	a2,a2,2
	add	a2,a2,t0
	lw	t4,0(a2)
	GPL	1
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	sll	a3,a3,2
	add	a3,a3,t0
	lw	t4,0(a3)
	GPL	1
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	nGPL	1
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
	swc2	C2_RGB2,FT_r0(s6)
.skinned_poly_ft4_prelit_skip:
	addiu	s0,s0,MFT4_NO_NORMAL_SIZE
	addiu	s6,s6,MFT4_NO_NORMAL_SIZE
	bne	s3,zero,.skinned_poly_ft4_prelit_next
	addiu	s3,s3,-1
.skinned_poly_ft4_prelit_done:
	addi	s0,s0,-MFT4_vertices
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

; DrawSkinnedPolyGt3Prelit (skinned type 40): Gouraud textured triangles in their prelit colours.
; In:   a0 the face count (not zero), s0 the MGT3 (no normal) records.
; Out:  s0 past them.
; Uses: v0, v1, a0..a2, t0..t6, the GTE.
DrawSkinnedPolyGt3Prelit:
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
	beq	a0,zero,.skinned_poly_gt3_prelit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,GT3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,GT3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MGT3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,GT_r0(s6)
.skinned_poly_gt3_prelit_next:
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
	bltz	t6,.skinned_poly_gt3_prelit_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.skinned_poly_gt3_prelit_skip
	nop
	bne	t6,zero,.skinned_poly_gt3_prelit_skip
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
	sw	t0,GT_x0(s6)
	wAVSZ3
	sw	t1,GT_x1(s6)
	mfc2	v0,C2_OTZ
	sw	t2,GT_x2(s6)
	blez	v0,.skinned_poly_gt3_prelit_skip
	sll	v0,v0,2
	lhu	a0,MGT3_prelitHigh-MGT3_vertices(s0)
	lhu	a1,MGT3_prelitLow-MGT3_vertices(s0)
	sll	a0,a0,16
	or	a0,a0,a1
	jal	FetchThreePrelitGt3Colors
	nop
	sw	t0,GT_r0(s6)
	sw	t1,GT_r1(s6)
	sw	t2,GT_r2(s6)
	nop
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,GT_tag(s6)
.skinned_poly_gt3_prelit_skip:
	addiu	s0,s0,MGT3_NO_NORMAL_SIZE
	addiu	s6,s6,MGT3_NO_NORMAL_SIZE
	bne	s3,zero,.skinned_poly_gt3_prelit_next
	addiu	s3,s3,-1
.skinned_poly_gt3_prelit_done:
	addi	s0,s0,-MGT3_vertices
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

; DrawSkinnedPolyGt4Prelit (skinned type 41): Gouraud textured quads in their prelit colours.
; In:   a0 the face count (not zero), s0 the MGT4 (no normal) records.
; Out:  s0 past them.
; Uses: v0, v1, a0..a3, t0..t8, the GTE.
DrawSkinnedPolyGt4Prelit:
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
	beq	a0,zero,.skinned_poly_gt4_prelit_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,GT4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,GT4_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MGT4_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,GT_r0(s6)
.skinned_poly_gt4_prelit_next:
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
	bltz	t8,.skinned_poly_gt4_prelit_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.skinned_poly_gt4_prelit_skip
	nop
	bne	t8,zero,.skinned_poly_gt4_prelit_skip
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
	blez	v0,.skinned_poly_gt4_prelit_skip
	nop
	sw	t0,GT_x0(s6)
	sw	t1,GT_x1(s6)
	sw	t2,GT_x2(s6)
	sw	t7,GT_x3(s6)
	lhu	a0,MGT4_prelit0-MGT4_vertices(s0)
	lhu	a1,MGT4_prelit1-MGT4_vertices(s0)
	lhu	a2,MGT4_prelit2-MGT4_vertices(s0)
	lhu	a3,MGT4_prelit3-MGT4_vertices(s0)
	jal	FetchFourPrelitGt4Colors
	nop
	sw	t0,GT_r0(s6)
	sw	t1,GT_r1(s6)
	sw	t2,GT_r2(s6)
	sw	t3,GT_r3(s6)
	nop
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,GT_tag(s6)
.skinned_poly_gt4_prelit_skip:
	addiu	s0,s0,MGT4_NO_NORMAL_SIZE
	addiu	s6,s6,MGT4_NO_NORMAL_SIZE
	bne	s3,zero,.skinned_poly_gt4_prelit_next
	addiu	s3,s3,-1
.skinned_poly_gt4_prelit_done:
	addi	s0,s0,-MGT4_vertices
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

; AverageThreePrelitColors: unused; the mean of three prelit colours.
; In:   a0 three 10-bit colour indices.
; Out:  the GTE's RGB2.
; Uses: t0..t4, the GTE.
AverageThreePrelitColors:
	ori	t0,zero,Q12_THIRD
	mtc2	t0,C2_IR0
	la	t0,gSkinnedPrelitColors
	srl	t4,a0,18
	andi	t4,t4,$ffc
	add	t4,t4,t0
	lw	t4,0(t4)
	nop
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	srl	t4,a0,8
	andi	t4,t4,$ffc
	add	t4,t4,t0
	lw	t4,0(t4)
	GPF	1
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	sll	t4,a0,2
	andi	t4,t4,$ffc
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	add	t4,t4,t0
	lw	t4,0(t4)
	GPL	1
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	nGPL	1
	jr	ra
	nop

; AverageFourPrelitColors: unused; the mean of four prelit colours.
; In:   a0..a3 the colour indices.
; Out:  the GTE's RGB2.
; Uses: a0..a3, t0..t4, the GTE.
AverageFourPrelitColors:
	ori	t0,zero,Q12_QUARTER
	mtc2	t0,C2_IR0
	la	t0,gSkinnedPrelitColors
	sll	a0,a0,2
	add	a0,a0,t0
	lw	t4,0(a0)
	nop
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	sll	a1,a1,2
	add	a1,a1,t0
	lw	t4,0(a1)
	GPF	1
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	sll	a2,a2,2
	add	a2,a2,t0
	lw	t4,0(a2)
	GPL	1
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	sll	a3,a3,2
	add	a3,a3,t0
	lw	t4,0(a3)
	GPL	1
	srl	t2,t4,4
	srl	t3,t4,12
	sll	t1,t4,4
	andi	t1,t1,$ff0
	andi	t2,t2,$ff0
	andi	t3,t3,$ff0
	mtc2	t1,C2_IR1
	mtc2	t2,C2_IR2
	mtc2	t3,C2_IR3
	nGPL	1
	jr	ra
	nop

; FetchThreePrelitGt3Colors: three prelit colours as POLY_GT3 colour words.
; In:   a0 three 10-bit colour indices, vertex 0's in bits 20-29.
; Out:  t0..t2.
; Uses: a0.
FetchThreePrelitGt3Colors:
	la	t0,gSkinnedPrelitColors
	srl	t1,a0,18
	srl	t2,a0,8
	sll	a0,a0,2
	andi	t1,t1,$ffc
	andi	t2,t2,$ffc
	andi	a0,a0,$ffc
	add	t1,t1,t0
	add	t2,t2,t0
	add	a0,a0,t0
	lw	t0,0(t1)
	lw	t1,0(t2)
	lw	t2,0(a0)
	lui	a0,GT3_CODE<<8
	or	t0,t0,a0
	or	t1,t1,a0
	or	t2,t2,a0
	jr	ra
	nop

; FetchFourPrelitGt4Colors: four prelit colours as POLY_GT4 colour words.
; In:   a0..a3 the colour indices.
; Out:  t0..t3.
; Uses: a0..a3.
FetchFourPrelitGt4Colors:
	la	t0,gSkinnedPrelitColors
	sll	a0,a0,2
	sll	a1,a1,2
	sll	a2,a2,2
	sll	a3,a3,2
	add	a0,a0,t0
	add	a1,a1,t0
	add	a2,a2,t0
	add	a3,a3,t0
	lw	t0,0(a0)
	lw	t1,0(a1)
	lw	t2,0(a2)
	lw	t3,0(a3)
	lui	a0,GT4_CODE<<8
	or	t0,t0,a0
	or	t1,t1,a0
	or	t2,t2,a0
	or	t3,t3,a0
	jr	ra
	nop

; PrelightSkinnedMeshNormals(groups, bones) -> past them: lights a skinned mesh's normals.
; In:   a0 the normal groups, a1 the bone matrices.
; Out:  v0 past the end marker.
; Uses: v0, v1, a0..a2, t0..t7, t9, the GTE.
PrelightSkinnedMeshNormals:
	addiu	sp,sp,-56
	sw	ra,40(sp)
	sw	fp,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	addu	s1,a0,zero
	addu	s2,a1,zero
	lw	s4,gSkinnedLightSlot
	nop
	addi	s4,s4,LS_lightMatrix
	lui	s5,SCRATCHPAD>>16
	mtc2	zero,C2_RGB
	la	s3,gSkinnedPrelitColors
	b	.prelight_next_group
	nop
.prelight_group:
	addi	a2,s5,RS_screenXy
	sll	t0,v0,2
	add	a1,s2,t0
	lw	a1,0(a1)
	jal	MulMatrix3x3Fast
	addu	a0,s4,zero
	lw	t0,RS_screenXy+M_m0(s5)
	lw	t1,RS_screenXy+M_m0+4(s5)
	lw	t2,RS_screenXy+M_m1+2(s5)
	lw	t3,RS_screenXy+M_m2(s5)
	lw	t4,RS_screenXy+M_m2+4(s5)
	ctc2	t0,C2_L11L12
	ctc2	t1,C2_L13L21
	ctc2	t2,C2_L22L23
	ctc2	t3,C2_L31L32
	ctc2	t4,C2_L33
	lh	v0,0(s1)
	addi	s1,s1,2
	lhu	t0,0(s1)
	lh	t2,2(s1)
	lh	t1,4(s1)
	sll	t2,t2,16
	or	t0,t0,t2
	lhu	t2,6(s1)
	lh	t4,8(s1)
	lh	t3,10(s1)
	sll	t4,t4,16
	or	t2,t2,t4
	lhu	t4,12(s1)
	lh	t6,14(s1)
	lh	t5,16(s1)
	sll	t6,t6,16
	or	t4,t4,t6
	b	.prelight_three
	nop
.prelight_next_three:
	lhu	t0,0(s1)
	lh	t2,2(s1)
	lh	t1,4(s1)
	sll	t2,t2,16
	or	t0,t0,t2
	lhu	t2,6(s1)
	lh	t4,8(s1)
	lh	t3,10(s1)
	sll	t4,t4,16
	or	t2,t2,t4
	lhu	t4,12(s1)
	lh	t6,14(s1)
	lh	t5,16(s1)
	sll	t6,t6,16
	or	t4,t4,t6
	swc2	C2_RGB0,0(s3)
	swc2	C2_RGB1,4(s3)
	swc2	C2_RGB2,8(s3)
	addi	s3,s3,12
.prelight_three:
	mtc2	t0,C2_VXY0
	mtc2	t1,C2_VZ0
	mtc2	t2,C2_VXY1
	mtc2	t3,C2_VZ1
	mtc2	t4,C2_VXY2
	mtc2	t5,C2_VZ2
	addi	v0,v0,-3
	nop
	NCT
	bgtz	v0,.prelight_next_three
	addi	s1,s1,18
	swc2	C2_RGB0,0(s3)
	swc2	C2_RGB1,4(s3)
	swc2	C2_RGB2,8(s3)
	addi	s3,s3,12
	add	v0,v0,v0
	add	t0,v0,v0
	add	s3,s3,t0
	add	t0,t0,v0
	add	s1,s1,t0
.prelight_next_group:
	lh	v0,0(s1)
	nop
	addi	t0,v0,1
	bne	t0,zero,.prelight_group
	addi	s1,s1,2
	addu	v0,s1,zero
	lw	ra,40(sp)
	lw	fp,36(sp)
	lw	s5,32(sp)
	lw	s4,28(sp)
	lw	s3,24(sp)
	lw	s2,20(sp)
	lw	s1,16(sp)
	jr	ra
	addiu	sp,sp,56

; IgnoreSkinnedPolyF3 (skinned type 0): returns at once, leaving s0 at the group; the skinned walk
; would read its records as the next group.
; In:   none.
; Out:  s0 unchanged.
; Uses: nothing.
IgnoreSkinnedPolyF3:
	jr	ra
	nop

; IgnoreSkinnedPolyF4 (skinned type 8): the same for quads.
IgnoreSkinnedPolyF4:
	jr	ra
	nop

; DrawPolyFt3DoubleLitDynamic (dynamic type 6): textured triangles lit on both sides.
; In:   a0 the face count (not zero), s0 the DTRI faces.
; Out:  s0 past them, gPacketCursor a packet further per face.
; Uses: at, v0, v1, a0..a3, t0..t6, t9, the GTE.
DrawPolyFt3DoubleLitDynamic:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_ft3_double_lit_dynamic_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT3_TAG
	li	fp,OT_ADDR_MASK
	lw	s6,gPacketCursor
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_ft3_double_lit_dynamic_next:
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
	bltz	t6,.poly_ft3_double_lit_dynamic_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bne	t6,zero,.poly_ft3_double_lit_dynamic_skip
	slt	a3,v0,zero
	addi	a3,a3,-1
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
	blez	v0,.poly_ft3_double_lit_dynamic_skip
	sll	v0,v0,2
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_ft3_double_lit_dynamic_skip
	nop
	lw	t0,DTRI_normal(s0)
	lw	t1,DTRI_normal+4(s0)
	xor	t0,t0,a3
	mtc2	t0,C2_VXY0
	xor	t1,t1,a3
	mtc2	t1,C2_VZ0
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCS
	lh	t4,DTRI_clut(s0)
	lh	t5,DTRI_tpage(s0)
	sll	t4,t4,16
	sll	t5,t5,16
	swc2	C2_RGB2,FT_r0(s6)
	lhu	t0,DTRI_uv0(s0)
	lhu	t1,DTRI_uv1(s0)
	lhu	t2,DTRI_uv2(s0)
	or	t0,t0,t4
	or	t1,t1,t5
	sw	t0,FT_u0(s6)
	sw	t1,FT_u1(s6)
	sh	t2,FT_u2(s6)
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
.poly_ft3_double_lit_dynamic_skip:
	addiu	s0,s0,DTRI_SIZE
	addiu	s6,s6,FT3_SIZE
	bne	s3,zero,.poly_ft3_double_lit_dynamic_next
	addiu	s3,s3,-1
.poly_ft3_double_lit_dynamic_done:
	opt	at+
	sw	s6,gPacketCursor
	opt	at-
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

; DrawPolyFt4DoubleLitDynamic (dynamic type 14): textured quads lit on both sides.
; In:   a0 the face count (not zero), s0 the DQUAD faces.
; Out:  s0 past them, gPacketCursor a packet further per face.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawPolyFt4DoubleLitDynamic:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_ft4_double_lit_dynamic_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT4_TAG
	li	fp,OT_ADDR_MASK
	lw	s6,gPacketCursor
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_ft4_double_lit_dynamic_next:
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
	bltz	t8,.poly_ft4_double_lit_dynamic_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bne	t8,zero,.poly_ft4_double_lit_dynamic_skip
	slt	a3,v0,zero
	addi	a3,a3,-1
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
	lh	t3,0(v0)
	add	a0,a0,a1
	add	a0,a0,a2
	add	a0,a0,t3
	sra	v0,a0,2
	blez	v0,.poly_ft4_double_lit_dynamic_skip
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_ft4_double_lit_dynamic_skip
	nop
	sw	t0,FT_x0(s6)
	sw	t1,FT_x1(s6)
	sw	t2,FT_x2(s6)
	sw	t7,FT_x3(s6)
	lhu	t0,DQUAD_normalZ(s0)
	lw	t1,DQUAD_normalXY(s0)
	xor	t0,t0,a3
	mtc2	t0,C2_VZ0
	xor	t1,t1,a3
	mtc2	t1,C2_VXY0
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCS
	lh	t4,DQUAD_clut(s0)
	lh	t5,DQUAD_tpage(s0)
	sll	t4,t4,16
	sll	t5,t5,16
	swc2	C2_RGB2,FT_r0(s6)
	lhu	t0,DQUAD_uv0(s0)
	lhu	t1,DQUAD_uv1(s0)
	lhu	t2,DQUAD_uv2(s0)
	lhu	t3,DQUAD_uv3(s0)
	or	t0,t0,t4
	or	t1,t1,t5
	sw	t0,FT_u0(s6)
	sw	t1,FT_u1(s6)
	sh	t2,FT_u2(s6)
	sh	t3,FT_u3(s6)
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
.poly_ft4_double_lit_dynamic_skip:
	addiu	s0,s0,DQUAD_SIZE
	addiu	s6,s6,FT4_SIZE
	bne	s3,zero,.poly_ft4_double_lit_dynamic_next
	addiu	s3,s3,-1
.poly_ft4_double_lit_dynamic_done:
	opt	at+
	sw	s6,gPacketCursor
	opt	at-
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

; DrawPolyFt3LitDynamic (dynamic type 4): textured triangles lit by NCS.
; In:   a0 the face count (not zero), s0 the DTRI faces.
; Out:  s0 past them, gPacketCursor a packet further per face.
; Uses: at, v0, v1, a0..a2, t0..t6, the GTE.
DrawPolyFt3LitDynamic:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_ft3_lit_dynamic_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT3_TAG
	li	fp,OT_ADDR_MASK
	lw	s6,gPacketCursor
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_ft3_lit_dynamic_next:
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
	bltz	t6,.poly_ft3_lit_dynamic_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.poly_ft3_lit_dynamic_skip
	nop
	bne	t6,zero,.poly_ft3_lit_dynamic_skip
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
	blez	v0,.poly_ft3_lit_dynamic_skip
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
	sw	t0,FT_u0(s6)
	sw	t1,FT_u1(s6)
	sh	t2,FT_u2(s6)
	swc2	C2_RGB2,FT_r0(s6)
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
.poly_ft3_lit_dynamic_skip:
	addiu	s0,s0,DTRI_SIZE
	addiu	s6,s6,FT3_SIZE
	bne	s3,zero,.poly_ft3_lit_dynamic_next
	addiu	s3,s3,-1
.poly_ft3_lit_dynamic_done:
	opt	at+
	sw	s6,gPacketCursor
	opt	at-
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

; DrawPolyFt4LitDynamic (dynamic type 12): textured quads lit by NCS.
; In:   a0 the face count (not zero), s0 the DQUAD faces.
; Out:  s0 past them, gPacketCursor a packet further per face.
; Uses: at, v0, v1, a0..a3, t0..t8, the GTE.
DrawPolyFt4LitDynamic:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_ft4_lit_dynamic_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,FT4_TAG
	li	fp,OT_ADDR_MASK
	lw	s6,gPacketCursor
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_ft4_lit_dynamic_next:
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
	bltz	t8,.poly_ft4_lit_dynamic_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.poly_ft4_lit_dynamic_skip
	nop
	bne	t8,zero,.poly_ft4_lit_dynamic_skip
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
	blez	v0,.poly_ft4_lit_dynamic_skip
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
	sw	t0,FT_u0(s6)
	sw	t1,FT_u1(s6)
	sh	t2,FT_u2(s6)
	sh	t3,FT_u3(s6)
	swc2	C2_RGB2,FT_r0(s6)
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,FT_tag(s6)
.poly_ft4_lit_dynamic_skip:
	addiu	s0,s0,DQUAD_SIZE
	addiu	s6,s6,FT4_SIZE
	bne	s3,zero,.poly_ft4_lit_dynamic_next
	addiu	s3,s3,-1
.poly_ft4_lit_dynamic_done:
	opt	at+
	sw	s6,gPacketCursor
	opt	at-
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

; DrawPolyGt3LitDynamic (dynamic type 20): Gouraud textured triangles lit by NCT.
; In:   a0 the face count (not zero), s0 the DGT3 faces.
; Out:  s0 past them, gPacketCursor a packet further per face.
; Uses: at, v0, v1, a0..a2, t0..t6, t9, the GTE.
DrawPolyGt3LitDynamic:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_gt3_lit_dynamic_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,GT3_TAG
	li	fp,OT_ADDR_MASK
	lw	s6,gPacketCursor
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,DTRI_tpage(s0)
.poly_gt3_lit_dynamic_next:
	lw	a0,DTRI_vertices(s0)
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
	bltz	t6,.poly_gt3_lit_dynamic_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.poly_gt3_lit_dynamic_skip
	nop
	bne	t6,zero,.poly_gt3_lit_dynamic_skip
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
	sw	t0,GT_x0(s6)
	wAVSZ3
	sw	t1,GT_x1(s6)
	mfc2	v0,C2_OTZ
	sw	t2,GT_x2(s6)
	blez	v0,.poly_gt3_lit_dynamic_skip
	sll	v0,v0,2
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_gt3_lit_dynamic_skip
	nop
	lwc2	C2_VXY0,DGT3_normalXy0(s0)
	lw	t0,DGT3_normalZ0(s0)
	lwc2	C2_VXY1,DGT3_normalXy1(s0)
	mtc2	t0,C2_VZ0
	lwc2	C2_VXY2,DGT3_normalXy2(s0)
	lwc2	C2_VZ2,DGT3_normalZ2(s0)
	srl	t0,t0,16
	mtc2	t0,C2_VZ1
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCT
	lh	t4,DTRI_clut(s0)
	lh	t5,DTRI_tpage(s0)
	sll	t4,t4,16
	sll	t5,t5,16
	lhu	t0,DTRI_uv0(s0)
	lhu	t1,DTRI_uv1(s0)
	lhu	t2,DTRI_uv2(s0)
	or	t0,t0,t4
	or	t1,t1,t5
	sw	t0,GT_u0(s6)
	sw	t1,GT_u1(s6)
	sh	t2,GT_u2(s6)
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,GT_tag(s6)
	swc2	C2_RGB0,GT_r0(s6)
	swc2	C2_RGB1,GT_r1(s6)
	swc2	C2_RGB2,GT_r2(s6)
.poly_gt3_lit_dynamic_skip:
	addiu	s0,s0,DGT3_SIZE
	addiu	s6,s6,GT3_SIZE
	bne	s3,zero,.poly_gt3_lit_dynamic_next
	addiu	s3,s3,-1
.poly_gt3_lit_dynamic_done:
	opt	at+
	sw	s6,gPacketCursor
	opt	at-
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

; DrawPolyGt4LitDynamic (dynamic type 28): Gouraud textured quads lit by NCS and NCT.
; In:   a0 the face count (not zero), s0 the DGT4 faces.
; Out:  s0 past them, gPacketCursor a packet further per face.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawPolyGt4LitDynamic:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_gt4_lit_dynamic_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,GT4_TAG
	li	fp,OT_ADDR_MASK
	lw	s6,gPacketCursor
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,DQUAD_tpage(s0)
.poly_gt4_lit_dynamic_next:
	lw	a0,DGT4_vertices(s0)
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
	bltz	t8,.poly_gt4_lit_dynamic_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.poly_gt4_lit_dynamic_skip
	nop
	bne	t8,zero,.poly_gt4_lit_dynamic_skip
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
	blez	v0,.poly_gt4_lit_dynamic_skip
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_gt4_lit_dynamic_skip
	nop
	sw	t0,GT_x0(s6)
	sw	t1,GT_x1(s6)
	sw	t2,GT_x2(s6)
	sw	t7,GT_x3(s6)
	lw	t9,DGT4_normalZ0(s0)
	lwc2	C2_VXY0,DGT4_normalXy0(s0)
	mtc2	t9,C2_VZ0
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	NCS
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	swc2	C2_RGB2,GT_r0(s6)
	lwc2	C2_VXY0,DGT4_normalXy1(s0)
	srl	t9,t9,16
	mtc2	t9,C2_VZ0
	lwc2	C2_VXY1,DGT4_normalXy2(s0)
	lw	t9,DGT4_normalZ2(s0)
	lwc2	C2_VXY2,DGT4_normalXy3(s0)
	mtc2	t9,C2_VZ1
	srl	t8,t9,16
	mtc2	t8,C2_VZ2
	lh	t4,DQUAD_clut(s0)
	lh	t5,DQUAD_tpage(s0)
	sll	t4,t4,16
	sll	t5,t5,16
	NCT
	lhu	t0,DQUAD_uv0(s0)
	lhu	t1,DQUAD_uv1(s0)
	lhu	t2,DQUAD_uv2(s0)
	lhu	t3,DQUAD_uv3(s0)
	or	t0,t0,t4
	or	t1,t1,t5
	sw	t0,GT_u0(s6)
	sw	t1,GT_u1(s6)
	sh	t2,GT_u2(s6)
	sh	t3,GT_u3(s6)
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,GT_tag(s6)
	swc2	C2_RGB0,GT_r1(s6)
	swc2	C2_RGB1,GT_r2(s6)
	swc2	C2_RGB2,GT_r3(s6)
.poly_gt4_lit_dynamic_skip:
	addiu	s0,s0,DGT4_SIZE
	addiu	s6,s6,GT4_SIZE
	bne	s3,zero,.poly_gt4_lit_dynamic_next
	addiu	s3,s3,-1
.poly_gt4_lit_dynamic_done:
	opt	at+
	sw	s6,gPacketCursor
	opt	at-
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

; DrawShadowPolyF3Dynamic (dynamic type 32): opaque black flat triangles.
; In:   a0 the face count (not zero), s0 their vertex index words.
; Out:  s0 past them, gPacketCursor a packet further per face.
; Uses: at, v0, v1, a0..a2, t0..t6, the GTE.
DrawShadowPolyF3Dynamic:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.shadow_poly_f3_dynamic_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,F3_TAG
	li	fp,OT_ADDR_MASK
	lw	s6,gPacketCursor
	lw	v1,SCRATCHPAD+RS_scaleShift
.shadow_poly_f3_dynamic_next:
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
	bltz	t6,.shadow_poly_f3_dynamic_skip
	andi	t6,t6,$8000
	mfc2	v0,C2_MAC0
	bne	t6,zero,.shadow_poly_f3_dynamic_skip
	nop
	bgez	v0,.shadow_poly_f3_dynamic_skip
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
	blez	v0,.shadow_poly_f3_dynamic_skip
	sll	v0,v0,2
	lui	t0,F3_CODE<<8
	sw	t0,F_r0(s6)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,F_tag(s6)
.shadow_poly_f3_dynamic_skip:
	addiu	s0,s0,4
	addiu	s6,s6,F3_SIZE
	bne	s3,zero,.shadow_poly_f3_dynamic_next
	addiu	s3,s3,-1
.shadow_poly_f3_dynamic_done:
	opt	at+
	sw	s6,gPacketCursor
	opt	at-
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

; DrawShadowPolyF4Dynamic (dynamic type 33): opaque black flat quads.
; In:   a0 the face count (not zero), s0 their vertex index words.
; Out:  s0 past them, gPacketCursor a packet further per face.
; Uses: at, v0, v1, a0..a3, t0..t8, the GTE.
DrawShadowPolyF4Dynamic:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.shadow_poly_f4_dynamic_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,F4_TAG
	li	fp,OT_ADDR_MASK
	lw	s6,gPacketCursor
	lw	v1,SCRATCHPAD+RS_scaleShift
.shadow_poly_f4_dynamic_next:
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
	bltz	t8,.shadow_poly_f4_dynamic_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.shadow_poly_f4_dynamic_skip
	nop
	bne	t8,zero,.shadow_poly_f4_dynamic_skip
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
	blez	v0,.shadow_poly_f4_dynamic_skip
	nop
	sw	t0,F_x0(s6)
	sw	t1,F_x1(s6)
	sw	t2,F_x2(s6)
	sw	t7,F_x3(s6)
	lui	t0,F4_CODE<<8
	sw	t0,F_r0(s6)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,F_tag(s6)
.shadow_poly_f4_dynamic_skip:
	addiu	s0,s0,4
	addiu	s6,s6,F4_SIZE
	bne	s3,zero,.shadow_poly_f4_dynamic_next
	addiu	s3,s3,-1
.shadow_poly_f4_dynamic_done:
	opt	at+
	sw	s6,gPacketCursor
	opt	at-
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

; DrawPolyF3LitCopy: unused; DrawPolyF3Lit instruction for instruction.
; In:   a0 the face count (not zero), s0 the MF3 records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a2, t0..t6, t9, the GTE.
DrawPolyF3LitCopy:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s6,36(sp)
	sw	s5,32(sp)
	sw	s4,28(sp)
	sw	s3,24(sp)
	sw	s2,20(sp)
	sw	s1,16(sp)
	beq	a0,zero,.poly_f3_lit_dup_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,F3_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,F3_SIZE
	and	t0,t0,t1
	add	s6,s0,t0
	addi	s0,s0,MF3_vertices
	lw	v1,SCRATCHPAD+RS_scaleShift
	lwc2	C2_RGB,F_r0(s6)		; replaced before NCCS reads it
.poly_f3_lit_dup_next:
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
	bltz	t6,.poly_f3_lit_dup_skip
	mfc2	v0,C2_MAC0
	andi	t6,t6,$8000
	bgez	v0,.poly_f3_lit_dup_skip
	nop
	bne	t6,zero,.poly_f3_lit_dup_skip
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
	blez	v0,.poly_f3_lit_dup_skip
	sll	v0,v0,2
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_f3_lit_dup_skip
	nop
	lwc2	C2_VXY0,MF3_normal-MF3_vertices(s0)
	lwc2	C2_VZ0,MF3_normal+4-MF3_vertices(s0)
	lwc2	C2_RGB,MF3_color-MF3_vertices(s0)
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	NCCS
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,F_tag(s6)
	swc2	C2_RGB2,F_r0(s6)
.poly_f3_lit_dup_skip:
	addiu	s0,s0,MF3_SIZE
	addiu	s6,s6,MF3_SIZE
	bne	s3,zero,.poly_f3_lit_dup_next
	addiu	s3,s3,-1
.poly_f3_lit_dup_done:
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

; DrawPolyF4WithDrawMode (type 37): flat quads, each with its own drawing mode.
; In:   a0 the face count (not zero), s0 the MF4DM records.
; Out:  s0 past them.
; Uses: at, v0, v1, a0..a3, t0..t9, the GTE.
DrawPolyF4WithDrawMode:
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
	beq	a0,zero,.poly_f4_with_draw_mode_done
	addiu	s3,a0,-1
	li	s1,SCRATCHPAD+RS_screenXy
	li	s2,SCRATCHPAD+RS_screenZ
	lw	s4,SCRATCHPAD+RS_orderingTable
	li	s5,F4_TAG
	li	fp,OT_ADDR_MASK
	lw	t0,SCRATCHPAD+RS_doubleBufferMask
	ori	t1,zero,F4_SIZE
	and	t1,t0,t1
	add	s6,s0,t1
	addi	s0,s0,MF4DM_vertices
	ori	t1,zero,DR_MODE_SIZE
	and	t1,t0,t1
	addi	t1,t1,MF4DM_mode-MF4DM_vertices
	add	s7,s0,t1
	lw	v1,SCRATCHPAD+RS_scaleShift
.poly_f4_with_draw_mode_next:
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
	bltz	t8,.poly_f4_with_draw_mode_skip
	mfc2	v0,C2_MAC0
	andi	t8,t8,$8000
	bgez	v0,.poly_f4_with_draw_mode_skip
	nop
	bne	t8,zero,.poly_f4_with_draw_mode_skip
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
	blez	v0,.poly_f4_with_draw_mode_skip
	lui	at,DEPTH_LIMIT>>16
	slt	t9,v0,at
	beq	t9,zero,.poly_f4_with_draw_mode_skip
	nop
	sw	t0,F_x0(s6)
	sw	t1,F_x1(s6)
	sw	t2,F_x2(s6)
	sw	t7,F_x3(s6)
	lw	t2,MF4DM_depthOffset-MF4DM_vertices(s0)
	nop
	add	v0,v0,t2
	srlv	v0,v0,v1
	srl	t0,v0,11
	andi	v0,v0,$7ff
	addi	v0,v0,-4096
	srav	v0,v0,t0
	andi	v0,v0,$fff
	sll	v0,v0,2
	addu	v0,v0,s4
	lw	t1,0(v0)
	and	t0,s6,fp
	sw	t0,0(v0)
	or	t1,t1,s5
	sw	t1,F_tag(s6)
	nop
	lw	t1,0(v0)
	and	t0,s7,fp
	sw	t0,0(v0)
	lui	at,DR_MODE_TAG>>16
	or	t1,t1,at
	sw	t1,DRM_tag(s7)
.poly_f4_with_draw_mode_skip:
	addiu	s0,s0,MF4DM_SIZE
	addiu	s6,s6,MF4DM_SIZE
	addiu	s7,s7,MF4DM_SIZE
	bne	s3,zero,.poly_f4_with_draw_mode_next
	addiu	s3,s3,-1
.poly_f4_with_draw_mode_done:
	addi	s0,s0,-MF4DM_vertices
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

; CopyWordsFast(source, destination, byteCount): copies whole words, 64 bytes a pass.
; In:   a0 the source, a1 the destination, a2 the byte count, rounded up to words.
; Out:  the words at a1.
; Uses: v0, v1, a0..a3, t0..t9.
CopyWordsFast:
	addiu	sp,sp,-24
	sw	s1,20(sp)
	sw	s0,16(sp)
	beq	a2,zero,.copy_done
	andi	t0,a2,$3
	slt	t0,zero,t0
	srl	a2,a2,2
	add	a2,a2,t0
	andi	v0,a2,$f
	srl	a2,a2,4
	beq	a2,zero,.copy_tail
	addi	a2,a2,-1
.copy_block:
	lw	t0,0(a0)
	lw	t1,4(a0)
	lw	t2,8(a0)
	lw	t3,12(a0)
	lw	t4,16(a0)
	lw	t5,20(a0)
	lw	t6,24(a0)
	lw	t7,28(a0)
	sw	t0,0(a1)
	sw	t1,4(a1)
	sw	t2,8(a1)
	sw	t3,12(a1)
	sw	t4,16(a1)
	sw	t5,20(a1)
	sw	t6,24(a1)
	sw	t7,28(a1)
	lw	t0,32(a0)
	lw	t1,36(a0)
	lw	t2,40(a0)
	lw	t3,44(a0)
	lw	t4,48(a0)
	lw	t5,52(a0)
	lw	t6,56(a0)
	lw	t7,60(a0)
	sw	t0,32(a1)
	sw	t1,36(a1)
	sw	t2,40(a1)
	sw	t3,44(a1)
	sw	t4,48(a1)
	sw	t5,52(a1)
	sw	t6,56(a1)
	sw	t7,60(a1)
	addi	a0,a0,64
	addi	a1,a1,64
	bne	a2,zero,.copy_block
	addi	a2,a2,-1
	beq	v0,zero,.copy_done
	nop
; jump into the unrolled loads and stores so that only the last v0 words are copied
.copy_tail:
	ori	v1,zero,$f
	sub	v1,v1,v0
	sll	v1,v1,2
	la	t9,.copy_tail_loads
	add	t9,t9,v1
	jr	t9
	nop
.copy_tail_loads:
	lw	t0,56(a0)
	lw	t1,52(a0)
	lw	t2,48(a0)
	lw	t3,44(a0)
	lw	t4,40(a0)
	lw	t5,36(a0)
	lw	t6,32(a0)
	lw	t7,28(a0)
	lw	t8,24(a0)
	lw	t9,20(a0)
	lw	a3,16(a0)
	lw	a2,12(a0)
	lw	v0,8(a0)
	lw	s1,4(a0)
	lw	s0,0(a0)
	la	a0,.copy_tail_stores
	add	a0,a0,v1
	jr	a0
	nop
.copy_tail_stores:
	sw	t0,56(a1)
	sw	t1,52(a1)
	sw	t2,48(a1)
	sw	t3,44(a1)
	sw	t4,40(a1)
	sw	t5,36(a1)
	sw	t6,32(a1)
	sw	t7,28(a1)
	sw	t8,24(a1)
	sw	t9,20(a1)
	sw	a3,16(a1)
	sw	a2,12(a1)
	sw	v0,8(a1)
	sw	s1,4(a1)
	sw	s0,0(a1)
.copy_done:
	lw	s1,20(sp)
	lw	s0,16(sp)
	jr	ra
	addiu	sp,sp,24

; InterpolateOrthonormalMatrix(from, to, weight, out): blends two rotations, made orthonormal.
; In:   a0 from, a1 to, a2 to's weight (Q12), a3 out.
; Out:  out's rotation; its translation is not written.
; Uses: v0, v1, a0..a3, t0..t6, hi/lo, the GTE.
InterpolateOrthonormalMatrix:
	addiu	sp,sp,-56
	sw	ra,44(sp)
	sw	fp,40(sp)
	sw	s5,36(sp)
	sw	s4,32(sp)
	sw	s3,28(sp)
	sw	s2,24(sp)
	sw	s1,20(sp)
	sw	s0,16(sp)
	addu	s0,a0,zero
	addu	s1,a1,zero
	addu	s3,a2,zero
	addu	s2,a3,zero
	ori	s4,zero,ONE
	sub	s4,s4,s3
	lh	t0,M_m0(s0)
	lh	t1,M_m0+2(s0)
	lh	t2,M_m0+4(s0)
	mtc2	s4,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	lh	t3,M_m0(s1)
	lh	t4,M_m0+2(s1)
	GPF	1
	lh	t5,M_m0+4(s1)
	lh	a0,M_m1(s0)
	lh	a1,M_m1+2(s0)
	lh	a2,M_m1+4(s0)
	mtc2	s3,C2_IR0
	mtc2	t3,C2_IR1
	mtc2	t4,C2_IR2
	mtc2	t5,C2_IR3
	lh	s5,M_m1(s1)
	lh	fp,M_m1+2(s1)
	GPL	1
	lh	a3,M_m1+4(s1)
	nop
	jal	NormalizeGteIrVector
	nop
	mtc2	s4,C2_IR0
	mtc2	a0,C2_IR1
	mtc2	a1,C2_IR2
	mtc2	a2,C2_IR3
	addu	a0,t0,zero
	addu	a1,t1,zero
	GPF	1
	addu	a2,t2,zero
	sh	a0,M_m0(s2)
	sh	a1,M_m0+2(s2)
	sh	a2,M_m0+4(s2)
	mtc2	s3,C2_IR0
	mtc2	s5,C2_IR1
	mtc2	fp,C2_IR2
	mtc2	a3,C2_IR3
	nGPL	1
	nop
	nop
	mfc2	t0,C2_IR1
	mfc2	t1,C2_IR2
	mfc2	t2,C2_IR3
	mult	a0,t0
	mflo	t3
	nop
	nop
	mult	a1,t1
	mflo	t4
	nop
	nop
	mult	a2,t2
	mflo	t5
	nop
	add	t3,t3,t4
	add	t3,t3,t5
	sra	t3,t3,12
	sub	t3,zero,t3
	mtc2	t3,C2_IR0
	mtc2	a0,C2_IR1
	mtc2	a1,C2_IR2
	mtc2	a2,C2_IR3
	mtc2	t0,C2_MAC1
	mtc2	t1,C2_MAC2
	mtc2	t2,C2_MAC3
	nGPL	1
	nop
	nop
	jal	NormalizeGteIrVector
	nop
	cfc2	t3,C2_R11R12
	cfc2	t4,C2_R22R23
	cfc2	t5,C2_R33
	ctc2	a0,C2_R11R12
	ctc2	a1,C2_R22R23
	ctc2	a2,C2_R33
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	nOP	1
	sh	t0,M_m1(s2)
	sh	t1,M_m1+2(s2)
	sh	t2,M_m1+4(s2)
	mfc2	t0,C2_MAC1
	mfc2	t1,C2_MAC2
	mfc2	t2,C2_MAC3
	sh	t0,M_m2(s2)
	sh	t1,M_m2+2(s2)
	sh	t2,M_m2+4(s2)
	ctc2	t3,C2_R11R12
	ctc2	t4,C2_R22R23
	ctc2	t5,C2_R33
	lw	ra,44(sp)
	lw	fp,40(sp)
	lw	s5,36(sp)
	lw	s4,32(sp)
	lw	s3,28(sp)
	lw	s2,24(sp)
	lw	s1,20(sp)
	lw	s0,16(sp)
	jr	ra
	addiu	sp,sp,56

; NormalizeGteIrVector: the vector in IR1..IR3 scaled to length 4096.
; In:   IR1..IR3.
; Out:  t0..t2 the scaled vector.
; Uses: v0, v1, t3..t6, the GTE.
NormalizeGteIrVector:
	mfc2	t0,C2_IR1
	mfc2	t1,C2_IR2
	mfc2	t2,C2_IR3
	nSQR	0
	nop
	mfc2	t3,C2_MAC1
	mfc2	t4,C2_MAC2
	mfc2	t5,C2_MAC3
	add	t3,t3,t4
	add	v0,t3,t5
	mtc2	v0,C2_LZCS
	nop
	nop
	mfc2	v1,C2_LZCR
	nop
	andi	v1,v1,$1e
	ori	t6,zero,$1f
	sub	t6,t6,v1
	addi	t3,v1,-24
	bltz	t3,.norm_small
	sra	t6,t6,1
	j	.norm_lookup
	sllv	t4,v0,t3
.norm_small:
	ori	t3,zero,$18
	sub	t3,t3,v1
	srav	t4,v0,t3
.norm_lookup:
	addi	t4,t4,-64
	sll	t4,t4,1
	la	t5,gRecipSqrtLutQ12
	addu	t5,t5,t4
	lh	t5,0(t5)
	nop
	mtc2	t5,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	nGPF	0
	mfc2	t0,C2_MAC1
	mfc2	t1,C2_MAC2
	mfc2	t2,C2_MAC3
	srav	t0,t0,t6
	srav	t1,t1,t6
	srav	t2,t2,t6
	jr	ra
	nop
; .integer_sqrt: v0 = sqrt(a0) by gSqrtLutQ9, for ComputeVectorLength3D; uses t0, t1.
.integer_sqrt:
	mtc2	a0,C2_LZCS
	beq	a0,zero,.integer_sqrt_zero
	ori	t0,zero,$1f		; replaced before it is read
	mfc2	v0,C2_LZCR
	la	t1,gSqrtLutQ9
	andi	v0,v0,$1e
	addi	t0,v0,-24
	bltz	t0,.integer_sqrt_large
	nop
	sll	v0,a0,1
	add	v0,v0,t1
	lh	v0,0(v0)
	nop
	jr	ra
	srl	v0,v0,9
.integer_sqrt_large:
	ori	t0,zero,$18
	sub	t0,t0,v0
	srav	t0,a0,t0
	sll	t0,t0,1
	add	t0,t0,t1
	lh	t0,0(t0)
	ori	t1,zero,$1f
	sub	t1,t1,v0
	sra	t1,t1,1
	sllv	v0,t0,t1
	jr	ra
	srl	v0,v0,12
.integer_sqrt_zero:
	jr	ra
	addu	v0,zero,zero
	nop		; unreached
	nop

; ComputeVectorLength3D(x, y, z) -> the vector's length.
; In:   a0, a1, a2 the components, 16 bits each.
; Out:  v0.
; Uses: a0, t0..t2, the GTE.
ComputeVectorLength3D:
	mtc2	a0,C2_IR1
	mtc2	a1,C2_IR2
	mtc2	a2,C2_IR3
	nSQR	0
	nop
	mfc2	t0,C2_MAC1
	mfc2	t1,C2_MAC2
	mfc2	t2,C2_MAC3
	add	a0,t0,t1
	j	.integer_sqrt
	add	a0,a0,t2

; ComputeVectorLengthSq3D(x, y, z) -> the squared length.
; In:   a0, a1, a2 the components, 16 bits each.
; Out:  v0.
; Uses: t0..t2, the GTE.
ComputeVectorLengthSq3D:
	mtc2	a0,C2_IR1
	mtc2	a1,C2_IR2
	mtc2	a2,C2_IR3
	nSQR	0
	nop
	mfc2	t0,C2_MAC1
	mfc2	t1,C2_MAC2
	mfc2	t2,C2_MAC3
	add	v0,t0,t1
	jr	ra
	add	v0,v0,t2

; LerpSvector(a, b, t, out): unused; out = (a * (4096 - t) + b * t) / 4096.
; In:   a0 a, a1 b, a2 t (Q12), a3 out.
; Out:  out's x, y and z.
; Uses: t0..t3, the GTE.
LerpSvector:
	lh	t0,SVEC_vx(a0)
	lh	t1,SVEC_vy(a0)
	lh	t2,SVEC_vz(a0)
	ori	t3,zero,ONE
	sub	t3,t3,a2
	mtc2	t3,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	lh	t0,SVEC_vx(a1)
	wGPF	1
	lh	t1,SVEC_vy(a1)
	lh	t2,SVEC_vz(a1)
	mtc2	a2,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	nop
	wGPL	1
	mfc2	t0,C2_IR1		; replaced before it is read
	mfc2	t0,C2_IR1
	mfc2	t1,C2_IR2
	mfc2	t2,C2_IR3
	sh	t0,SVEC_vx(a3)
	sh	t1,SVEC_vy(a3)
	jr	ra
	sh	t2,SVEC_vz(a3)

; LerpMatrix3x3(a, b, t, out): unused; LerpSvector for each rotation row.
; In:   a0 a, a1 b, a2 t (Q12), a3 out.
; Out:  out's rotation; its translation is not written.
; Uses: v0, t0..t5, the GTE.
LerpMatrix3x3:
	lw	t0,M_m0(a0)
	lh	t2,M_m0+4(a0)
	srl	t1,t0,16
	ori	v0,zero,ONE
	sub	v0,v0,a2
	mtc2	v0,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	lw	t0,M_m0(a1)
	wGPF	1
	lh	t2,M_m0+4(a1)
	sra	t1,t0,16
	mtc2	a2,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	lw	t1,M_m1+2(a0)
	wGPL	1
	lh	t0,M_m1(a0)
	srl	t2,t1,16
	mfc2	t3,C2_IR1
	mfc2	t4,C2_IR2
	mfc2	t5,C2_IR3
	sh	t3,M_m0(a3)
	sh	t4,M_m0+2(a3)
	sh	t5,M_m0+4(a3)
	mtc2	v0,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	lw	t1,M_m1+2(a1)
	wGPF	1
	lh	t0,M_m1(a1)
	srl	t2,t1,16
	mtc2	a2,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	lw	t0,M_m2(a0)
	wGPL	1
	lh	t2,M_m2+4(a0)
	srl	t1,t0,16
	mfc2	t3,C2_IR1
	mfc2	t4,C2_IR2
	mfc2	t5,C2_IR3
	sh	t3,M_m1(a3)
	sh	t4,M_m1+2(a3)
	sh	t5,M_m1+4(a3)
	mtc2	v0,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	lw	t0,M_m2(a1)
	wGPF	1
	lh	t2,M_m2+4(a1)
	srl	t1,t0,16
	mtc2	a2,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t1,C2_IR2
	mtc2	t2,C2_IR3
	nop
	wGPL	1
	mfc2	t3,C2_IR1		; replaced before it is read
	mfc2	t3,C2_IR1
	mfc2	t4,C2_IR2
	mfc2	t5,C2_IR3
	sh	t3,M_m2(a3)
	sh	t4,M_m2+2(a3)
	jr	ra
	sh	t5,M_m2+4(a3)

#ifdef VERSION_JP
	xdef	GetPackedSinCos
	xdef	GetPackedSin
	xdef	GetPackedCos

; GetPackedSinCos: unused (Japanese build); the packed (cos << 16 | sin) word for an angle.
; In:   a0 the angle, t0 the table.
; Out:  v0.
; Uses: nothing else.
GetPackedSinCos:
	andi	v0,a0,$fff
	sll	v0,v0,2
	add	v0,v0,t0
	jr	ra
	lw	v0,0(v0)

; GetPackedSin: unused (Japanese build); its sine.
; In:   a0 the angle, t0 the table.
; Out:  v0.
; Uses: t0.
GetPackedSin:
	andi	v0,a0,$fff
	sll	v0,v0,2
	add	t0,t0,v0
	jr	ra
	lh	v0,0(t0)

; GetPackedCos: unused (Japanese build); its cosine.
; In:   a0 the angle, t0 the table.
; Out:  v0.
; Uses: t0.
GetPackedCos:
	andi	v0,a0,$fff
	sll	v0,v0,2
	add	t0,t0,v0
	jr	ra
	lh	v0,2(t0)
#endif

; MulMatrix3x3Fast(left, right, out): out = left x right for the rotations.
; In:   a0 left, a1 right, a2 out.
; Out:  out's rotation; the GTE's rotation is left's, its translation zero.
; Uses: v0, v1, t0..t7, t9, the GTE.
MulMatrix3x3Fast:
	lw	t0,M_r11r12(a0)
	lw	t1,M_r13r21(a0)
	lw	t2,M_r22r23(a0)
	lw	t3,M_r31r32(a0)
	lw	t4,M_r33(a0)
	lui	t9,$ffff
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	lw	t0,M_r11r12(a1)
	lw	t1,M_r13r21(a1)
	lw	t2,M_r22r23(a1)
	lw	t3,M_r31r32(a1)
	lw	t4,M_r33(a1)
	and	t5,t1,t9
	andi	t6,t0,$ffff
	or	t5,t5,t6
	mtc2	t5,C2_VXY0
	andi	t6,t3,$ffff
	mtc2	t6,C2_VZ0
	sll	v0,t2,16
	wMVMVA	1,0,0,0,0
	srl	v1,t0,16
	or	v0,v0,v1
	srl	v1,t3,16
	mfc2	t5,C2_IR1
	mfc2	t6,C2_IR2
	mfc2	t7,C2_IR3
	mtc2	v0,C2_VXY0
	mtc2	v1,C2_VZ0
	sh	t5,M_m0(a2)
	sh	t6,M_m1(a2)
	sh	t7,M_m2(a2)
	MVMVA	1,0,0,0,0
	and	v0,t2,t9
	andi	v1,t1,$ffff
	or	v0,v0,v1
	andi	v1,t4,$ffff
	mfc2	t5,C2_IR1
	mfc2	t6,C2_IR2
	mfc2	t7,C2_IR3
	mtc2	v0,C2_VXY0
	mtc2	v1,C2_VZ0
	sh	t5,M_m0+2(a2)
	sh	t6,M_m1+2(a2)
	sh	t7,M_m2+2(a2)
	MVMVA	1,0,0,0,0
	mfc2	t5,C2_IR1
	mfc2	t6,C2_IR2
	mfc2	t7,C2_IR3
	sh	t5,M_m0+4(a2)
	sh	t6,M_m1+4(a2)
	jr	ra
	sh	t7,M_m2+4(a2)

#ifdef VERSION_JP
	xdef	BuildRotationMatrixPackedLut
	xdef	BuildRotationMatrixSumAngles

; BuildRotationMatrixPackedLut: unused (Japanese build); BuildRotationMatrixFast by a packed table.
; In:   a0 the angles, a1 the matrix, v0 the table.
; Out:  its rotation.
; Uses: a2, a3, t0..t9, hi/lo, the GTE.
BuildRotationMatrixPackedLut:
	lh	t0,SVEC_vx(a0)
	lh	t1,SVEC_vy(a0)
	lh	t2,SVEC_vz(a0)
	andi	t0,t0,$fff
	andi	t1,t1,$fff
	andi	t2,t2,$fff
	sll	t0,t0,2
	sll	t1,t1,2
	sll	t2,t2,2
	add	t0,t0,v0
	add	t1,t1,v0
	add	t2,t2,v0
	lw	t0,0(t0)
	lw	t1,0(t1)
	lw	t2,0(t2)
	sra	t3,t0,16
	sra	t4,t1,16
	sra	t5,t2,16
	mtc2	t5,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t3,C2_IR2
	mtc2	t4,C2_IR3
	mfc2	t0,C2_IR1		; reads back what was just written: its low half, sign-extended
	wGPF	1
	mult	t0,t4
	sub	t7,zero,t1
	sh	t7,M_m2(a1)
	mfc2	t6,C2_IR1
	mfc2	t7,C2_IR2
	mfc2	t8,C2_IR3
	mtc2	t2,C2_IR0
	mtc2	t0,C2_IR1
	mtc2	t3,C2_IR2
	mtc2	t4,C2_IR3
	sh	t8,M_m0(a1)
	mflo	t9
	GPF	1
	sra	t9,t9,12
	mult	t3,t4
	mtc2	t1,C2_IR0
	sh	t9,M_m2+2(a1)
	mfc2	t8,C2_IR1
	mfc2	t9,C2_IR2
	mfc2	a2,C2_IR3
	mfc2	t1,C2_IR0		; reads back what was just written: its low half, sign-extended
	mtc2	t7,C2_MAC1
	sub	a3,zero,t6
	mtc2	a3,C2_MAC2
	mtc2	t8,C2_MAC3
	mtc2	t7,C2_IR3
	mflo	a3
	wGPL	1
	sh	a2,M_m1(a1)
	mult	t1,t6
	sra	a3,a3,12
	sh	a3,M_m2+4(a1)
	mfc2	t6,C2_IR1
	mfc2	t7,C2_IR2
	mfc2	t8,C2_IR3
	sh	t6,M_m1+2(a1)
	sh	t7,M_m1+4(a1)
	mflo	a2
	sh	t8,M_m0+4(a1)
	sra	a2,a2,12
	sub	a2,a2,t9
	jr	ra
	sh	a2,M_m0+2(a1)

; BuildRotationMatrixSumAngles: unused (Japanese build); the rotation by angle sums.
; In:   a0 the angles, a1 the matrix, v0 the table.
; Out:  its rotation.
; Uses: v0, v1, a1..a3, t0..t9, the GTE.
BuildRotationMatrixSumAngles:
	addu	v1,a1,zero
	lh	a1,SVEC_vx(a0)
	lh	a2,SVEC_vy(a0)
	lh	a3,SVEC_vz(a0)
	sll	a1,a1,2
	sll	a2,a2,2
	sll	a3,a3,2
	add	t0,a1,a2
	sub	t1,a1,a2
	add	t2,a1,a3
	sub	t3,a1,a3
	andi	t8,t0,$3ffc
	andi	t9,t1,$3ffc
	andi	t2,t2,$3ffc
	andi	t3,t3,$3ffc
	add	t0,t8,v0
	add	t1,t9,v0
	add	t2,t2,v0
	add	t3,t3,v0
	lw	t0,0(t0)
	lw	t1,0(t1)
	lw	t2,0(t2)
	lw	t3,0(t3)
	sra	t4,t0,16
	sra	t5,t1,16
	sra	t6,t2,16
	sra	t7,t3,16
	sub	t8,t0,t1
	sra	t8,t8,1
	sh	t8,M_m0+4(v1)
	sub	t9,t2,t3
	sra	t9,t9,1
	sh	t9,M_m1(v1)
	add	t8,t4,t5
	sra	t8,t8,1
	sh	t8,M_m1+2(v1)
	add	t9,t6,t7
	sra	t9,t9,1
	sh	t9,M_m2+4(v1)
	add	t4,a2,a3
	sub	t5,a2,a3
	add	t0,t8,a3
	sub	t1,t8,a3
	add	t2,t9,a3
	sub	t3,t9,a3
	andi	t0,t0,$3ffc
	andi	t1,t1,$3ffc
	andi	t2,t2,$3ffc
	andi	t3,t3,$3ffc
	add	t0,t0,v0
	add	t1,t1,v0
	add	t2,t2,v0
	add	t3,t3,v0
	lw	t0,0(t0)
	lw	t1,0(t1)
	lw	t2,0(t2)
	lw	t3,0(t3)
	add	t6,t1,t2
	sra	t7,t5,16
	add	t7,t7,t7
	add	t6,t6,t7
	sra	t7,t4,16
	add	t7,t7,t7
	sub	t7,t7,t3
	sub	t7,t7,t0
	add	t8,t6,t7
	mtc2	t8,C2_IR0
	sub	t9,t6,t7
	mtc2	t9,C2_IR1
	add	t4,t4,t4
	add	t5,t5,t5
	sra	t6,t0,16
	sra	t7,t3,16
	sub	t6,t7,t6
	sub	t6,t6,t4
	sra	t7,t1,16
	sra	t8,t2,16
	sub	t7,t8,t7
	add	t7,t7,t5
	add	t8,t6,t7
	mtc2	t8,C2_IR2
	sub	t9,t6,t7
	mtc2	t9,C2_IR3
	mfc2	t0,C2_IR0
	mfc2	t1,C2_IR1
	mfc2	t2,C2_IR2
	mfc2	t3,C2_IR3
	sra	t0,t0,2
	sh	t0,M_m0(v1)
	sra	t1,t1,2
	sh	t1,M_m2+2(v1)
	sra	t2,t2,2
	sh	t2,M_m0+2(v1)
	sra	t3,t3,2
	sh	t3,M_m2(v1)
	andi	a1,a1,$3ffc
	add	v0,a1,v0
	lh	t0,0(v0)
	nop
	sub	t0,zero,t0
	jr	ra
	sh	t0,M_m1+4(v1)
#endif

; TransformSvectorByMatrix(m, in, out): out = R x in + T, through IR1..IR3.
; In:   a0 the MATRIX, a1 the SVECTOR, a2 the VECTOR.
; Out:  out; the GTE's rotation and translation are m's.
; Uses: t0..t7, the GTE.
TransformSvectorByMatrix:
	lw	t0,M_r11r12(a0)
	lw	t1,M_r13r21(a0)
	lw	t2,M_r22r23(a0)
	lw	t3,M_r31r32(a0)
	lw	t4,M_r33(a0)
	lw	t5,M_t(a0)
	lw	t6,M_t+4(a0)
	lw	t7,M_t+8(a0)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	t5,C2_TRX
	ctc2	t6,C2_TRY
	ctc2	t7,C2_TRZ
	lwc2	C2_VXY0,SVEC_vx(a1)
	lwc2	C2_VZ0,SVEC_vz(a1)
	nop
	wMVMVA	1,0,0,0,0
	nop
	swc2	C2_IR1,VEC_vx(a2)
	swc2	C2_IR2,VEC_vy(a2)
	jr	ra
	swc2	C2_IR3,VEC_vz(a2)

; SetGteRotAndTransMatrix(m): loads m into the GTE's rotation and translation.
; In:   a0 the MATRIX.
; Out:  the GTE's rotation and translation.
; Uses: t0..t7.
SetGteRotAndTransMatrix:
	lw	t0,M_r11r12(a0)
	lw	t1,M_r13r21(a0)
	lw	t2,M_r22r23(a0)
	lw	t3,M_r31r32(a0)
	lw	t4,M_r33(a0)
	lw	t5,M_t(a0)
	lw	t6,M_t+4(a0)
	lw	t7,M_t+8(a0)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	t5,C2_TRX
	ctc2	t6,C2_TRY
	jr	ra
	ctc2	t7,C2_TRZ

; SetGteRotMatrixZeroTrans(m): unused; loads m's rotation and a zero translation.
; In:   a0 the MATRIX.
; Out:  the GTE's rotation and translation.
; Uses: t0..t4.
SetGteRotMatrixZeroTrans:
	lw	t0,M_r11r12(a0)
	lw	t1,M_r13r21(a0)
	lw	t2,M_r22r23(a0)
	lw	t3,M_r31r32(a0)
	lw	t4,M_r33(a0)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	jr	ra
	ctc2	zero,C2_TRZ

; TransformSvectorWithShortTrans(m, t, in, out): unused; out = R x in + t, t three halfwords.
; In:   a0 the MATRIX, a1 t, a2 the SVECTOR, a3 the VECTOR.
; Out:  out; the GTE's rotation is m's, its translation t.
; Uses: t0..t7, the GTE.
TransformSvectorWithShortTrans:
	lw	t0,M_r11r12(a0)
	lw	t1,M_r13r21(a0)
	lw	t2,M_r22r23(a0)
	lw	t3,M_r31r32(a0)
	lw	t4,M_r33(a0)
	lh	t5,0(a1)
	lh	t6,2(a1)
	lh	t7,4(a1)
	ctc2	t0,C2_R11R12
	ctc2	t1,C2_R13R21
	ctc2	t2,C2_R22R23
	ctc2	t3,C2_R31R32
	ctc2	t4,C2_R33
	ctc2	t5,C2_TRX
	ctc2	t6,C2_TRY
	ctc2	t7,C2_TRZ
	lwc2	C2_VXY0,SVEC_vx(a2)
	lwc2	C2_VZ0,SVEC_vz(a2)
	nop
	wMVMVA	1,0,0,0,0
	nop
	swc2	C2_IR1,VEC_vx(a3)
	swc2	C2_IR2,VEC_vy(a3)
	jr	ra
	swc2	C2_IR3,VEC_vz(a3)

; GetGteFarColor(color): the GTE's far colour, each channel shifted right by 4.
; In:   a0 the CVECTOR.
; Out:  its r, g and b.
; Uses: t0..t2.
GetGteFarColor:
	cfc2	t0,C2_RFC
	cfc2	t1,C2_GFC
	cfc2	t2,C2_BFC
	srl	t0,t0,4
	srl	t1,t1,4
	srl	t2,t2,4
	sb	t0,CV_r(a0)
	sb	t1,CV_g(a0)
	sb	t2,CV_b(a0)
	jr	ra
	nop

; TriggerDebugTrap(): stores zero to 0x1F800400, the word past the scratchpad.
; In:   nothing.
; Out:  nothing.
; Uses: t9.
TriggerDebugTrap:
	lui	t9,SCRATCHPAD>>16
	ori	t9,t9,SCRATCHPAD_SIZE
	sw	zero,0(t9)
	jr	ra
	nop

#ifdef VERSION_JP
	xdef	ProbeGteMvmva
	nop		; unreached

; ProbeGteMvmva: unused (Japanese build); one MVMVA of (4096, 4096, 4096) by matrix 3.
; In:   nothing.
; Out:  t0..t2 IR1..IR3, t3..t5 MAC1..MAC3, v0 ORGB.
; Uses: the GTE.
ProbeGteMvmva:
	ctc2	zero,C2_TRX
	ctc2	zero,C2_TRY
	ctc2	zero,C2_TRZ
	ctc2	zero,C2_RBK
	ctc2	zero,C2_GBK
	ctc2	zero,C2_BBK
	ctc2	zero,C2_RFC
	ctc2	zero,C2_GFC
	ctc2	zero,C2_BFC
	lui	t0,$1000
	ori	t0,t0,$1000
	lui	t1,$1000
	ori	t1,t1,$1000
	mtc2	t0,C2_VXY0
	mtc2	t1,C2_VZ0
	lui	t2,$3		; t2..t4 are zeroed before use
	ori	t2,t2,$201
	lui	t3,$6
	ori	t3,t3,$504
	lui	t4,$9
	ori	t4,t4,$807
	addu	t2,zero,zero
	addu	t3,zero,zero
	addu	t4,zero,zero
	mtc2	t2,C2_RGB0
	mtc2	t3,C2_RGB1
	mtc2	t4,C2_RGB2
	nop
	nMVMVA	1,3,0,3,1
	nop
	nop
	nop
	mfc2	t0,C2_IR1
	mfc2	t1,C2_IR2
	mfc2	t2,C2_IR3
	mfc2	t3,C2_MAC1
	mfc2	t4,C2_MAC2
	mfc2	t5,C2_MAC3
	mfc2	v0,C2_ORGB
	jr	ra
	nop
	nop		; unreached
	nop
	nop
	nop
#else
	nop		; unreached
	nop
	nop
	nop
#endif

	section	.rdata
; gSceneNodeRenderDispatch: by SceneNode.renderType.
gSceneNodeRenderDispatch:
	dw	RenderStandardSceneNode
	dw	.render_world_scene_node
	dw	.scene_node_done
	dw	.render_shadow_scene_node
	dw	.scene_node_done
	dw	.render_reused_matrix_node
	dw	.scene_node_done
	dw	.scene_node_done
	dw	.scene_node_done
	dw	.scene_node_done
	dw	.render_skinned_scene_node
	dw	.scene_node_done
	dw	.render_tile_scene_node
	dw	.render_cloth_scene_node
	dw	.render_ocean_scene_node
	dw	.render_pov_overlay_node
; gMeshBatchDispatch: by the high half of a mesh batch's word.
gMeshBatchDispatch:
	dw	RenderMeshBatchAlignedVerts
	dw	.mesh_batch_packed_verts
	dw	.mesh_batch_pov_overlay
; gTilePrimRenderDispatch: by a tile primitive's type >> 3.
gTilePrimRenderDispatch:
	dw	DrawSubdivPolyFt3Dynamic
	dw	DrawSubdivPolyFt4Dynamic
	dw	TriggerDebugTrap
	dw	TriggerDebugTrap
; gClothPrimRenderDispatch: by a cloth primitive's type >> 3.
gClothPrimRenderDispatch:
	dw	DrawClothPolyFt3DoubleLit
	dw	DrawClothPolyFt4DoubleLit
	dw	DrawPolyGt3DoubleLit
	dw	DrawPolyGt4DoubleLit
; gRecipSqrtLutQ12: entry i is 32768 / sqrt(64 + i), truncated.
gRecipSqrtLutQ12:
	dh	$1000,$0fe0,$0fc1,$0fa3,$0f85,$0f68,$0f4c,$0f30
	dh	$0f15,$0efb,$0ee1,$0ec7,$0eae,$0e96,$0e7e,$0e66
	dh	$0e4f,$0e38,$0e22,$0e0c,$0df7,$0de2,$0dcd,$0db9
	dh	$0da5,$0d91,$0d7e,$0d6b,$0d58,$0d45,$0d33,$0d21
	dh	$0d10,$0cff,$0cee,$0cdd,$0ccc,$0cbc,$0cac,$0c9c
	dh	$0c8d,$0c7d,$0c6e,$0c5f,$0c51,$0c42,$0c34,$0c26
	dh	$0c18,$0c0a,$0bfd,$0bef,$0be2,$0bd5,$0bc8,$0bbb
	dh	$0baf,$0ba2,$0b96,$0b8a,$0b7e,$0b72,$0b67,$0b5b
	dh	$0b50,$0b45,$0b39,$0b2e,$0b24,$0b19,$0b0e,$0b04
	dh	$0af9,$0aef,$0ae5,$0adb,$0ad1,$0ac7,$0abd,$0ab4
	dh	$0aaa,$0aa1,$0a97,$0a8e,$0a85,$0a7c,$0a73,$0a6a
	dh	$0a61,$0a59,$0a50,$0a47,$0a3f,$0a37,$0a2e,$0a26
	dh	$0a1e,$0a16,$0a0e,$0a06,$09fe,$09f6,$09ef,$09e7
	dh	$09e0,$09d8,$09d1,$09c9,$09c2,$09bb,$09b4,$09ad
	dh	$09a5,$099e,$0998,$0991,$098a,$0983,$097c,$0976
	dh	$096f,$0969,$0962,$095c,$0955,$094f,$0949,$0943
	dh	$093c,$0936,$0930,$092a,$0924,$091e,$0918,$0912
	dh	$090d,$0907,$0901,$08fb,$08f6,$08f0,$08eb,$08e5
	dh	$08e0,$08da,$08d5,$08cf,$08ca,$08c5,$08bf,$08ba
	dh	$08b5,$08b0,$08ab,$08a6,$08a1,$089c,$0897,$0892
	dh	$088d,$0888,$0883,$087e,$087a,$0875,$0870,$086b
	dh	$0867,$0862,$085e,$0859,$0855,$0850,$084c,$0847
	dh	$0843,$083e,$083a,$0836,$0831,$082d,$0829,$0824
	dh	$0820,$081c,$0818,$0814,$0810,$080c,$0808,$0804
; gSqrtLutQ9: entry i is sqrt(i) * 512, truncated.
gSqrtLutQ9:
	dh	$0000,$0200,$02d4,$0376,$0400,$0478,$04e6,$054a
	dh	$05a8,$0600,$0653,$06a2,$06ed,$0736,$077b,$07be
	dh	$0800,$083f,$087c,$08b7,$08f1,$092a,$0961,$0997
	dh	$09cc,$0a00,$0a32,$0a64,$0a95,$0ac5,$0af4,$0b22
	dh	$0b50,$0b7d,$0ba9,$0bd5,$0c00,$0c2a,$0c54,$0c7d
	dh	$0ca6,$0cce,$0cf6,$0d1d,$0d44,$0d6a,$0d90,$0db6
	dh	$0ddb,$0e00,$0e24,$0e48,$0e6c,$0e8f,$0eb2,$0ed5
	dh	$0ef7,$0f19,$0f3b,$0f5c,$0f7d,$0f9e,$0fbf,$0fdf
	dh	$1000,$101f,$103f,$105e,$107e,$109c,$10bb,$10da
	dh	$10f8,$1116,$1134,$1152,$116f,$118c,$11a9,$11c6
	dh	$11e3,$1200,$121c,$1238,$1254,$1270,$128c,$12a7
	dh	$12c2,$12de,$12f9,$1314,$132e,$1349,$1364,$137e
	dh	$1398,$13b2,$13cc,$13e6,$1400,$1419,$1432,$144c
	dh	$1465,$147e,$1497,$14b0,$14c8,$14e1,$14f9,$1512
	dh	$152a,$1542,$155a,$1572,$158a,$15a2,$15b9,$15d1
	dh	$15e8,$1600,$1617,$162e,$1645,$165c,$1673,$1689
	dh	$16a0,$16b7,$16cd,$16e4,$16fa,$1710,$1726,$173c
	dh	$1752,$1768,$177e,$1794,$17aa,$17bf,$17d5,$17ea
	dh	$1800,$1815,$182a,$183f,$1854,$1869,$187e,$1893
	dh	$18a8,$18bd,$18d1,$18e6,$18fa,$190f,$1923,$1938
	dh	$194c,$1960,$1974,$1988,$199c,$19b0,$19c4,$19d8
	dh	$19ec,$1a00,$1a13,$1a27,$1a3a,$1a4e,$1a61,$1a75
	dh	$1a88,$1a9b,$1aae,$1ac2,$1ad5,$1ae8,$1afb,$1b0e
	dh	$1b21,$1b33,$1b46,$1b59,$1b6c,$1b7e,$1b91,$1ba3
	dh	$1bb6,$1bc8,$1bdb,$1bed,$1c00,$1c12,$1c24,$1c36
	dh	$1c48,$1c5a,$1c6c,$1c7e,$1c90,$1ca2,$1cb4,$1cc6
	dh	$1cd8,$1ce9,$1cfb,$1d0d,$1d1e,$1d30,$1d41,$1d53
	dh	$1d64,$1d76,$1d87,$1d98,$1daa,$1dbb,$1dcc,$1ddd
	dh	$1dee,$1e00,$1e11,$1e22,$1e33,$1e43,$1e54,$1e65
	dh	$1e76,$1e87,$1e98,$1ea8,$1eb9,$1eca,$1eda,$1eeb
	dh	$1efb,$1f0c,$1f1c,$1f2d,$1f3d,$1f4e,$1f5e,$1f6e
	dh	$1f7e,$1f8f,$1f9f,$1faf,$1fbf,$1fcf,$1fdf,$1fef
