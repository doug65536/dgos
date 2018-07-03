
#include "gdt_sel_pxe.h"
#include "gdt_macros.h"
#include "pxestruct.h"

#define UNRELOCATED(n) ((n)-0x1000+0x7C00)
#define ZEROBASED(n) ((n)-0x1000)

.code16
.section .head
.global early_entry
early_entry:
    movw %sp,%bp
    // Copy !PXE pointer from stack
    movw 4(%bp),%ax
    movw 6(%bp),%dx
    movw %ax,%cs:UNRELOCATED(bangpxe_ofs)
    movw %dx,%cs:UNRELOCATED(bangpxe_seg)

    // Save PXENV+ pointer
    movw %bx,%cs:UNRELOCATED(pxenv_ofs)
    movw %es,%cs:UNRELOCATED(pxenv_seg)

    movw %sp,%cs:UNRELOCATED(orig_ss_sp)
    movw %ss,%cs:UNRELOCATED(orig_ss_sp+2)
    ljmp $0xC00,$ UNRELOCATED(reloc_self) - 0xC000

orig_ss_sp:
    .space 4

bangpxe_ofs:
    .space 2

bangpxe_seg:
    .space 2

pxenv_ofs:
    .space 2

pxenv_seg:
    .space 2

// Linker script puts this at the end to avoid overwriting itself
.section .reloc
reloc_self:

    movw $ 0x7c0,%ax
    movw %ax,%ds
    movw $ 0x100,%ax
    movw %ax,%es
    mov $ ZEROBASED(___initialized_data_en),%cx
    sub $ ZEROBASED(___initialized_data_st),%cx
    shrw $1,%cx
    xorw %si,%si
    xorw %di,%di
    cld
    rep movsw
    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs

    ljmp $ 0, $ entry

.section .early
.global entry
entry:
    mov $ .Linit_ret,%bx
    jmp early_init
.Linit_ret:

    movzwl pxenv_seg,%eax
    movzwl bangpxe_seg,%edx
    shll $4,%eax
    shll $4,%edx
    movzwl pxenv_ofs,%ecx
    addl %ecx,%eax
    movzwl bangpxe_ofs,%ecx
    addl %ecx,%edx

    mov $ pxe_main,%ecx
    mov $ 0f,%ebx
    jmp boot

0:  lss orig_ss_sp,%sp
    retf

.code32
.section .text

.global pxe_call_rm
pxe_call_rm:
    pushl %ebp
    movl %esp,%ebp
    pushl %edi
    pushl %esi
    pushl %ebx

    movl %eax,%ebx
    movl %edx,%esi
    movl %ecx,%edi

    call cpu_a20_exitpm

    // Enter 16-bit protected mode to load segment limits
    ljmp $ GDT_SEL_PM_CODE16,$ 0f
.code16
0:  movw $ GDT_SEL_PM_DATA16, %ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs
    movw %ax,%ss

    movl %cr0,%eax
    andl $~1,%eax
    movl %eax,%cr0

    // Enter real mode
    ljmp $ 0,$ 0f
0:  xor %ax,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%fs
    mov %ax,%gs
    mov %ax,%ss

    sti

    movl %ebx,%eax
    movl %esi,%edx

    call *%di

    movl %eax,%ebx
    movl %edx,%esi

    cli

    lgdtl gdtr

    movl %cr0,%eax
    orl $1,%eax
    movl %eax,%cr0

    ljmp $ GDT_SEL_PM_CODE32,$ 0f
.code32
0:  mov $ GDT_SEL_PM_DATA32,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs
    movw %ax,%ss

    call cpu_a20_enterpm

    movl %ebx,%eax
    movl %esi,%edx

    popl %ebx
    popl %esi
    popl %edi
    leavel
    ret

// uint16_t pxe_call_pxenv(uint16_t opcode, void *arg_struct);
//  ax = opcode
//  dx -> arg_struct
.global pxe_call_pxenv
pxe_call_pxenv:
    movl $ pxe_rm_call_pxenv,%ecx
    call pxe_call_rm
    ret

// uint16_t pxe_call_bangpxe_rm(uint16_t opcode, void *arg_struct);
//  ax = opcode
//  dx -> arg_struct
.global pxe_call_bangpxe_rm
pxe_call_bangpxe_rm:
    movl $ pxe_rm_call_bangpxe,%ecx
    call pxe_call_rm
    ret

// uint16_t pxe_call_bangpxe_pm(uint16_t opcode, void *arg_struct);
//  ax = opcode
//  edx -> arg_struct
.global pxe_call_bangpxe_pm
pxe_call_bangpxe_pm:
    pushl %ebp
    movl %esp,%ebp
    pushl %edi
    pushl %esi
    pushl %ebx

    movw $ GDT_SEL_PM_DATA16,%di
    movw %di,%ss

    // Update GDT entry base to point to argument
    mov $ gdt + GDT_SEL_PXE_TEMP*8,%edi
    mov %dx,2(%edi)
    shr $16,%edx
    mov %dl,4(%edi)
    mov %dh,7(%edi)

    // 16 bit cdecl call
    pushw bang_pxe_ptr+2
    pushw bang_pxe_ptr
    pushw $ GDT_SEL_PXE_TEMP
    pushw $0
    pushw %ax
    lcallw *pxe_entry_vec
    add $10,%sp

    movw $ GDT_SEL_PM_DATA32,%di
    movw %di,%ss

    popl %ebx
    popl %esi
    popl %edi
    leavel
    ret

// This may clobber all registers (even segments), runs in real mode
.code16
pxe_rm_call_pxenv:
    // Call PXE
    // bx = opcode
    mov %ax,%bx
    // es:di -> arg_struct
    mov %edx,%edi
    shr $4,%edx
    and $0xF,%edi
    mov %dx,%es
    lcallw *pxe_entry_vec
    ret

// This may clobber all registers (even segments), runs in real mode
.code16
pxe_rm_call_bangpxe:
    // dx:di -> arg_struct
    mov %edx,%edi
    shr $4,%edx
    and $0xF,%di
    pushw bang_pxe_ptr+2
    pushw bang_pxe_ptr
    pushw %dx
    pushw %di
    pushw %ax
    lcallw *pxe_entry_vec
    add $10,%sp
    ret
