#include "x86/gdt_sel.h"

.code16

.section .lowtext, "ax", @progbits

// Return address in bx
.global early_init
early_init:
    mov $ .Ldone_init_stack,%si
    jmp init_stack
.Ldone_init_stack:

    call detect_ancient_cpu

    // It is at least a 486 with CPUID instruction if it made it to here
    // without freezing with unsupported CPU message

    callw clear_bss

    // 32 bit instructions are okay if detect_ancient_cpu returned...

    movzwl %sp,%esp
    jmpw *%bx

.global init_stack
init_stack:
    // Convert absolute address to seg:off (because the linker cannot)
    mov %cs:initial_stack_ptr,%ax
    mov %cs:initial_stack_ptr+2,%dx

    // shift dx:ax right 4 bits (with 8088 instructions)
    mov $ 4,%cl
.Ldoubleword_shift:
    shr $ 1,%dx
    rcr $ 1,%ax
    dec %cl
    jnz .Ldoubleword_shift
    sub $ 0x1000,%ax
    mov %ax,%ss
    xor %sp,%sp
    jmp *%si

initial_stack_ptr:
    .int ___initial_stack

// Inputs:
//  edx:eax: parameter to partition boot call
//  ecx: pointer to partition boot function
//  ebx: return address

.global boot
boot:
    .cfi_startproc
    .cfi_def_cfa esp,0
    .cfi_register eip,ebx

    pushl %ebx
    .cfi_adjust_cfa_offset 4
    .cfi_offset ebx,2*-4

    pushl %ebp
    .cfi_offset ebp,3*-4

    movl %esp,%ebp
    .cfi_def_cfa ebp,3*4
    .cfi_offset eip,1*-4
    .cfi_offset ebx,2*-4
    .cfi_offset ebp,3*-4

    pushl %edi
    pushl %esi

    // esi:ebx holds boot function 64-bit parameter
    // edi holds boot function
    movl %eax,%ebx
    movl %edx,%esi
    movl %ecx,%edi

    call idt_init_32
    call idt_init_64
    call cpu_a20_check
    // Do need a20 toggles if A20 was off
    xorb $ 1,%al
    movb %al,cpu_a20_need_toggle

    cli
    movl $ gdt,2+gdtr
    lgdtl gdtr

    movl %cr0,%eax
    btsl $ 0,%eax
    movl %eax,%cr0

    movw $ GDT_SEL_PM_DATA32,%dx
    ljmpw $ GDT_SEL_PM_CODE32,$ pmode_entry

    .cfi_endproc

pmode_entry:
.code32
    .cfi_startproc
    .cfi_def_cfa esp,0
    // Tell stack traces to give up here
    .cfi_undefined eip
    .cfi_undefined esp

    movw %dx,%ds
    movw %dx,%es
    movw %dx,%fs
    movw %dx,%gs

    // Setup 32 bit stack pointing to same place it was in real mode
    // Add 64KB to final pointer if sp was 0
    // (and 1st push would wrap around 64KB forward)
    movl %ss,%eax
    movw %dx,%ss
    movl $ 0x10000,%edx
    shl $ 4,%eax
    add %edx,%eax
    test %sp,%sp
    jz .Lwraparound_sp
    sub %edx,%eax
.Lwraparound_sp:
    add %eax,%esp

    xorl %eax,%eax
    lldt %ax

    lidt idtr_32+2

    mov %esp,%ebp
    and $ -16,%esp

    // Clean eflags
    pushl $ 2
    popfl

    call cpu_a20_enterpm

    call ctors_invoke

    movl %ebx,%eax
    movl %esi,%edx
    call *%edi

    // Usually won't reach here...

    call cpu_a20_exitpm

    mov %ebp,%esp

    // Jump to 16 bit protected mode and load segments
    ljmpw $ GDT_SEL_PM_CODE16,$ 0f
.code16
0:  movw $ GDT_SEL_PM_DATA16,%dx
    movw %dx,%ds
    movw %dx,%es
    movw %dx,%fs
    movw %dx,%gs
    movw $ GDT_SEL_PM_STAC16,%dx
    movw %dx,%ss

    // Turn off protected mode
    movl %cr0,%eax
    btrl $ 0,%eax
    movl %eax,%cr0

    // Jump to real mode
    ljmpw $ 0,$ 0f
0:  xorw %dx,%dx
    movw %dx,%ds
    movw %dx,%es
    movw %dx,%fs
    movw %dx,%gs
    movl $ 0x20000,%edx
    movw %dx,%ss
    movl %ss,%edx
    andl $ 0xFFFF,%esp

    popl %esi
    popl %edi
    leavel
    retl
    .cfi_endproc
