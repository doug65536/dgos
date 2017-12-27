.code64
.section .text.isr, "ax"

#include "asm_constants.h"
#include "control_regs_constants.h"
#include "../cfi_helpers.h"
#include "interrupts.h"

// This must be position independent code because the GDB stub clones off
// a redundant copy to protect against infinite recursion


.macro isr_entry has_code int_num
.global isr_entry_\int_num\()
.hidden isr_entry_\int_num\()
.type isr_entry_\int_num\(),@function
.align 16
isr_entry_\int_num\():
    .cfi_startproc
    .if \has_code == 0
        .cfi_def_cfa_offset 8
        push_cfi $0
    .else
        .cfi_def_cfa_offset 16
    .endif
    push_cfi $\int_num
    jmp isr_common
    .cfi_endproc
.endm

.irp int_num,252,251,250,249,248,247,246,245,244,243,242,241,240
     isr_entry 0 \int_num
.endr
.irp int_num,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224
     isr_entry 0 \int_num
.endr
.irp int_num,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208
     isr_entry 0 \int_num
.endr
.irp int_num,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192
     isr_entry 0 \int_num
.endr
.irp int_num,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176
     isr_entry 0 \int_num
.endr
.irp int_num,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160
     isr_entry 0 \int_num
.endr
.irp int_num,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144
     isr_entry 0 \int_num
.endr
.irp int_num,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128
     isr_entry 0 \int_num
.endr
.irp int_num,127,126,125,124,123,122,121,120,119,118,117,116,115,114,113,112
     isr_entry 0 \int_num
.endr
.irp int_num,111,110,109,108,107,106,105,104,103,102,101,100,99,98,97,96
     isr_entry 0 \int_num
.endr
.irp int_num,95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80
     isr_entry 0 \int_num
.endr
.irp int_num,79,78,77,76,75,74,73,72,71,70,69,68,67,66,65,64
     isr_entry 0 \int_num
.endr
.irp int_num,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48
     isr_entry 0 \int_num
.endr

// PIC IRQ handlers (16 IRQs)
.irp int_num,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32
     isr_entry 0 \int_num
.endr

// Reserved (unused) exception vectors
.irp int_num,31,30,29,28,27,26,25,24,23,22,21
     isr_entry 0 \int_num
.endr

// Separate exception entry points for conveniently placing breakpoints
isr_entry 0 INTR_EX_VIRTUALIZE
isr_entry 0 INTR_EX_SIMD
isr_entry 0 INTR_EX_MACHINE
isr_entry 1 INTR_EX_ALIGNMENT
isr_entry 0 INTR_EX_MATH
isr_entry 0 15
isr_entry 1 INTR_EX_GPF
isr_entry 1 INTR_EX_STACK
isr_entry 1 INTR_EX_SEGMENT
isr_entry 1 INTR_EX_TSS
isr_entry 0 INTR_EX_COPR_SEG
isr_entry 1 INTR_EX_DBLFAULT
isr_entry 0 INTR_EX_DEV_NOT_AV
isr_entry 0 INTR_EX_OPCODE
isr_entry 0 INTR_EX_BOUND
isr_entry 0 INTR_EX_OVF
isr_entry 0 INTR_EX_BREAKPOINT
isr_entry 0 INTR_EX_NMI
isr_entry 0 INTR_EX_DEBUG
isr_entry 0 INTR_EX_DIV

// Hottest vectors
isr_entry 1 INTR_EX_PAGE
isr_entry 0 INTR_TLB_SHOOTDOWN
isr_entry 0 INTR_THREAD_YIELD
isr_entry 0 INTR_APIC_TIMER

