.section .text.isr, "ax"

#include "isr_constants.h"

// This must be position independent code because the GDB stub clones off
// a redundant copy to protect against infinite recursion

.macro push_cfi val
	pushq \val
	.cfi_adjust_cfa_offset 8
.endm

.macro pop_cfi val
	popq \val
	.cfi_adjust_cfa_offset -8
.endm

.macro add_rsp	ofs
	add $\ofs,%rsp
	.cfi_adjust_cfa_offset -(\ofs)
.endm

.macro sub_rsp	ofs
	sub $\ofs,%rsp
	.cfi_adjust_cfa_offset (\ofs)
.endm

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

.align 16
isr_common:
	.cfi_startproc
	.cfi_def_cfa_offset 24

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
	push_cfi %r9
	push_cfi %r8
	push_cfi %rcx
	push_cfi %rdx
	push_cfi %rsi
	push_cfi %rdi

	// C code requires that the direction flag is clear
	cld

	// Save FSBASE MSR
	mov $0xC0000100,%ecx
	rdmsr
	shl $32,%rdx
	or %rdx,%rax
	push_cfi %rax

	// Push CR3
	mov %cr3,%rax
	push_cfi %rax

	// Push segment registers
	sub_rsp 8
	mov %gs,6(%rsp)
	mov %fs,4(%rsp)
	mov %es,2(%rsp)
	mov %ds,(%rsp)

	// See if we are coming from kernel code
	cmpq $8,21*8(%rsp)
	je 0f

	// ...came from user code
	swapgs
    mov $0x10,%eax
	mov %eax,%ds
	mov %eax,%es
0:

	// Save entire sse/mmx/fpu state
	jmp *sse_context_save
8:

	// Make structure on the stack
	push_cfi %rsp
	push_cfi %rax
	xor %eax,%eax
	push_cfi %rax
	push_cfi %rax

	// Pass intr, ctx
    mov %rsp,%rsi
	mov 22*8(%rsp),%rdi

	// Align stack
	push_cfi %rax

	// Interrupt dispatch
	//      0xFF -> intr_invoke
	// 0x80-0xFE -> irq_dispatcher
	// 0x30-0x7F -> intr_invoke
	// 0x20-0x2F -> irq_dispatcher
	// 0x00-0x1F -> exception_isr_handler

	// Assume 0xFF -> intr_invoke
	mov $intr_invoke,%rax

	// If <= 0xFE -> irq_dispatcher
	mov $irq_dispatcher,%rcx
	cmp $0xFE,%edi
	cmovbe %rcx,%rax

	// If <= 0x7F -> intr_invoke
	mov $intr_invoke,%rcx
	cmp $0x7F,%edi
	cmovbe %rcx,%rax

	// If <= 0x2F -> irq_dispatcher
	mov $irq_dispatcher,%rcx
    cmp $0x2F,%edi
	cmovbe %rcx,%rax

	// If < 0x1F -> exception_isr_handler
	lea exception_isr_handler(%rip),%rcx
	cmp $0x1F,%edi
	cmovbe %rcx,%rax

	// Pass pointer to the context structure to handler
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
	jmp *sse_context_restore
9:

	// See if we're returning to user code
	cmpl $8,21*8(%rsp)
	jz 0f

	// ...yes, returning to user code
	swapgs

	// Restore segments
	mov (%rsp),%ds
	mov 2(%rsp),%es
	mov 4(%rsp),%fs
	mov 6(%rsp),%gs
0:
	add_rsp 8

	// Restore CR3
	pop_cfi %rax
	// mov %cr3,%rdx
	// cmp %rax,%rdx
	// jz 1f
	mov %rax,%cr3
//1:

	// Restore FSBASE
	pop_cfi %rax
	mov %rax,%rdx
	shr $32,%rdx
	mov $0xC0000100,%ecx
	wrmsr

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

	// Tolerate CPU storage not being ready (yet)
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

	// Set all bits of edx:eax
	mov $-1,%eax
	mov %eax,%edx

	// Save context using instruction passed to macro
	\insn (%rdi)
	mov %rdi,%rax

	// Update xsave stack pointer in thread
	add sse_context_size,%rdi
	mov %rdi,THREAD_XSAVE_PTR_OFS(%rcx)
1:
    jmp 8b

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
	xsave_ctx xsaveopt64 1

.align 16
.global isr_save_xsave
.hidden isr_save_xsave
isr_save_xsave:
	xsave_ctx xsave64 1

.align 16
.global isr_save_xsavec
.hidden isr_save_xsavec
isr_save_xsavec:
	xsave_ctx xsavec64 1

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

//  isr_context_t *exception_isr_handler(int intr, isr_context_t *ctx)
//  {
//      if (!intr_has_handler(intr) || !intr_invoke(intr, ctx)) {
//          if (__exception_handler_invoke(intr))
//              return unhandled_exception_handler(ctx);
//          cpu_debug_break();
//      }
//
//      return ctx;
//  }
//
// isr_context_t *exception_isr_handler(int intr, isr_context_t *ctx)
exception_isr_handler:
	.cfi_startproc

	push_cfi %rbp
    mov %rsp,%rbp
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
