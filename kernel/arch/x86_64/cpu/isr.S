
#include "asm_constants.h"
#include "cpu_info.ofs.h"
#include "thread_info.ofs.h"
#include "control_regs_constants.h"
#include "../cfi_helpers.h"
#include "interrupts.h"
#include "cpu_bug.h"
#include "isr.h"
#include "fixup.h"

.code64

// .struct is horrible, better to use #define than create weirdo symbols at 0
//.struct 0
//    frame_rbp:  .struct frame_rbp+8
//    frame_intr: .struct frame_intr+8
//    frame_err:  .struct frame_err+8
//    frame_rip:  .struct frame_rip+8
//    frame_cs:   .struct frame_cs+8
//    frame_efl:  .struct frame_efl+8
//    frame_rsp:  .struct frame_rsp+8
//    frame_ss:   .struct frame_ss+8
//    frame_end:

#define frame_rbp  0
#define frame_intr 8
#define frame_err  16
#define frame_rip  24
#define frame_cs   32
#define frame_efl  40
#define frame_rsp  48
#define frame_ss   56
#define frame_end  64

.section .text.isr, "ax", @progbits

.balign 16
.global isr_entry_st
isr_entry_st:

.macro fn_trace fn, op
#ifdef _CALL_TRACE_ENABLED
    push_cfi %rax
    push_cfi %rcx
    push_cfi %rdx
    push_cfi %rsi
    push_cfi %rdi
    push_cfi %r8
    push_cfi %r9
    push_cfi %r10
    push_cfi %r11
    adj_rsp_cfi -8

    lea (\fn)(%rip),%rdi
    xor %esi,%esi
    call __cyg_profile_func_\op

    adj_rsp_cfi 8
    pop_cfi %r11
    pop_cfi %r10
    pop_cfi %r9
    pop_cfi %r8
    pop_cfi %rdi
    pop_cfi %rsi
    pop_cfi %rdx
    pop_cfi %rcx
    pop_cfi %rax
#endif
.endm

.macro isr_section_name num
    .if \num < 10
        .section .text.isr.00\num, "x", @progbits
    .elseif \num < 100
        .section .text.isr.0\num, "x", @progbits
    .else
        .section .text.isr.\num, "x", @progbits
    .endif
.endm

.macro isr_entry_impl has_code int_num entryname pathname
isr_section_name \int_num
.global \entryname\()_\int_num\()
.hidden \entryname\()_\int_num\()
.type \entryname\()_\int_num\(),@function
.balign 16
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
        // -4    1*8(%rsp) cs
        // -5    0*8(%rsp) rip
        .cfi_def_cfa rsp,5*8
        .cfi_offset ss,-1*8
        .cfi_offset rsp,-2*8
        .cfi_offset rflags,-3*8
        .cfi_offset cs,-4*8
        .cfi_offset rip,-5*8
        // Push 0 error code to normalize
        push_cfi $ 0
        fn_trace \pathname,enter
    .else
        // CFA
        // index
        //  0    6*8(%rsp) <-- CFA
        // -1    5*8(%rsp) ss
        // -2    4*8(%rsp) rsp
        // -3    3*8(%rsp) rflags
        // -4    2*8(%rsp) cs
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
    // ctx_flags will be zeroed by this too (the upper 32 bits)
    push_cfi $(\int_num)
    jmp \pathname
    .cfi_endproc
.endm

.macro isr_entry_custom has_code int_num handler
    isr_entry_impl \has_code \int_num isr_entry \handler
.endm

.macro isr_entry has_code int_num
     isr_entry_impl \has_code \int_num isr_entry isr_common
.endm

// Individually to make placing breakpoints trivial

// Although every attempt is made to make these lines be written
// in the order of vector number, the linker script and isr_entry
// macro are working together to force them all to the correct order
// anyway, so don't worry if it isn't perfect
// Worry about not having all 256 entries specified when making edits,
// that case can go unnoticed with severe problems

// Exceptions
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
isr_entry 1 21
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

// APIC interrupt but not really because no EOI
isr_entry 0 INTR_APIC_SPURIOUS

// Software interrupts
isr_entry 0 INTR_THREAD_YIELD

isr_entry 0 34
isr_entry 0 35
isr_entry 0 36
isr_entry 0 37
isr_entry 0 38
isr_entry 0 39

// Very high priority interrupts
isr_entry 0 INTR_IPI_PANIC
isr_entry 0 INTR_APIC_ERROR
isr_entry 0 INTR_APIC_THERMAL
isr_entry 0 INTR_APIC_TIMER
isr_entry 0 INTR_IPI_TLB_SHTDN
isr_entry 0 INTR_IPI_RESCHED
isr_entry 0 INTR_IPI_FL_TRACE
isr_entry 0 47

// APIC and MSI(x) IRQs [48-240) = 192 interrupt vectors available
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

// PIC IRQs (lowest priority interrupts)
isr_entry 0 INTR_PIC1_IRQ_BASE
isr_entry 0 241
isr_entry 0 242
isr_entry 0 243
isr_entry 0 244
isr_entry 0 245
isr_entry 0 246
isr_entry 0 INTR_PIC1_SPURIOUS
isr_entry 0 INTR_PIC2_IRQ_BASE
isr_entry 0 249
isr_entry 0 250
isr_entry 0 251
isr_entry 0 252
isr_entry 0 253
isr_entry 0 254
isr_entry 0 INTR_PIC2_SPURIOUS

.section .text.isr, "ax", @progbits

