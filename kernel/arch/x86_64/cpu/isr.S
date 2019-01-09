
#include "asm_constants.h"
#include "control_regs_constants.h"
#include "../cfi_helpers.h"
#include "interrupts.h"
#include "cpu_bug.h"

.code64

.struct 0
    frame_rbp:  .struct frame_rbp+8
    frame_intr: .struct frame_intr+8
    frame_err:  .struct frame_err+8
    frame_rip:  .struct frame_rip+8
    frame_cs:   .struct frame_cs+8
    frame_efl:  .struct frame_efl+8
    frame_rsp:  .struct frame_rsp+8
    frame_ss:   .struct frame_ss+8
    frame_end:

.section .text.isr, "ax"

.global isr_entry_st
isr_entry_st:

.macro fn_trace fn, op
#ifdef _CALL_TRACE_ENABLED
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    sub $8,%rsp

    leaq \fn(%rip),%rdi
    xor %esi,%esi
    call __cyg_profile_func_\op

    add $8,%rsp
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax
#endif
.endm

.macro isr_section_name num
    .if \num < 10
        .section .text.isr.00\num
    .elseif \num < 100
        .section .text.isr.0\num
    .else
        .section .text.isr.\num
    .endif
.endm

.macro isr_entry_impl has_code int_num entryname pathname
isr_section_name \int_num
.global \entryname\()_\int_num\()
.hidden \entryname\()_\int_num\()
.type \entryname\()_\int_num\(),@function
.align 16
\entryname\()_\int_num\():
    .cfi_startproc
    .cfi_signal_frame
    .if \has_code == 0
        // CFA
        // index
        //  0    5*8(%rsp) <-- CFA
        // -1    4*8(%rsp) ss
        // -2    3*8(%rsp) rsp
        // -3    2*8(%rsp) rflags
        // -4    1*8(%rsp) ss
        // -5    0*8(%rsp) rip
        .cfi_def_cfa rsp,5*8
        .cfi_offset ss,-1*8
        .cfi_offset rsp,-2*8
        .cfi_offset rflags,-3*8
        .cfi_offset cs,-4*8
        .cfi_offset rip,-5*8
        fn_trace \pathname,enter
        push_cfi $0
    .else
        // CFA
        // index
        //  0    6*8(%rsp) <-- CFA
        // -1    5*8(%rsp) ss
        // -2    4*8(%rsp) rsp
        // -3    3*8(%rsp) rflags
        // -4    2*8(%rsp) ss
        // -5    1*8(%rsp) rip
        // -6    0*8(%rsp) error code
        .cfi_def_cfa rsp,6*8
        .cfi_offset ss,-1*8
        .cfi_offset rsp,-2*8
        .cfi_offset rflags,-3*8
        .cfi_offset cs,-4*8
        .cfi_offset rip,-5*8
        fn_trace \pathname,enter
    .endif
    // Push the interrupt number below the error code
    push_cfi $\int_num
    jmp \pathname
    .cfi_endproc
.endm

.macro isr_entry has_code int_num
     isr_entry_impl \has_code \int_num isr_entry isr_common
.endm

// Individually to make placing breakpoints trivial

isr_entry 0 INTR_EX_DIV
isr_entry 0 INTR_EX_DEBUG
isr_entry 0 INTR_EX_NMI
isr_entry 0 INTR_EX_BREAKPOINT
isr_entry 0 INTR_EX_OVF
isr_entry 0 INTR_EX_BOUND
isr_entry 0 INTR_EX_OPCODE
isr_entry 0 INTR_EX_DEV_NOT_AV
isr_entry 1 INTR_EX_DBLFAULT
isr_entry 0 INTR_EX_COPR_SEG
isr_entry 1 INTR_EX_TSS
isr_entry 1 INTR_EX_SEGMENT
isr_entry 1 INTR_EX_STACK
isr_entry 1 INTR_EX_GPF
isr_entry 1 INTR_EX_PAGE
isr_entry 0 15
isr_entry 0 INTR_EX_MATH
isr_entry 1 INTR_EX_ALIGNMENT
isr_entry 0 INTR_EX_MACHINE
isr_entry 0 INTR_EX_SIMD
isr_entry 0 INTR_EX_VIRTUALIZE

