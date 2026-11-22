; What the renderer takes from libgte.h.

ONE		equ	4096

; MATRIX.
		rsreset
M_m0		rh	3
M_m1		rh	3
M_m2		rh	3
		rh	1
M_t		rw	3
MATRIX_SIZE	rb	0
M_r11r12	equ	M_m0
M_r13r21	equ	M_m0+4
M_r22r23	equ	M_m1+2
M_r31r32	equ	M_m2
M_r33		equ	M_m2+4

; SVECTOR, VECTOR and CVECTOR.
		rsreset
SVEC_vx		rh	1
SVEC_vy		rh	1
SVEC_vz		rh	1
SVEC_pad	rh	1
		rsreset
VEC_vx		rw	1
VEC_vy		rw	1
VEC_vz		rw	1
VEC_pad		rw	1
		rsreset
CV_r		rb	1
CV_g		rb	1
CV_b		rb	1
CV_cd		rb	1