.macro isr_common_init_cfa
    // Initial CFA using raw stack pointer
    // CFA
    // index
    //  0    7*8(%rsp) <-- CFA
    // -1    6*8(%rsp) ss
    // -2    5*8(%rsp) rsp
    // -3    4*8(%rsp) rflags
    // -4    3*8(%rsp) cs
    // -5    2*8(%rsp) rip
    // -6    1*8(%rsp) error code
    // -7    0*8(%rsp) interrupt number and ctx_flags

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
.balign 16
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
    //          frame      raw
    //   0    8*8(%rbp) 25*8(%rsp) <-- CFA
    //  -1    7*8(%rbp) 24*8(%rsp) -> ss            ┐
    //  -2    6*8(%rbp) 23*8(%rsp) -> rsp           │
    //  -3    5*8(%rbp) 22*8(%rsp) -> flags         │
    //  -4    4*8(%rbp) 21*8(%rsp) -> cs            ├ interrupt frame
    //  -5    3*8(%rbp) 20*8(%rsp) -> rip           │
    //  -6    2*8(%rbp) 19*8(%rsp) -> error code    │
    //  -7    1*8(%rbp) 18*8(%rsp) -> interrupt     ┘
    //  -8    0*8(%rbp) 17*8(%rsp) -> caller's rbp  ┐ <- &*rbp
    //  -9   -1*8(%rbp) 16*8(%rsp) -> rbx           │
    // -10   -2*8(%rbp) 15*8(%rsp) -> r15           ├ call preserved
    // -11   -3*8(%rbp) 14*8(%rsp) -> r14           │
    // -12   -4*8(%rbp) 13*8(%rsp) -> r13           │
    // -13   -5*8(%rbp) 12*8(%rsp) -> r12           ┘
    // -14   -6*8(%rbp) 11*8(%rsp) -> r11           ┐
    // -15   -7*8(%rbp) 10*8(%rsp) -> r10           │
    // -16   -8*8(%rbp)  9*8(%rsp) -> rax           │
    // -17   -9*8(%rbp)  8*8(%rsp) -> r9            │
    // -18  -10*8(%rbp)  7*8(%rsp) -> r8            ├ call clobbered
    // -19  -11*8(%rbp)  6*8(%rsp) -> rcx           │
    // -20  -12*8(%rbp)  5*8(%rsp) -> rdx           │
    // -21  -13*8(%rbp)  4*8(%rsp) -> rsi           │
    // -22  -14*8(%rbp)  3*8(%rsp) -> rdi           ┘
    // -23  -15*8(%rbp)  2*8(%rsp) -> cr3
    // -24  -16*8(%rbp)  1*8(%rsp) -> segments
    // -25  -17*8(%rbp)  0*8(%rsp) -> fpu ctx ptr     <- Saved thread rsp

    /// Register usage:
    ///  r15 accumulates segment registers

    pushq %rbx
    pushq %r15
    pushq %r14
    pushq %r13
    .cfi_offset rbx,-9*8
    .cfi_offset r15,-10*8
    .cfi_offset r14,-11*8
    .cfi_offset r13,-12*8

    movq %cr3,%rbx
    movl %ds,%r15d
    movl %es,%r14d
    movl %fs,%r13d

    pushq %r12
    pushq %r11
    pushq %r10
    pushq %rax
    .cfi_offset r12,-13*8
    .cfi_offset r11,-14*8
    .cfi_offset r10,-15*8
    .cfi_offset rax,-16*8

    cld

    movl %gs,%r12d
    shll $ 16,%r14d
    shlq $ 32,%r13

    // Make sure access to user pages is blocked
    insn_fixup
    clac

    pushq %r9
    pushq %r8
    pushq %rcx
    pushq %rdx
    .cfi_offset r9,-17*8
    .cfi_offset r8,-18*8
    .cfi_offset rcx,-19*8
    .cfi_offset rdx,-20*8

    shlq $ 48,%r12

    // Combine r12-r15 into r15
    orl %r14d,%r15d
    orq %r12,%r13

    orq %r13,%r15

    /// Register usage:
    ///  r15 now has all segment registers, rbx has cr3

    pushq %rsi
    pushq %rdi
    .cfi_offset rsi,-21*8
    .cfi_offset rdi,-22*8

    // Save CR3
    pushq %rbx

    // Save segments
    pushq %r15

    // Avoid giant instruction below
    mov frame_intr(%rbp),%rbx

    // Get cs into edi for RPL checks
    movl frame_cs(%rbp),%edi

    /// Register usage:
    ///  rbx now holds frame_intr
    ///  r15 now has all segment registers

    // Guaranteed to swap if came from user mode
    // Note that maybe_swap detour clobbers rax,rdx,rcx
    testl $ 3,%edi
    jz .Lmaybe_swap

    // No branch taken if interrupting user mode

.Lneed_swap:
    // set flag bit indicating non-kernel gs context
    orq $ 1 << ISR_CTX_CTX_FLAGS_USERGS_QWBIT,%rbx
    mov %rbx,frame_intr(%rbp)
    swapgs
.Lno_swap:

    // Now definitely have kernel gsbase...

    // See if we are coming from kernel code
    // Handles doublefault nonsense 0 cs too
    testl $ 3,%edi
    jz .Lfrom_kernel

    /// Register usage:
    ///  rsi = 0
    ///  r12 = this_cpu()->cur_thread
    ///  r15 = state of cpu's segment registers

    // Flush indirect branch prediction data because the cpu
    // is tainted by user mode process
    // This could be patched to a long nop or call to something else
    insn_fixup
    call protection_barrier_from_user

.Lfrom_kernel:

    movzbl frame_intr(%rbp),%ebx

    // Push FPU context pointer field, initially null
    pushq $ 0

    /// Register usage:
    ///  Hold interrupt number in ebx

    lea isr_lookup(%rip),%rax
    movl %ebx,%edi
    movq %rsp,%rsi
    add $ -8,%rsp
    indirect_call_mem rcx,"(%rax,%rbx,8)"

    // If context pointer is null, invoke the exception handler for this thread
    test %rax,%rax
    jz .Linvoke_catch

    // If we are restoring a fast context then jump to simpler code
    // Fast context will have sign bit of ctx flag field set
    // (packed in interrupt value)
    // Fast context restore requires
    testl $ (1 << ISR_CTX_CTX_FLAGS_FAST_QWBIT), ISR_CTX_OFS_INTERRUPT(%rax)
    jnz .Lrestore_fast_ctx

    // Branches here from fast switcher if we switched to a slow context
.Lrestore_slow_ctx:
    // Load a zero to use much smaller encodings for gs acccesses below
    xor %ecx,%ecx

    // isr can return a new stack pointer, or just return
    // the passed one to continue with this thread
    mov %rax,%rsp

    // Infer pointer to incoming thread's interrupt frame from ctx pointer
    lea ISR_CTX_OFS_RBP(%rax),%rbp

    // rcx = this_cpu()
    movq %gs:(%rcx),%rcx

    cmpl $ INTR_EX_NMI,frame_intr(%rbp)
    jz .Ldone_cleanup_call_slow

    xor %edx,%edx

    lea thread_clear_busy(%rip),%rsi

    // Run cpu-local outgoing cleanup after stack switch (now)
    // Used to adjust outgoing thread state after switching stack

    // Load function pointer
    mov CPU_INFO_AFTER_CSW_FN_OFS(%rcx),%rax
    mov %rdx,CPU_INFO_AFTER_CSW_FN_OFS(%rcx)

    // No cleanup case
    test %rax,%rax
    jz .Ldone_cleanup_call_slow

    // Falls through when actually context switching (the important case)

    // Parameter
    mov CPU_INFO_AFTER_CSW_VP_OFS(%rcx),%rdi
    mov %rdx,CPU_INFO_AFTER_CSW_VP_OFS(%rcx)

    // Devirtualize thread_clear_busy or jump to out of line unlikely case
    cmp %rsi,%rax
    jnz .Lindirect_cleanup_call_slow

    // Fall through hottest case

    call thread_clear_busy
.Ldone_cleanup_call_slow:

    // Avoid giant instruction below
    movl $ 1 << ISR_CTX_CTX_FLAGS_USERGS_QWBIT,%eax

    // Discard the pointer to the FPU context, segments, cr3
    add $ 8*3,%rsp

    testl %eax,frame_intr(%rbp)
    jz .Lno_restore_swap

    // No branch taken when returning to user code
    swapgs

.Lno_restore_swap:

    .cfi_remember_state

    // mov is sometimes faster than pop

    mov 0*8(%rsp),%rdi
    mov 1*8(%rsp),%rsi
    mov 2*8(%rsp),%rdx
    mov 3*8(%rsp),%rcx
    mov 4*8(%rsp),%r8
    mov 5*8(%rsp),%r9
    mov 6*8(%rsp),%rax
    mov 7*8(%rsp),%r10
    mov 8*8(%rsp),%r11
    mov 9*8(%rsp),%r12
    mov 10*8(%rsp),%r13
    mov 11*8(%rsp),%r14
    mov 12*8(%rsp),%r15
    mov 13*8(%rsp),%rbx
    mov 14*8(%rsp),%rbp

    // Discard stuff moved from stack, plus the error code and intr number
    add_rsp_cfi 17*8

    // rbp has been restored, back to raw rsp-relative CFA
    .cfi_def_cfa rsp,5*8
    .cfi_offset ss,-1*8
    .cfi_offset rsp,-2*8
    .cfi_offset rflags,-3*8
    .cfi_offset cs,-4*8
    .cfi_offset rip,-5*8

    testl $ 3,8(%rsp)
    jz .Lnot_iret_to_user

    // This could be patched to a long nop
    insn_fixup
    call protection_barrier_to_user