// Reserved for CPU
isr_entry 0 21
isr_entry 0 22
isr_entry 0 23
isr_entry 0 24
isr_entry 0 25
isr_entry 0 26
isr_entry 0 27
isr_entry 0 28
isr_entry 0 29
isr_entry 0 30
isr_entry 0 31

isr_entry 0 INTR_APIC_SPURIOUS
isr_entry 0 INTR_APIC_ERROR
isr_entry 0 INTR_APIC_THERMAL

// Reserved
isr_entry 0 35
isr_entry 0 36
isr_entry 0 37
isr_entry 0 38

isr_entry 0 INTR_APIC_TIMER
isr_entry 0 INTR_TLB_SHOOTDOWN
isr_entry 0 INTR_THREAD_YIELD
isr_entry 0 INTR_FLUSH_TRACE

// Reserved
isr_entry 0 43
isr_entry 0 44
isr_entry 0 45
isr_entry 0 46
isr_entry 0 47

// APIC and MSI(x) IRQs
isr_entry 0 48
isr_entry 0 49
isr_entry 0 50
isr_entry 0 51
isr_entry 0 52
isr_entry 0 53
isr_entry 0 54
isr_entry 0 55
isr_entry 0 56
isr_entry 0 57
isr_entry 0 58
isr_entry 0 59
isr_entry 0 60
isr_entry 0 61
isr_entry 0 62
isr_entry 0 63
isr_entry 0 64
isr_entry 0 65
isr_entry 0 66
isr_entry 0 67
isr_entry 0 68
isr_entry 0 69
isr_entry 0 70
isr_entry 0 71
isr_entry 0 72
isr_entry 0 73
isr_entry 0 74
isr_entry 0 75
isr_entry 0 76
isr_entry 0 77
isr_entry 0 78
isr_entry 0 79
isr_entry 0 80
isr_entry 0 81
isr_entry 0 82
isr_entry 0 83
isr_entry 0 84
isr_entry 0 85
isr_entry 0 86
isr_entry 0 87
isr_entry 0 88
isr_entry 0 89
isr_entry 0 90
isr_entry 0 91
isr_entry 0 92
isr_entry 0 93
isr_entry 0 94
isr_entry 0 95
isr_entry 0 96
isr_entry 0 97
isr_entry 0 98
isr_entry 0 99
isr_entry 0 100
isr_entry 0 101
isr_entry 0 102
isr_entry 0 103
isr_entry 0 104
isr_entry 0 105
isr_entry 0 106
isr_entry 0 107
isr_entry 0 108
isr_entry 0 109
isr_entry 0 110
isr_entry 0 111
isr_entry 0 112
isr_entry 0 113
isr_entry 0 114
isr_entry 0 115
isr_entry 0 116
isr_entry 0 117
isr_entry 0 118
isr_entry 0 119
isr_entry 0 120
isr_entry 0 121
isr_entry 0 122
isr_entry 0 123
isr_entry 0 124
isr_entry 0 125
isr_entry 0 126
isr_entry 0 127
isr_entry 0 128
isr_entry 0 129
isr_entry 0 130
isr_entry 0 131
isr_entry 0 132
isr_entry 0 133
isr_entry 0 134
isr_entry 0 135
isr_entry 0 136
isr_entry 0 137
isr_entry 0 138
isr_entry 0 139
isr_entry 0 140
isr_entry 0 141
isr_entry 0 142
isr_entry 0 143
isr_entry 0 144
isr_entry 0 145
isr_entry 0 146
isr_entry 0 147
isr_entry 0 148
isr_entry 0 149
isr_entry 0 150
isr_entry 0 151
isr_entry 0 152
isr_entry 0 153
isr_entry 0 154
isr_entry 0 155
isr_entry 0 156
isr_entry 0 157
isr_entry 0 158
isr_entry 0 159
isr_entry 0 160
isr_entry 0 161
isr_entry 0 162
isr_entry 0 163
isr_entry 0 164
isr_entry 0 165
isr_entry 0 166
isr_entry 0 167
isr_entry 0 168
isr_entry 0 169
isr_entry 0 170
isr_entry 0 171
isr_entry 0 172
isr_entry 0 173
isr_entry 0 174
isr_entry 0 175
isr_entry 0 176
isr_entry 0 177
isr_entry 0 178
isr_entry 0 179
isr_entry 0 180
isr_entry 0 181
isr_entry 0 182
isr_entry 0 183
isr_entry 0 184
isr_entry 0 185
isr_entry 0 186
isr_entry 0 187
isr_entry 0 188
isr_entry 0 189
isr_entry 0 190
isr_entry 0 191
isr_entry 0 192
isr_entry 0 193
isr_entry 0 194
isr_entry 0 195
isr_entry 0 196
isr_entry 0 197
isr_entry 0 198
isr_entry 0 199
isr_entry 0 200
isr_entry 0 201
isr_entry 0 202
isr_entry 0 203
isr_entry 0 204
isr_entry 0 205
isr_entry 0 206
isr_entry 0 207
isr_entry 0 208
isr_entry 0 209
isr_entry 0 210
isr_entry 0 211
isr_entry 0 212
isr_entry 0 213
isr_entry 0 214
isr_entry 0 215
isr_entry 0 216
isr_entry 0 217
isr_entry 0 218
isr_entry 0 219
isr_entry 0 220
isr_entry 0 221
isr_entry 0 222
isr_entry 0 223
isr_entry 0 224
isr_entry 0 225
isr_entry 0 226
isr_entry 0 227
isr_entry 0 228
isr_entry 0 229
isr_entry 0 230
isr_entry 0 231
isr_entry 0 232
isr_entry 0 233
isr_entry 0 234
isr_entry 0 235
isr_entry 0 236
isr_entry 0 237
isr_entry 0 238
isr_entry 0 239

