#include "gdt.h"

.code32

.global bioscall
bioscall:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    // Save pointer to bios_regs_t parameter on stack
    pushl %eax

    // Use ebp for accessing the stack
    movl %eax,%ebp

    // Save interrupt vector on the stack
    pushl (,%edx,4)

    call cpu_a20_exitpm

    // Jump to 16 bit protected mode to load 64KB segment limits
    ljmp $GDT_SEL_PM_CODE16,$0f
0:
.code16
    // 16-bit protected mode

    // Load 16-bit data segments
    movw $GDT_SEL_PM_DATA16,%dx
    movw %dx,%ds
    movw %dx,%es
    movw %dx,%fs
    movw %dx,%gs
    movw %dx,%ss

    // Turn off protected mode
    movl %cr0,%edx
    and $-2,%dx
    mov %edx,%cr0

    // Real mode
    ljmp $0,$0f
0:
    xorw %dx,%dx
    movw %dx,%ds
    movw %dx,%es
    movw %dx,%fs
    movw %dx,%gs
    movw %dx,%ss

    // Sane enough to allow BIOS to handle IRQs now
    sti

    movl   (%bp),%eax
    movl  4(%bp),%ebx
    movl  8(%bp),%ecx
    movl 12(%bp),%edx
    movl 16(%bp),%esi
    movl 20(%bp),%edi
    // ...do ebp last
    movw 28(%bp),%ds
    movw 30(%bp),%es
    movw 32(%bp),%fs
    movw 34(%bp),%gs
    // Lose ebp last
    movl 24(%bp),%ebp
    // Ignore incoming eflags

    // Simulate interrupt call
    pushfw
    lcallw *2(%esp)

    // Save returned eflags
    pushfl

    // Save returned ebp
    pushl %ebp

    // get regs pointer off stack into ebp
    movl 12(%esp),%ebp

    // Write back output registers to regs
    movl %eax,  (%bp)
    movl %ebx, 4(%bp)
    movl %ecx, 8(%bp)
    movl %edx,12(%bp)
    movl %esi,16(%bp)
    movl %edi,20(%bp)
    // store ebp last
    movw %ds ,28(%bp)
    movw %es ,30(%bp)
    movw %fs ,32(%bp)
    movw %gs ,34(%bp)

    // ecx = saved ebp, edx = saved flags
    popl %ecx
    popl %edx

    // Write back output ebp and flags
    movl %ecx,24(%bp)
    movl %edx,36(%bp)

    // Go back into protected mode
    cli
    lgdt gdtr

    // Turn on protected mode
    movl %cr0,%edx
    or $1,%dx
    movl %edx,%cr0

    ljmpw $GDT_SEL_PM_CODE32,$0f
0:
.code32
    movl $GDT_SEL_KERNEL_DATA,%eax
    movl %eax,%ds
    movl %eax,%es
    movl %eax,%fs
    movl %eax,%gs
    movl %eax,%ss

    addl $8,%esp

    call cpu_a20_enterpm

    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
