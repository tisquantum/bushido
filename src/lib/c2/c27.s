#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.globl	strlen
strlen:
	KERNEL_JUMP(0xa0, 27)
