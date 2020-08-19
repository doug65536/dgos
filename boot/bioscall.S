#include "gdt_sel.h"

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

    // Setup %ss close to 64KB below esp (alignment permitting)
    // setup esp to the distance from ebp to esp,
    // so nothing really changed
    // the stack frame for this function is reachable from real mode stack
0:  lea (-(64 * 1024) + 15 + 16*4)(%esp),%ebp
    and $ -16,%ebp
    mov %ebp,%edx
    shr $ 4,%edx

    // Jump to 16 bit protected mode to load 64KB segment limits
    ljmp $ GDT_SEL_PM_CODE16,$ 0f
.code16
    // 16-bit protected mode

    // Load 16-bit data segments
0:  movw $ GDT_SEL_PM_DATA16,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs
    movw %ax,%ss

    // Turn off protected mode
    movl %cr0,%eax
    and $ -2,%eax
    mov %eax,%cr0

    // Real mode
    ljmp $ 0,$ 0f

    // Point ss:esp to where %esp was, where we copied the bios_regs_t
0:  mov %dx,%ss
    sub %ebp,%esp

    // If it got a perfect 64KB stack size, clear bit 16 (which would be 1)
    movzwl %sp,%esp

    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs

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

    // Translate real mode ss:sp back to flat esp
    mov %ss,%edx
    shl $ 4,%edx
    // There's no way sp could be zero here, don't waste space handling it
    movzwl %sp,%ebp
    movw %ax,%ss
    lea (%edx,%ebp),%esp

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
