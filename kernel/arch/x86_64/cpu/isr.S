.section .text.isr, "ax"

#include "asm_constants.h"
#include "control_regs_constants.h"
#include "../cfi_helpers.h"
#include "interrupts.h"

// This must be position independent code because the GDB stub clones off
// a redundant copy to protect against infinite recursion


.macro isr_entry has_code int_num
.global isr_entry_\int_num\()
.hidden isr_entry_\int_num\()
.align 16
isr_entry_\int_num\():
	.cfi_startproc
	.if \has_code == 0
		.cfi_def_cfa_offset 16
		push_cfi $0
	.else
		.cfi_def_cfa_offset 24
	.endif
	push_cfi $\int_num
	jmp isr_common
	.cfi_endproc
.endm

// Software interrupts
// 72 thru 74 moved close
.irp int_num,75,76,77,78,79
	isr_entry 0 \int_num
.endr
.irp int_num,80,81,82,83,84,85,86,87
	isr_entry 0 \int_num
.endr
.irp int_num,88,89,90,91,92,93,94,95
	isr_entry 0 \int_num
.endr
.irp int_num,96,97,98,99,100,101,102,103
	isr_entry 0 \int_num
.endr
.irp int_num,104,105,106,107,108,109,110,111
	isr_entry 0 \int_num
.endr
.irp int_num,112,113,114,115,116,117,118,119
	isr_entry 0 \int_num
.endr
.irp int_num,120,121,122,123,124,125,126,127
	isr_entry 0 \int_num
.endr
.irp int_num,128,129,130,131,132,133,134,135
	isr_entry 0 \int_num
.endr
.irp int_num,136,137,138,139,140,141,142,143
	isr_entry 0 \int_num
.endr
.irp int_num,144,145,146,147,148,149,150,151
	isr_entry 0 \int_num
.endr
.irp int_num,152,153,154,155,156,157,158,159
	isr_entry 0 \int_num
.endr
.irp int_num,160,161,162,163,164,165,166,167
	isr_entry 0 \int_num
.endr
.irp int_num,168,169,170,171,172,173,174,175
	isr_entry 0 \int_num
.endr
.irp int_num,176,177,178,179,180,181,182,183
	isr_entry 0 \int_num
.endr
.irp int_num,184,185,186,187,188,189,190,191
	isr_entry 0 \int_num
.endr
.irp int_num,192,193,194,195,196,197,198,199
	isr_entry 0 \int_num
.endr
.irp int_num,200,201,202,203,204,205,206,207
	isr_entry 0 \int_num
.endr
.irp int_num,208,209,210,211,212,213,214,215
	isr_entry 0 \int_num
.endr
.irp int_num,216,217,218,219,220,221,222,223
	isr_entry 0 \int_num
.endr
.irp int_num,224,225,226,227,228,229,230,231
	isr_entry 0 \int_num
.endr
.irp int_num,232,233,234,235,236,237,238,239
	isr_entry 0 \int_num
.endr
.irp int_num,240,241,242,243,244,245,246,247
	isr_entry 0 \int_num
.endr
.irp int_num,248,249,250,251,252,253,254,255
	isr_entry 0 \int_num
.endr

// Exception handlers (32 exception handlers)
isr_entry 0 0
isr_entry 0 1
isr_entry 0 2
isr_entry 0 3
isr_entry 0 4
isr_entry 0 5
isr_entry 0 6
isr_entry 0 7
isr_entry 1 8
isr_entry 0 9
isr_entry 1 10
isr_entry 1 11
isr_entry 1 12
isr_entry 1 13
// 14 moved close to handler
isr_entry 0 15
isr_entry 0 16
isr_entry 1 17
isr_entry 0 18
isr_entry 0 19
isr_entry 0 20
isr_entry 0 21
isr_entry 0 22
isr_entry 0 23
isr_entry 0 24
isr_entry 0 25
isr_entry 0 26
isr_entry 0 27
isr_entry 0 28
isr_entry 0 29
isr_entry 1 30
isr_entry 0 31

// PIC IRQ handlers (16 IRQs)
.irp int_num,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
	isr_entry 0 \int_num
.endr

// APIC handlers (24 IRQs)
.irp int_num,64,65,66,67,68,69,70,71
	isr_entry 0 \int_num
