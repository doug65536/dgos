#include <sys/cpu_bug.h>

.section .text, "ax", @progbits

.balign 16
.global _start
_start:
	.cfi_startproc

// ============================================================================
#ifdef __x86_64__
    // Can't return, terminate stack traces here
    .cfi_undefined rip
    .cfi_undefined rsp
    .cfi_undefined rbp
    .cfi_undefined rbx
    .cfi_undefined r12
    .cfi_undefined r13
    .cfi_undefined r14
    .cfi_undefined r15
    .cfi_def_cfa rsp,0

    // Set TLS current thread ID immediately
    mov %edi,%fs:__tid@tpoff

    // r12 = argc
    movl (%rsp),%r12d

    // r13 = argv
    movq 8(%rsp),%r13

	// r14 = envp = argv + (argc + 1)
    lea 8(%r13,%r12,8),%r14

    // Scan through the envp array until the nullptr is encountered
    // Finish the loop pointing to the item after the nullptr
    movq %r14,%r15
.Lcheck_envp:
    cmpq $ 0,(%r15)
    leaq 8(%r15),%r15
    jnz .Lcheck_envp

    // r15 = auxv

    // Resolve .rela.iplt
    leaq __rela_iplt_start(%rip),%rbx

.Lrela_iplt_next:
    leaq __rela_iplt_end(%rip),%rax
	cmpq %rax,%rbx
    jz .Lrela_iplt_done

    // Only apply STT_GNU_IFUNC (0x25)
    cmpl $ 0x25,8(%rbx)
    jnz .Lrela_iplt_skip

    // Call the resolver
	movq 16(%rbx),%rax
	indirect_call rax

    // Read PLT vector pointer from relocation record
    movq (%rbx),%rdx
	// Update the PLT with the resolver return value
    movq %rax,(%rdx)
.Lrela_iplt_skip:
    addq $ 24,%rbx
    jmp .Lrela_iplt_next
.Lrela_iplt_done:

    lea __init_array_start(%rip),%rbx
.Lnext_init:
    leaq __init_array_end(%rip),%rax
    cmpq %rax,%rbx
    je .Ldone_init
    movq (%rbx),%rax
    testq %rax,%rax
    jz .Ldone_init
    indirect_call rax
    addq $ 8,%rbx
    jmp .Lnext_init
.Ldone_init:

    // Save stack pointer and absolutely guarantee alignment
	mov %rsp,%rbx
	and $ -16,%rsp

    // Pass argc, argv, envp, auxv to main
    mov %r12,%rdi
    mov %r13,%rsi
    mov %r14,%rdx
    mov %r15,%rcx
    call main

    // Restore stack pointer
    mov %rbx,%rsp

    // Save program exit code
	mov %rax,%r12
.Lexit_returned:
	mov %r12,%rdi
    call exit
	jmp .Lexit_returned

// ============================================================================
#elif defined(__i386__)

    // Can't return, terminate stack traces here
    .cfi_undefined eip
    .cfi_undefined esp
    .cfi_undefined ebp
    .cfi_undefined ebx
    .cfi_undefined esi
    .cfi_undefined edi
    .cfi_def_cfa esp,0

    // last hex digit of stack pointer ends in 0xc at this point
    // need it at zero when calling main below

    // parameters, in asending address order,
    // initial stack pointer pointing at argc:
    // [envp] later
    //  argc
    //  argv

    // eax = argc
    mov (%esp),%eax

    // ecx = argv
    mov 4(%esp),%ecx

    // environment pointers start after argv null terminator
    // esi = envp = argv + (argc + 1)
    lea 4(%ecx,%eax,4),%ebp

    // Preserve envp in edi
    mov %ebp,%edi

//.Lcheck_envp:
//    cmpl $ 0,(%ebp)
//    lea 4(%ebp),%ebp
//    jnz .Lcheck_envp
//
//    // Save end of envp
//    push %ebp
//    .cfi_adjust_cfa_offset 4

    // Get position of 0: below into eax
    call __x86.get_pc_thunk.ax
0:

    // Resolve position independent start and end .rela.iplt
    // into callee preserved register variables
    lea __init_array_start-0b(%eax),%ebx
    lea __init_array_end-0b(%eax),%esi

    // Call all the constructors
    call __call_fns

    // Nice. Do nothing and the stack is 16 byte aligned anyway
    // Call main(int argc, char **argv, char **envp)
    push %edi
    pushl 1*4+4(%esp)
    pushl 2*4(%esp)
    call main

    mov %eax,%ebx