.code16

// Returns if the CPU is at least an 80486 with CPUID
// does not return if CPU is too old to be usable
// clobbers eax, ecx, edx
.global detect_ancient_cpu
detect_ancient_cpu:
    // An 8086 has flags 15:12 permanently set
    movw $ 0x0FFF,%cx
    xorw %dx,%dx
    callw change_flags
    testw $ 0xF000,%ax
    jnz detected_8086

    // An 80286 has flags 15:12 permanently clear in real mode
    orw $ -1,%cx
    movw $ 0xF000,%dx
    callw change_flags
    testw $ 0xF000,%ax
    jz detected_80286

    // It is safe to use 32 bit instructions now...

    // An 80386 has eflags 18 (AC) permanently clear
    orl $ -1,%ecx
    movl $ 1 << 18,%edx
    callw change_eflags
    testl $ 1 << 18,%eax
    jz detected_80386

    //
    // An early 80486 has eflags 21 (ID) stuck

    // Isolate bit 21 and save it
    andl $ 1 << 21,%eax
    push %eax

    // Clear nothing, set nothing
    orl $ -1,%ecx
    xorl %edx,%edx

    // Clear it if it was set
    xorl %eax,%ecx

    // Set it if it was clear
    xorl $ 1 << 21,%eax
    orl %eax,%edx

    callw change_eflags
    popl %ecx
    cmpl %ecx,%eax
    jz detected_old_486
    // CPUID instruction will work

    // Make sure the FPU exists
    // Initialized status word will have exceptions masked (1-bits)
    fninit
    xorw %ax,%ax
    // According to the 80287 manual, ax will only be zero if there is an FPU
    fnstsw %ax
    testw %ax,%ax
    jnz detected_486sx
    // FPU exists

    ret

.pushsection .lowdata, "w", @progbits
str_prefix:	.asciz "An "
str_8086:	.asciz "8086"
str_80286:	.asciz "80286"
str_80386:	.asciz "80386"
str_80486:	.asciz "early 80486"
str_80486sx:.asciz "80486-SX"
str_suffix:	.asciz " cannot run this operating system"
.popsection

detected_8086:
    mov $ str_8086,%bx
    jmp detection_failed

detected_80286:
    mov $ str_80286,%bx
    jmp detection_failed

detected_80386:
    mov $ str_80386,%bx
    jmp detection_failed

detected_old_486:
    mov $ str_80486,%bx
    jmp detection_failed

detected_486sx:
    mov $ str_80486sx,%bx

// ...fall through
detection_failed:
    mov $ str_prefix,%di
    call fail_str
    mov %bx,%di
    call fail_str
    mov $ str_suffix,%di
    call fail_str
    cli
0:  hlt
    jmp 0b

// Clobbers si, di
fail_str:
    mov $ 0xb800,%ax
    mov %ax,%es
    mov $ 0x0800,%ax
0:  lodsb
    test %al,%al
    jz 0f
    stosw
    jmp 0b
0:  xor %ax,%ax
    mov %ax,%es
    ret

// Read flags,
// bitwise and flags with %cx,
// bitwise or flags with %dx
// put flags into %ax
change_flags:
    pushf
    popw %ax
    andw %cx,%ax
    orw %dx,%ax
// ... fall through
// Write ax to the flags register then update ax with updated flags
update_flags:
    pushw %ax
    popfw
    pushfw
    popw %ax
    ret

change_eflags:
    pushfl
    popl %eax
    andl %ecx,%eax
    orl %edx,%eax
// ... fall through
// Write eax to the eflags register then update eax with updated eflags
update_eflags:
    pushl %eax
    popfl
    pushfl
    popl %eax
    ret

.global idt_init_64
idt_init_64:
    push %edi
    push %esi

    // Set up a far pointer to isr_table of 16bit at %es:(%esi,%ecx,2)
    movl $ isr_table,%eax
    mov %eax,%ecx
    mov %eax,%esi
    shr $ 4,%ecx
    and $ 0xF,%esi
    movw %cx,%es

    // Setup a far pointer to idt at %ds:(%edi,%ecx,16)
    mov $ idt_64,%eax
    mov %eax,%ecx
    mov %eax,%edi
    shr $ 4,%ecx
    and $ 0xF,%edi
    mov %cx,%ds

    add $ 64*8,%edi
    mov $ -64,%ecx
    sub %ecx,%esi
