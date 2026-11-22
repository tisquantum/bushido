#include <asm.h>
#include <kernel_call.h>

	.set	noreorder
	.text
	.word	0x0a017350, 0x1036d11a	/* "Ps", a byte that differs by library, then five more; nothing reads them */
	.globl	puts
puts:
	KERNEL_JUMP(0xb0, 63)
	nop
	nop