.Lnot_iret_to_user:
    fn_trace isr_common,exit

    iretq

    .cfi_restore_state

.balign 16
.Lmaybe_swap:
    mov $ CPU_MSR_GSBASE,%ecx
    rdmsr
    // If bit 63 of gsbase is set, it must be the kernel gs
    test %edx,%edx
    js .Lno_swap
    jmp .Lneed_swap

.Lindirect_cleanup_call_slow:
    indirect_call rax
    jmp .Ldone_cleanup_call_slow

    .cfi_endproc

// Yield as efficiently as possible
// Assumes yielding thread is in kernel mode
.balign 16
.global thread_yield
thread_yield:
    .cfi_startproc

    // Snag return address from the stack,
    // we're replacing that with a synthetic interrupt frame
    mov (%rsp),%rcx

    // Switch to getting return address from a register
    .cfi_register rip,rcx

    // At this point, the stack pointer conveniently has
    // the value it will have when the thread resumes
    // Save caller's rsp in rax
    leaq 8(%rsp),%rax
    .cfi_register rsp,rax

    // push ss:rsp

    push_cfi $(GDT_SEL_KERNEL_DATA)
    .cfi_offset ss,-2*8

    push_cfi %rax
    .cfi_offset rsp,-3*8

    // push eflags
    pushfq_cfi
    .cfi_offset rflags,-4*8

    // Must be after saving flags
    cli

    // rax remains a zero constant for a while
    xorl %eax,%eax

    // push cs:rip
    push_cfi $(GDT_SEL_KERNEL_CODE64)
    .cfi_offset cs,-5*8
    push_cfi %rcx
    .cfi_offset rip,-6*8

    //          frame      raw
    //   0   8*8(%rbp) 25*8(%rsp) <-- CFA
    //  -1   7*8(%rbp) 24*8(%rsp) -> ss            ┐
    //  -2   6*8(%rbp) 23*8(%rsp) -> rsp           │
    //  -3   5*8(%rbp) 22*8(%rsp) -> flags         │
    //  -4   4*8(%rbp) 21*8(%rsp) -> cs            ├ interrupt frame
    //  -5   3*8(%rbp) 20*8(%rsp) -> rip           │
    //  -6   2*8(%rbp) 19*8(%rsp) -> error code    │
    //  -7   1*8(%rbp) 18*8(%rsp) -> interrupt     ┘ <- has fast ctx bit set
    //  -8   0*8(%rbp) 17*8(%rsp) -> caller's rbp  ┐
    //  -9  -1*8(%rbp) 16*8(%rsp) -> rbx           │
    // -10  -2*8(%rbp) 15*8(%rsp) -> r15           ├ call preserved
    // -11  -3*8(%rbp) 14*8(%rsp) -> r14           │
    // -12  -4*8(%rbp) 13*8(%rsp) -> r13           │
    // -13  -5*8(%rbp) 12*8(%rsp) -> r12           ┘
    // -14  -6*8(%rbp) 11*8(%rsp) -> r11  XXX      ┐
    // -15  -7*8(%rbp) 10*8(%rsp) -> r10  XXX      │
    // -16  -8*8(%rbp)  9*8(%rsp) -> rax  XXX      │
    // -17  -9*8(%rbp)  8*8(%rsp) -> r9   XXX      │
    // -18 -10*8(%rbp)  7*8(%rsp) -> r8   XXX      ├ call clobbered
    // -19 -11*8(%rbp)  6*8(%rsp) -> rcx  XXX      │ (garbage)
    // -20 -12*8(%rbp)  5*8(%rsp) -> rdx  XXX      │
    // -21 -13*8(%rbp)  4*8(%rsp) -> rsi  XXX      │
    // -22 -14*8(%rbp)  3*8(%rsp) -> rdi  XXX      ┘
    // -23 -15*8(%rbp)  2*8(%rsp) -> cr3
    // -24 -16*8(%rbp)  1*8(%rsp) -> segments
    // -25 -17*8(%rbp)  0*8(%rsp) -> fpu ctx ptr

    // Push zero error code
    push_cfi %rax

    // Save PDBR (scheduled ahead)
    movq %cr3,%rcx

    // Push interrupt number
    push_cfi $(INTR_THREAD_YIELD|(1<<ISR_CTX_CTX_FLAGS_FAST_QWBIT))

    // Save caller rbp

    push_cfi %rbp
    .cfi_offset rbp,-9*8

    // Switch to rbp-relative frame, avoid CFA adjustments I hope

    movq %rsp,%rbp
    .cfi_def_cfa rbp,8*8
    .cfi_offset ss,-1*8
    .cfi_offset rsp,-2*8
    .cfi_offset rflags,-3*8
    .cfi_offset cs,-4*8
    .cfi_offset rip,-5*8
    // errorcode here
    // interrupt number and flags here
    .cfi_offset rbp,-8*8

    // Push call preserved registers

    pushq %rbx
    .cfi_offset rbx,-9*8

    pushq %r15
    .cfi_offset r15,-10*8

    pushq %r14
    .cfi_offset r14,-11*8

    pushq %r13
    .cfi_offset r13,-12*8

    pushq %r12
    .cfi_offset r12,-13*8

    // Uninitialized call clobbered save area

    sub $ 9*8,%rsp
    .cfi_undefined r11
    .cfi_undefined r10
    .cfi_undefined rax
    .cfi_undefined r9
    .cfi_undefined r8
    .cfi_undefined rcx
    .cfi_undefined rdx
    .cfi_undefined rsi
    .cfi_undefined rdi

    // Segments (hardcoded kernel ones)
    movabs $ (GDT_SEL_USER_DATA | 3) * 0x0001000100010001,%r15

    // Save CR3
    push %rcx

    // Push segments
    push %r15

    // FPU context pointer is initially null
    push $ 0

    // Reschedule directly, bypassing everything to do with interrupts
    movq %rsp,%rdi
    xorl %esi,%esi
    call thread_schedule

    // See if we are restoring a fast context
    movl $ (1 << ISR_CTX_CTX_FLAGS_FAST_QWBIT),%ecx
    testl %ecx,ISR_CTX_OFS_INTERRUPT(%rax)
    jz .Lrestore_slow_ctx

    // Could branch here from full interrupt handler to restore a fast context
    // rbp must point to rbp in incoming context
    // rsp must point to context
    // r15 must be the state of the segment registers
.Lrestore_fast_ctx:
    // Zero constant
    xor %ecx,%ecx

    // Switch to the incoming CFA
    lea ISR_CTX_OFS_RBP(%rsp),%rbp
    mov %rax,%rsp

    lea thread_clear_busy(%rip),%rdx

    // Run cpu-local outgoing cleanup after stack switch (now)
    // Used to adjust outgoing thread state after switching stack
    mov %gs:CPU_INFO_AFTER_CSW_FN_OFS(%rcx),%rax
    mov %rcx,%gs:CPU_INFO_AFTER_CSW_FN_OFS(%rcx)

    test %rax,%rax
    jz .Ldone_cleanup_call_fast

    mov %gs:CPU_INFO_AFTER_CSW_VP_OFS(%rcx),%rdi
    mov %rcx,%gs:CPU_INFO_AFTER_CSW_VP_OFS(%rcx)

    cmp %rdx,%rax
    jnz .Lindirect_cleanup_call_fast

    // Common case
    call thread_clear_busy

