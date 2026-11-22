; The GPU packets the renderer writes (libgpu.h): their words, sizes, tag lengths and codes.

; POLY_FT3 and POLY_FT4.
		rsreset
FT_tag		rw	1
FT_r0		rw	1
FT_x0		rw	1
FT_u0		rw	1
FT_x1		rw	1
FT_u1		rw	1
FT_x2		rw	1
FT_u2		rw	1
FT3_SIZE	rb	0
FT_x3		rw	1
FT_u3		rw	1
FT4_SIZE	rb	0
FT_pad1		equ	FT_u2+2
FT_pad2		equ	FT_u3+2

; POLY_F3 and POLY_F4.
		rsreset
F_tag		rw	1
F_r0		rw	1
F_x0		rw	1
F_x1		rw	1
F_x2		rw	1
F3_SIZE		rb	0
F_x3		rw	1
F4_SIZE		rb	0

; POLY_G3 and POLY_G4.
		rsreset
G_tag		rw	1
G_r0		rw	1
G_x0		rw	1
G_r1		rw	1
G_x1		rw	1
G_r2		rw	1
G_x2		rw	1
G3_SIZE		rb	0
G_r3		rw	1
G_x3		rw	1
G4_SIZE		rb	0

; POLY_GT3 and POLY_GT4.
		rsreset
GT_tag		rw	1
GT_r0		rw	1
GT_x0		rw	1
GT_u0		rw	1
GT_r1		rw	1
GT_x1		rw	1
GT_u1		rw	1
GT_r2		rw	1
GT_x2		rw	1
GT_u2		rw	1
GT3_SIZE	rb	0
GT_r3		rw	1
GT_x3		rw	1
GT_u3		rw	1
GT4_SIZE	rb	0
GT_pad2		equ	GT_u2+2
GT_pad3		equ	GT_u3+2

; DR_MODE.
		rsreset
DRM_tag		rw	1
DRM_code	rw	2
DR_MODE_SIZE	rb	0

F3_TAG		equ	4<<24
F4_TAG		equ	5<<24
G3_TAG		equ	6<<24
G4_TAG		equ	8<<24
FT3_TAG		equ	7<<24
FT4_TAG		equ	9<<24
GT3_TAG		equ	9<<24
GT4_TAG		equ	12<<24
DR_MODE_TAG	equ	2<<24
F3_CODE		equ	$20
F4_CODE		equ	$28
GT3_CODE	equ	$34
GT4_CODE	equ	$3c
OT_ADDR_MASK	equ	$ffffff