.endr
.irp int_num,56,57,58,59,60,61,62,63
	isr_entry 0 \int_num
.endr
.irp int_num,48,49,50,51,52,53,54,55
	isr_entry 0 \int_num
.endr

.irp int_num,72,73
	isr_entry 0 \int_num
.endr

isr_entry 0 74

isr_entry 1 14

.type isr_common,@function
.align 16
isr_common:
	.cfi_startproc
	.cfi_def_cfa_offset 24

	// Save call-clobbered registers
	// (in System-V parameter order in memory)
	push_cfi %rbp
	mov %rsp,%rbp

	//  1*8(%rbp) -> interrupt
	//  2*8(%rbp) -> error code
	//  3*8(%rbp) -> rip
	//  4*8(%rbp) -> cs
	//  5*8(%rbp) -> flags
	//  6*8(%rbp) -> rsp
	//  7*8(%rbp) -> ss

	push_cfi %r15
	push_cfi %r14
	push_cfi %r13
	push_cfi %r12
	push_cfi %r11
	push_cfi %r10
	push_cfi %rbx
	push_cfi %rax
	cld
	push_cfi %r9
	mov %cr3,%rax
	push_cfi %r8
	push_cfi %rcx
	push_cfi %rdx
	push_cfi %rsi
	push_cfi %rdi

	// Register usage:
	//  r13: segment register values at interrupt entry

	push_cfi %rax

	// Get segment registers packed into %r13
	mov %gs,%eax
	mov %es,%r13d
	shl $16,%eax
	shl $16,%r13d
	mov %fs,%ax
	mov %ds,%r13w
	shl $32,%rax
	or %rax,%r13

	push_cfi %r13

	// See if we are coming from kernel code
	cmpl $GDT_SEL_KERNEL_CODE64,4*8(%rbp)
	je .Lfrom_kernel

	// ...came from user code
	swapgs

.Lfrom_kernel:

	// Save entire sse/mmx/fpu state
	// NOTE: this instruction may be patched to call one of the xsave versions!
	jmp isr_save_fxsave
.Lsse_context_save_jmp:

.align 16
.Lafter_fpu_save:

	// Make structure on the stack
	push_cfi %rax
	push_cfi $0
	push_cfi $0

	// Pass intr, ctx
	mov %rsp,%rsi
	mov 1*8(%rbp),%edi

	// Align stack pointer
	sub_rsp 8

	// Interrupt dispatch
	// 0x00-0x1F -> exception_isr_handler
	// 0x20-0x2F -> pic_dispatcher
	// 0x30-0xEF -> apic_dispatcher
	// 0xF0-0xFF -> intr_invoke

	// 0x00-0x1F -> exception_isr_handler
	lea exception_isr_handler(%rip),%rax

	// 0x20-0x2F -> pic_dispatcher
	mov $pic8259_dispatcher,%rcx
	cmp $INTR_PIC1_IRQ_BASE,%edi
	cmovae %rcx,%rax

	// 0x30-0xEF -> apic_dispatcher
	mov $apic_dispatcher,%rcx
	cmp $INTR_APIC_IRQ_BASE,%edi
	cmovae %rcx,%rax

	// 0xF0-0xFF -> intr_invoke
	mov $intr_invoke,%rcx
	cmp $INTR_SOFT_BASE,%edi
	cmovae %rcx,%rax

	// Call handler
	call *%rax

	// isr can return a new stack pointer, or just return
	// the passed one to continue with this thread
	mov %rax,%rsp

	lea 19*8(%rax),%rbp

	// Pop outgoing cleanup data
	// Used to adjust outgoing thread state after switching stack
	pop_cfi %rax
	pop_cfi %rdi
	test %rax,%rax
	jz .Lno_cleanup_call
	call *%rax
.Lno_cleanup_call:

	// Pop the pointer to the FPU context
	pop_cfi %rdi

	// NOTE: this instruction may be patched to call one of the xsave versions!
	jmp isr_restore_fxrstor
.Lsse_context_restore_jmp:

