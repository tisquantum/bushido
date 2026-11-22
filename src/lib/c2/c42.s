#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.globl	memcpy
memcpy:
	KERNEL_JUMP(0xa0, 42)