// PIC IRQs
isr_entry 0 240
isr_entry 0 241
isr_entry 0 242
isr_entry 0 243
isr_entry 0 244
isr_entry 0 245
isr_entry 0 246
isr_entry 0 247
isr_entry 0 248
isr_entry 0 249
isr_entry 0 250
isr_entry 0 251
isr_entry 0 252
isr_entry 0 253
isr_entry 0 254
isr_entry 0 255

.section .text.isr

.macro isr_common_init_cfa
     // Initial CFA using raw stack pointer
     // CFA
     // index
     //  0    7*8(%rsp) <-- CFA
     // -1    6*8(%rsp) ss
     // -2    5*8(%rsp) rsp
     // -3    4*8(%rsp) rflags
     // -4    3*8(%rsp) ss
     // -5    2*8(%rsp) rip
     // -6    1*8(%rsp) error code
     // -7    0*8(%rsp) interrupt number

    .cfi_startproc
    .cfi_signal_frame
    .cfi_def_cfa rsp,7*8
    .cfi_offset ss,-1*8
    .cfi_offset rsp,-2*8
    .cfi_offset rflags,-3*8
    .cfi_offset cs,-4*8
    .cfi_offset rip,-5*8
.endm

.type isr_common,@function
.align 16
.global isr_common
isr_common:
    isr_common_init_cfa

    // Use fixed rbp for CFA to save lots of CFA adjustment eh_frame records

    // push order: rbp, rbx, r15-r10, rax, r9-8, rcx, rdx, rsi, rdi
    push_cfi %rbp
    .cfi_offset rbp,-8*8
    movq %rsp,%rbp
    .cfi_def_cfa rbp,8*8
    .cfi_offset ss,-1*8
    .cfi_offset rsp,-2*8
    .cfi_offset rflags,-3*8
    .cfi_offset cs,-4*8
    .cfi_offset rip,-5*8

    .cfi_offset rbp,-8*8

    // CFA
    // index
    //   0    8*8(%rbp) <-- CFA
    //  -1    7*8(%rbp) -> ss            ┐
    //  -2    6*8(%rbp) -> rsp           │
    //  -3    5*8(%rbp) -> flags         │
    //  -4    4*8(%rbp) -> cs            ├ interrupt frame
    //  -5    3*8(%rbp) -> rip           │
    //  -6    2*8(%rbp) -> error code    │
    //  -7    1*8(%rbp) -> interrupt     ┘
    //  -8    0*8(%rbp) -> caller's rbp  ┐
    //  -9   -1*8(%rbp) -> rbx           │
    // -10   -2*8(%rbp) -> r15           ├ call preserved
    // -11   -3*8(%rbp) -> r14           │
    // -12   -4*8(%rbp) -> r13           │
    // -13   -5*8(%rbp) -> r12           ┘
    // -14   -6*8(%rbp) -> r11           ┐
    // -15   -7*8(%rbp) -> r10           │
    // -16   -8*8(%rbp) -> rax           │
    // -17   -9*8(%rbp) -> r9            │
    // -18  -10*8(%rbp) -> r8            ├ call clobbered
    // -19  -11*8(%rbp) -> rcx           │
    // -20  -12*8(%rbp) -> rdx           │
    // -21  -13*8(%rbp) -> rsi           │
    // -22  -14*8(%rbp) -> rdi           ┘
    // -23  -15*8(%rbp) -> cr3
    // -24  -16*8(%rbp) -> segments
    // -25  -17*8(%rbp) -> fpu ctx ptr
    // -26  -18*8(%rbp) -> cleanup arg
    // -27  -19*8(%rbp) -> cleanup fn

    pushq %rbx
    .cfi_offset rbx,-9*8

    pushq %r15
    .cfi_offset r15,-10*8

    // r15d=ds, r14d=es, r13d=fs, r12d=gs
    movl %ds,%r15d

    pushq %r14
    .cfi_offset r14,-11*8

    movl %es,%r14d

    pushq %r13
    .cfi_offset r13,-12*8

    movl %fs,%r13d

    pushq %r12
    .cfi_offset r12,-13*8

    movl %gs,%r12d

    pushq %r11
    .cfi_offset r11,-14*8

    // shift es, fs, gs into position to pack into r13
    shll $16,%r14d

    pushq %r10
    .cfi_offset r10,-15*8

    shlq $32,%r13

    pushq %rax
    .cfi_offset rax,-16*8

    shlq $48,%r12

    movq %cr3,%rax

    // Merge ds,es into r15
    orl %r14d,%r15d

    cld
    pushq %r9
    .cfi_offset r9,-17*8

    // Merge fs,gs into r13
    orq %r12,%r13

    pushq %r8
    .cfi_offset r8,-18*8

    // Merge ds|es,fs|gs into r13
    orq %r15,%r13

    pushq %rcx
    .cfi_offset rcx,-19*8

    // Get interrupted code segment
    movzwl frame_cs(%rbp),%ecx

    pushq %rdx
    .cfi_offset rdx,-20*8

    pushq %rsi
    .cfi_offset rsi,-21*8

    pushq %rdi
    .cfi_offset rdi,-22*8

    // Assume null FPU context pointer
    xorl %edi,%edi

    // Save CR3
    pushq %rax

    // Save segments
    pushq %r13

    // Check for doublefault nonsense cs
    testl %ecx,%ecx
    jz .Lfrom_kernel

    // See if we are coming from kernel code
    cmpl $ GDT_SEL_KERNEL_CODE64,%ecx
    je .Lfrom_kernel

    // ...came from user code
    swapgs

