#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.globl	printf
printf:
	KERNEL_JUMP(0xa0, 63)
