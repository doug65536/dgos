#include "gdt_sel.h"

.section .early, "wx", @progbits
.code16

// Return address in bx
.global early_init
early_init:
    mov $ .Ldone_init_stack,%si
    jmp init_stack
.Ldone_init_stack:

    call detect_ancient_cpu

    call clear_bss

    // It is at least a 486 with CPUID instruction if it made it to here
    // without freezing with unsupported CPU message

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

    call idt_init
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
    ljmp $ GDT_SEL_PM_CODE32,$ pmode_entry

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

    call cpu_a20_exitpm

    mov %ebp,%esp

    // Jump to 16 bit protected mode and load segments
    ljmp $ GDT_SEL_PM_CODE16,$ 0f
.code16
0:  movw $ GDT_SEL_PM_DATA16,%dx
    movw %dx,%ds
    movw %dx,%es
    movw %dx,%fs
    movw %dx,%gs
    movw %dx,%ss

    // Turn off protected mode
    movl %cr0,%eax
    btrl $ 0,%eax
    movl %eax,%cr0

    // Jump to real mode
    ljmp $ 0,$ 0f
0:  xorw %dx,%dx
    movw %dx,%ds
    movw %dx,%es
    movw %dx,%fs
    movw %dx,%gs
    movw %dx,%ss

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
    mov $ 0x0FFF,%cx
    xor %dx,%dx
    call change_flags
    test $ 0xF000,%ax
    jnz detected_8086

    // An 80286 has flags 15:12 permanently clear in real mode
    or $ -1,%cx
    mov $ 0xF000,%dx
    call change_flags
    test $ 0xF000,%ax
    jz detected_80286

    // It is safe to use 32 bit instructions now...

    // An 80386 has eflags 18 (AC) permanently clear
    or $ -1,%ecx
    mov $ 1 << 18,%edx
    call change_eflags
    test $ 1 << 18,%eax
    jz detected_80386

    //
    // An early 80486 has eflags 21 (ID) stuck

    // Isolate bit 21 and save it
    and $ 1 << 21,%eax
    push %eax

    // Clear nothing, set nothing
    or $ -1,%ecx
    xor %edx,%edx

    // Clear it if it was set
    xor %eax,%ecx

    // Set it if it was clear
    xor $ 1 << 21,%eax
    or %eax,%edx

    call change_eflags
    pop %ecx
    cmp %ecx,%eax
    jz detected_old_486

    // Make sure the FPU exists
    // Initialized status word will have exceptions masked (1-bits)
    fninit
    xorw %ax,%ax
    // According to the 80287 manual, ax will only be zero if there is an FPU
    fnstsw %ax
    testw %ax,%ax
    jnz detected_486sx

    // CPUID instruction will work
    ret

.pushsection .lowdata
str_prefix:	.asciz "An "
str_8086:	.asciz "8086"
str_80286:	.asciz "80286"
str_80386:	.asciz "80386"
str_80486:	.asciz "early 80486"
str_80486sx:.asciz "80486sx"
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

.global idt_init
idt_init:
    mov $ idt,%edx
    mov %edx,%ecx
    shr $ 4,%ecx
    and $ 0xF,%edx
    mov %cx,%ds
    mov $ 0,%ecx

0:  movw %cs:isr_table(,%ecx,4),%ax
    movw %ax,(%edx,%ecx,8)
    movw $ GDT_SEL_KERNEL_CODE64,2(%edx,%ecx,8)
    movw $ 0,4(%edx,%ecx,8)
    movw $ 0x8e,5(%edx,%ecx,8)
    movw $ 0,6(%edx,%ecx,8)
    addl $ 1,%ecx
    cmpl $ 32,%ecx
    jb 0b

    mov $ idtr_64,%ecx
    mov %ecx,%eax
    shr $ 4,%ecx
    and $ 0xF,%eax
    movw %cx,%ds
    xor %eax,%eax

    // Set limit
    movw $ 32 * 8 - 1,6(%eax)
    movl $ idt,8(%eax)
    mov %ax,%ds

    ret

.Lidt_ptr:
    .int idt

// Returns true if a20 is on
.global cpu_a20_check
cpu_a20_check:
    push %es

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

    pop %es
    ret

.code32
// Wait for the a20 line to be in the specified state
//  %al = 1 to expect on, 0 to expect off
.global cpu_a20_wait
cpu_a20_wait:
    movb %al,%dl

    // cpu_a20_chk = wraparound(cpu_a20_chk) + 1
0:  movb cpu_a20_chk + 0x100000,%al
    addb $ 1,%al
    movb %al,cpu_a20_chk

    // If wraparound(cpu_a20_chk) has not changed, a20 is on
    decb %al
    cmpb %al,cpu_a20_chk + 0x100000
    sete %al
    cmp %al,%dl

    // Statically predict equal
    jne 1f
    ret
1:  jmp 0b
.code16

.section .early, "wx", @progbits

.global clear_bss
clear_bss:
    push %edi
    push %ecx
    push %eax
    movl $ ___bss_st,%edi
    movl $ ___bss_en,%ecx
    subl %edi,%ecx
    // Translate flat edi to es:di
    mov %edi,%eax
    shr $ 4,%eax
    mov %ax,%es
    and $ 0xF,%di
    xor %ax,%ax
    cld
    rep stosb
    mov %ax,%es
    pop %eax
    pop %ecx
    pop %edi
    ret

.code32
.balign 16
.global __cxa_atexit
__cxa_atexit:
    ret

.section .early
cpu_a20_chk: .space 1

.section .early, "wx", @progbits

.global __dso_handle
.hidden __dso_handle
__dso_handle:
    // 32 bit pointer to itself
    .int __dso_handle
