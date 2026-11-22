#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.globl	firstfile
firstfile:
	KERNEL_JUMP(0xb0, 66)