.Ldone_cleanup_call_fast:

    // Discard FPU ctx, segments, cr3, call clobbered registers
    add $ 12*8,%rsp

    mov (%rsp),%r12
    .cfi_same_value r12

    mov 1*8(%rsp),%r13
    .cfi_same_value r13

    mov 2*8(%rsp),%r14
    .cfi_same_value r14

    mov 3*8(%rsp),%r15
    .cfi_same_value r15

    mov 4*8(%rsp),%rbx
    .cfi_same_value rbx

    mov 5*8(%rsp),%rbp
    .cfi_same_value rbp

    // rbp has been restored, back to raw rsp-relative CFA
    .cfi_remember_state
    .cfi_def_cfa rsp,8*8+5*8
    .cfi_offset ss,-1*8
    .cfi_offset rsp,-2*8
    .cfi_offset rflags,-3*8
    .cfi_offset cs,-4*8
    .cfi_offset rip,-5*8

    // Error code is wakeup return value
    mov 7*8(%rsp),%rax

    add_rsp_cfi 8*8

    // Zero all clobbered registers
    xorl %ecx,%ecx
    movl %ecx,%edx
    movl %ecx,%esi
    movl %ecx,%edi
    movl %ecx,%r8d
    movl %ecx,%r9d
    movl %ecx,%r10d
    movl %ecx,%r11d

    // Load flags from interrupt frame (reach over intr+errcode+rip+cs)
    push_cfi 2*8(%rsp)
    popfq_cfi

    // Return and discard cs, eflags, rsp, ss
    retq $ ((1+5-1)*8)

    .cfi_restore_state

// Out of line uncommon case
.Lindirect_cleanup_call_fast:
    indirect_call rax
    jmp .Ldone_cleanup_call_fast

    .cfi_endproc

.balign 16
.global cpu_clear_fpu
.hidden cpu_clear_fpu
cpu_clear_fpu:
    vzeroall_insn
    emms
    pxor %mm0,%mm0
    movq %mm0,%mm1
    movq %mm0,%mm2
    movq %mm0,%mm3
    movq %mm0,%mm4
    movq %mm0,%mm5
    movq %mm0,%mm6
    movq %mm0,%mm7
    fcom %st(0)
    emms
    ret

.balign 16
.global soft_vzeroall
.hidden soft_vzeroall
soft_vzeroall:
    pxor   %xmm0, %xmm0
    movdqa %xmm0, %xmm1
    movdqa %xmm0, %xmm2
    movdqa %xmm0, %xmm3
    movdqa %xmm0, %xmm4
    movdqa %xmm0, %xmm5
    movdqa %xmm0, %xmm6
    movdqa %xmm0, %xmm7
    movdqa %xmm0, %xmm8
    movdqa %xmm0, %xmm9
    movdqa %xmm0,%xmm10
    movdqa %xmm0,%xmm11
    movdqa %xmm0,%xmm12
    movdqa %xmm0,%xmm13
    movdqa %xmm0,%xmm14
    movdqa %xmm0,%xmm15
    ret

.balign 16
.global soft_rdfsgsbase_r13r14
.hidden soft_rdfsgsbase_r13r14
soft_rdfsgsbase_r13r14:
    .cfi_startproc
    push_cfi %rcx
    mov $ CPU_MSR_FSBASE,%ecx
    push_cfi %rdx
    push_cfi %rax
    rdmsr
    mov $ CPU_MSR_GSBASE,%ecx
    shl $ 32,%rdx
    mov %eax,%r13d
    or %rdx,%r13
    rdmsr
    shl $ 32,%rdx
    mov %eax,%r14d
    pop_cfi %rax
    or %rdx,%r14
    pop_cfi %rdx
    pop_cfi %rcx
    ret
    .cfi_endproc

.balign 16
.global soft_wrfsgsbase_r13r14
.hidden soft_wrfsgsbase_r13r14
soft_wrfsgsbase_r13r14:
    .cfi_startproc
    push_cfi %rcx
    mov $ CPU_MSR_FSBASE,%ecx
    push_cfi %rdx
    mov %r13,%rdx
    push_cfi %rax
    mov %r13d,%eax
    shr $ 32,%rdx
    wrmsr
    mov $ CPU_MSR_GSBASE,%ecx
    mov %r14,%rdx
    mov %r14d,%eax
    shr $ 32,%rdx
    wrmsr
    pop_cfi %rax
    pop_cfi %rdx
    pop_cfi %rcx
    ret
    .cfi_endproc

// Replaces opcodes sequence f3 49 0f ae d5: wrfsbase %r13
.balign 16
.global soft_wrfsbase_r13
.hidden soft_wrfsbase_r13
soft_wrfsbase_r13:
    .cfi_startproc
    push_cfi %rcx
    movl $ CPU_MSR_FSBASE,%ecx
    push_cfi %rax
    movl %r13d,%eax
    push_cfi %rdx
    movq %r13,%rdx
    shrq $ 32,%rdx
    wrmsr
    pop_cfi %rdx
    pop_cfi %rax
    pop_cfi %rcx
    ret
    .cfi_endproc

// Replaces opcodes sequence f3 49 0f ae de: wrgsbase %r14
.balign 16
.global soft_wrgsbase_r14
.hidden soft_wrgsbase_r14
soft_wrgsbase_r14:
    .cfi_startproc
    push_cfi %rax
    movl %r14d,%eax
    push_cfi %rdx
    movq %r14,%rdx
    push_cfi %rcx
    shrq $ 32,%rdx
    movl $ CPU_MSR_GSBASE,%ecx
    wrmsr
    pop_cfi %rcx
    pop_cfi %rdx
    pop_cfi %rax
    ret
    .cfi_endproc

// Replaces opcodes sequence f3 49 0f ae c5: rdfsbase %r13
.balign 16
.global soft_rdfsbase_r13
.hidden soft_rdfsbase_r13
soft_rdfsbase_r13:
    .cfi_startproc
    push_cfi %rdx
    push_cfi %rax
    push_cfi %rcx
    movl $ CPU_MSR_FSBASE,%ecx
    rdmsr
    shlq $ 32,%rdx
    pop_cfi %rcx
    orq %rax,%rdx
    pop_cfi %rax
    mov %rdx,%r13
    pop_cfi %rdx
    ret
    .cfi_endproc

// Replaces opcodes sequence f3 49 0f ae ce: rdgsbase %r14
.balign 16
.global soft_rdgsbase_r14
.hidden soft_rdgsbase_r14
soft_rdgsbase_r14:
    .cfi_startproc
    push_cfi %rdx
    push_cfi %rax
    push_cfi %rcx
    movl $ CPU_MSR_GSBASE,%ecx
    rdmsr
    shlq $ 32,%rdx
    pop_cfi %rcx
    orq %rax,%rdx
    pop_cfi %rax
    mov %rdx,%r14
    pop_cfi %rdx
    ret
    .cfi_endproc

// Pass thread_info_t pointer in rdi
// Clobbers rsi,rdx,rax
// Returns passed thread_info_t pointer
.macro xsave_ctx insn
    .cfi_startproc

    // Read xsave stack pointer from thread
    movq THREAD_INFO_FPUSAVE_PTR_OFS(%rdi),%rsi
    subq sse_context_size(%rip),%rsi

    // Set all bits of edx:eax
    movl $ -1,%eax
    movl %eax,%edx

    // Save context using instruction passed to macro
    // replaced with best supported instruction
    insn_fixup
    \insn (%rsi)

    // Update xsave stack pointer in thread
    movq %rsi,THREAD_INFO_FPUSAVE_PTR_OFS(%rdi)
    movq %rsi,%rax
    ret

    .cfi_endproc
.endm

