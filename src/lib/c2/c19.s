#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.globl	setjmp
setjmp:
	KERNEL_JUMP(0xa0, 19)
