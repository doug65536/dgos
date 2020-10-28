#include <sys/cpu_bug.h>
#include "sys/syscall_num.h"

#if defined(__x86_64__)

.macro pop_cfi val:vararg
    popq \val
    .cfi_adjust_cfa_offset -8
.endm

.macro adj_rsp_cfi	ofs:vararg
    lea (\ofs)(%rsp),%rsp
    .cfi_adjust_cfa_offset -(\ofs)
.endm

//void signal_restorer(int sig, siginfo_t *,
//                     void (*handler)(int, siginfo_t*, void*));
// The kernel signal handler places a ucontext_t on the stack
// at the first 16-byte boundary below the 128-byte red zone,
// and points rsp at the beginning of it
// edi is int sig
// rsi is siginfo *
// rdx points to the signal handler code
.global __signal_restorer
__signal_restorer:
    .cfi_startproc
    .cfi_signal_frame

    // Kernel put mcontext at rsp
    .cfi_def_cfa rsp,8*21
    .cfi_offset ss,8*-1
    .cfi_offset rsp,8*-2
    .cfi_offset rflags,8*-3
    .cfi_offset cs,8*-4
    .cfi_offset rip,8*-5
    .cfi_offset rbp,8*-6
    .cfi_offset rbx,8*-7
    .cfi_offset r15,8*-8
    .cfi_offset r14,8*-9
    .cfi_offset r13,8*-10
    .cfi_offset r12,8*-11
    .cfi_offset r11,8*-12
    .cfi_offset r10,8*-13
    .cfi_offset rax,8*-14
    .cfi_offset r9,8*-15
    .cfi_offset r8,8*-16
    .cfi_offset rcx,8*-17
    .cfi_offset rdx,8*-18
    .cfi_offset rsi,8*-19
    .cfi_offset rdi,8*-20
    // fpu context pointer here

    // Hold signal handler in rbx so we can continue to see it when debugging
    mov %rdx,%rbx

    // Pass ucontext_t parameter to signal handler
    movq %rsp,%rdx

    // Call signal handler (3rd parameter)
    lfence
    indirect_call rbx

    // Call signal return syscall
    mov %rsp,%rdi
    mov $ SYS_sigreturn,%eax
    syscall

    // Make syscall return address still inside this function
    nop

    // Unreachable

    .cfi_endproc

#elif defined(__i386__)

.macro pop_cfi val:vararg
    popl \val
    .cfi_adjust_cfa_offset -4
.endm

.macro adj_esp_cfi	ofs:vararg
    lea (\ofs)(%esp),%esp
    .cfi_adjust_cfa_offset -(\ofs)
.endm

// rsp points to 16-byte aligned mcontext32_t
// eax is int sig
// edx is 16-byte aligned siginfo *
// ecx points to the signal handler code
.global __signal_restorer
__signal_restorer:
    .cfi_startproc
    .cfi_signal_frame
    .cfi_def_cfa esp,4*12
    .cfi_offset ss,4*-1
    .cfi_offset esp,4*-2
    .cfi_offset eflags,4*-3
    .cfi_offset cs,4*-4
    .cfi_offset eip,4*-5
    .cfi_offset ebp,4*-6
    .cfi_offset ebx,4*-7
    .cfi_offset edi,4*-8
    .cfi_offset esi,4*-9
    .cfi_offset ecx,4*-10
    .cfi_offset edx,4*-11
    .cfi_offset eax,4*-12

    // Pass (int sig) (siginfo *) (context *) parameters to signal handler
    adj_esp_cfi -16

    // Call signal handler with sig, info, and mctx arguments
    movl %eax,(%esp)
    movl %edx,4(%esp)
    leal 16(%esp),%eax
    movl %eax,8(%esp)
    lfence
    indirect_call ecx

    // Restore the ucontext_t context (pass as first parameter)
    mov %esp,(%esp)
    call __sigreturn

    // Make syscall return address still inside this function
    nop

    // Unreachable

    .cfi_endproc
#endif
