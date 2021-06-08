#include "x86/gdt_sel.h"
#include "x86/gdt_macros.h"
#include "x86/cpu_constants.h"

.code16
.section .smp.entry, "wx", @progbits

// This must be position independent code
// Note that this code is called with the cs set to
// some segment and ip is zero
.balign 16
.global ap_entry
ap_entry:
    cli
    cld

    // ds = cs
    movl %cs,%eax
    movw %ax,%ds

    // Expect 32 bit operations and addressing modes to be much faster
    // even though they need prefixes in real mode
    xor %eax,%eax
    mov %ax,%ss
    mov $ 0xFFF0,%esp

    // Enable the cache immediately, in case the BIOS is bad
    // Enable write protection and configure FPU to modern operation
    movl %cr0,%eax
    andl $ ~CPU_CR0_CD & ~CPU_CR0_NW & ~CPU_CR0_EM,%eax
    orl $ CPU_CR0_WP | CPU_CR0_MP | CPU_CR0_NE,%eax
    wbinvd
    movl %eax,%cr0

    // Get linear address of ap_entry into ebx
    movl %cs,%ebx
    shll $ 4,%ebx

    //
    // Patch the linear address of the trampoline GDT into the trampoline GDTR
    //

    // Calculate the cs-relative offset of gdtr
    movl $ gdtr - ap_entry,%edi

    // Calculate the linear address of gdt
    leal gdt - ap_entry(%ebx),%ecx

    // Update the offset of the GDTR
    movl %ecx,2(%edi)

    // Load the cs-relative GDTR
    lgdtl (%edi)

    //
    // Patch the linear address of the pm_entry label into pm_entry_vector

    // Calculate the cs-relative offset of pm_entry_vector
    movl $ pm_entry_vector - ap_entry,%edi

    // Calculate the linear address of pm_entry
    leal pm_entry - ap_entry(%ebx),%ecx

    // Update the offset part of the pm_entry_vector far pointer
    movl %ecx,(%edi)

    // Enable protected mode and far jump into protected mode, loading cs
    orl $ CPU_CR0_PE,%eax
    movl %eax,%cr0
    ljmpl *(%edi)

.balign 16
pm_entry:
.code32
    // Load all of the segment registers with 32 bit data segments
    movl $ GDT_SEL_PM_DATA32,%eax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs
    movw %ax,%ss
    movl $ 0xFFF0,%esp

    //
    // Patch the linear address of lm_entry into lm_entry_vector
    //

    // Calculate the linear address of lm_entry_vector
    leal lm_entry_vector - ap_entry(%ebx),%edi

    // Calculate the linear address of the lm_entry label
    leal lm_entry - ap_entry(%ebx),%ecx

    // Update the lm_entry_vector far pointer offset to point to lm_entry
    movl %ecx,(%edi)

    //
    // Enter long mode
    //

    // Enable PAE
    movl %cr4,%eax
    orl $ CPU_CR4_PAE,%eax
    movl %eax,%cr4

    //
    // Load the PDBR
    //

    // Calculate linear address of root_page_dir
    leal root_page_dir - ap_entry(%ebx),%eax

    // Load PDBR
    movl (%eax),%eax
    movl %eax,%cr3

    // Enable long mode (set MSR bit EFER.LME)
    movl $ CPU_MSR_EFER,%ecx
    rdmsr
    orl $ CPU_MSR_EFER_LME,%eax
    wrmsr

    // Enable paging
    movl %cr0,%eax
    orl $ CPU_CR0_PG,%eax
    movl %eax,%cr0
    ljmpl *(%edi)

.balign 16
lm_entry:
.code64
    //
    // Get mp_entry_point
    //

    // Calculate linear address of smp_entry_point
    leal smp_entry_point - ap_entry(%rbx),%edi

    // rdi = entry point
    movq (%rdi),%rdi

    // rax = arch_run_kernel - ap_entry + linear address of trampoline
    leal arch_run_kernel - ap_entry(%rbx),%eax

    // rsi = params parameter, nullptr
    xorl %esi,%esi

    // set cr3 parameter
    movq %cr3,%rdx

    // rcx = nx_available
    leal nx_available - ap_entry(%rbx),%ecx
    movzbl (%rcx),%ecx

    // jump to code64_run_kernel
    jmpq *%rax

.balign 8
pm_entry_vector:
    .int 0
    .hword GDT_SEL_PM_CODE32

.balign 8
lm_entry_vector:
    .int 0
    .hword GDT_SEL_KERNEL_CODE64

    // Emit a copy of the GDT here so it ends up in low memory
    // The BSP GDT is relocated far above 1MB boundary
.balign 64
.global gdt
.global gdtr
    gdt_emit gdt, gdtr
