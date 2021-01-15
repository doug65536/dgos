#include "x86/cpu_constants.h"
#include "x86/gdt_sel.h"
#include "../kernel/arch/x86_64/cfi_helpers.h"

.code64
.section .text.code64, "ax", @progbits

// void arch_run_kernel(void *p);

.global arch_run_kernel
code64_run_kernel:
	.cfi_startproc
	mov $ 0x4B,%eax
	movw %ax,%ds
	movw %ax,%es
	movw %ax,%fs
	movw %ax,%gs
	xor %eax,%eax
	movw %ax,%ss
	nop
	mov (%rdi),%rax
	mov 8(%rdi),%ecx
	call *%rax
	ret
	.cfi_endproc

.code32
// void run_kernel(void (**entry)(void *), void **arg)
.global run_kernel
run_kernel:
	.cfi_startproc simple
	.cfi_def_cfa esp,4
	.cfi_offset eip,-1*4

	pushl %esi
	.cfi_adjust_cfa_offset 4
	.cfi_offset esi,-2*4

	movl %eax,%esi

	pushl %edi
	.cfi_adjust_cfa_offset 4
	.cfi_offset edi,-3*4

	movl %edx,%edi

	pushl %ebx
	.cfi_adjust_cfa_offset 4
	.cfi_offset edi,-12


	// Align stack
	pushl %ebp
	.cfi_adjust_cfa_offset 4
	.cfi_offset ebp,-16

	movl %esp,%ebp
	.cfi_def_cfa_register ebp

	andl $ -16,%esp

	// Set CR3 (page table physaddr)
	call paging_root_addr
	movl %eax,%cr3

	// Enable PAE
	movl %cr4,%eax
	orl $ 1 << CPU_CR4_PAE_BIT,%eax
	orl gp_available,%eax
	movl %eax,%cr4

	// Set NX if supported, and set LME always
	movl $ CPU_MSR_EFER,%ecx
	rdmsr
	movzbl nx_available,%eax
	negl %eax
	andl $ 1 << CPU_MSR_EFER_NX_BIT,%eax
	orl $ 1 << CPU_MSR_EFER_LME_BIT,%eax
	wrmsr

	// Enable paging
	movl %cr0,%eax
	orl $ 1 << CPU_CR0_PG_BIT,%eax
	movl %eax,%cr0

	// Far jump to long mode code segment

	pushl $ GDT_SEL_KERNEL_CODE64
	.cfi_adjust_cfa_offset 4

	pushl $ 0f
	.cfi_adjust_cfa_offset 4

	lretl
	.cfi_adjust_cfa_offset -8

.code64
0:	// 64 bit code won't preserve esi,edi
	movl %esi,%r12d
	movl %edi,%r13d

	// Avoid 64 bit relocation
	movl $ 6+idtr_64,%eax
	lidtq (%rax)

    // uses ecx for parameter because of mregparm=3 ABI
	movl %edi,%ecx
	callq *(%rsi)

	// Restore esi,edi
	movl %r12d,%esi
	movl %r13d,%edi

	// Far jump to 32 bit code segment
	// Use registers to avoid unsupported relocations
	movl $ GDT_SEL_PM_CODE32,%edx
    pushq %rdx
	movl $ 0f,%eax
	pushq %rax
	lretq

.code32
0:	// 32 bit
	// Paging off
	movl %cr0,%eax
	andl $ ~(1 << CPU_CR0_PG_BIT),%eax
	movl %eax,%cr0

	movl $ CPU_MSR_EFER,%ecx
	rdmsr
	andl $ ~(1 << CPU_MSR_EFER_LME_BIT),%eax
	wrmsr

	lidtl 2+idtr_16

	movl %ebp,%esp
	.cfi_def_cfa_register esp

	leavel
	.cfi_adjust_cfa_offset -8
	.cfi_same_value ebp

	popl %edi
	.cfi_adjust_cfa_offset -8
	.cfi_same_value edi

	popl %esi
	.cfi_adjust_cfa_offset -8
	.cfi_same_value esi

	retl
	.cfi_endproc

