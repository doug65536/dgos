#include "x86/gdt_sel.h"

.section .lowtext, "ax", @progbits
.code32

// eax points to bios_regs_t structure,
// edx contains interrupt number,
// cl is true if A20 should be toggled
// void bioscall(bios_regs_t *regs, uint32_t intr,
//               bool cpu_a20_toggle = true);
.global bioscall
bioscall:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    // Save A20 toggle flag
    pushl %ecx

    // Save pointer to bios_regs_t parameter on stack
    pushl %eax

    // Save interrupt vector (cs:ip from real mode IDT) on the stack
    pushl (,%edx,4)

    // Copy bios_regs_t to the stack, in case eax is outside first 1MB
    pushl 36(%eax)
    pushl 32(%eax)
    pushl 28(%eax)
    pushl 24(%eax)
    pushl 20(%eax)
    pushl 16(%eax)
    pushl 12(%eax)
    pushl 8(%eax)
    pushl 4(%eax)
    pushl (%eax)

    test %cl,%cl
    jz 0f
    call cpu_a20_exitpm

0:  // Jump to 16 bit protected mode to load 64KB segment limits
    ljmpw $ GDT_SEL_PM_CODE16,$ 0f
.code16
    // 16-bit protected mode

    // Load 16-bit data segments
0:  movl $ GDT_SEL_PM_DATA16,%eax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs
    // Point ss:esp to where %esp was, where we copied the bios_regs_t
    // using stack segment with nonzero base
    movl $ GDT_SEL_PM_STAC16,%eax
    mov $ ___initial_stack_limit,%edx
    // Atomically set ss and adjust esp
    movw %ax,%ss
    subl %edx,%esp
    // Turn ss base into real mode segment
    shrl $ 4,%edx

    // Turn off protected mode
    movl %cr0,%eax
    and $ -2,%eax
    mov %eax,%cr0

    // Real mode
    ljmpw $ 0,$ 0f

    // Load real mode ss
0:  mov %dx,%ss

    xorl %eax,%eax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs

    // Real mode IDT
    lidtl idtr_rm+2

    // Sane enough to allow BIOS to handle IRQs now
    sti

    // Load the context from the bios_regs_t structure copy we pushed
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx
    popl %esi
    popl %edi
    popl %ebp
    popw %ds
    popw %es
    popw %fs
    popw %gs
    popfl

    // Simulate interrupt call
    pushfw
    lcallw *2(%esp)

    // Copy the returned context to the stack
    pushfl
    pushw %gs
    pushw %fs
    pushw %es
    pushw %ds
    pushl %ebp
    pushl %edi
    pushl %esi
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax

    // Go back into protected mode
    cli
    cs lgdtl gdtr

    // Compiled code is assuming EFLAGS.DF is 0
    cld

    // Turn on protected mode
    movl %cr0,%eax
    or $ 1,%eax
    movl %eax,%cr0

    ljmpw $ GDT_SEL_PM_CODE32,$ 0f

.code32
0:  movl $ GDT_SEL_KERNEL_DATA,%eax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs

    // Load 16 bit stack segment
    mov $ ___initial_stack_limit,%edx
    // Atomically set ss and adjust esp
    movw %ax,%ss
    addl %edx,%esp

    // 32-bit IDT
    lidtl idtr_32+2

    // Copy saved outgoing context back into bios_regs_t structure
    mov 44(%esp),%eax
    popl (%eax)
    popl 4(%eax)
    popl 8(%eax)
    popl 12(%eax)
    popl 16(%eax)
    popl 20(%eax)
    popl 24(%eax)
    popl 28(%eax)
    popl 32(%eax)
    popl 36(%eax)

    addl $ 8,%esp

    // Restore A20 toggle parameter
    popl %ecx

    // Restore callee saved registers and tail call
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    // Either tail call or just return
    test %cl,%cl
    jz 0f
    jmp cpu_a20_enterpm
0:  ret