.align 16
.Lafter_cpu_restore:

	// pop packed segments qword into rdx
	// fetch cpu local storage pointer into r14
	// if not returning to kernel
	//   update tss rsp0
	//   swapgs
	//   if any data segment is not GDT_SEL_USER_DATA | 3
	//     load all segment registers with GDT_SEL_USER_DATA | 3
	//   endif
	//   restore fsbase
	// endif

	// Load return cs
	movzwl 4*8(%rbp),%ecx

	// Pop segments
	pop_cfi %rdx

	// Fetch pointer to cpu_info
	mov %gs:0,%r14

	// See if we're not returning to user code
	// Branch past swapgs and restoration of segments if not
	cmpl $GDT_SEL_KERNEL_CODE64,%ecx
	jz .Lreturning_to_kernel

	// ...returning to user code

	// Fetch pointer to this CPU's TSS for TSS.RSP0 update
	mov CPU_INFO_TSS_PTR_OFS(%r14),%rbx
	lea 8*8(%rbp),%rax
	mov %rax,TSS_RSP0_OFS(%rbx)

	// Restore user gs
	swapgs

	// If segments are not changing, avoid 136*4 cycles
	cmp %r13,%rdx
	jnz .Lsegments_changed
	jmp .Lsegments_not_changed

.align 16
.Lsegments_changed:
	// Restore segments
	movl $(GDT_SEL_USER_DATA | 3),%eax
	movw %ax,%ds
	movw %ax,%es
	movw %ax,%fs
	movw %ax,%gs

.align 16
.Lsegments_not_changed:
.Lreturning_to_kernel:
	// Restore CR3
	pop_cfi %rax
	mov %rax,%cr3

	pop_cfi %rdi
	pop_cfi %rsi
	pop_cfi %rdx
	pop_cfi %rcx
	pop_cfi %r8
	pop_cfi %r9
	pop_cfi %rax
	pop_cfi %rbx
	pop_cfi %r10
	pop_cfi %r11
	pop_cfi %r12
	pop_cfi %r13
	pop_cfi %r14
	pop_cfi %r15
	pop_cfi %rbp

	add_rsp 16
	.cfi_def_cfa_offset 8

	iretq

	.cfi_endproc

// Expects ds loaded with kernel data segment
// Clobbers rcx,rdx,rax,rdi
// Returns rax=pointer to context
.macro xsave_ctx insn
	// Get pointer to current thread from CPU-local storage
	mov %gs:CPU_INFO_CURTHREAD_OFS,%rcx

	// Set all bits of edx:eax
	mov $-1,%eax
	mov $-1,%edx

	// Skip if threading not initialized yet (still needed?)
	test %rcx,%rcx
	jz 1f

	// Read xsave stack pointer from thread
	mov THREAD_XSAVE_PTR_OFS(%rcx),%rdi

	// Save context using instruction passed to macro
	\insn (%rdi)
	mov %rdi,%rax

	// Update xsave stack pointer in thread
	add sse_context_size,%rdi
	mov %rdi,THREAD_XSAVE_PTR_OFS(%rcx)
1:
	jmp .Lafter_fpu_save
.endm

// Expects ds to be kernel data segment
// Expects rdi to point to saved context
// Clobbers eax,edx,ecx
.macro xrstor_ctx insn
	// Set all bits of edx:eax
	mov $-1,%eax
	mov $-1,%edx

	// Restore context using instruction passed to macro
	\insn (%rdi)

	// Get pointer to current thread from CPU-local storage
	mov %gs:CPU_INFO_CURTHREAD_OFS,%rcx
	mov %rdi,THREAD_XSAVE_PTR_OFS(%rcx)

	jmp .Lafter_cpu_restore
.endm

.align 16
.global isr_save_xsaveopt
.hidden isr_save_xsaveopt
isr_save_xsaveopt:
	xsave_ctx xsaveopt64

.align 16
.global isr_save_xsave
.hidden isr_save_xsave
isr_save_xsave:
	xsave_ctx xsave64

.align 16
.global isr_save_xsavec
.hidden isr_save_xsavec
isr_save_xsavec:
	xsave_ctx xsavec64

.align 16
.global isr_restore_xrstor
.hidden isr_restore_xrstor
isr_restore_xrstor:
	xrstor_ctx xrstor64

.align 16
.global isr_save_fxsave
.hidden isr_save_fxsave
isr_save_fxsave:
	xsave_ctx fxsave64