0:  movzwl %es:(%esi,%ecx,1),%eax
    movw %ax,(%edi,%ecx,8)
    movw $ GDT_SEL_KERNEL_CODE64,2(%edi,%ecx,8)
    movw $ 0x8e00,%ax
    movl %eax,4(%edi,%ecx,8)
    xorl %eax,%eax
    movl %eax,8(%edi,%ecx,8)
    movl %eax,12(%edi,%ecx,8)
    addl $ 2,%ecx
    jnz 0b

    // Far pointer to idtr in %ds:(%eax)
    mov $ idtr_64,%ecx
    mov %ecx,%edx
    mov %ecx,%eax
    shr $ 4,%ecx
    and $ 0xF,%eax
    movw %cx,%ds

    // Set limit
    movw $ 32 * 16 - 1,6(%eax)
    movl %edx,8(%eax)

    xor %eax,%eax
    mov %ax,%ds
    mov %ax,%es

    pop %esi
    pop %edi
    ret

.global idt_init_32
idt_init_32:
    push %esi
    push %edi

    // Set up a far pointer to idt_32 at %es:(%esi,%ecx,4)
    movl $ idt_32,%eax
    mov %eax,%esi
    shr $ 4,%eax
    and $ 0xF,%esi
    movw %ax,%ds

    // Translate several simple function pointers to IDT interrupt gates
    mov $ -32,%ecx
0:  movl 32*8(%esi,%ecx,8),%eax
    movw $ GDT_SEL_PM_CODE32,32*8+2(%esi,%ecx,8)
    movw $ 0x8e00,%ax
    movl %eax,32*8+4(%esi,%ecx,8)
    addl $ 1,%ecx
    jnz 0b

    xor %eax,%eax
    movw %ax,%ds
    pop %edi
    pop %esi
    ret

// Returns true if a20 is on
.global cpu_a20_check
cpu_a20_check:
    pushl %es

    // Setup es for wraparound
    orl $ -1,%eax
    movw %ax,%es

    // Zero ax
    addl $ 1,%eax

    // cpu_a20_chk = wraparound(cpu_a20_chk) + 1
    movb %es:cpu_a20_chk + 0x10,%al
    addb $ 1,%al
    movb %al,cpu_a20_chk

    // If wraparound(cpu_a20_chk) has not changed, a20 is on
    addb $ -1,%al
    cmpb %al,%es:cpu_a20_chk + 0x10
    sete %al

    popl %es
    ret

.code32
// Wait for the a20 line to be in the specified state
//  %al = 1 to expect on, 0 to expect off
.global cpu_a20_wait
cpu_a20_wait:
    movzbl %al,%edx

    // cpu_a20_chk = wraparound(cpu_a20_chk) + 1
0:  movb cpu_a20_chk + 0x100000,%al
    addb $ 1,%al
    movb %al,cpu_a20_chk

    // If wraparound(cpu_a20_chk) has not changed, a20 is on
    addb $ -1,%al
    cmpb %al,cpu_a20_chk + 0x100000
    sete %al
    cmpb %al,%dl

    // Statically predict equal
    jne 1f
    ret
1:  jmp 0b
.code16

.section .lowtext, "ax", @progbits

.global clear_bss
clear_bss:
    pushl %edi
    pushl %ecx
    pushl %eax

    movl $ ___bss_st,%edi
    movl $ ___bss_en,%ecx
    subl %edi,%ecx

    // Translate flat edi to es:di
    movl %edi,%eax
    shrl $ 4,%eax
    movw %ax,%es
    xorl %eax,%eax
    andl $ 0xF,%edi

    cld
    rep stosb

    movw %ax,%es

    popl %eax
    popl %ecx
    popl %edi
    retw

.code32
.balign 16
.global __cxa_atexit
__cxa_atexit:
    ret

.section .lowdata, "w", @progbits
cpu_a20_chk:
    .space 1

.balign 8
.global idtr_64
idtr_64:
    .short 0, 0, 0
    .short 32*16-1
    .int idt_64
    .int 0

.global idtr_32
idtr_32:
    .short 0
    .short (idt_32_en-1) - idt_32
    .int idt_32

.global idtr_rm
idtr_rm:
    .short 0
    .short 256*4-1
    .int 0

.section .lowtext, "ax", @progbits

.balign 4
.global __dso_handle
.hidden __dso_handle
__dso_handle:
    // 32 bit pointer to itself
    .int __dso_handle

.section .unreloc, ""
.global ___unrelocated
___unrelocated:
    .skip 512
___unrelocated_end:

