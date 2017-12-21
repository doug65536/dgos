#include "cpu/control_regs_constants.h"
#include "cpu/interrupts.h"

.section .entry, "ax"

.macro debugind ch
    movb $\ch,0xb8000
.endm

// Things that must be done before entering any compiler
// generated code:
//  - Set up stack (aligned!)
//  - Enable SSE
//  - Initialize SSE control word (mxcsr)
//  - Initialize x87 control word
//  - Initialize SSE control word mask
//  - Initialize xsave

.align 16
.global entry
.hidden entry
.type entry,@function
entry:
    debugind '1'

    // Align the stack
    and $-16,%rsp

    // Save parameter in call-preserved register
    mov %ecx,%r15d
    xor %ebp,%ebp

    // Enable SSE
    // This must be done before jumping into C code
    mov %cr4,%rax
    or $(CPU_CR4_OFXSR | CPU_CR4_OSXMMEX),%rax
    mov %rax,%cr4

    debugind '2'

    // See if this is the bootstrap processor
    mov $0x1B,%ecx
    rdmsr
    test $0x100,%eax
    jnz bsp_init

    debugind 'Z'

ap_init:
    // This is not the bootstrap CPU

    // Get IDT up asap
    call idt_gdt_tss_init

    // Initialize AP MXCSR
    mov $(CPU_MXCSR_ELF_INIT),%eax
    and default_mxcsr_mask,%eax
    push %rax
    ldmxcsr (%rsp)

    // Initialize AP FPU control word
    // Round nearest, 53 bit significand, all exceptions masked
    movw $(CPU_FPUCW_ELF_INIT),(%rsp)
    fldcw (%rsp)

    pop %rax

    // xsave-enable AVX on AP
    call set_xsave_states_ap

    // Align stack
    xor %ebp,%ebp
    push %rbp

    // MP processor entry
    jmp mp_main

bsp_init:
    // This is the bootstrap processor

    debugind '3'

    lea kernel_stack,%rdx
    mov kernel_stack_size,%rbx
    lea (%rdx,%rbx),%rsp

    // Store the physical memory map address
    // passed in from bootloader
    mov %r15d,%eax
    shr $20,%eax
    mov %rax,phys_mem_map_count

    mov %r15d,%eax
    and $0x000FFFFF,%eax
    mov %rax,phys_mem_map

    // Get IDT and TSS up asap
    call idt_gdt_tss_init

    // Save the CPU_MXCSR_MASK

    // Allocate 512 bytes and cache line align stack
    mov %rsp,%rdx
    sub $512,%rsp
    and $-64,%rsp

    // Initialize FPU to 64 bit precision,
    // all exceptions masked, round to nearest
    fninit
    movw $((0 << 10) | (2 << 8) | 0x3F),(%rsp)
    fldcw (%rsp)

    fxsave64 (%rsp)

    // Read CPU_MXCSR_MASK from fxsave output and store it
    mov 28(%rsp),%eax
    mov %eax,default_mxcsr_mask

    // Set MXCSR
    // 0x3F = all exceptions masked
    // 0 = round to nearest
    mov $(CPU_MXCSR_RC_n(CPU_MXCSR_RC_NEAREST) | CPU_MXCSR_MASK_ALL),%ecx
    and %eax,%ecx
    mov %ecx,24(%rsp)
    ldmxcsr 24(%rsp)

    // Restore stack pointer
    mov %rdx,%rsp

    debugind '4'
    call cpuid_init

    debugind '6'

    // Must xsave-enable AVX early if available
    call detect_xsave_states_bsp

    // Get SSE context offsets
    xor %edi,%edi
    call idt_xsave_detect

    // Call the constructors
    mov $___init_st,%rdi
    mov $___init_en,%rsi
    call invoke_function_array

    // Notify constructors ran
    mov $'C',%edi
    call callout_call

    debugind '7'

    xor %edi,%edi
    call cpu_init

    call e9debug_init

    debugind '8'

    // Initialize text devices
    mov $'V',%edi
    call callout_call

    xor %edi,%edi
    call cpu_init_stage2

    debugind '9'

    xor %edi,%edi
    call cpu_hw_init

    debugind 'A'

    // Initialize GDB stub
    //call gdb_init

    // Initialize early-initialized devices
    mov $'E',%edi
    call callout_call

    debugind 'B'

    call main

    debugind '?'

    mov %rax,%rdi
    call exit

idt_gdt_tss_init:
    push %rbx
    xor %ebx,%ebx
    mov $256,%ecx