.type isr_common,@function
.align 16
isr_common:
    .cfi_startproc
    .cfi_def_cfa_offset 24
    .cfi_rel_offset rsp,16

    // Save call-clobbered registers
    // (in System-V parameter order in memory)
    push_cfi %rbp
    .cfi_rel_offset rbp,0
    mov %rsp,%rbp
    .cfi_register rsp,rbp
    .cfi_def_cfa_register rbp
    .cfi_def_cfa_offset 32

    //  0*8(%rbp) -> caller's rbp
    //  1*8(%rbp) -> interrupt
    //  2*8(%rbp) -> error code
    //  3*8(%rbp) -> rip
    //  4*8(%rbp) -> cs
    //  5*8(%rbp) -> flags
    //  6*8(%rbp) -> rsp
    //  7*8(%rbp) -> ss

    push %r15
    .cfi_rel_offset r15,72+0*8

    push %r14
    .cfi_rel_offset r14,72+1*8

    push %r13
    .cfi_rel_offset r13,72+2*8

    push %r12
    .cfi_rel_offset r12,72+3*8

    push %r11
    .cfi_rel_offset r11,72+4*8

    push %r10
    .cfi_rel_offset r10,72+5*8

    push %rbx
    .cfi_rel_offset rbx,72+6*8

    push %rax
    .cfi_rel_offset rax,72+7*8

    cld
    push %r9
    .cfi_rel_offset r9,72+8*8

    mov %cr3,%rax

    push %r8
    .cfi_rel_offset r8,72+9*8

    push %rcx
    .cfi_rel_offset rcx,72+10*8

    push %rdx
    .cfi_rel_offset rdx,72+11*8

    push %rsi
    .cfi_rel_offset rsi,72+12*8

    push %rdi
    .cfi_rel_offset rdi,72+13*8

    // Register usage:
    //  r13: segment register values at interrupt entry

    push %rax

    // Get segment registers packed into %r13
    mov %gs,%eax
    mov %es,%r13d
    shl $16,%eax
    shl $16,%r13d
    mov %fs,%ax
    mov %ds,%r13w
    shl $32,%rax
    or %rax,%r13

    push %r13

    // Get interrupted code segment
    mov 4*8(%rbp),%ecx

    // See if we are coming from kernel code
    cmpl $(GDT_SEL_KERNEL_CODE64),%ecx
    je .Lfrom_kernel

    // ...came from user code
    swapgs

.Lfrom_kernel:

    // Save entire sse/mmx/fpu state
    // NOTE: this instruction may be patched to call one of the xsave versions!
    call isr_save_fxsave
.Lsse_context_save_jmp:

    // Make structure on the stack
    push %rax
    push $0
    push $0

    // Pass intr, ctx
    mov %rsp,%rsi
    mov 1*8(%rbp),%edi

    // Align stack pointer
    sub $8,%rsp

    // Interrupt dispatch
    // 0x00-0x1F -> exception_isr_handler
    // 0x20-0x2F -> pic_dispatcher
    // 0x30-0xEF -> apic_dispatcher
    // 0xF0-0xFF -> intr_invoke

    // 0x00-0x1F -> exception_isr_handler
    lea exception_isr_handler(%rip),%rax

    // 0x20-0x2F -> pic_dispatcher
    mov $pic8259_dispatcher,%rcx
    cmp $INTR_PIC1_IRQ_BASE,%edi
    cmovae %rcx,%rax

    // 0x30-0xEF -> apic_dispatcher
    mov $apic_dispatcher,%rcx
    cmp $INTR_APIC_IRQ_BASE,%edi
    cmovae %rcx,%rax

    // 0xF0-0xFF -> intr_invoke
    mov $intr_invoke,%rcx
    cmp $INTR_SOFT_BASE,%edi
    cmovae %rcx,%rax

    // Call handler
    call *%rax

    // isr can return a new stack pointer, or just return
    // the passed one to continue with this thread
    mov %rax,%rsp

    lea 19*8(%rax),%rbp

    // Pop outgoing cleanup data
    // Used to adjust outgoing thread state after switching stack
    pop %rax
    pop %rdi
    test %rax,%rax
    jz .Lno_cleanup_call
    call *%rax
.Lno_cleanup_call:

    // Pop the pointer to the FPU context
    pop %rdi

    // NOTE: this instruction may be patched to call one of the xsave versions!
    call isr_restore_fxrstor