.align 16
.Lfrom_kernel:
    // Push FPU context pointer field, initially null
    pushq $0

    // Outgoing cleanup
    pushq $0
    pushq $0

    // Hold isr_context_t pointer in r12
    // Hold interrupt number in ebx

    movq %rsp,%r12
    movl frame_intr(%rbp),%ebx

    // Align stack pointer
    subq $8,%rsp

    // Interrupt dispatch
    // 0x00-0x1F -> exception_isr_handler
    // 0x20-0x2F -> intr_invoke
    // 0x30-0xEF -> apic_dispatcher
    // 0xF0-0xFF -> pic_dispatcher

    // 0x00-0x1F -> exception_isr_handler
    leaq exception_isr_handler(%rip),%rax

    // Fast path page fault
    leaq mmu_page_fault_handler(%rip),%rcx
    cmpl $ INTR_EX_PAGE,%ebx
    cmoveq %rcx,%rax

    // 0x20-0x2F -> intr_invoke
    leaq intr_invoke(%rip),%rcx
    cmpl $ INTR_SOFT_BASE,%ebx
    cmovaeq %rcx,%rax

    // 0x30-0xEF -> apic_dispatcher
    leaq apic_dispatcher(%rip),%rcx
    cmpl $ INTR_APIC_IRQ_BASE,%ebx
    cmovaeq %rcx,%rax

    // 0xF0-0xFF -> pic_dispatcher
    leaq pic8259_dispatcher(%rip),%rcx
    cmpl $ INTR_PIC1_IRQ_BASE,%ebx
    cmovaeq %rcx,%rax

    // Pass intr, 1st parameter
    // Pass context pointer, 2nd parameter
    movl %ebx,%edi
    movq %r12,%rsi

    // Call handler
    indirect_call rax

    // If context pointer is null, invoke the exception handler for this thread
    test %rax,%rax
    jz .Linvoke_catch

    // isr can return a new stack pointer, or just return
    // the passed one to continue with this thread
    mov %rax,%rsp

    // Infer pointer to incoming thread's interrupt frame from ctx pointer
    lea 19*8(%rax),%rbp

    // Pop outgoing cleanup data
    // Used to adjust outgoing thread state after switching stack
    popq %rax
    popq %rdi
    testq %rax,%rax
    jz .Lno_cleanup_call
    indirect_call rax
