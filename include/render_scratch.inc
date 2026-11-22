; The renderer's scratchpad (render_scratch.h), its vertex buffers sized as MeshScratch's.

SCRATCHPAD	equ	$1f800000
SCRATCHPAD_SIZE	equ	$400
MESH_SCRATCH_MAX_VERTICES	equ	165

		rsreset
RS_vertices		rw	1
RS_dispatchReturn	rw	1
RS_scaleShift		rw	1
RS_orderingTable	rw	1
RS_fogFarOtz		rw	1
RS_doubleBufferMask	rw	1
RS_stageTileMinOtz	rw	1
RS_primTable		rw	1
RS_screenXy		rw	MESH_SCRATCH_MAX_VERTICES
RS_screenZ		rh	MESH_SCRATCH_MAX_VERTICES