// Expects rdi to point to saved context
// Clobbers eax,edx,ecx, returns new xsave stack pointer
.macro xrstor_ctx insn
    .cfi_startproc

    movq THREAD_INFO_FPUSAVE_PTR_OFS(%rdi),%rsi

    // Set all bits of edx:eax
    movl $ -1,%eax
    movl $ -1,%edx

    // Restore context using instruction passed to macro
    // replaced with best supported instruction
    insn_fixup
    \insn (%rsi)

    addq sse_context_size(%rip),%rsi
    movq %rsi,THREAD_INFO_FPUSAVE_PTR_OFS(%rdi)
    movq %rsi,%rax

    ret

    .cfi_endproc
.endm

.section .text.isr, "ax", @progbits

.balign 16
.global isr_save_fpu_ctx64
.hidden isr_save_fpu_ctx64
isr_save_fpu_ctx64:
    xsave_ctx fxsave64

.balign 16
.global isr_restore_fpu_ctx64
.hidden isr_restore_fpu_ctx64
isr_restore_fpu_ctx64:
    xrstor_ctx fxrstor64

.balign 16
.global isr_save_fpu_ctx32
.hidden isr_save_fpu_ctx32
isr_save_fpu_ctx32:
    xsave_ctx fxsave

.balign 16
.global isr_restore_fpu_ctx32
.hidden isr_restore_fpu_ctx32
isr_restore_fpu_ctx32:
    xrstor_ctx fxrstor

.balign 16
.hidden protection_barrier_from_user
.global protection_barrier_from_user
protection_barrier_from_user:
    // Might be called early, but later, calls to this are NOP'd out
    // Just return if it is getting called before patching to IBPB version
    ret

.balign 16
.hidden protection_barrier_to_user
.global protection_barrier_to_user
protection_barrier_to_user:
    // Might be called early, but later, calls to this are NOP'd out
    // Just return if it is getting called before patching to MDS clear version
    ret

.hidden protection_barrier_from_user_ibpb
.global protection_barrier_from_user_ibpb
.balign 16
protection_barrier_from_user_ibpb:
    .cfi_startproc

    push_cfi %rax
    movl $ CPU_MSR_PRED_CMD_IBPB,%eax

    push_cfi %rdx
    movl $ 0,%edx

    push_cfi %rcx
    movl $ CPU_MSR_PRED_CMD,%ecx

    wrmsr

    pop_cfi %rcx
    pop_cfi %rdx
    pop_cfi %rax

    ret

    .cfi_endproc


.hidden protection_barrier_to_user_verw
.global protection_barrier_to_user_verw
.balign 16
protection_barrier_to_user_verw:
    .cfi_startproc

    pushfq_cfi

    // Must be memory operand
    push_cfi $(GDT_SEL_KERNEL_DATA)

    // Stall dispatch until everything up to now retires
    lfence

    // Flush microarchitectural state (MDS mitigation)
    // Must be memory argument!
    verw (%rsp)

    // Stall dispatch until verw completes flushing
    lfence

    // Discard memory argument
    adj_rsp_cfi 8

    popfq_cfi

    ret

    .cfi_endproc

// on entry, rbp=frame,
// ebx=intr, edi=intr,
// r12=ctx, esi=ctx
.balign 16
.hidden exception_isr_handler
.global exception_isr_handler
exception_isr_handler:
    .cfi_startproc
    .cfi_signal_frame
    .cfi_def_cfa rbp,8*8
    .cfi_offset ss,-1*8
    .cfi_offset rsp,-2*8
    .cfi_offset rflags,-3*8
    .cfi_offset cs,-4*8
    .cfi_offset rip,-5*8

    .cfi_offset rbp,-8*8
    .cfi_offset rbx,-9*8
    .cfi_offset r15,-10*8
    .cfi_offset r14,-11*8
    .cfi_offset r13,-12*8
    .cfi_offset r12,-13*8
    .cfi_offset r11,-14*8
    .cfi_offset r10,-15*8
    .cfi_offset rax,-16*8
    .cfi_offset r9,-17*8
    .cfi_offset r8,-18*8
    .cfi_offset rcx,-19*8
    .cfi_offset rdx,-20*8
    .cfi_offset rsi,-21*8
    .cfi_offset rdi,-22*8

    // Align
    add $ -8,%rsp

    call intr_has_handler
    testl %eax,%eax
    jz .Lno_handler

    // There is a handler
    movl %ebx,%edi
    movq %r12,%rsi
    call intr_invoke

    // Align
    add $ 8,%rsp

    retq

.Linvoke_catch:
    // Can breakpoint here to only intercept catch
    nop

.balign 16
.Lno_handler:
    // ...no handler or handler rejected it
    movl %ebx,%edi
    call __exception_handler_invoke

    // if that returned instead of longjmped, then it is unhandled
    movq %r12,%rdi
    call unhandled_exception_handler
    ret

    .cfi_endproc

//thread_fn_t fn, void *p, thread_t id
.global thread_entry
.balign 16
thread_entry:
    .cfi_startproc simple

    // No need to preserve anything, the caller doesn't exist

    // Stop the stack trace
    no_caller_cfi

    // Store parameters so they can be recovered by unwind
    mov %rdi,%r12
    mov %rsi,%r13

    call thread_startup
    ud2

    .cfi_endproc

// _noreturn void isr_sysret(uintptr_t rip,
//                           uintptr_t rsp,
//                           uintptr_t kernel_rsp,
//                           bool use64,
//                           uintptr_t rdi,
//                           uintptr_t rsi,
//                           uintptr_t rdx);
// expects rdi=rip, rsi=rsp, rdx=kernel_rsp, cl=use64
.hidden isr_sysret
.global isr_sysret
.balign 16
isr_sysret:
    .cfi_startproc

    // rdi = rip parameter
    // rsi = rsp parameter
    // rdx = kernel_rsp parameter
    // cl = use64 rip parameter
    // r8 = rdi usermode parameter
    // r9 = rsi usermode parameter

    // zero constant
    xorl %eax,%eax

    // Don't even bother saving register variables, this is a goto user mode
    // that information is already saved by the caller and we are in the
    // "setjmp returned 0" path right now

    // Copy use64 to bpl
    movb %cl,%bpl
    .cfi_undefined rbp

    // Entry point in rcx
    mov %rdi,%rcx

    // Flags in r11
    mov $ CPU_EFLAGS_IF | CPU_EFLAGS_ALWAYS,%r11d

    mov %eax,%r13d
    mov %eax,%r14d

    // Avoid leaking any register values to user code
    mov %eax,%ebx
    mov %eax,%r12d
    mov %eax,%r15d

    // Load parameter spilled to stack before switching stack
    mov 8(%rsp),%r10

    // Needs to be on the kernel stack to save/restore things
    insn_fixup
    call protection_barrier_to_user

    // Switch to user stack (and tell debugger frame is at rdx now)
    mov %rsi,%rdx
    xchg %rdx,%rsp
    .cfi_def_cfa_register rdx

    // goto user mode with rip=rcx and load rflags=r11, cs and ss from STAR MSR
    // interrupts will be reenabled due to restoring their flags

    mov %r8,%rdi
    mov %eax,%r8d
    mov %r9,%rsi
    mov %eax,%r9d

    test %bpl,%bpl
    mov %eax,%ebp
    jz 0f

    // 64-bit entry
    .cfi_remember_state

    // Hide kernel stack frame address from user mode
    // Lose ability to see call stack after this executes
    mov %r10,%rdx

    // No idea where this call came from anymore
    // or the location of the kernel stack frame
    // or their callee preserved registers
    .cfi_undefined rsp
    .cfi_undefined rip
    .cfi_undefined rbx
    .cfi_undefined rbp
    .cfi_undefined r12
    .cfi_undefined r13
    .cfi_undefined r14
    .cfi_undefined r15

    swapgs
    sysretq