.Lsse_context_restore_jmp:

    // pop packed segments qword into rdx
    // if not returning to kernel
    //   update tss rsp0
    //   swapgs
    //   if any data segment is not GDT_SEL_USER_DATA | 3
    //     load all segment registers with GDT_SEL_USER_DATA | 3
    //   endif
    //   restore fsbase
    // endif

    // Load return cs
    movzwl 4*8(%rbp),%ecx

    // Pop segments
    pop %rdx

    // See if we're not returning to user code
    // Branch past swapgs and restoration of segments if not
    // Compare against user segment because the CPU pushes nonsense for #DF
    cmpl $(GDT_SEL_USER_CODE64 | 3),%ecx
    jne .Lreturning_to_kernel

    // ...returning to user code

    // Fetch pointer to this CPU's TSS for TSS.RSP0 update
    mov %gs:CPU_INFO_TSS_PTR_OFS,%rbx
    lea 8*8(%rbp),%rax
    mov %rax,TSS_RSP0_OFS(%rbx)

    // Restore user gs
    swapgs

    // If segments are not changing, avoid 136*4 cycles
    cmp %r13,%rdx
    jnz .Lsegments_changed
    jmp .Lsegments_not_changed

.align 16
.Lsegments_changed:
    // Restore segments
    movl $(GDT_SEL_USER_DATA | 3),%eax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%fs
    movw %ax,%gs

.align 16
.Lsegments_not_changed:
.Lreturning_to_kernel:
    // Restore CR3
    pop %rax
    mov %rax,%cr3

    pop %rdi
    .cfi_restore rdi

    pop %rsi
    .cfi_restore rsi

    pop %rdx
    .cfi_restore rdx

    pop %rcx
    .cfi_restore rcx

    pop %r8
    .cfi_restore r8

    pop %r9
    .cfi_restore r9

    pop %rax
    .cfi_restore rax

    pop %rbx
    .cfi_restore rbx

    pop %r10
    .cfi_restore r10

    pop %r11
    .cfi_restore r11

    pop %r12
    .cfi_restore r12

    pop %r13
    .cfi_restore r13

    pop %r14
    .cfi_restore r14

    pop %r15
    .cfi_restore r15

    pop %rbp
    .cfi_restore rbp
    .cfi_def_cfa_register rsp
    .cfi_def_cfa_offset 24

    add_rsp 16

    iretq

    .cfi_endproc

// Expects ds loaded with kernel data segment
// Clobbers rcx,rdx,rax,rdi
// Returns rax=pointer to context
.macro xsave_ctx insn
    .cfi_startproc

    // Get pointer to current thread from CPU-local storage
    mov %gs:CPU_INFO_CURTHREAD_OFS,%rcx

    // Set all bits of edx:eax
    mov $-1,%eax
    mov $-1,%edx

    // Skip if threading not initialized yet (still needed?)
    test %rcx,%rcx
    jz 1f

    // Read xsave stack pointer from thread
    mov THREAD_XSAVE_PTR_OFS(%rcx),%rdi

    // Save context using instruction passed to macro
    \insn (%rdi)
    mov %rdi,%rax

    // Update xsave stack pointer in thread
    add sse_context_size,%rdi
    mov %rdi,THREAD_XSAVE_PTR_OFS(%rcx)
1:
    ret

    .cfi_endproc
.endm

// Expects ds to be kernel data segment
// Expects rdi to point to saved context
// Clobbers eax,edx,ecx
.macro xrstor_ctx insn
    .cfi_startproc

    // Set all bits of edx:eax
    mov $-1,%eax
    mov $-1,%edx

    // Restore context using instruction passed to macro
    \insn (%rdi)

    // Get pointer to current thread from CPU-local storage
    mov %gs:CPU_INFO_CURTHREAD_OFS,%rcx
    mov %rdi,THREAD_XSAVE_PTR_OFS(%rcx)

    ret

    .cfi_endproc
.endm

.align 16
.global isr_save_xsaveopt
.hidden isr_save_xsaveopt
isr_save_xsaveopt:
    xsave_ctx xsaveopt64

.align 16
.global isr_save_xsave
.hidden isr_save_xsave
isr_save_xsave:
    xsave_ctx xsave64

.align 16
.global isr_save_xsavec
.hidden isr_save_xsavec
isr_save_xsavec:
    xsave_ctx xsavec64

.align 16
.global isr_restore_xrstor
.hidden isr_restore_xrstor
isr_restore_xrstor:
    xrstor_ctx xrstor64

.align 16
.global isr_save_fxsave
.hidden isr_save_fxsave
isr_save_fxsave:
    xsave_ctx fxsave64

