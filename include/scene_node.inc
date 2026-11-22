; SceneNode (scene_node.h); include libgte.inc first.

		rsreset
SN_renderType		rh	1
SN_rotation		rh	4	; SVECTOR
			rh	1
SN_localMatrix		rb	MATRIX_SIZE
SN_worldMatrix		rb	MATRIX_SIZE
SN_unused0		rw	4
SN_matrixState		rh	1
			rh	1
SN_parent		rw	1
SN_modelSlot		rh	1
SN_meshIndex		rh	1
SN_rotOrder		rw	1
SN_unused1		rh	1
SN_lightSlot		rh	1
SN_unused2		rh	1
SN_flags		rh	1
SN_param		rw	1
SN_clothSlotOrHeading	rw	1
SN_clothWorkOrFade	rw	1
SN_clothColliders	rw	1
SCENE_NODE_SIZE		rb	0
SN_propFade		equ	SN_clothWorkOrFade
