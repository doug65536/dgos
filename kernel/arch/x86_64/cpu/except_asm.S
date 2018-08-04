#include "cfi_helpers.h"

.section .text, "ax"

.global __exception_setjmp
.hidden __exception_setjmp
__exception_setjmp:
	.cfi_startproc
	.cfi_def_cfa_offset 8

	// Save call preserved registers
	movq %rbx,2*8(%rdi)
	movq %rbp,3*8(%rdi)
	movq %r12,4*8(%rdi)
	movq %r13,5*8(%rdi)
	movq %r14,6*8(%rdi)
	movq %r15,7*8(%rdi)

	// Save flags
	pushfq
	popq 8*8(%rdi)

	// Save return address
	movq (%rsp),%rax
	movq %rax,(%rdi)

	// Save callers stack pointer
	leaq 8(%rsp),%rax
	movq %rax,1*8(%rdi)

	// Return value is zero
	xorl %eax,%eax
	ret

	.cfi_endproc

.global __exception_longjmp
.hidden __exception_longjmp
__exception_longjmp:
	.cfi_startproc
	.cfi_def_cfa_offset 8

	// Restore call preserved registers
	movq 2*8(%rdi),%rbx
	movq 3*8(%rdi),%rbp
	movq 4*8(%rdi),%r12
	movq 5*8(%rdi),%r13
	movq 6*8(%rdi),%r14
	movq 7*8(%rdi),%r15

	// Restore flags
	pushq 8*8(%rdi)
	popfq

	// Restore stack pointer
	movq 1*8(%rdi),%rsp

	// Return value is second parameter and "return" from setjmp again
	movq %rsi,%rax
	jmpq *(%rdi)

	.cfi_endproc

// void __exception_longjmp_unwind(__exception_jmp_buf_t *__ctx, int value);
.global __exception_longjmp_unwind
__exception_longjmp_unwind:
	.cfi_startproc
	.cfi_def_cfa_offset 8

	push_cfi %rbp
	.cfi_offset rbp,-16
	movq %rsp,%rbp
	push_cfi %rbx
	.cfi_offset rbx,-24
	push_cfi %r8
	movq %rdi,%rbx
	movq %rsi,%r8

	// exception object (1st parameter)
	mov $1024,%rdi
	call __cxa_allocate_exception
	mov %rax,%rdi

	// Pass jmpbuf as stop (3rd) parameter
	movq %rbx,%rdx
	// stopfn is 2nd parameter
	leaq __exception_stop_fn(%rip),%rsi
	call _Unwind_ForcedUnwind
	ud2

	.cfi_endproc

// __exception_stop_fn(int, _Unwind_Action, _Unwind_Exception_Class,
//       struct _Unwind_Exception *, struct _Unwind_Context *, void *)
__exception_stop_fn:
	.cfi_startproc
	.cfi_def_cfa_offset 8

	push_cfi %rbp
	.cfi_offset rbp,-16
	movq %rsp,%rbp
	push_cfi %rbx
	.cfi_offset rbx,-24
	push_cfi %r12
	.cfi_offset r12,-32
	push_cfi %r13
	.cfi_offset r13,-40
	push_cfi %r14
	.cfi_offset r14,-48
	push_cfi %r15
	.cfi_offset r15,-56

	movq %rdi,%rbx
	movq %rsi,%r12
	movq %rdx,%r13
	movq %rcx,%r14
	movq %r8,%r15
	push_cfi %r9

	movq %r15,%rdi
	call _Unwind_GetCFA

	pop_cfi %r9
	pop_cfi %r15
	.cfi_same_value r15
	pop_cfi %r14
	.cfi_same_value r14
	pop_cfi %r13
	.cfi_same_value r13
	pop_cfi %r12
	.cfi_same_value r12
	pop_cfi %rbx
	.cfi_same_value rbx
	pop_cfi %rbp
	.cfi_same_value rbp
	// return _URC_NO_REASON
	xorl %eax,%eax
	ret
	//jmp _Unwind_Resume

	.cfi_endproc

//.quad _Unwind_ForcedUnwind
//.quad _Unwind_SetGR
//.quad _Unwind_SetIP
//.quad _Unwind_GetLanguageSpecificData