.align 16
.global isr_restore_fxrstor
.hidden isr_restore_fxrstor
isr_restore_fxrstor:
    xrstor_ctx fxrstor64

// isr_context_t *exception_isr_handler(int intr, isr_context_t *ctx)
.type exception_isr_handler,@function
exception_isr_handler:
    .cfi_startproc

    push_cfi %rbp
    push_cfi %rbx
    push_cfi %r12

    mov $intr_has_handler,%rax

    // ebx=intr, r12=ctx
    mov %edi,%ebx
    mov %rsi,%r12

    // If there is no handler...
    call *%rax
    test %eax,%eax
    jz 0f

    // There is a handler
    mov $intr_invoke,%rax
    mov %ebx,%edi
    mov %r12,%rsi
    call *%rax

    // See if the handler rejected it
    test %eax,%eax
    jz 0f

1:
    // Handler handled it
    mov %r12,%rax
    pop_cfi %r12
    pop_cfi %rbx
    pop_cfi %rbp
    retq

.align 16
0:
    // ...no handler or handler rejected it
    mov $__exception_handler_invoke,%rax
    mov %ebx,%edi
    call *%rax
    test %eax,%eax
    jnz 2f

    mov $cpu_debug_break,%rax
    call *%rax
    jmp 1b

    // Tail call to unhandled_exception_handler
.align 16
2:
    mov $unhandled_exception_handler,%rax
    mov %r12,%rdi
    pop_cfi %r12
    pop_cfi %rbx
    pop_cfi %rbp
    jmp *%rax

    .cfi_endproc

// __noreturn void isr_sysret64(uintptr_t rip, uintptr_t rsp);
.global isr_sysret64
isr_sysret64:
    push %rbp
    mov %rsp,%rbp

    xor %eax,%eax

    // Clear the opmask registers

    cmpw $0,sse_avx512_opmask_offset(%rip)
    jne .Lclear_opmask
    jmp .Ldone_opmask

.Lclear_opmask:
    kmovw %eax,%k1
    kmovw %eax,%k2
    kmovw %eax,%k3
    kmovw %eax,%k4
    kmovw %eax,%k5
    kmovw %eax,%k6
    kmovw %eax,%k7

.align 16
.Ldone_opmask:
    // Check for AVX
    cmpw $0,sse_avx_offset(%rip)
    je .Lno_avx

    // Clear all vector registers
    vzeroall
    jmp .Ldone_sse

.align 16
.Lno_avx:
    pxor %xmm0,%xmm0
    movq %xmm0,%xmm1
    movq %xmm0,%xmm2
    movq %xmm0,%xmm3
    movq %xmm0,%xmm4
    movq %xmm0,%xmm5
    movq %xmm0,%xmm6
    movq %xmm0,%xmm7
    movq %xmm0,%xmm8
    movq %xmm0,%xmm9
    movq %xmm0,%xmm10
    movq %xmm0,%xmm11
    movq %xmm0,%xmm12
    movq %xmm0,%xmm13
    movq %xmm0,%xmm14
    movq %xmm0,%xmm15

.align 16
.Ldone_sse:
    // Clear the x87 stack
    fld1
    fsub %st,%st
    fld %st
    fld %st
    fld %st
    fld %st
    fld %st
    fld %st
    fld %st
    fstp %st
    fstp %st
    fstp %st
    fstp %st
    fstp %st
    fstp %st
    fstp %st
    fstp %st

    mov %rdi,%rcx
    mov $(CPU_EFLAGS_IF | 2),%r11d

    // Avoid leaking any register values to user code
    mov %eax,%ebx
    // not ecx
    mov %eax,%edx
    // not esi
    mov %eax,%edi
    mov %eax,%ebp
    mov %eax,%r8d
    mov %eax,%r9d
    mov %eax,%r10d
    // not r11
    mov %eax,%r12d
    mov %eax,%r13d
    mov %eax,%r14d
    mov %eax,%r15d
    cli
    swapgs
    mov %rsi,%rsp
    sysretq

.section .rodata
.align 8

// Pointers to patch points
.global sse_context_save
sse_context_save:
    .quad .Lsse_context_save_jmp

.global sse_context_restore
sse_context_restore:
    .quad .Lsse_context_restore_jmp
