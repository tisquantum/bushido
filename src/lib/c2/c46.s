#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.globl	memchr
memchr:
	KERNEL_JUMP(0xa0, 46)
