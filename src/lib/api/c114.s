#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.word	0x0a007350, 0x0036384b	/* "Ps", a byte that differs by library, then five more; nothing reads them */
	.globl	_96_remove
_96_remove:
	KERNEL_JUMP(0xa0, 114)
	nop
	nop
