#include "cpu/control_regs_constants.h"
#include "cpu/interrupts.h"
#include "cfi_helpers.h"
#include "cpu_bug.h"

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
    .cfi_startproc

    debugind '1'

    // Align the stack
    andq $-16,%rsp

    cld

    // Save parameter in call-preserved register
    movl %ecx,%r15d
    xorl %ebp,%ebp

    debugind '2'

#ifdef _CALL_TRACE_ENABLED
    // Enable FSGSBASE ASAP if tracing
    movq %cr4,%rax
    orq $0x10000,%rax
    movq %rax,%cr4

    // Copy APIC ID into IA32_TSC_AUX
    xorl %eax,%eax
    cpuid
    cmpl $ 0xB,%eax
    jb .Lapic8bit

    // Read cpuid leaf 0xB, edx[31:0] contains the x2APIC ID
    movl $ 0xB,%eax
    cpuid
    movl %edx,%eax
    xorl %edx,%edx
    jmp .Lset_tsc_aux

.Lapic8bit:
    // Read cpuid leaf 1, ebx[31:24] contains the APIC ID
    movl $ 1,%eax
    cpuid
    shrl $ 24,%ebx
    movl %ebx,%eax
    xorl %edx,%edx

.Lset_tsc_aux:
    movl $ CPU_MSR_TSC_AUX,%ecx
    wrmsr
#endif

    // See if this is the bootstrap processor
    movl $ CPU_APIC_BASE_MSR,%ecx
    rdmsr
    testl $ CPU_APIC_BASE_BSP,%eax
    jnz bsp_init

    debugind 'Z'

ap_init:
    // This is not the bootstrap CPU

    // Get off the bootloader stack immediately
    // Use the low half of the BSP stack briefly until thread startup
    leaq kernel_stack(%rip),%rdx
    movq kernel_stack_size(%rip),%rbx
    shrq $1,%rbx
    leaq (%rdx,%rbx),%rsp

    xorl %ebp,%ebp
    pushq %rbp

    // MP processor entry
    call mp_main

    // mp_main should not return
0:  call halt_forever
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

    xorl %edi,%edi
    call cpu_init_early

    debugind '7'

    // Get SSE context offsets
    xorl %edi,%edi
    call idt_xsave_detect

    debugind '8'

    call e9debug_init

    debugind '9'

//    // Resolve ifunc
//    leaq __rela_dyn_st(%rip),%r12
//    leaq __rela_dyn_en(%rip),%r13
//.Lrela_iplt_next:
//    cmpq %r13,%r12
//    jz .Lrela_iplt_done
//
//    movq 8(%r12),%rax
//
//    // Only apply STT_GNU_IFUNC (0x25)
//    cmpq $ 0x25,%rax
//    jnz .Lrela_iplt_skip
//
//    // Call the resolver
//    mov 16(%r12),%rax
//    indirect_call rax
//    //call * 16(%r12)
//
//    // Read PLT vector pointer from relocation record
//    movq (%r12),%rdx
//    // Update the PLT with the resolver return value
//    movq %rax,(%rdx)
//.Lrela_iplt_skip:
//    addq $ 24,%r12
//    jmp .Lrela_iplt_next
//.Lrela_iplt_done:

#ifdef _ASAN_ENABLED
    // Mark the callout array initialized
    leaq ___callout_array_st(%rip),%rdi
    leaq ___callout_array_en(%rip),%rsi
    subq %rdi,%rsi
    call __asan_storeN_noabort

    // Mark the kernel parameters initialized
    movq %r15,%rdi
    movl $ 13*8,%esi
    call __asan_storeN_noabort

    // Mark .rodata initialized
    leaq ___rodata_st(%rip),%rdi
    leaq ___rodata_en(%rip),%rsi
    subq %rdi,%rsi
    call __asan_storeN_noabort

    // Mark .rodata initialized
    leaq __bspstk_st(%rip),%rdi
    leaq __bspstk_en(%rip),%rsi
    subq %rdi,%rsi
    call __asan_storeN_noabort

    // Mark .data initialized
    leaq ___data_st(%rip),%rdi
    leaq ___data_en(%rip),%rsi
    subq %rdi,%rsi
    call __asan_storeN_noabort

    // Mark .bss initialized
    leaq ___bss_st(%rip),%rdi
    leaq ___bss_en(%rip),%rsi
    subq %rdi,%rsi
    call __asan_storeN_noabort

    // XX: hardcoded
    movabs $(-(518 << 30)),%rdi
    movabs $(4 << 30),%rsi
    call __asan_storeN_noabort

    // Mark VGA initialized
    movl $ 0xb8000,%edi
    movl $ 0x08000,%esi
    subl %edi,%esi
    call __asan_storeN_noabort

    movb $1,asan_ready(%rip)
#endif

    // Call the constructors
    leaq ___ctors_st(%rip),%rdi
    leaq ___ctors_en(%rip),%rsi
    call invoke_function_array

    call main

    debugind '?'

    movq %rax,%rdi
    call exit

    .cfi_endproc

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
0:  leaq ___dtors_st(%rip),%rdi
    leaq ___dtors_en(%rip),%rsi
    call invoke_function_array

    call halt_forever

.align 16
invoke_function_array:
    .cfi_startproc

    push_cfi %rbx
    push_cfi %rbp
    push_cfi %rbp

    movq %rdi,%rbx
    movq %rsi,%rbp
0:  cmpq %rbx,%rbp
    jbe 0f
    mov (%rbx),%rax
    indirect_call rax
    //call *(%rbx)
    addq $8,%rbx
    jmp 0b
0:  pop_cfi %rbp
    pop_cfi %rbp
    pop_cfi %rbx

    .cfi_endproc
    ret

.align 16
.global register_eh_frame
register_eh_frame:
    // Register .eh_frame for libgcc unwind
    leaq ___eh_frame_st(%rip),%rdi
    jmp __register_frame

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
