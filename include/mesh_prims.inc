; The drawers' primitive records and the depth limits they test.

; Depth limits, against a face's mean SZ (a triangle's AVSZ3 times four).
DEPTH_LIMIT		equ	$10000
DEPTH_LIMIT_G		equ	$4000
DEPTH_LIMIT_FAR_CLIP	equ	$1000

; A mesh record: its packet once per packet bank, its vertex indices (one word, a byte each, the
; first in the low byte), then what lights it.
		rsreset
MF3_prim		rb	2*F3_SIZE
MF3_vertices		rw	1
MF3_NO_NORMAL_SIZE	rb	0
MF3_normal		rh	4
MF3_color		rw	1
MF3_SIZE		rb	0
		rsreset
MF4_prim		rb	2*F4_SIZE
MF4_vertices		rw	1
MF4_NO_NORMAL_SIZE	rb	0
MF4_normal		rh	4
MF4_color		rw	1
MF4_SIZE		rb	0
		rsreset
MG3_prim		rb	2*G3_SIZE
MG3_vertices		rw	1
MG3_normal0		rh	4
MG3_color0		rw	1
MG3_normal1		rh	4
MG3_color1		rw	1
MG3_normal2		rh	4
MG3_color2		rw	1
MG3_SIZE		rb	0
		rsreset
MG4_prim		rb	2*G4_SIZE
MG4_vertices		rw	1
MG4_normal0		rh	4
MG4_color0		rw	1
MG4_normal1		rh	4
MG4_color1		rw	1
MG4_normal2		rh	4
MG4_color2		rw	1
MG4_normal3		rh	4
MG4_color3		rw	1
MG4_SIZE		rb	0
		rsreset
MFT3_prim		rb	2*FT3_SIZE
MFT3_vertices		rw	1
MFT3_NO_NORMAL_SIZE	rb	0
MFT3_normal		rh	4
MFT3_SIZE		rb	0
		rsreset
MFT4_prim		rb	2*FT4_SIZE
MFT4_vertices		rw	1
MFT4_NO_NORMAL_SIZE	rb	0
MFT4_normal		rh	4
MFT4_SIZE		rb	0
		rsreset
MGT3_prim		rb	2*GT3_SIZE
MGT3_vertices		rw	1
MGT3_NO_NORMAL_SIZE	rb	0
MGT3_normalXy0		rw	1
MGT3_normalZ0		rh	1
MGT3_normalZ1		rh	1
MGT3_normalXy1		rw	1
MGT3_normalXy2		rw	1
MGT3_normalZ2		rh	1
			rh	1
MGT3_SIZE		rb	0
		rsreset
MGT4_prim		rb	2*GT4_SIZE
MGT4_vertices		rw	1
MGT4_NO_NORMAL_SIZE	rb	0
MGT4_normalXy0		rw	1
MGT4_normalZ0		rh	1
MGT4_normalZ1		rh	1
MGT4_normalXy1		rw	1
MGT4_normalXy2		rw	1
MGT4_normalZ2		rh	1
MGT4_normalZ3		rh	1
MGT4_normalXy3		rw	1
MGT4_SIZE		rb	0
; A skinned prelit record keeps its corners' indices into gSkinnedPrelitColors in the halfwords its two
; packets leave unused: a triangle's three packed in one word over both packets, a quad's one a halfword.
MFT3_prelitHigh		equ	MFT3_prim+FT_pad1
MFT3_prelitLow		equ	MFT3_prim+FT3_SIZE+FT_pad1
MFT4_prelit0		equ	MFT4_prim+FT_pad1
MFT4_prelit1		equ	MFT4_prim+FT_pad2
MFT4_prelit2		equ	MFT4_prim+FT4_SIZE+FT_pad1
MFT4_prelit3		equ	MFT4_prim+FT4_SIZE+FT_pad2
MGT3_prelitHigh		equ	MGT3_prim+GT_pad2
MGT3_prelitLow		equ	MGT3_prim+GT3_SIZE+GT_pad2
MGT4_prelit0		equ	MGT4_prim+GT_pad2
MGT4_prelit1		equ	MGT4_prim+GT_pad3
MGT4_prelit2		equ	MGT4_prim+GT4_SIZE+GT_pad2
MGT4_prelit3		equ	MGT4_prim+GT4_SIZE+GT_pad3

		rsreset
MF4DM_prim		rb	2*F4_SIZE
MF4DM_vertices		rw	1
MF4DM_depthOffset	rw	1
MF4DM_mode		rb	2*DR_MODE_SIZE
MF4DM_SIZE		rb	0

; A dynamic mesh's faces.
		rsreset
DTRI_tpage	rh	1
		rb	1
DTRI_code	rb	1
DTRI_clut	rh	1
DTRI_uv0	rh	1
DTRI_uv1	rh	1
DTRI_uv2	rh	1
DTRI_vertices	rw	1
DTRI_normal	rh	4
DTRI_SIZE	rb	0
		rsreset
DQUAD_tpage	rh	1
DQUAD_flags	rb	1	; bit 0 drawn deeper, bits 1-2 split fewer times
DQUAD_code	rb	1
DQUAD_clut	rh	1
DQUAD_uv0	rh	1
DQUAD_uv1	rh	1
DQUAD_uv2	rh	1
DQUAD_uv3	rh	1
DQUAD_vertices	rh	2
DQUAD_normalZ	rh	1
DQUAD_normalXY	rw	1
DQUAD_SIZE	rb	0
		rsset	DTRI_normal
DGT3_normalXy0	rw	1
DGT3_normalZ0	rh	1
DGT3_normalZ1	rh	1
DGT3_normalXy1	rw	1
DGT3_normalXy2	rw	1
DGT3_normalZ2	rh	1
		rh	1
DGT3_SIZE	rb	0
		rsset	DQUAD_uv3+2
		rh	1
DGT4_vertices	rw	1
DGT4_normalXy0	rw	1
DGT4_normalZ0	rh	1
DGT4_normalZ1	rh	1
DGT4_normalXy1	rw	1
DGT4_normalXy2	rw	1
DGT4_normalZ2	rh	1
DGT4_normalZ3	rh	1
DGT4_normalXy3	rw	1
DGT4_SIZE	rb	0

; A skinned mesh's vertex lists, five per bone in the order SKVO, SKFO, SKV, SKF, SKV, each led by its count:
; vertices in the bone's space (SKV), the same with an offset added after the bone's transform (SKVO), and
; vertices placed from an earlier vertex by its index (SKF, SKFO). Its seams (SEAM) follow the primitive
; groups, ended by $ffff.
		rsreset
SKV_x		rh	1
SKV_y		rh	1
SKV_z		rh	1
SKV_SIZE	rb	0
		rsreset
SKVO_x		rh	1
SKVO_y		rh	1
SKVO_z		rh	1
SKVO_dx		rh	1
SKVO_dy		rh	1
SKVO_dz		rh	1
SKVO_SIZE	rb	0
		rsreset
SKF_base	rh	1
SKF_x		rh	1
SKF_y		rh	1
SKF_z		rh	1
SKF_SIZE	rb	0
		rsreset
SKFO_base	rh	1
SKFO_x		rh	1
SKFO_y		rh	1
SKFO_z		rh	1
SKFO_dx		rh	1
SKFO_dy		rh	1
SKFO_dz		rh	1
SKFO_SIZE	rb	0
		rsreset
SEAM_source	rh	1	; copied downward from here
SEAM_dest	rh	1	; and upward from here
SEAM_count	rh	1
SEAM_SIZE	rb	0
