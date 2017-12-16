#include "gdt.h"

.code32

.global bioscall
bioscall:
	push %ebp
	push %ebx
	push %esi
	push %edi

	// Save pointer to bios_regs_t parameter on stack
	push %eax

	// Use ebp for accessing the stack
	mov %eax,%ebp

	// Save interrupt vector on the stack
	pushl (,%edx,4)

	call cpu_a20_exitpm

	// Jump to 16 bit protected mode to load 64KB segment limits
	ljmp $GDT_SEL_KERNEL_CODE16,$0f
0:
.code16
	// 16-bit protected mode

	// Load 16-bit data segments
	mov $GDT_SEL_KERNEL_DATA16,%edx
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	mov %dx,%gs
	mov %dx,%ss

	// Turn off protected mode
	mov %cr0,%edx
	btr $0,%edx
	mov %edx,%cr0

	// Real mode
	ljmp $0,$0f
0:
	xor %edx,%edx
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	mov %dx,%gs
	mov %dx,%ss

	// Sane enough to allow BIOS to handle IRQs now
	sti

	mov   (%ebp),%eax
	mov  4(%ebp),%ebx
	mov  8(%ebp),%ecx
	mov 12(%ebp),%edx
	mov 16(%ebp),%esi
	mov 20(%ebp),%edi
	// ...do ebp last
	mov 28(%ebp),%ds
	mov 30(%ebp),%es
	mov 32(%ebp),%fs
	mov 34(%ebp),%gs
	// Lose ebp last
	mov 24(%ebp),%ebp
	// Ignore incoming eflags

	// Simulate interrupt call
	pushfw
	lcallw *2(%esp)

	// Save returned eflags
	pushfl

	// Save returned ebp
	push %ebp

	// get regs pointer off stack into ebp
	mov 12(%esp),%ebp

	// Write back output registers to regs
	mov %eax,  (%ebp)
	mov %ebx, 4(%ebp)
	mov %ecx, 8(%ebp)
	mov %edx,12(%ebp)
	mov %esi,16(%ebp)
	mov %edi,20(%ebp)
	// store ebp last
	mov %ds ,28(%ebp)
	mov %es ,30(%ebp)
	mov %fs ,32(%ebp)
	mov %gs ,34(%ebp)

	// ecx = saved ebp, edx = saved flags
	pop %ecx
	pop %edx

	// Write back output ebp and flags
	mov %ecx,24(%ebp)
	mov %edx,36(%ebp)

	// Go back into protected mode
	cli
	lgdt gdtr

	// Turn on protected mode
	mov %cr0,%edx
	bts $0,%edx
	mov %edx,%cr0

	ljmpw $GDT_SEL_KERNEL_CODE32,$0f
0:
.code32
	mov $GDT_SEL_KERNEL_DATA,%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	mov %ax,%gs
	mov %ax,%ss

	add $8,%esp

	call cpu_a20_enterpm

	pop %edi
	pop %esi
	pop %ebx
	pop %ebp
	ret
