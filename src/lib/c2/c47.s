#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.globl	rand
rand:
	KERNEL_JUMP(0xa0, 47)