.align 16
.global isr_restore_fxrstor
.hidden isr_restore_fxrstor
isr_restore_fxrstor:
	xrstor_ctx fxrstor64

// isr_context_t *exception_isr_handler(int intr, isr_context_t *ctx)
.type exception_isr_handler,@function
exception_isr_handler:
	.cfi_startproc

	push_cfi %rbp
	push_cfi %rbx
	push_cfi %r12

	mov $intr_has_handler,%rax

	// ebx=intr, r12=ctx
	mov %edi,%ebx
	mov %rsi,%r12

	// If there is no handler...
	call *%rax
	test %eax,%eax
	jz 0f

	// There is a handler
	mov $intr_invoke,%rax
	mov %ebx,%edi
	mov %r12,%rsi
	call *%rax

	// See if the handler rejected it
	test %eax,%eax
	jz 0f

1:
	// Handler handled it
	mov %r12,%rax
	pop_cfi %r12
	pop_cfi %rbx
	pop_cfi %rbp
	retq

.align 16
0:
	// ...no handler or handler rejected it
	mov $__exception_handler_invoke,%rax
	mov %ebx,%edi
	call *%rax
	test %eax,%eax
	jnz 2f

	mov $cpu_debug_break,%rax
	call *%rax
	jmp 1b

	// Tail call to unhandled_exception_handler
.align 16
2:
	mov $unhandled_exception_handler,%rax
	mov %r12,%rdi
	pop_cfi %r12
	pop_cfi %rbx
	pop_cfi %rbp
	jmp *%rax

	.cfi_endproc

// __noreturn void isr_sysret64(uintptr_t rip, uintptr_t rsp);
.global isr_sysret64
isr_sysret64:
	push %rbp
	mov %rsp,%rbp

	xor %eax,%eax

	// Clear the opmask registers

	cmpw $0,sse_avx512_opmask_offset(%rip)
	jne .Lclear_opmask
	jmp .Ldone_opmask

.Lclear_opmask:
	kmovw %eax,%k1
	kmovw %eax,%k2
	kmovw %eax,%k3
	kmovw %eax,%k4
	kmovw %eax,%k5
	kmovw %eax,%k6
	kmovw %eax,%k7

.align 16
.Ldone_opmask:
	// Check for AVX
	cmpw $0,sse_avx_offset(%rip)
	je .Lno_avx

	// Clear all vector registers
	vzeroall
	jmp .Ldone_sse

.align 16
.Lno_avx:
	pxor %xmm0,%xmm0
	movq %xmm0,%xmm1
	movq %xmm0,%xmm2
	movq %xmm0,%xmm3
	movq %xmm0,%xmm4
	movq %xmm0,%xmm5
	movq %xmm0,%xmm6
	movq %xmm0,%xmm7
	movq %xmm0,%xmm8
	movq %xmm0,%xmm9
	movq %xmm0,%xmm10
	movq %xmm0,%xmm11
	movq %xmm0,%xmm12
	movq %xmm0,%xmm13
	movq %xmm0,%xmm14
	movq %xmm0,%xmm15

.align 16
.Ldone_sse:
	// Clear the x87 stack
	fld1
	fsub %st,%st
	fld %st
	fld %st
	fld %st
	fld %st
	fld %st
	fld %st
	fld %st
	fstp %st
	fstp %st
	fstp %st
	fstp %st
	fstp %st
	fstp %st
	fstp %st
	fstp %st

	mov %rdi,%rcx
	mov $(CPU_EFLAGS_IF | 2),%r11d

	// Avoid leaking any register values to user code
	mov %eax,%ebx
	// not ecx
	mov %eax,%edx
	// not esi
	mov %eax,%edi
	mov %eax,%ebp
	mov %eax,%r8d
	mov %eax,%r9d
	mov %eax,%r10d
	// not r11
	mov %eax,%r12d
	mov %eax,%r13d
	mov %eax,%r14d
	mov %eax,%r15d
	cli
	swapgs
	mov %rsi,%rsp
	sysretq

.section .rodata
.align 8

// Pointers to patch points
.global sse_context_save
sse_context_save:
	.quad .Lsse_context_save_jmp

.global sse_context_restore
sse_context_restore:
	.quad .Lsse_context_restore_jmp
