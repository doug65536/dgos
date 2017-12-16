.section .text.isr, "ax"

#include "asm_constants.h"
#include "control_regs_constants.h"
#include "../cfi_helpers.h"
#include "interrupts.h"

// This must be position independent code because the GDB stub clones off
// a redundant copy to protect against infinite recursion

// Self modify code to use xsave when appropriate
.global sse_context_save
.global sse_context_restore


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

	// At this point:
	//  0*8(%rsp) -> interrupt
	//  1*8(%rsp) -> error code
	//  2*8(%rsp) -> rip
	//  3*8(%rsp) -> cs
	//  4*8(%rsp) -> flags
	//  5*8(%rsp) -> rsp
	//  6*8(%rsp) -> ss

	// Save call-clobbered registers
	// (in System-V parameter order in memory)
	push_cfi %r15
	push_cfi %r14
	push_cfi %r13
	push_cfi %r12
	push_cfi %r11
	push_cfi %r10
	push_cfi %rbp
	push_cfi %rbx
	push_cfi %rax
	mov %cr3,%rax
	push_cfi %r9
	push_cfi %r8
	push_cfi %rcx
	push_cfi %rdx
	push_cfi %rsi
	push_cfi %rdi

	// Register usage:
	//  r13: segment register values at interrupt entry

	cld
	push_cfi %rax

	// Get segment registers packed into %r13
	mov %es,%r13w
	shl $16,%r13d
	mov %ds,%r13w
	mov %gs,%ax
	shl $16,%eax
	mov %fs,%ax
	shl $32,%rax
	or %rax,%r13

	push_cfi %r13

	// See if we are coming from kernel code
	cmpw $GDT_SEL_KERNEL_CODE64,20*8(%rsp)
	je 0f

	// ...came from user code
	swapgs
0:

	// Save entire sse/mmx/fpu state
	// NOTE: this instruction may be patched to call one of the xsave versions!
	jmp isr_save_fxsave
sse_context_save:

.align 16
8:

	// Make structure on the stack
	push_cfi %rsp
	push_cfi %rax
	xor %eax,%eax
	push_cfi %rax
	push_cfi %rax

	// Pass intr, ctx
	mov %rsp,%rsi
	mov 22*8(%rsp),%edi

	// Setup frame pointer so rbp stack traces will work
	//lea 24*8(%rsp),%rbp  -- worse

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

	// Pop outgoing cleanup data
	// Used to adjust outgoing thread state after switching stack
	pop_cfi %rax
	pop_cfi %rdi
	test %rax,%rax
	jz 0f
	call *%rax
0:

	// Pop the pointer to the FPU context
	pop_cfi %rdi
	// Pop the pointer to the general registers
	pop_cfi %rsp

	// NOTE: this instruction may be patched to call one of the xsave versions!
	jmp isr_restore_fxrstor
sse_context_restore:

.align 16
9:

	// Load return cs
	movzwl 20*8(%rsp),%ecx

	// See if we're not returning to user code
	// Branch past swapgs and restoration of segments if not
	cmpl $GDT_SEL_KERNEL_CODE64,%ecx
	jz 0f

	// ...yes, returning to user code

	// Fetch pointer to this CPU's TSS for TSS.RSP0 update
	mov %gs:CPU_INFO_TSS_PTR_OFS,%rbx
	lea 24*16(%rsp),%rax
	mov %rax,TSS_RSP0_OFS(%rbx)

	// Restore user gs
	swapgs

	// Only restore segments if returning to compatibility mode
	pop_cfi %rax
	cmp $(GDT_SEL_USER_CODE32 | 3),%ecx
	je 1f
	jmp 0f

1:
	// If segments are not changing, avoid 136*4 cycles
	cmp %r13,%rax
	jnz 1f
	jmp 0f

1:
	// Restore segments
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%eax
	mov %ax,%gs

.align 16
0:
	// Restore CR3
	pop_cfi %rax
	mov %rax,%cr3

	// Restore FSBASE
	pop_cfi %rax
	cmpb $0,cpu_has_fsgsbase
	je 1f

	wrfsbase %rax
	jmp 0f

.align 16
1:
	mov $CPU_MSR_FSBASE,%ecx
	mov %rax,%rdx
	shr $32,%rdx
	wrmsr