.Lno_cleanup_call:

    // Pop the pointer to the FPU context
    popq %rdi

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
    movzwl frame_cs(%rbp),%ecx

    // Pop segments
    popq %rdx

    movq %gs:CPU_INFO_CURTHREAD_OFS,%r15

    // See if we're not returning to user code
    // Branch past swapgs and restoration of segments if not
    cmpl $ GDT_SEL_KERNEL_CODE64,%ecx
    je .Lreturning_to_kernel

    // ...returning to user code

    // Fetch pointer to this CPU's TSS for TSS.RSP0 update
    movq %gs:CPU_INFO_TSS_PTR_OFS,%rcx
    leaq frame_end(%rbp),%rax
    movq %rax,TSS_RSP0_OFS(%rcx)

    // Restore user gs
    swapgs

    // If segments are not changing, avoid 136*4 cycles
    cmpq %r13,%rdx
    jnz .Lsegments_changed

.Lsegments_restored:
    movq THREAD_FSBASE_OFS(%r15),%r13
    movq THREAD_GSBASE_OFS(%r15),%r14

    // Load fsbase and gsbase
    // NOTE: this may be patched to call
    // .Lload_fs_gs_fast (to use wr(fs|gs)base)
    // rax, rcx, rdx may be clobbered here
    call .Lload_fs_gs_slow
.Lpatch_fsgsbase_csw_end:

.Lreturning_to_kernel:

    .cfi_remember_state

    // Restore CR3
    popq %rax
    movq %rax,%cr3

    popq %rdi
    .cfi_same_value rdi

    popq %rsi
    .cfi_same_value rsi

    popq %rdx
    .cfi_same_value rdx

    popq %rcx
    .cfi_same_value rcx

    popq %r8
    .cfi_same_value r8

    popq %r9
    .cfi_same_value r9

    popq %rax
    .cfi_same_value rax

    popq %r10
    .cfi_same_value r10

    popq %r11
    .cfi_same_value r11

    popq %r12
    .cfi_same_value r12

    popq %r13
    .cfi_same_value r13

    popq %r14
    .cfi_same_value r14

    popq %r15
    .cfi_same_value r15

    popq %rbx
    .cfi_same_value rbx

    popq %rbp

    // rbp has been restored, back to raw rsp-relative CFA
    .cfi_def_cfa rsp,7*8
    .cfi_offset ss,-1*8
    .cfi_offset rsp,-2*8
    .cfi_offset rflags,-3*8
    .cfi_offset cs,-4*8
    .cfi_offset rip,-5*8

    fn_trace isr_common,exit

    add_rsp 16

    iretq

.align 16
.Lsegments_changed:
    .cfi_restore_state
    // Restore segments
    movw %dx,%ds
    shrq $16,%rdx
    movw %dx,%es
    shrq $16,%rdx
    movw %dx,%fs
    shrl $16,%edx
    movw %dx,%gs
    jmp .Lsegments_restored

    .cfi_endproc