0:  // 32-bit entry
    .cfi_restore_state

    // Hide kernel stack frame address from user mode
    // Lose ability to see call stack after this executes
    mov %eax,%edx

    // No idea where this call came from anymore
    // or the location of the kernel stack frame
    // or their callee preserved registers
    .cfi_undefined rsp
    .cfi_undefined rip
    .cfi_undefined rbx
    .cfi_undefined rbp
    .cfi_undefined r12
    .cfi_undefined r13
    .cfi_undefined r14
    .cfi_undefined r15

    sysretl

    .cfi_endproc

.global __module_dynlink_plt_thunk
.balign 16
__module_dynlink_plt_thunk:
    .cfi_startproc
    .cfi_def_cfa_offset 8*3

    // Stack
    // +-------------+ <-- CFA
    // | return_addr | <-- return address from call to @plt stub
    // +=============+
    // | plt_index   | <-- pushed by PLT stub
    // *-------------*
    // | dl_ctx      | <-- from GOT[1]
    // +-------------+ <-- rsp at entry
    // | result_buf  | <-- dynamic linker writes resolved address here, init 0
    // *-------------*
    // | rflags      |
    // +-------------+
    // | r11         |
    // *-------------*
    // | r10         |
    // +-------------+
    // | r9          |
    // *-------------*
    // | r8          |
    // +-------------+
    // | rcx         |
    // *-------------*
    // | rdx         |
    // +-------------+
    // | rsi         |
    // *-------------*
    // | rdi         |
    // +-------------+
    // | rax         |
    // *-------------*

    // The call to __module_dynamic_linker will be
    // preserving the rest if necessary

    push_cfi $(0)

    pushfq_cfi
    .cfi_offset rflags,-5*8

    push_cfi %r11
    .cfi_offset r11,-6*8

    push_cfi %r10
    .cfi_offset r10,-7*8

    push_cfi %r9
    .cfi_offset r9,-8*8

    push_cfi %r8
    .cfi_offset r8,-9*8

    push_cfi %rcx
    .cfi_offset rcx,-10*8

    push_cfi %rdx
    .cfi_offset rdx,-11*8

    push_cfi %rsi
    .cfi_offset rsi,-12*8

    push_cfi %rdi
    .cfi_offset rdi,-13*8

    push_cfi %rax
    .cfi_offset rax,-14*8

    movq %rsp,%rdi
    call __module_dynamic_linker

    pop_cfi %rax
    .cfi_same_value rax

    pop_cfi %rdi
    .cfi_same_value rdi

    pop_cfi %rsi
    .cfi_same_value rsi

    pop_cfi %rdx
    .cfi_same_value rdx

    pop_cfi %rcx
    .cfi_same_value rcx

    pop_cfi %r8
    .cfi_same_value r8

    pop_cfi %r9
    .cfi_same_value r9

    pop_cfi %r10
    .cfi_same_value r10

    pop_cfi %r11
    .cfi_same_value r11

    popfq_cfi
    .cfi_same_value rflags

    // Balance the RSB so this false call matches up with the hacked return
    // on first entry to the function, and when that returns it returns to the
    // actual caller nicely (RSB continues working fine from there on)

    call 0f

    // ret mispredicts to here, when ret completes, decode will restart in the
    // right place and this speculated path is discarded
    // If BTB misses and we fall through to here past the call,
    // then this is the wrong path too
    lfence

.balign 16
0:  lea 8(%rsp),%rsp

    // "return" is really tail calling to the entry point of the dynamically
    // linked function that has been written to the GOT for next time.
    // This pops the return address from the stack, then adds 16 to rsp.
    // 16 to throw away the two things the PLT stub pushed that told us
    // the PLT index to which function we're lazy linking and gave us
    // an opaque callback context pointer
    // The CPU lands on the dynamically linked function with CPU registers
    // precisely as they were when the caller called the function
    retq $ 16

    .cfi_endproc

// This unblocks context switches and forces a switch if one was deferred
.global cs_leave_asm
.balign 16
cs_leave_asm:
    .cfi_startproc
    xorl %eax,%eax
    addl $ -1,%gs:CPU_INFO_LOCKS_HELD_OFS(%rax)
    js .Lpanic
    jnz .Lreturn

    // Making it here means we just left the outermost critical section
    cmpl %eax,%gs:CPU_INFO_CSW_DEFERRED_OFS(%rax)
    jnz .Lis_deferred_csw

    // Making it here means there was no deferred context switch
.Lreturn:
    ret

.Lis_deferred_csw:
    // Making it here means a context switch was deferred, reset flag
    movl %eax,%gs:CPU_INFO_CSW_DEFERRED_OFS(%rax)

    // Tail call
    jmp thread_yield

.Lpanic:
    lea .Lpanic_msg(%rip),%rdi
    call panic
    jmp .Lpanic

    .cfi_endproc

// Not used, now inline assembly single inc instruction
// Entering N critical sections requires you to leave N to get out
//.global cs_enter_asm
//.balign 16
//cs_enter_asm:
//    .cfi_startproc
//    incl %gs:CPU_INFO_LOCKS_HELD_OFS
//    //jz .Lpanic
//.Lreturn:
//    ret
//    .cfi_endproc

.section .rodata, "", @progbits
.Lpanic_msg:
.asciz "Counter out of range"

.section .text.isr, "ax", @progbits

// Retpoline. Works on the principle that the return stack overrides other
// branch prediction information. Cause it to mispredict into a pause loop
// until the ret retires, at which point it is guaranteed to branch to the
// correct destination. Attacker cannot train a mispredict to a malicious
// location.
.macro make_retpoline_thunk reg
.global __x86_indirect_thunk_\reg
.balign 16
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

// Each fault resistant function consists of an address range where faults
// are dismissed and handled. Each range has a landing pad. When a fault
// occurs in that range, the rax register is reset to -1, and the
// instruction pointer is reset to the landing pad (*_faulted location)

.macro nofault_define_region name

.pushsection .nofault.tab.st
.balign 8
.quad \name
.popsection

.pushsection .nofault.tab.en
.balign 8
.quad .L\name\()_end
.popsection

.pushsection .nofault.tab.lp
.balign 8
.quad .L\name\()_faulted
.popsection

.endm

.section .nofault.text, "ax", @progbits

// uint128_t nofault_rdmsr(uint32_t msr)
// returns 128 bit result in rax,rdx
// upper 64 bits (rdx) is is zero on success, lower 64 bits (rax) is value
.global nofault_rdmsr
.hidden nofault_rdmsr
.balign 16
nofault_rdmsr:
    .cfi_startproc

    // Register number into ecx for rdmsr instruction
    mov %edi,%ecx
    rdmsr

    // edx:eax -> rax
    shl $ 32,%rdx
    or %rdx,%rax

    // Zero upper half means success
    xor %edx,%edx

    ret

.Lnofault_rdmsr_faulted:
    // Put -1 into upper half and return 0 value
    mov %rax,%rdx
    xor %eax,%eax
    ret

.Lnofault_rdmsr_end:

    .cfi_endproc

nofault_define_region nofault_rdmsr

// Returns 0 on success
// int nofault_wrmsr(uint32_t msr, uint64_t value)
.global nofault_wrmsr
.hidden nofault_wrmsr
.balign 16
nofault_wrmsr:
    .cfi_startproc

    mov %rsi,%rdx
    mov %edi,%ecx
    mov %esi,%eax
    shr $ 32,%rdx
    wrmsr
    shl $ 32,%rdx
    or %rdx,%rax
    xor %eax,%eax