.align 16
0:

	pop_cfi %rdi
	pop_cfi %rsi
	pop_cfi %rdx
	pop_cfi %rcx
	pop_cfi %r8
	pop_cfi %r9
	pop_cfi %rax
	pop_cfi %rbx
	pop_cfi %rbp
	pop_cfi %r10
	pop_cfi %r11
	pop_cfi %r12
	pop_cfi %r13
	pop_cfi %r14
	pop_cfi %r15

	addq $16,%rsp
	.cfi_def_cfa_offset 8

	iretq

	.cfi_endproc

// Expects ds loaded with kernel data segment
// Clobbers rcx,rdx,rax,rdi
// Returns rax=pointer to context
.macro xsave_ctx insn clear_hdr
	// Get pointer to current thread from CPU-local storage
	mov %gs:CPU_INFO_CURTHREAD_OFS,%rcx

	// Set all bits of edx:eax
	mov $-1,%eax
	mov $-1,%edx

	// Panic on CPU storage not being ready (yet)
	test %rcx,%rcx
	jz 1f

	// Read xsave stack pointer from thread
	mov THREAD_XSAVE_PTR_OFS(%rcx),%rdi

	// Only clear xsave header depending on macro parameter
	.if \clear_hdr != 0
		// If this is not the first context, clear xsave header
		cmp THREAD_XSAVE_STACK_OFS(%rcx),%rdi
		jnz 0f
	.endif

	// Save context using instruction passed to macro
	\insn (%rdi)
	mov %rdi,%rax

	// Update xsave stack pointer in thread
	add sse_context_size,%rdi
	mov %rdi,THREAD_XSAVE_PTR_OFS(%rcx)
1:
	jmp 8b

0:  call cpu_debug_break
	jmp 0b

	// Out of line clear xsave header for recursive context save
	.if \clear_hdr != 0
		.align 16
0:
		xor %eax,%eax
		// Clear xsave header
		mov $512,%edx
		mov %rax,   (%rdi,%rdx)
		mov %rax,1*8(%rdi,%rdx)
		mov %rax,2*8(%rdi,%rdx)
		mov %rax,3*8(%rdi,%rdx)
		mov %rax,4*8(%rdi,%rdx)
		mov %rax,5*8(%rdi,%rdx)
		mov %rax,6*8(%rdi,%rdx)
		mov %rax,7*8(%rdi,%rdx)
		dec %eax
		mov %eax,%edx
		\insn (%rdi)
		mov %rdi,%rax
		add sse_context_size,%rdi
		mov %rdi,THREAD_XSAVE_PTR_OFS(%rcx)
		jmp 8b
	.endif
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

	jmp 9b
.endm

.align 16
.global isr_save_xsaveopt
.hidden isr_save_xsaveopt
isr_save_xsaveopt:
	xsave_ctx xsaveopt64 0

.align 16
.global isr_save_xsave
.hidden isr_save_xsave
isr_save_xsave:
	xsave_ctx xsave64 0

.align 16
.global isr_save_xsavec
.hidden isr_save_xsavec
isr_save_xsavec:
	xsave_ctx xsavec64 0

.align 16
.global isr_restore_xrstor
.hidden isr_restore_xrstor
isr_restore_xrstor:
	xrstor_ctx xrstor64

.align 16
.global isr_save_fxsave
.hidden isr_save_fxsave
isr_save_fxsave:
	xsave_ctx fxsave64 0

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

	// ...or the handler rejected it
	mov $intr_invoke,%rax
	mov %ebx,%edi
	mov %r12,%rsi
	call *%rax
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
.align 16

	// Tail call to unhandled_exception_handler
2:
	mov $unhandled_exception_handler,%rax
	mov %r12,%rdi
	pop_cfi %r12
	pop_cfi %rbx
	pop_cfi %rbp
	jmp *%rax

	.cfi_endproc

// Load segments 256,000,000 times from top of stack
// old way 136 cycles
.global mov_seg_perf
mov_seg_perf:
	push $0
	mov $1000000,%ecx

	xor %eax,%eax
.align 16
0:
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	mov (%rsp),%rax
	mov %ax,%ds
	shr $16,%rax
	mov %ax,%es
	shr $16,%rax
	mov %ax,%fs
	shr $16,%rax
	mov %ax,%gs
	dec %ecx
	jnz 0b
	pop %rax
	ret

// Load segments 256,000,000 times from top of stack
.global str_seg_perf
str_seg_perf:
	push $0
	mov $1000000,%ecx

.align 16
0:
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	mov %ds,(%rsp)
	mov %es,2(%rsp)
	mov %fs,4(%rsp)
	mov %gs,6(%rsp)
	dec %ecx
	jnz 0b
	pop %rax
	ret
