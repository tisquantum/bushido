#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.globl	srand
srand:
	KERNEL_JUMP(0xa0, 48)