// Quickly load FS/GS base
// Expects r13=user_fsbase, r14=user_gsbase
.align 16
.Lload_fs_gs_fast:
    .cfi_startproc

    wrfsbase %r13
    wrgsbase %r14
    ret

    .cfi_endproc

// Slow load FS/GS base for processors that don't support wrfsbase/wrgsbase
// Reaches here with r13=user_fsbase, r14=user_gsbase
.align 16
.Lload_fs_gs_slow:
    .cfi_startproc

    movl $ CPU_MSR_FSBASE,%ecx
    movq %r13,%rdx
    movl %r13d,%eax
    shrq $32,%rdx
    wrmsr
    movl $ CPU_MSR_GSBASE,%ecx
    movq %r14,%rdx
    movl %r14d,%eax
    shrq $32,%rdx
    wrmsr
    // Clear registers for sysret scenario
    xor %eax,%eax
    mov %eax,%edx
    mov %eax,%ecx
    ret

    .cfi_endproc

// Pass thread_info_t pointer in rdi
// Clobbers rsi,rdx,rax
// Returns rdi=pointer to context
.macro xsave_ctx insn
    .cfi_startproc

    // Set all bits of edx:eax
    movl $-1,%eax
    movl %eax,%edx

    // Read xsave stack pointer from thread
    movq THREAD_XSAVE_PTR_OFS(%rdi),%rsi
    subq sse_context_size(%rip),%rsi

    // Save context using instruction passed to macro
    \insn (%rsi)

    // Update xsave stack pointer in thread
    movq %rsi,THREAD_XSAVE_PTR_OFS(%rdi)
    ret

    .cfi_endproc
.endm

// Expects rdi to point to saved context
// Clobbers eax,edx,ecx
.macro xrstor_ctx insn
    .cfi_startproc

    // Set all bits of edx:eax
    movl $-1,%eax
    movl %eax,%edx

    movq THREAD_XSAVE_PTR_OFS(%rdi),%rsi

    // Restore context using instruction passed to macro
    \insn (%rsi)

    addq sse_context_size(%rip),%rsi
    movq %rsi,THREAD_XSAVE_PTR_OFS(%rdi)

    ret

    .cfi_endproc
.endm

// Branch directly to appropriate save/restore

.align 16
.global isr_save_fpu_ctx
.hidden isr_save_fpu_ctx
isr_save_fpu_ctx:
    jmp isr_save_fxsave
.Lsse_context_save_jmp:

.align 16
.global isr_restore_fpu_ctx
.hidden isr_restore_fpu_ctx
isr_restore_fpu_ctx:
    jmp isr_restore_fxrstor
.Lsse_context_restore_jmp:

// Save/restore implementations

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
.global isr_save_xsaves
.hidden isr_save_xsaves
isr_save_xsaves:
    xsave_ctx xsaves64

.align 16
.global isr_restore_xrstors
.hidden isr_restore_xrstors
isr_restore_xrstors:
    xrstor_ctx xrstors64

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

// on entry, ebx=intr, r12=ctx
.align 16
exception_isr_handler:
    .cfi_startproc
    .cfi_def_cfa rbp,4*8
    .cfi_offset ss,3*8
    .cfi_offset rsp,2*8
    .cfi_offset rflags,1*8
    .cfi_offset cs,0*8
    .cfi_offset rip,-1*8

    leaq intr_has_handler(%rip),%rax

    // If there is no handler...
    indirect_call rax
    //callq *%rax
    testl %eax,%eax
    jz .Lno_handler

    // There is a handler
    leaq intr_invoke(%rip),%rax
    movl %ebx,%edi
    movq %r12,%rsi
    indirect_call rax
    //callq *%rax

    movq %r12,%rax
    retq

.Lno_handler:

// fall through...

