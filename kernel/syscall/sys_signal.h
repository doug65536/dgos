#pragma once

#include "types.h"
#include "assert.h"

union sigval {
    int sival_int;
    void *sival_ptr;
};

struct siginfo_t {
    // Signal number.
    int si_signo;

    // Signal code.
    int si_code;

    // If non-zero, an errno value associated with this signal, as
    // described in <errno.h>.
    int si_errno;


    // Sending process ID.
    pid_t si_pid;

    // Real user ID of sending process.
    uid_t si_uid;

    // Address of faulting instruction.
    void *si_addr;

    // Exit value or signal.
    int si_status;

    // Band event for SIGPOLL.
    long si_band;

    // Signal value.
    union sigval si_value;
};

typedef struct __stack_t stack_t;

typedef uint64_t sigset_t;

struct mcontext_x86_fptreg {
    // "long double" 80-bit register

    // 64-bit mantissa
    uint16_t __significand[4];

    // 16-bit exponent
    uint16_t __exponent[1];

    // Padding to 16-byte alignment
    uint16_t __unused[3];
};

struct mcontext_x86_xmmreg_t {
    uint32_t __word[4];
};

struct mcontext_x86_fpu_t {
    // Match fxsave format
    // 32+128+256+96

    // 32 byte header
    uint16_t __cwd, __swd, __ftw, __fop;
    uint64_t __rip;
    uint64_t __rdp;
    uint32_t __mxcsr, __mxcr_mask;

    // 8 16-byte x87 registers (128 bytes)
    mcontext_x86_fptreg __st[8];

    // 16 16-byte sse registers (256 bytes)
    mcontext_x86_xmmreg_t __xmm[16];

    // Reserved 96 bytes
    uint32_t __reserved1[24];
};

enum reg_index_t {
    // Parameter registers
    R_RDI,
    R_RSI,
    R_RDX,
    R_RCX,
    R_R8,
    R_R9,
    // Call clobbered registers
    R_RAX,
    R_R10,
    R_R11,
    // Call preserved registers
    R_R12,
    R_R13,
    R_R14,
    R_R15,
    R_RBX,
    R_RBP,

    R_REGS
};

enum reg_index32_t {
    R_EAX,
    R_EDX,
    R_ECX,
    R_ESI,
    R_EDI,
    R_EBX,
    R_EBP,

    R_REGS32
};

// Context copied into the user stack for the signal trampoline
// 20*8 (16*10) bytes
struct mcontext_t {
    // FPU
    uint64_t __fpu;

    // General registers
    uint64_t __regs[R_REGS];

    // iret frame
    uint64_t __rip;
    uint64_t __cs;
    uint64_t __rflags;
    uint64_t __rsp;
    uint64_t __ss;
};

C_ASSERT(sizeof(mcontext_t) == 21 * 8);

// 12*4=48 (16*3) bytes
struct mcontext32_t {
    // FPU
    uint32_t __fpu;

    // General registers
    uint32_t __regs[R_REGS32];

    // iret frame
    uint32_t __eip;
    uint32_t __cs;
    uint32_t __eflags;
    uint32_t __esp;
    uint32_t __ss;
};

C_ASSERT(sizeof(mcontext32_t) == 13 * 4);

struct sigaction {
    // Pointer to a signal-catching function or one of the
    // SIG_IGN or SIG_DFL.
    void (*sa_handler)(int);

    // Set of signals to be blocked during execution
    // of the signal handling function.
    sigset_t sa_mask;

    // Special flags.
    int sa_flags;

    // Pointer to a signal-catching function.
    void (*sa_sigaction)(int, siginfo_t *, void *);

    // Pointer to signal trampoline
    void (*sa_restorer)(int, siginfo_t *, void (*)(int, siginfo_t *, void *));
};

struct sigaction32 {
    uint32_t sa_handler;
    uint32_t sa_mask;
    uint32_t sa_flags;
    uint32_t sa_sigaction;
    uint32_t sa_restorer;
};

int sys_sigaction(int signum, sigaction const *user_act,
                  sigaction *user_oldact, void *restorer);

int sys_sigreturn(void *mctx);

// Hangup.
#define SIGHUP      1

// Terminal interrupt signal.
#define SIGINT      2

// Terminal quit signal.
#define SIGQUIT     3

// Illegal instruction.
#define SIGILL      4

// Trace/breakpoint trap.
#define SIGTRAP     5

// Process abort signal.
#define SIGABRT     6

// Emulator trap
#define SIGEMT      7

// Erroneous arithmetic operation.
#define SIGFPE      8

// Kill (cannot be caught or ignored).
#define SIGKILL     9

// Access to an undefined portion of a memory object.
#define SIGBUS      10

// Invalid memory reference.
#define SIGSEGV     11

// Bad system call.
#define SIGSYS      12

// Write on a pipe with no one to read it.
#define SIGPIPE     13

// Alarm clock.
#define SIGALRM     14

// Termination signal.
#define SIGTERM     15

// High bandwidth data is available at a socket.
#define SIGURG      16

// Stop executing (cannot be caught or ignored).
#define SIGSTOP     17

// Terminal stop signal.
#define SIGTSTP     18

// Continue executing, if stopped.
#define SIGCONT     19

// Child process terminated, stopped, or continued.
#define SIGCHLD     20

// Background process attempting read.
#define SIGTTIN     21

// Background process attempting write.
#define SIGTTOU     22

//// I/O possible
//#define SIGPOLL     23

// CPU time limit exceeded.
#define SIGXCPU     24

// File size limit exceeded.
#define SIGXFSZ     25

// Virtual timer expired.
#define SIGVTALRM   26

// Profiling timer expired.
#define SIGPROF     27

// Window changed
#define SIGWINCH    28

// Pollable event.
#define SIGPOLL     29

// User-defined signal 1.
#define SIGUSR1     30

// User-defined signal 2.
#define SIGUSR2     31

//
// sig_info code

// Integer divide by zero.
// SIGFPE
#define FPE_INTDIV      1

// Integer overflow.
#define FPE_INTOVF      2

// Floating-point divide by zero.
#define FPE_FLTDIV      3

// Floating-point overflow.
#define FPE_FLTOVF      4

// Floating-point underflow.
#define FPE_FLTUND      5

// Floating-point inexact result.
#define FPE_FLTRES      6

// Invalid floating-point operation.
#define FPE_FLTINV      7

// Subscript out of range.
#define FPE_FLTSUB      8
