## entry.S

#include "cpu/control_regs_constants.h"
#include "cpu/interrupts.h"
#include "cpu/asm_constants.h"
#include "cfi_helpers.h"
#include "cpu_bug.h"

.section .entry, "ax", @progbits

// Things that must be done before entering any compiler
// generated code:
//  - Set up stack (aligned!)
//  - Clear EFLAGS.DF
//  - cs base == es base == ss base

.balign 16
.global entry
.hidden entry
.type entry,@function
entry:
    .cfi_startproc

    .cfi_undefined rip
    .cfi_undefined rsp
    .cfi_undefined rbp
    .cfi_undefined rbx
    .cfi_undefined r12
    .cfi_undefined r13
    .cfi_undefined r14
    .cfi_undefined r15

    // Save parameter in call-preserved register
    movl %ecx,%r15d
    xorl %ebp,%ebp

    // Initialize GSBASE ASAP
    imul $ CPU_INFO_SIZE,thread_booting_ap_index(%rip),%eax
    leaq cpus(%rip),%rdx
    addq %rdx,%rax
    movq %rax,%rdx
    shrq $ 32,%rdx
    movl $ CPU_MSR_GSBASE,%ecx
    wrmsr
    // Set cpu self pointer at gs:0 (abs 0 encoding is 9 bytes, rbp rel is 5)
    mov %rax,%gs:0(%rbp)

    // Force initialize cpus[cur_cpu].cur_thread
    imul $ THREAD_INFO_SIZE,thread_booting_ap_index(%rip),%eax
    leaq threads(%rip),%rdx
    addq %rdx,%rax
    movq %rax,%gs:CPU_INFO_CURTHREAD_OFS(%rbp)

    // Initialize APIC ID ASAP
    // Copy APIC ID into gs:CPU_INFO_APIC_ID_OFS
    xorl %eax,%eax
    cpuid
    cmpl $ 0xB,%eax
    jb .Lapic8bit

    // See if x2apic is supported
    mov $ 1,%eax
    cpuid
    test $ (1 << 21),%ecx
    jz .Lapic8bit

    // Read cpuid leaf 0xB, edx[31:0] contains the x2APIC ID
    movl $ 0xB,%eax
    cpuid
    movl %edx,%eax
    jmp .Lgot_apic_id

.Lapic8bit:
    // Read cpuid leaf 1, ebx[31:24] contains the APIC ID
    movl $ 1,%eax
    cpuid
    shrl $ 24,%ebx
    movl %ebx,%eax

.Lgot_apic_id:
    movl %eax,%gs:CPU_INFO_APIC_ID_OFS

#ifdef _CALL_TRACE_ENABLED
    // Enable FSGSBASE ASAP if tracing
    movq %cr4,%rax
    orq $ 0x10000,%rax
    movq %rax,%cr4
#endif

    // See if this is the bootstrap processor
    movl $ CPU_APIC_BASE_MSR,%ecx
    rdmsr
    testl $ CPU_APIC_BASE_BSP,%eax
    jnz bsp_init

ap_init:
    // This is not the bootstrap CPU

    // Get off the bootloader stack immediately
    // Use the low half of the BSP stack briefly until thread startup
    leaq kernel_stack(%rip),%rdx
    movq kernel_stack_size(%rip),%rbx
    shrq $ 1,%rbx
    leaq (%rdx,%rbx),%rsp

    xorl %ebp,%ebp
    pushq %rbp

    // MP processor entry
    call mp_main

    // mp_main should not return
0:  call halt_forever
    // halt_forever should not return either! Stop poking at the mainboard
    jmp 0b

bsp_init:
    // This is the bootstrap processor

    leaq kernel_stack(%rip),%rdx
    movq kernel_stack_size(%rip),%rbx
    leaq (%rdx,%rbx),%rsp

    // Store the kernel parameters passed in from bootloader
    movq %r15,kernel_params(%rip)

    call cpuid_init

    xorl %edi,%edi
    call cpu_init_early

    call e9debug_init

    // Resolve ifunc
    leaq ___rela_plt(%rip),%r12
    leaq __rela_iplt_end(%rip),%r13
.Lrela_iplt_next:
    cmpq %r13,%r12
    jae .Lrela_iplt_done

    movq 8(%r12),%rax

    // Only apply STT_GNU_IFUNC (0x25)
    cmpq $ 0x25,%rax
    jnz .Lrela_iplt_skip

    // Call the resolver
    mov 16(%r12),%rax
    indirect_call rax

    // Read PLT vector pointer from relocation record
    movq (%r12),%rdx
    // Update the PLT with the resolver return value
    movq %rax,(%rdx)