.Lnofault_wrmsr_faulted:
    ret

.Lnofault_wrmsr_end:

    .cfi_endproc

nofault_define_region nofault_wrmsr



// ptrdiff_t nofault_strncpy(char *dest, char const *value, size_t size);
.global nofault_strncpy
.hidden nofault_strncpy
.balign 16
nofault_strncpy:
    .cfi_startproc

    insn_fixup
    stac

    xor %eax,%eax

.balign 16
.Lnofault_strncpy_loop:
    movzbl (%rsi,%rax),%ecx
    movb %cl,(%rdi,%rax)
    test %ecx,%ecx
    jz .Lnofault_strncpy_done
    add $ 1,%rax
    cmp %rdx,%rax
    jb .Lnofault_strncpy_loop

    // Failed! String too long (-2 long, get it?)
    mov $ -2,%rax

.Lnofault_strncpy_faulted:
.Lnofault_strncpy_done:
    insn_fixup
    clac

    ret

.Lnofault_strncpy_end:

    .cfi_endproc

nofault_define_region nofault_strncpy



// Returns offset to the byte matching the low byte of value
// If not found, returns -2
// If faults, returns -1
// ptrdiff_t nofault_offsetof(char const *s, int value, size_t size)
.global nofault_offsetof
.hidden nofault_offsetof
.balign 16
nofault_offsetof:
    .cfi_startproc

    insn_fixup
    stac

    mov %esi,%eax
    mov %rdx,%rcx
    repne scasb
    jne .Lnofault_offsetof_nomatch

    sub %rcx,%rdx
    dec %rdx
    mov %rdx,%rax

.Lnofault_offsetof_done:
    insn_fixup
    clac

    ret

.Lnofault_offsetof_nomatch:
    mov $ -2,%rax
.Lnofault_offsetof_faulted:
    jmp .Lnofault_offsetof_done

.Lnofault_offsetof_end:

    .cfi_endproc

nofault_define_region nofault_offsetof


// Returns the length if the string fits in a max_size buffer with null
// Returns -2 if string too long
// Returns -1 if faulted
// Returns 0 without touching buffer if max_size is zero
// ptrdiff_t nofault_strnlen(char const *s, size_t max_size)
.global nofault_strnlen
.hidden nofault_strnlen
.balign 16
nofault_strnlen:
    .cfi_startproc

    xor %eax,%eax

    insn_fixup
    stac

    // Do not touch buffer if max_size is zero
    test %rsi,%rsi
    jz .Lnofault_strnlen_done

.balign 16
.Lnofault_strnlen_loop:
    movzbl (%rdi,%rax),%ecx

    // If the value that was loaded was zero then done
    test %ecx,%ecx
    jz .Lnofault_strnlen_done

    // Rather use simplest addressing mode above and increment two registers
    add $ 1,%rax

    // Continue looping if the offset has not reached the maximum yet
    cmp %rsi,%rax
    jb .Lnofault_strnlen_loop

    // Max length! Return -2 (to differentiate it from -1 from fault)
    // -2 for "too" long
    mov $ -2,%rax

.Lnofault_strnlen_done:
.Lnofault_strnlen_faulted:// crash will resume here with -1 in rax

    insn_fixup
    clac

    ret

.Lnofault_strnlen_end:

    .cfi_endproc

nofault_define_region nofault_strnlen


// Returns 1 if replacement occurred
// Returns 0 if expected value was not seen
// Returns -1 if faulted
// int nofault_compare_exchange_32(int *dest, int *expect, int replacement)
.global nofault_compare_exchange_32
.hidden nofault_compare_exchange_32
nofault_compare_exchange_32:
    .cfi_startproc

    insn_fixup
    stac

    // Load expected value
    mov (%rsi),%eax

    // compare eax with destination,
    //  if equal, copy source to destination and set eflags.ZF,
    //  otherwise copy destination to eax
    lock cmpxchg %edx,(%rdi)
    jnz .Lnofault_compare_exchange_32_race

    // Return 1 on success
    mov $ 1,%eax

.Lnofault_compare_exchange_32_faulted: // crash will resume here with -1 in rax
    insn_fixup
    clac

    ret

.Lnofault_compare_exchange_32_race:
    // Update caller's expected value with value we got from failed exchange
    mov %eax,(%rsi)
    xor %eax,%eax

    insn_fixup
    clac

    ret

    .cfi_endproc

.Lnofault_compare_exchange_32_end:

nofault_define_region nofault_compare_exchange_32

// Returns 0 on success
// Returns -1 if faulted
// int nofault_memset(void *dest, int value, size_t size) noexcept;
.global nofault_memset
.hidden nofault_memset
.balign 16
nofault_memset:
    .cfi_startproc

    insn_fixup
    stac

    mov %rdx,%rcx
    mov %esi,%eax
    rep stosb

    xor %eax,%eax
.Lnofault_memset_faulted:

    insn_fixup
    clac

    ret

    .cfi_endproc

.Lnofault_memset_end:

// Returns

// Returns 0 on success
// Returns -1 if faulted
// int nofault_memcpy(void *dest, void const *src, size_t size) noexcept;
.global nofault_memcpy
.hidden nofault_memcpy
.balign 16
nofault_memcpy:
    .cfi_startproc

#if 0

    insn_fixup
    stac

    mov %rdx,%rcx
    xor %eax,%eax
    rep movsb

.Lnofault_memcpy_faulted:

    insn_fixup
    clac

    ret

#else

    // To test against -64 and to adjust loop counters/pointers
    mov $ -64,%rcx

    // Becomes nop if cpu does not support SMAP
    insn_fixup
    stac

    // Handle zero size copy
    test %rdx,%rdx
    jz .Lnofault_memcpy_done

    // branch to alignment loop if not 64-bit aligned
.Lnofault_memcpy_misaligned_mainloop:
    test $ 7,%rdi
    jnz .Lmisaligned_dest

    // At this point, the destination is guaranteed to be 64-bit aligned

.Lnofault_memcpy_fast_mainloop:
    // See if >= 64 bytes remaining
    test %rcx,%rdx
    jz .Lnofault_memcpy_little_remaining

    // At least 64 bytes remaining and destination is 64 bit aligned

    // Copy 64 bytes unconditionally
    mov (%rsi),%rax
    mov %rax,(%rdi)
    mov 1*8(%rsi),%rax
    mov %rax,1*8(%rdi)
    mov 2*8(%rsi),%rax
    mov %rax,2*8(%rdi)
    mov 3*8(%rsi),%rax
    mov %rax,3*8(%rdi)
    mov 4*8(%rsi),%rax
    mov %rax,4*8(%rdi)
    mov 5*8(%rsi),%rax
    mov %rax,5*8(%rdi)
    mov 6*8(%rsi),%rax
    mov %rax,6*8(%rdi)
    mov 7*8(%rsi),%rax
    mov %rax,7*8(%rdi)

    sub %rcx,%rdi
    sub %rcx,%rsi
    add %rcx,%rdx
    jnz .Lnofault_memcpy_fast_mainloop

.Lnofault_memcpy_done:
    mov $ 1,%eax
.Lnofault_memcpy_faulted:

    // Becomes nop if cpu does not support SMAP
    insn_fixup
    clac

    ret