.section .data
// extern idt_entry_t idt_32[]
// extern idt_entry_t idt_32_en[]
.global idt_32
idt_32:
    .int int_entry_00_div, 0
    .int int_entry_01_debug, 0
    .int int_entry_02_nmi, 0
    .int int_entry_03_breakpoint, 0
    .int int_entry_04_overflow, 0
    .int int_entry_05_bound, 0
    .int int_entry_06_opcode, 0
    .int int_entry_07_dev_not_av, 0
    .int int_entry_08_dblfault, 0
    .int int_entry_09_copr_seg, 0
    .int int_entry_0A_tss, 0
    .int int_entry_0B_segment, 0
    .int int_entry_0C_stack, 0
    .int int_entry_0D_gen_prot, 0
    .int int_entry_0E_page_fault, 0
    .int int_entry_0F_reserved, 0
    .int int_entry_10_ex_math, 0
    .int int_entry_11_alignment, 0
    .int int_entry_12_machine_check, 0
    .int int_entry_13_simd, 0
    .int int_entry_14_virtualize, 0
    .int int_entry_15_reserved, 0
    .int int_entry_16_reserved, 0
    .int int_entry_17_reserved, 0
    .int int_entry_18_reserved, 0
    .int int_entry_19_reserved, 0
    .int int_entry_1A_reserved, 0
    .int int_entry_1B_reserved, 0
    .int int_entry_1C_reserved, 0
    .int int_entry_1D_reserved, 0
    .int int_entry_1E_reserved, 0
    .int int_entry_1F_reserved, 0

.global idt_32_en
idt_32_en:

.macro isr_entry has_code intr
    .cfi_startproc
    .if \has_code == 0
        // Stack has ss,esp,eflags,cs,eip
        .cfi_def_cfa esp,5*4
        .cfi_offset ss,-1*4
        .cfi_offset esp,-2*4
        .cfi_offset eflags,-3*4
        .cfi_offset cs,-4*4
        .cfi_offset eip,-5*4
        push $ 0
        .cfi_adjust_cfa_offset 4
    .else
        .cfi_def_cfa esp,6*4
        .cfi_offset ss,-1*4
        .cfi_offset esp,-2*4
        .cfi_offset eflags,-3*4
        .cfi_offset cs,-4*4
        .cfi_offset eip,-5*4
    .endif
    pushl $ \intr
    .cfi_adjust_cfa_offset 4
    jmp isr_common_32
    .cfi_endproc
.endm

.code32

int_entry_00_div          : isr_entry 0 0
int_entry_01_debug        : isr_entry 0 1
int_entry_02_nmi          : isr_entry 0 2
int_entry_03_breakpoint   : isr_entry 0 3
int_entry_04_overflow     : isr_entry 0 4
int_entry_05_bound        : isr_entry 0 5
int_entry_06_opcode       : isr_entry 0 6
int_entry_07_dev_not_av   : isr_entry 0 7
int_entry_08_dblfault     : isr_entry 1 8
int_entry_09_copr_seg     : isr_entry 0 9
int_entry_0A_tss          : isr_entry 1 10
int_entry_0B_segment      : isr_entry 1 11
int_entry_0C_stack        : isr_entry 1 12
int_entry_0D_gen_prot     : isr_entry 1 13
int_entry_0E_page_fault   : isr_entry 1 14
int_entry_0F_reserved     : isr_entry 0 15
int_entry_10_ex_math      : isr_entry 0 16
int_entry_11_alignment    : isr_entry 1 17
int_entry_12_machine_check: isr_entry 0 18
int_entry_13_simd         : isr_entry 0 19
int_entry_14_virtualize   : isr_entry 0 20
int_entry_15_reserved     : isr_entry 1 21
int_entry_16_reserved     : isr_entry 0 22
int_entry_17_reserved     : isr_entry 0 23
int_entry_18_reserved     : isr_entry 0 24
int_entry_19_reserved     : isr_entry 0 25
int_entry_1A_reserved     : isr_entry 0 26
int_entry_1B_reserved     : isr_entry 0 27
int_entry_1C_reserved     : isr_entry 0 28
int_entry_1D_reserved     : isr_entry 0 29
int_entry_1E_reserved     : isr_entry 0 30
int_entry_1F_reserved     : isr_entry 0 31

isr_common_32:
    .cfi_startproc
    .cfi_def_cfa esp,7*4
    .cfi_offset ss,6*4
    .cfi_offset esp,5*4
    .cfi_offset eflags,4*4
    .cfi_offset cs,3*4
    .cfi_offset eip,2*4
    push %eax
    push %ecx
    push %edx
0:
    call cpu_debug_break
    cli
    hlt
    jmp 0b
    .cfi_endproc