0:
    movq isr_entry_points(%rbx),%rax
    // IDT structure:
    //  uint16_t offset_lo;     // offset bits 15:0
    //  uint16_t selector;      // a code segment selector in GDT
    //  uint8_t ist;            // interrupt stack table index
    //  uint8_t type_attr;      // type and attributes
    //  uint16_t offset_hi;     // offset bits 31:16
    //  uint32_t offset_64_31;  // offset bits 63:32
    //  uint32_t reserved;
    movw %ax,idt(,%rbx,2)
    sarq $16,%rax
    movw $IDT_SEL,2+idt(,%rbx,2)
    movb $0,4+idt(,%rbx,2)
    movb $(IDT_PRESENT | IDT_INTR),5+idt(,%rbx,2)
    movw %ax,6+idt(,%rbx,2)
    sarq $16,%rax
    movl %eax,8+idt(,%rbx,2)
    movl $0,12+idt(,%rbx,2)
    add $8,%ebx
    dec %ecx
    jnz 0b

    // Setup IST entries
    movb $1,idt+4+16*INTR_EX_STACK
    movb $2,idt+4+16*INTR_EX_DBLFAULT

    // Load IDTR
    sub $(24*8),%rsp
    movw $(16 * 256 - 1),6(%rsp)
    movq $idt,8(%rsp)
    lidtq 6(%rsp)

    // Load GDTR
    movw $(GDT_SEL_END - 1),6(%rsp)
    movq $gdt,8(%rsp)
    lgdtq 6(%rsp)

    // Poke initial TSS into TSS GDT entry
    //  uint16_t limit_low;
    //	uint16_t base_low;
    //	uint8_t base_middle;
    //	uint8_t access;
    //	uint8_t flags_limit_high;
    //	uint8_t base_high;

    movq $init_tss,%rax
    // Limit 15:0
    movw $(init_tss_end - init_tss - 1),GDT_SEL_TSS+gdt
    // Base 15:0
    movw %ax,2+GDT_SEL_TSS+gdt
    sarq $16,%rax
    // Base 23:16
    movb %al,4+GDT_SEL_TSS+gdt
    sarq $8,%rax
    // access
    movb $(GDT_ACCESS_PRESENT | GDT_TYPE_TSS),5+GDT_SEL_TSS+gdt
    // Limit 19:16, flags
    movb $0,6+GDT_SEL_TSS+gdt
    // Base 21:24
    movb %al,7+GDT_SEL_TSS+gdt
    sarq $8,%rax
    // Base 63:32
    movl %eax,8+GDT_SEL_TSS+gdt
    // Reserved
    movl $0,12+GDT_SEL_TSS+gdt

    mov $GDT_SEL_TSS,%eax
    ltr %ax

    add $(24*8),%rsp

    pop %rbx
    ret

.section .data

.align 8
    // TSS layout is completely misaligned without nudging it up 4 bytes
    //  uint32_t dummy_align;
    .space 4
init_tss:
    // TSS structure
    //  uint32_t reserved0;
    .int 0
    //
    //  uint64_t rsp[3];
    .quad 0
    .quad 0
    .quad 0
    //
    // uint64_t reserved;
    .quad 0

    //  uint64_t ist[8];
    .quad kernel_stack-32768
    .quad kernel_stack-32768
    .space 6*8
    //
    //  uint32_t reserved3;
    .int 0
    //  uint32_t reserved4;
    .int 0
    //
    //  uint16_t reserved5;
    .short 0
    //  uint16_t iomap_base;
    .short init_tss_end-init_tss
    //  uint32_t dummy_align2;
    .int 0
    //
    //  // entry 0 is rsp[0], rest are ist stacks
    //  void *stack[8];
    .space 8*8
init_tss_end:

.section .text
.align 16
.global exit
.hidden exit
exit:
    // Ignore exitcode
    // Kernel exit just calls destructors
    // and deliberately hangs
0:
    lea ___fini_st(%rip),%rdi
    lea ___fini_en(%rip),%rsi
    call invoke_function_array

    call halt_forever

.align 16
invoke_function_array:
    push %rbx
    push %rbp
    push %rbp
    mov %rdi,%rbx
    mov %rsi,%rbp
0:
    cmp %rbx,%rbp
    jbe 0f
    call *(%rbx)
    add $8,%rbx
    jmp 0b
0:
    pop %rbp
    pop %rbp
    pop %rbx
    ret

#define XSAVE_WANTED \
    XCR0_X87 | \
    XCR0_SSE | XCR0_AVX | \
    XCR0_AVX512_OPMASK | \
    XCR0_AVX512_UPPER | \
    XCR0_AVX512_XREGS

.align 16
detect_xsave_states_bsp:
    push %rbx
    call cpuid_has_xsave
    test %al,%al
    jz 0f

    mov $CPUID_INFO_XSAVE,%eax
    xor %ecx,%ecx
    cpuid

    mov %eax,xsave_supported_states
    and $XSAVE_WANTED,%eax
    mov %eax,xsave_enabled_states

0:
    pop %rbx
    jmp set_xsave_states_ap

.align 16
set_xsave_states_ap:
    push %rbx

    call cpuid_has_xsave
    test %al,%al
    jz 0f

    mov %cr4,%rax
    or $CPU_CR4_OSXSAVE,%rax
    mov %rax,%cr4

    xor %ecx,%ecx
    xgetbv
    and xsave_supported_states,%eax
    or xsave_enabled_states,%eax
    xsetbv

0:
    pop %rbx
    ret

// Callout to initialize AP CPU
.align 16
.global mp_main
.hidden mp_main
mp_main:
    push %rbp
    mov %rsp,%rbp

    mov $'S',%edi
    call callout_call

    pop %rbp
    ret

.global __cxa_pure_virtual
.hidden __cxa_pure_virtual
__cxa_pure_virtual:
    mov $pure_call_message,%rdi
    jmp panic

.align 16
.global __cxa_atexit
__cxa_atexit:
    ret

.section .rodata
pure_call_message:
    .string "Pure virtual function called"

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
