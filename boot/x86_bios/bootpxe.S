// This is the entry point code for BIOS LAN boot (PXE)

#include "gdt_sel_pxe.h"
#include "x86/gdt_macros.h"
#include "pxestruct.h"

// This is linked at 0x1000 but runs at 0x7C00
// Wrap these up in a couple of macros
// that offset your argument appropriately
// This is needed before the bootloader has relocated itself

#define UNRELOCATED(n) ((n)-0x1000+0x7C00)
#define ZEROBASED(n) ((n)-0x1000)

// Just assume a 32 bit processor, realistically a pre-386 would never
// even try to boot from LAN

.code16
.section .head, "wx", @progbits
.global bootpxe_early_entry
bootpxe_early_entry:
    cli
    cld
    movzwl %sp,%esp
    mov %esp,%ebp

    // Load whole cs:eip
    ljmpw $ 0,$ UNRELOCATED(.Lbootpxe_load_cs_eip)

.Lbootpxe_load_cs_eip:
    // Copy !PXE pointer from stack
    // (%bp) dereferences will use ss segment
    movl 4(%bp),%eax
    movl %eax,%cs:UNRELOCATED(bangpxe_ofs)

    // Save PXENV+ pointer
    movw %bx,%cs:UNRELOCATED(pxenv_ofs)
    movw %es,%cs:UNRELOCATED(pxenv_seg)

    // Save original stack pointer
    movw %sp,%cs:UNRELOCATED(orig_ss_sp)
    movw %ss,%cs:UNRELOCATED(orig_ss_sp+2)

    movzwl %sp,%esp

    jmp reloc_self

.section .lowdata

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

.balign 64

// Linker script puts this at the end to avoid overwriting itself
.section .lowtext, "x", @progbits
.global reloc_self
reloc_self:
    // Then jump into the relocated version of the code

    // Copy 0x7C00-0xE7FF to 0x1000-0x7BFF (0x6c00 bytes)
    movl $ ___text_st,%eax
    movl $ ___unreloc_st,%edx
    movl %edx,%ecx
    subl %eax,%ecx
    movl %ecx,%ebx
    calll bootpxe_reloc_chunk

    // Jump into relocated copy of this code, before we wipe it
    ljmpw $ 0,$ bootpxe_relocated_reloc_self_continuation

bootpxe_relocated_reloc_self_continuation:
    // Then copy 0xE800-___bootpxe_image_en (as high as 0x1F000)
    // to 0x7C00-N
    movl $ ___unreloc_st,%eax
    leal (%eax,%ebx),%edx
    movl $ ___bootpxe_image_en,%ecx
    subl %eax,%ecx
    calll bootpxe_reloc_chunk

    // Move stack to 0x20000-0x2FFFF
    movl $ 0x2000,%eax
    movw %ax,%ss
    xorl %esp,%esp

    ljmpw $ 0,$ pxebios_entry

// eax=destination linear address
// edx=source linear address
// ecx=size in bytes
// expects 32 bit return address, use calll
.global bootpxe_reloc_chunk
.hidden bootpxe_reloc_chunk
bootpxe_reloc_chunk:
    pushl %edi
    pushl %esi
    pushl %ecx  // count at 8(%esp)
    pushl %edx  // source at 4(%esp)
    pushl %eax  // destination at 0(%esp)
    cld

.Lreloc_more:
    // Convert linear address to normalized 16:16 far pointers
    // in %es:(%edi) %ds:(%esi)

    // Set segment part
    shrl $ 4,%eax
    shrl $ 4,%edx
    movw %ax,%es
    movw %dx,%ds

    // Set 4-bit initial offset
    mov (%esp),%edi
    mov 4(%esp),%esi
    andl $ 0xF,%edi
    andl $ 0xF,%esi

    cmpl $ 0x8000,%ecx
    jb .Lno_clamp
    movl $ 0x8000,%ecx
.Lno_clamp:
    // Adjust remaining count before clobbering ecx
    addl %ecx,(%esp)
    addl %ecx,4(%esp)
    subl %ecx,8(%esp)
    rep movsb
    jnz .Lreloc_chunk_not_done

    xorl %eax,%eax
    movw %ax,%ds
    movw %ax,%es

    addl $ 3*4,%esp
    popl %esi
    popl %edi
    retl

// Unlikely out of line path
.Lreloc_chunk_not_done:
    movl (%esp),%eax
    movl 4(%esp),%edx
    movl 8(%esp),%ecx
    jmp .Lreloc_more

.section .lowtext, "x", @progbits
.global pxebios_entry
pxebios_entry:
    movl $ .Linit_ret,%ebx
    jmp early_init
.Linit_ret:

    movzwl pxenv_seg,%eax
    movzwl bangpxe_seg,%edx
    shll $ 4,%eax
    shll $ 4,%edx
    movzwl pxenv_ofs,%ecx
    addl %ecx,%eax
    movzwl bangpxe_ofs,%ecx
    addl %ecx,%edx

    mov $ pxe_main,%ecx
    mov $ 0f,%ebx
    jmp boot

0:  mov orig_ss_sp+2,%ss
    movzwl orig_ss_sp,%esp
    lret

.code32
.section .lowtext, "x", @progbits

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
    ljmpw $ GDT_SEL_PM_CODE16,$ 0f
.code16
0:  movl $ GDT_SEL_PM_DATA16, %eax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs

    movl %cr0,%eax
    andl $ ~1,%eax
    movl %eax,%cr0

    // Enter real mode
    ljmpw $ 0,$ 0f
0:  xor %ax,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%fs
    mov %ax,%gs
    mov %ax,%ss

    movl $ 0x2000,%eax
    movw %ax,%ss
    subl $ 0x20000,%esp

    sti

    movl %ebx,%eax
    movl %esi,%edx

    call *%di

    movl %eax,%ebx
    movl %edx,%esi

    cli

    lgdtl gdtr

    movl %cr0,%eax
    orl $ 1,%eax
    movl %eax,%cr0

    ljmpw $ GDT_SEL_PM_CODE32,$ 0f
.code32
0:  mov $ GDT_SEL_PM_DATA32,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs
    movw %ax,%ss

    addl $ 0x20000,%esp

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
    shr $ 16,%edx
    mov %dl,4(%edi)
    mov %dh,7(%edi)

    // 16 bit cdecl call
    pushw bang_pxe_farp16+2
    pushw bang_pxe_farp16
    pushw $ GDT_SEL_PXE_TEMP
    pushw $ 0
    pushw %ax
    lcallw *pxe_entry_farp16
    addl $ 5*2,%esp

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
    shr $ 4,%edx
    and $ 0xF,%edi
    mov %dx,%es
    lcallw *pxe_entry_farp16
    ret

// This may clobber all registers (even segments), runs in real mode
.code16
pxe_rm_call_bangpxe:
    // dx:di -> arg_struct
    mov %edx,%edi
    shr $ 4,%edx
    and $ 0xF,%edi
    pushw bang_pxe_farp16+2
    pushw bang_pxe_farp16
    pushw %dx
    pushw %di
    pushw %ax
    lcallw *pxe_entry_farp16
    addl $ 10,%esp
    ret
