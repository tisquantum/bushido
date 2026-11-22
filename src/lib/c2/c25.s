#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.globl	strcpy
strcpy:
	KERNEL_JUMP(0xa0, 25)