.Lrela_iplt_skip:
    addq $ 24,%r12
    jmp .Lrela_iplt_next
.Lrela_iplt_done:

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

    // Mark .bspstk initialized
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

    // Mark VGA initialized
//    movl $ 0xb8000,%edi
//    movl $ 0x08000,%esi
//    call __asan_storeN_noabort

    movb $ 1,asan_ready(%rip)
#endif

    // Call the constructors
    leaq __init_array_early_start(%rip),%rdi
    leaq __init_array_early_end(%rip),%rsi
    call __invoke_function_array

    leaq __init_array_start(%rip),%rdi
    leaq __init_array_end(%rip),%rsi
    call __invoke_function_array

//.pushsection .data, "aw", @progbits
//.balign 8
//.space 8
//perfdata:
//.space 8*64
//.popsection
//
//    // run 4+ billion loops for warmup
//    xor %eax,%eax
//.balign 16
//0:
//    add $ -1,%eax
//    jnz 0b
//
//    andq $ -16,%rsp
//
//    movl %ss,%eax
//    push %rax
//
//    push $ 0
//
//    pushfq
//
//    movl %cs,%eax
//    push %rax
//
//    mov %rsp,2*8(%rsp)
//
//    mov $ 64,%r9d
//    lea perfdata(%rip),%r8
//.balign 16
//0:
//    lfence
//    rdtsc
//    mov %eax,perfdata-8(%rip)
//    mov %edx,perfdata-4(%rip)
//
//    .rept 1000
//    call just_iretq
//    .endr
//
//    lfence
//    rdtsc
//    sub perfdata-8(%rip),%eax
//    sbb perfdata-4(%rip),%edx
//    shlq $ 32,%rdx
//    orq %rdx,%rax
//    mov %rax,(%r8)
//    add $ 8,%r8
//    add $ -1,%r9d
//    jnz 0b
//
//    jmp .Ldone_perf
//
//.balign 16
//just_iretq:
//    iretq
//
//.Ldone_perf:
//    add $ 8*4,%rsp

    call kernel_main

    movq %rax,%rdi
    call exit

    .cfi_endproc

.section .data, "aw", @progbits

.balign 16
    // TSS layout is completely misaligned without nudging it up 4 bytes
.global early_tss
early_tss:
    .space 4
    // TSS structure
    //  uint32_t reserved0;
    .int 0
    //
    //  uint64_t rsp[3];
    .quad 0
    .quad 0
    .quad 0
    //
    //  uint64_t ist[8]; 2 emergency stacks in last 8KB of main stack
    .quad 0
    .quad kernel_stack+65536-4096*1
    .quad kernel_stack+65536-4096*2
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
    .short early_tss_end - (early_tss + 4) - 1
    //  uint32_t dummy_align2;
    .int 0
    //
    //  // entry 0 is rsp[0], rest are ist stacks
    //  void *stack[8];
    .space 8*8
early_tss_end:

.section .text, "ax", @progbits
.balign 16
.type exit,@function
.global exit
.hidden exit
exit:
    // Ignore exitcode
    // Kernel exit just calls destructors
    // and deliberately hangs
0:  leaq ___dtors_st(%rip),%rdi
    leaq ___dtors_en(%rip),%rsi
    call __invoke_function_array

    call halt_forever

.balign 16
.type __invoke_function_array,@function
__invoke_function_array:
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
    addq $ 8,%rbx
    jmp 0b
0:  pop_cfi %rbp
    pop_cfi %rbp
    pop_cfi %rbx

    .cfi_endproc
    ret

.balign 16
.type register_eh_frame,@function
.global register_eh_frame
register_eh_frame:
    // Register .eh_frame for libgcc unwind
    leaq ___eh_frame_en(%rip),%rsi
    leaq ___eh_frame_st(%rip),%rdi
    subq %rdi,%rsi
    jmp __register_frame

.balign 16
.type __cxa_pure_virtual,@function
.global __cxa_pure_virtual
// Not .hidden!
__cxa_pure_virtual:
    leaq pure_call_message(%rip),%rdi
    jmp panic

.section .rodata, "", @progbits
pure_call_message:
    .string "Pure virtual function called"

// hidden by definition, it's the whole point
.balign 8
.global __dso_handle
.hidden __dso_handle
__dso_handle: .quad __dso_handle