.Lexit_returned:
    mov %ebx,(%esp)
    call exit
    jmp .Lexit_returned

    .cfi_endproc

.type __call_dtors, @function
.global __call_dtors
.hidden __call_dtors
__call_dtors:
    .cfi_startproc

    sub $ 4,%esp
    .cfi_adjust_cfa_offset 4

    push %ebx
    .cfi_adjust_cfa_offset 4
    .cfi_offset ebx,-3*4

    push %esi
    .cfi_adjust_cfa_offset 4
    .cfi_offset esi,-4*4

    // Resolve position independent start and end .rela.iplt
    // into callee preserved register variables
    lea __fini_array_start-0b(%eax),%ebx
    lea __fini_array_end-0b(%eax),%esi

    call __call_fns

    pop %esi
    .cfi_same_value esi
    .cfi_adjust_cfa_offset -4

    pop %ebx
    .cfi_same_value ebx
    .cfi_adjust_cfa_offset -4

    add $ 4,%esp
    .cfi_adjust_cfa_offset -4

    ret

    .cfi_endproc

// Call all the function pointers between ebx and esi
// Returns with ebx == esi
.type call_fns,@function
__call_fns:
    .cfi_startproc

    .cfi_undefined ebx

    // Lower `while (expr) body` loop
    // into `if (expr) do body while (expr)`
    cmp %esi,%ebx
    je .Ldone_init

.Lnext_init:
    // Load the constructor function pointer
    mov (%ebx),%eax
    add $ 8,%ebx

    // Done at nullptr
    test %eax,%eax
    jz .Ldone_init

    // Call the constructor
    call *%eax

    cmp %esi,%ebx
    jne .Lnext_init
.Ldone_init:
    ret

// ============================================================================
#elif defined(__aarch64__)
    // Terminate stack trace
    no_caller_cfi

    // On entry:
    //  x0=tid
    //  x1=argc
    //  x2=argv
    //  x3=envp

    // Store tid into TLS immediately
    mrs x5,tpidr_el0
    add x5,x5,:tprel_hi12:__tid, lsl 12
    add x5,x5,:tprel_lo12_nc:__tid
    str x0,[x5]

    // eventually:
    //  x20=argc
    //  x21=argv
    //  x22=envp
    //  x23=auxv

    mov x20,x1
    mov x21,x2
    mov x22,x3

    // initial w0 is set by loader to argc
    // initial x1 is set by loader to argv
    // initial x2 is set by loader to envp

    // Scan through the envp array until the nullptr is encountered
    // Finish the loop pointing to the item after the nullptr
    mov x23,x3
.Lcheck_envp:
    ldr x4,[x23]
    add x23,x23,8
    cmp x4,0
    b.ne .Lcheck_envp

    // x23 = auxv

    // Resolve .rela.iplt (x24=rbx)
    adr x24,__rela_iplt_start

    .Lrela_iplt_next:
    adr x0,__rela_iplt_end
    cmp x24,x0
    b.eq .Lrela_iplt_done

    // Only apply STT_GNU_IFUNC (0x25)
    ldr x1,[x24, 8]
    cmp x1,0x25
    b.ne .Lrela_iplt_skip

    // Call the resolver
    ldr x0,[x24, 16]
    blr x0

    // Read PLT vector pointer from relocation record
    ldr x1,[x24]
    // Update the PLT with the resolver return value
    str x0,[x1]
.Lrela_iplt_skip:
    add x24,x24,24
    b .Lrela_iplt_next
.Lrela_iplt_done:

    adr x24,__init_array_start
.Lnext_init:
    adr x0,__init_array_end
    cmp x0,x24
    b.eq .Ldone_init
    ldr x0,[x24]
    cmp x0,0
    b.eq .Ldone_init
    blr x0
    add x24,x24,8
    b .Lnext_init
.Ldone_init:

    // Save stack pointer
    mov x24,sp

    // Pass argc, argv, envp, auxv to main
    mov x0,x20
    mov x1,x21
    mov x2,x22
    mov x3,x23
    bl main

    mov sp,x24

    // Save program exit code
    mov x20,x0
.Lexit_returned:
    mov x0,x20
    bl exit
    b .Lexit_returned

#endif
// ============================================================================

    .cfi_endproc
