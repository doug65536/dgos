#include "cpu/control_regs_constants.h"
#include "cpu/interrupts.h"
#include "cfi_helpers.h"

.section .entry, "ax"

.macro debugind ch
    movb $\ch,0xb8000
.endm

// Things that must be done before entering any compiler
// generated code:
//  - Set up stack (aligned!)
//  - Clear EFLAGS.DF

.align 16
.global entry
.hidden entry
.type entry,@function
entry:
    debugind '1'

    // Align the stack
    andq $-16,%rsp

    cld

    // Save parameter in call-preserved register
    movl %ecx,%r15d
    xorl %ebp,%ebp

    debugind '2'

    // See if this is the bootstrap processor
    movl $ CPU_APIC_BASE_MSR,%ecx
    rdmsr
    testl $ CPU_APIC_BASE_BSP,%eax
    jnz bsp_init

    debugind 'Z'

ap_init:
    // This is not the bootstrap CPU

    xorl %ebp,%ebp
    pushq %rbp

    // MP processor entry
    call mp_main

    // mp_main should not return
0:
    call halt_forever
    // halt_forever should not return either!
    jmp 0b

bsp_init:
    // This is the bootstrap processor

    debugind '3'

    leaq kernel_stack(%rip),%rdx
    movq kernel_stack_size(%rip),%rbx
    leaq (%rdx,%rbx),%rsp

    // Store the kernel parameters passed in from bootloader
    movq %r15,kernel_params(%rip)

    debugind '4'
    call cpuid_init

    xor %edi,%edi
    call cpu_init_early

    debugind '7'

    // Get SSE context offsets
    xorl %edi,%edi
    call idt_xsave_detect

    debugind '8'

    call e9debug_init

    debugind '9'

    // Call the constructors
    leaq ___ctors_st(%rip),%rdi
    leaq ___ctors_en(%rip),%rsi
    call invoke_function_array

    debugind 'A'

    // Notify constructors ran
    movl $'C',%edi
    call callout_call

    debugind 'B'

    xorl %edi,%edi
    call cpu_init

    debugind 'C'

    // Initialize text devices
    movl $'V',%edi
    call callout_call

    debugind 'D'

    xorl %edi,%edi
    call cpu_init_stage2

    debugind 'E'

    xorl %edi,%edi
    call cpu_hw_init

    debugind 'F'

    // Initialize early-initialized devices
    movl $'E',%edi
    call callout_call

    debugind 'G'

    call main

    debugind '?'

    movq %rax,%rdi
    call exit

.section .data

.align 16
    // TSS layout is completely misaligned without nudging it up 4 bytes
    //  uint32_t dummy_align;
    .space 4
    .global early_tss
early_tss:
    // TSS structure
    //  uint32_t reserved0;
    .int 0
    //
    //  uint64_t rsp[3];
    .quad 0
    .quad 0
    .quad 0
    //
    //  uint64_t ist[8];
    .quad 0
    .quad kernel_stack-32768
    .quad kernel_stack-32768
    .space 5*8
    //
    //  uint32_t reserved3;
    .int 0
    //  uint32_t reserved4;
    .int 0
    //
    //  uint16_t reserved5;
    .short 0
    //  uint16_t iomap_base;
    .short early_tss_end-early_tss
    //  uint32_t dummy_align2;
    .int 0
    //
    //  // entry 0 is rsp[0], rest are ist stacks
    //  void *stack[8];
    .space 8*8
early_tss_end:

.section .text
.align 16
.global exit
.hidden exit
exit:
    // Ignore exitcode
    // Kernel exit just calls destructors
    // and deliberately hangs
0:
    leaq ___dtors_st(%rip),%rdi
    leaq ___dtors_en(%rip),%rsi
    call invoke_function_array

    call halt_forever

.align 16
invoke_function_array:
    .cfi_startproc
    .cfi_def_cfa_offset 8

    push_cfi %rbx
    push_cfi %rbp
    push_cfi %rbp

    movq %rdi,%rbx
    movq %rsi,%rbp
0:
    cmpq %rbx,%rbp
    jbe 0f
    call *(%rbx)
    addq $8,%rbx
    jmp 0b
0:
    pop_cfi %rbp
    pop_cfi %rbp
    pop_cfi %rbx

    .cfi_endproc
    ret

.align 16
.global __cxa_pure_virtual
.hidden __cxa_pure_virtual
__cxa_pure_virtual:
    leaq pure_call_message(%rip),%rdi
    jmp panic

.align 16
.global __cxa_atexit
__cxa_atexit:
    ret

.section .rodata
pure_call_message:
    .string "Pure virtual function called"

.align 8
.global __dso_handle
__dso_handle: .quad __dso_handle


//    push %rax
//    push %rdx
//    push %rcx
//    mov %cr4,%rax
//    or $CPU_CR4_FSGSBASE,%rax
//    mov %rax,%cr4
//    rdtscp
//    shl $32,%rdx
//    or %rdx,%rax
//    mov %rax,%r12
//    call rdfsbase_perf
//    rdtscp
//    shl $32,%rdx
//    or %rdx,%rax
//    sub %r12,%rax
//    mov $60000000,%rcx
//    xor %edx,%edx
//    div %rcx
//    pop %rcx
//    pop %rdx
//    pop %rax


//            push %rax
//            push %rdx
//            push %rcx
//            mov %cr4,%rax
//            or $CPU_CR4_FSGSBASE,%rax
//            mov %rax,%cr4
//            rdtscp
//            shl $32,%rdx
//            or %rdx,%rax
//            mov %rax,%r12
//            mov $60000000,%ecx
//        .align 16
//        0:
//        .macro iter_step
//            mov %cr2,%rax
//            add $23,%r9
//            add $23,%r9
//            add $23,%r9
//            add $23,%r9
//            add $23,%r9
//            add $23,%r9
//            add $23,%r9
//            add $23,%r9
//        .endm

//        .macro iter
//            iter_step
//            iter_step
//            iter_step
//            iter_step
//            iter_step
//            iter_step
//            iter_step
//            iter_step
//        .endm

//        .macro iter_block
//            iter
//            iter
//            iter
//            iter
//            iter
//            iter
//            iter
//            iter
//        .endm

//            iter_block

//            add $-1,%ecx
//            jnz 0b
//            rdtscp
//            shl $32,%rdx
//            or %rdx,%rax
//            sub %r12,%rax
//            mov $60000000,%rcx
//            xor %edx,%edx
//            div %rcx
//            pop %rcx
//            pop %rdx
//            pop %rax
