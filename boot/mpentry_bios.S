#include "cpu_constants.h"
#include "gdt_sel.h"

.code16
.section .smp.entry, "wx", @progbits

// This must be position independent code
// Note that this code is called with the cs set to some segment
// and ip is zero
.global ap_entry
.global ap_entry_size
ap_entry:
    cli
    cld

    //wbinvd

    xor %ax,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%ss
    movl $ 0x0000FFF0,%esp

    lgdtl gdtr

    // Calculate the linear address of this trampoline from cs register
    mov %cs,%ecx
    shl $ 4,%ecx

    // Calculate address of protected mode entry point below
    add $ 0f - ap_entry,%ecx

    // ecx now holds address of ap_entry, push a far pointer to the stack
    pushl $ GDT_SEL_PM_CODE32
    pushl %ecx

    // Enter protected mode, enable cache and cache coherency,
    // and jump into protected mode immediately after setting CR0.PE
    mov %cr0,%eax
    or $ 1,%eax
    and $ ~CPU_CR0_CD & ~CPU_CR0_NW,%eax
    mov %eax,%cr0
    ljmpl *(%esp)

.code32
0:  movl $ GDT_SEL_PM_DATA32,%ecx
    mov %cx,%ds
    mov %cx,%es
    mov %cx,%fs
    mov %cx,%gs
    mov %cx,%ss
    movl smp_entry_point,%eax
    movl smp_entry_point+4,%edx
    mov $ enter_kernel,%ecx
    jmp *%ecx
0:  cli
    hlt
    jmp 0b