.align 16
.Linvoke_catch:
    // ...no handler or handler rejected it
    leaq __exception_handler_invoke(%rip),%rax
    movl %ebx,%edi

    indirect_call rax
    //callq *%rax
    testl %eax,%eax
    jnz 2f

    leaq cpu_debug_break(%rip),%rax
    indirect_call rax
    //call *%rax

    // (not Tail) call to unhandled_exception_handler
    // tail call breaks the backtrace, which is bad and not worth
    // an insignificant tail call optimization  on an already
    // expensive code path
.align 16
2:  leaq unhandled_exception_handler(%rip),%rax
    movq %r12,%rdi
    indirect_call rax
    ret
    //jmpq *%rax

    .cfi_endproc

// _noreturn void isr_sysret64(uintptr_t rip, uintptr_t rsp);
.hidden isr_sysret64
.global isr_sysret64
isr_sysret64:
    .cfi_startproc

    push_cfi %rbx
    .cfi_offset rbx,-2*8

    push_cfi %rbp
    .cfi_offset rbp,-3*8

    push_cfi %r12
    .cfi_offset r12,-4*8

    push_cfi %r13
    .cfi_offset r13,-5*8

    push_cfi %r14
    .cfi_offset r14,-6*8

    push_cfi %r15
    .cfi_offset r15,-7*8

    mov %rdi,%rcx
    mov $ CPU_EFLAGS_IF | 2,%r11d

    // Initialize kernel stack pointer for this thread
    // cpu_info_t *rbx = this_cpu()
    movq %gs:CPU_INFO_SELF_OFS,%rbx

    // tss_t *rdx = rbx->tss_ptr
    movq CPU_INFO_TSS_PTR_OFS(%rbx),%rdx

    // thread_info_t *rbx = rbx->cur_thread
    movq CPU_INFO_CURTHREAD_OFS(%rbx),%rbx

    // void *fs_base = rbx->fsbase
    movq THREAD_FSBASE_OFS(%rbx),%r13

    // void *gs_base = rbx->gsbase
    movq THREAD_GSBASE_OFS(%rbx),%r14

    // void *rbx = rbx->stack
    movq THREAD_STACK_OFS(%rbx),%rbx

    // rdx->rsp0 = rbx
    movq %rbx,TSS_RSP0_OFS(%rdx)

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
    // not r13
    // not r14
    // not r15

    // Prevent IRQ while swapped to user gs or using user stack
    // in kernel mode
    cli
    swapgs
    call .Lload_fs_gs_slow
.Lpatch_fsgsbase_sysret_end:
    mov %rsi,%rsp
    .cfi_undefined rip
    .cfi_undefined rsp
    .cfi_undefined rbx
    .cfi_undefined rbp
    .cfi_undefined r12
    .cfi_undefined r13
    .cfi_undefined r14
    .cfi_undefined r15
    sysretq

    .cfi_endproc

// Retpoline. Works on the principle that the return stack overrides other
// branch prediction information. Cause it to mispredict into a pause loop
// until the ret retires, at which point it is guaranteed to branch to the
// correct destination. Attacker cannot train a mispredict to a malicious
// location.
.macro make_retpoline_thunk reg
.global __x86_indirect_thunk_\reg
.align 16
__x86_indirect_thunk_\reg:
    .cfi_startproc
    .cfi_def_cfa rsp,8
    call 0f
1:  lfence
    pause
    jmp 1b
0:  .cfi_adjust_cfa_offset 8
    movq %\reg,(%rsp)
    // This will speculatively return to the 1 label, but at retirement
    // it will see that it should have branched to *%\reg
    ret
    .cfi_endproc
.endm

.irp reg,rax,rcx,rdx,rbx,rbp,r8,r9,r10,r11,r12,r13,r14,r15
    make_retpoline_thunk \reg
.endr

.section .rodata
.align 8

// Pointers to patch points
.hidden sse_context_save
.global sse_context_save
sse_context_save:
    .quad .Lsse_context_save_jmp

.hidden sse_context_restore
.global sse_context_restore
sse_context_restore:
    .quad .Lsse_context_restore_jmp

.hidden load_fsgsbase_range
.global load_fsgsbase_range
load_fsgsbase_range:
    // Implementations
    .quad .Lload_fs_gs_fast
    // Patch points
    .quad .Lpatch_fsgsbase_csw_end
    .quad .Lpatch_fsgsbase_sysret_end