.balign 16
.Lmisaligned_dest:
    // If destination is not 32 bit aligned, don't bother trying 32 bit
    test $ 3,%edi
    jnz .Lmisaligned_dest_tiny

    // If there are less than 32 bits to be moved, don't do 32 bit
    cmp $ 4,%rdx
    jb .Lmisaligned_dest_tiny

    movl (%rsi),%eax
    add $ 4,%rsi
    movl %eax,(%rdi)
    add $ 4,%rdi
    add $ -4,%rdx
    jz .Lnofault_memcpy_done
    jmp .Lnofault_memcpy_misaligned_mainloop

.balign 16
.Lmisaligned_dest_tiny:
    movzbl (%rsi),%eax
    add $ 1,%rsi
    movb %al,(%rdi)
    add $ 1,%rdi
    add $ -1,%rdx
    jz .Lnofault_memcpy_done
    jmp .Lnofault_memcpy_misaligned_mainloop

.balign 16
.Lnofault_memcpy_little_remaining:
    // See if any number of 64 bit chunks are remaining
    test $ -8,%rdx
    jz .Lnofault_memcpy_little_remaining_not64

    // Copy a 64 bit chunk
    mov (%rsi),%rax
    add $ 8,%rsi
    mov %rax,(%rdi)
    add $ 8,%rdi
    add $ -8,%rdx
    jz .Lnofault_memcpy_done
    jmp .Lnofault_memcpy_little_remaining

.balign 16
.Lnofault_memcpy_little_remaining_not64:
    // See if any number of 32 bit chunks are remaining
    test $ -4,%rdx
    jz .Lnofault_memcpy_little_remaining_not32

    // Copy a 32 bit chunk
    mov (%rsi),%eax
    add $ 4,%rsi
    mov %eax,(%rdi)
    add $ 4,%rdi
    add $ -4,%rdx
    jz .Lnofault_memcpy_done
    jmp .Lnofault_memcpy_little_remaining_not64

.balign 16
.Lnofault_memcpy_little_remaining_not32:
    movzbl (%rsi),%eax
    add $ 1,%rsi
    movb %al,(%rdi)
    add $ 1,%rdi
    add $ -1,%rdx
    jz .Lnofault_memcpy_done
    jmp .Lnofault_memcpy_little_remaining_not32
#endif

.Lnofault_memcpy_end:

    .cfi_endproc

nofault_define_region nofault_memcpy

.balign 16
.hidden sys_sigreturn_impl_64
.global sys_sigreturn_impl_64
sys_sigreturn_impl_64:
    .cfi_startproc
    .cfi_signal_frame

    // Disable interrupts and point stack at context save area
    cli
    mov %rdi,%rsp

    .cfi_def_cfa rsp,21*8
    .cfi_offset ss,-1*8
    .cfi_offset rsp,-2*8
    .cfi_offset rflags,-3*8
    .cfi_offset cs,-4*8
    .cfi_offset rip,-5*8
    .cfi_offset rbp,-6*8
    .cfi_offset rbx,-7*8
    .cfi_offset r15,-8*8
    .cfi_offset r14,-9*8
    .cfi_offset r13,-10*8
    .cfi_offset r12,-11*8
    .cfi_offset r11,-12*8
    .cfi_offset r10,-13*8
    .cfi_offset rax,-14*8
    .cfi_offset r9,-15*8
    .cfi_offset r8,-16*8
    .cfi_offset rcx,-17*8
    .cfi_offset rdx,-18*8
    .cfi_offset rsi,-19*8
    .cfi_offset rdi,-20*8
    // fpu pointer here

    // Guarantee no funny changes to eflags. We're not that foolish.
    // Guarantee user code and data segments

    // Get pointer to iret frame so we can use small offsets
    leaq 16*8(%rsp),%rbx

    // Get pointer to fpu context and load fpu context
    pop_cfi %rcx

    .cfi_remember_state
    test %sil,%sil
    jz .Lold_cpu

    // Likely
    mov $ -1,%eax
    mov $ -1,%edx
    xrstor (%rcx)
.Ldone_rstor:

    pop_cfi %rdi
    .cfi_same_value rdi

    // Load eflags image
    movl 2*8(%rbx),%eax

    pop_cfi %rsi
    .cfi_same_value rsi

    // Force cs to specific value
    movq $ GDT_SEL_USER_CODE64|3,1*8(%rbx)

    pop_cfi %rdx
    .cfi_same_value rdx

    // Clear prohibited eflags bits
    andl $ ~(CPU_EFLAGS_IOPL | CPU_EFLAGS_NT | CPU_EFLAGS_VIF | \
        CPU_EFLAGS_VIP | CPU_EFLAGS_VM),%eax

    pop_cfi %rcx
    .cfi_same_value rcx

    // Force ss to specific value
    movq $ GDT_SEL_USER_DATA|3,4*8(%rbx)

    pop_cfi %r8
    .cfi_same_value r8

    // Force interrupts enabled
    orl $ CPU_EFLAGS_IF,%eax

    pop_cfi %r9
    .cfi_same_value r9

    // Update eflags in iret frame
    movl %eax,2*8(%rbx)

    pop_cfi %rax
    .cfi_same_value rax

    pop_cfi %r10
    .cfi_same_value r10

    pop_cfi %r11
    .cfi_same_value r11

    pop_cfi %r12
    .cfi_same_value r12

    pop_cfi %r13
    .cfi_same_value r13

    pop_cfi %r14
    .cfi_same_value r14

    pop_cfi %r15
    .cfi_same_value r15

    pop_cfi %rbx
    .cfi_same_value rbx

    pop_cfi %rbp
    .cfi_same_value rbp

    swapgs
    iretq

.Lold_cpu:
    .cfi_restore_state
    fxrstor (%rcx)
    jmp .Ldone_rstor

    .cfi_endproc

.balign 16
.hidden sys_sigreturn_impl_32
.global sys_sigreturn_impl_32
sys_sigreturn_impl_32:
    .cfi_startproc
    .cfi_signal_frame

    // Interleave pushing an iretq frame,
    // loading 32 bit register context,
    // and zeroing upper registers
    // Two loads and one store per cycle

    cli

    mov %rdi,%r8
    xor %r15d,%r15d
    .cfi_undefined r15
    push_cfi $ GDT_SEL_USER_DATA
    xor %r14d,%r14d
    .cfi_undefined r14

    // r9d=return_eip, r10d=return_eflags, r11d=return_esp
    movl 10*4(%r8),%r11d
    movl 9*4(%r8),%r10d
    push_cfi %r11
    xorl %r13d,%r13d
    .cfi_undefined r13

    movl 7*4(%r8),%r9d
    movl 6*4(%r8),%ebp
    .cfi_undefined rbp
    push_cfi %r10
    xorl %r12d,%r12d
    .cfi_undefined r12

    movl 5*4(%r8),%ebx
    .cfi_undefined rbx
    movl 4*4(%r8),%esi
    push_cfi $ GDT_SEL_USER_CODE32
    xorl %r11d,%r11d

    movl 3*4(%r8),%edi
    movl 2*4(%r8),%edx
    push_cfi %r9
    xorl %r10d,%r10d

    movl 1*4(%r8),%ecx
    movl (%r8),%eax

    .cfi_def_cfa rsp,5*8
    .cfi_offset ss,-1*8
    .cfi_offset rsp,-2*8
    .cfi_offset rflags,-3*8
    .cfi_offset cs,-4*8
    .cfi_offset rip,-5*8

    xorl %r9d,%r9d
    xorl %r8d,%r8d

    swapgs
    iretq

    .cfi_endproc

.global __module_unwind_resume
.balign 16
__module_unwind_resume:
    jmp _Unwind_Resume
