#pragma once

#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>

__BEGIN_DECLS

struct siginfo_t;

// The <signal.h> header shall define the following macros,
// which shall expand to constant expressions with distinct values
// that have a type compatible with the second argument to, and
// the return value of, the signal() function, and whose values
// shall compare unequal to the address of any declarable function.

// Request for default signal handling.
#define SIG_DFL     0

// Return value from signal() in case of error.
#define SIG_ERR     ((void(*)(int))-1)

//  Request that signal be held.
#define SIG_HOLD    2

// Request that signal be ignored.
#define SIG_IGN     1

// The <signal.h> header shall define the
// pthread_t, size_t, and uid_t types as described in <sys/types.h>.

// The <signal.h> header shall define the timespec structure as
// described in <time.h>.

// The <signal.h> header shall define the following data types:

#ifndef __SIG_ATOMIC_TYPE__
#define __SIG_ATOMIC_TYPE__ long
#endif

// Possibly volatile-qualified integer type of an object that can be
// accessed as an atomic entity, even in the presence of
// asynchronous interrupts.
typedef __SIG_ATOMIC_TYPE__ sig_atomic_t;

// Integer or structure type of an object used to represent sets of signals.
typedef uint64_t sigset_t;

// The <signal.h> header shall define the pthread_attr_t type as
// described in <sys/types.h>.
struct pthread_attr_t;

union sigval {
    int    sival_int;
    void  *sival_ptr;
};

// The <signal.h> header shall define the sigevent structure,
// which shall include at least the following members:

typedef struct sigevent {
    // Notification type.
    int sigev_notify;

    // Signal number.
    int sigev_signo;

    // Signal value.
    union sigval sigev_value;

    // Notification function.
    void (*sigev_notify_function)(union sigval);
} sigevent;

// pthread_attr_t *sigev_notify_attributes  Notification attributes.

// The <signal.h> header shall define the following symbolic constants
//  for the values of sigev_notify:

// No asynchronous notification is delivered when the event of interest occurs.
#define SIGEV_NONE      1

// A queued signal, with an application-defined value, is generated when
// the event of interest occurs.
#define SIGEV_SIGNAL    0

// A notification function is called to perform notification.
#define SIGEV_THREAD    2

// The <signal.h> header shall declare the SIGRTMIN and SIGRTMAX macros,
// which shall expand to positive integer expressions with type int, but which
// need not be constant expressions. These macros specify a range of
// signal numbers that are reserved for application use and for which the
// realtime signal behavior specified in this volume of POSIX.1-2008 is
// supported. The signal numbers in this range do not overlap any of the
// signals specified in the following table.

// The range SIGRTMIN through SIGRTMAX inclusive shall include
// at least {RTSIG_MAX} signal numbers.

// It is implementation-defined whether realtime signal behavior is
// supported for other signals.

// The <signal.h> header shall define the following macros that are used
// to refer to the signals that occur in the system. Signals defined here
// begin with the letters SIG followed by an uppercase letter. The macros
// shall expand to positive integer constant expressions with type int and
// distinct values. The value 0 is reserved for use as the null signal
// (see kill()). Additional implementation-defined signals may occur
// in the system.

// The ISO C standard only requires the signal names
// SIGABRT, SIGFPE, SIGILL, SIGINT, SIGSEGV, and SIGTERM
// to be defined. An implementation need not generate any of these six
// signals, except as a result of explicit use of interfaces that generate
// signals, such as raise(), kill(), the General Terminal Interface
// (see Special Characters), and the kill utility, unless otherwise
// stated (see, for example, XSH Memory Protection).

// The following signals shall be supported on all implementations
// (default actions are explained below the table):

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

// I/O possible
#define SIGPOLL     23

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

// The <signal.h> header shall declare the sigaction structure,
// which shall include at least the following members:

typedef struct __stack_t stack_t;

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

    void (*sa_restorer)(int sig, siginfo_t *,
                        void (*)(int, siginfo_t, void*));
};

// The storage occupied by sa_handler and sa_sigaction may overlap, and
// a conforming application shall not use both simultaneously.

// The <signal.h> header shall define the following macros which shall
// expand to integer constant expressions that need not be usable in
// #if preprocessing directives:

// The resulting set is the union of the current set and the signal set
// pointed to by the argument set.
#define SIG_BLOCK       0

// The resulting set is the intersection of the current set and the
// complement of the signal set pointed to by the argument set.
#define SIG_UNBLOCK     1

// The resulting set is the signal set pointed to by the argument set.
#define SIG_SETMASK     2

// The <signal.h> header shall also define the following symbolic constants:

// Do not generate SIGCHLD when children stop or stopped children continue.
#define SA_NOCLDSTOP    1

// Causes signal delivery to occur on an alternate stack.
#define SA_ONSTACK      0x8000000

// Causes signal dispositions to be set to SIG_DFL on entry to signal handlers.
#define SA_RESETHAND    0x80000000

// Causes certain functions to become restartable.
#define SA_RESTART      0x10000000

// Causes extra information to be passed to signal handlers at the time
// of receipt of a signal.
#define SA_SIGINFO      4

// Causes implementations not to create zombie processes or status
// information on child termination. See sigaction.
#define SA_NOCLDWAIT    2

// Causes signal not to be automatically blocked on entry to signal handler.
#define SA_NODEFER      0x40000000

// Process is executing on an alternate signal stack.
#define SS_ONSTACK      1

// Alternate signal stack is disabled.
#define SS_DISABLE      2

// Minimum stack size for a signal handler.
#define MINSIGSTKSZ     0x800

// Default size in bytes for the alternate signal stack.
#define SIGSTKSZ        0x2000

// The <signal.h> header shall define the mcontext_t type through typedef.

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

struct __fpxreg {
    uint16_t __significand[4];
    uint16_t __exponent[1];
    uint16_t __unused[3];
};

struct __xmmreg {
    uint32_t __word[4];
};

struct mcontext_x86_fpu_t {
    // Match fxsave format
    // 32+128+256+96

    // 32 byte header
    uint16_t __cwd, __swd, __ftw, __fop;
    uint64_t __rip;
    uint64_t __rdp;
    uint32_t __mxcsr, __mxcsr_mask;

    // 8 16-byte x87 registers (128 bytes)
    __fpxreg __st[8];

    // 16 16-byte sse registers (256 bytes)
    __xmmreg __xmm[16];

    // Reserved 96 bytes
    uint32_t __reserved1[24];
};

static_assert(sizeof(mcontext_x86_fpu_t) == 512, "Expected 512-byte structure");

// The <signal.h> header shall define the stack_t type as a structure,
// which shall include at least the following members:

struct __stack_t {
    // Stack base or pointer.
    void     *ss_sp;

    // Stack size.
    size_t    ss_size;
    // Flags.
    int       ss_flags;
};

// The <signal.h> header shall define the ucontext_t type as a structure
// that shall include at least the following members:

struct ucontext_t {
    // Pointer to the context that is resumed when this context returns.
    ucontext_t *uc_link;
    // The set of signals that are blocked when this context is active.
    sigset_t uc_sigmask;
    // The stack used by this context.
    stack_t uc_stack;
    // A machine-specific representation of the saved context.
    mcontext_t uc_mcontext;
};

// The <signal.h> header shall define the siginfo_t type as a structure,
// which shall include at least the following members:

typedef struct siginfo_t {
    // Signal number.
    int           si_signo;
    // Signal code.
    int           si_code;


    // If non-zero, an errno value associated with this signal, as
    // described in <errno.h>.
    int           si_errno;


    // Sending process ID.
    pid_t         si_pid;
    // Real user ID of sending process.
    uid_t         si_uid;
    // Address of faulting instruction.
    void         *si_addr;
    // Exit value or signal.
    int           si_status;


    // Band event for SIGPOLL.
    long          si_band;


    // Signal value.
    union sigval  si_value;
} siginfo_t;

// The <signal.h> header shall define the symbolic constants in the
// Code column of the following table for use as values of si_code that
// are signal-specific or non-signal-specific reasons why the
// signal was generated.

// Illegal opcode.
// SIGILL
#define ILL_ILLOPC      1

// Illegal operand.
#define ILL_ILLOPN      2

// Illegal addressing mode.
#define ILL_ILLADR      3

// Illegal trap.
#define ILL_ILLTRP      4

// Privileged opcode.
#define ILL_PRVOPC      5

// Privileged register.
#define ILL_PRVREG      6

// Coprocessor error.
#define ILL_COPROC      7

// Internal stack error.
#define ILL_BADSTK      8

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


// Address not mapped to object.
// SIGSEGV
#define SEGV_MAPERR     1

// Invalid permissions for mapped object.
#define SEGV_ACCERR     2

// Invalid address alignment.
// SIGBUS
#define BUS_ADRALN      1

// Nonexistent physical address.
#define BUS_ADRERR      2

// Object-specific hardware error.
#define BUS_OBJERR      3

// Process breakpoint.
// SIGTRAP
#define TRAP_BRKPT      1

// Process trace trap.
#define TRAP_TRACE      2

// Child has exited.
// SIGCHLD
#define CLD_EXITED      1

// Child has terminated abnormally and did not create a core file.
#define CLD_KILLED      2

// Child has terminated abnormally and created a core file.
#define CLD_DUMPED      3

// Traced child has trapped.
#define CLD_TRAPPED     4

// Child has stopped.
#define CLD_STOPPED     5

// Stopped child has continued.
#define CLD_CONTINUED   6

// Data input available.
// SIGPOLL
#define POLL_IN     1

// Output buffers available.
#define POLL_OUT    2

// Input message available.
#define POLL_MSG    3

// I/O error.
#define POLL_ERR    4

// High priority input available.
#define POLL_PRI    5

// Device disconnected.
#define POLL_HUP    6

// Signal sent by kill().
#define SI_USER     0

// Signal sent by sigqueue().
#define SI_QUEUE    -1

// Signal generated by expiration of a timer set by timer_settime().
#define SI_TIMER    -2

// Signal generated by completion of an asynchronous I/O request.
#define SI_ASYNCIO  -4

// Signal generated by arrival of a message on an empty message queue.
#define SI_MESGQ    -3

// Implementations may support additional si_code values not included in
// this list, may generate values included in this list under circumstances
// other than those described in this list, and may contain extensions
// or limitations that prevent some values from being generated.
// Implementations do not generate a different value from the ones
// described in this list for circumstances described in this list.
// In addition, the following signal-specific information shall be available:

// Signal
//
// Member
//
// Value
//
// SIGILL
// SIGFPE
//
// Address of faulting instruction.
// void * si_addr
//
//
// SIGSEGV
// SIGBUS
//
// Address of faulting memory reference.
// void * si_addr
//
//
// SIGCHLD
//
// Child process ID.
// pid_t si_pid
//
//
//
//
// If si_code is equal to CLD_EXITED, then si_status holds the exit value of
// the process; otherwise, it is equal to the signal that caused the process
// to change state. The exit value in si_status shall be equal to the full
// exit value (that is, the value passed to _exit(), _Exit(), or exit(),
// or returned from main()); it shall not be limited to the least
// significant eight bits of the value.
// int si_status
//
//
//
//
// uid_t si_uid
//
// Real user ID of the process that sent the signal.
//
// SIGPOLL
//
// long si_band

// Band event for POLL_IN, POLL_OUT, or POLL_MSG.

// For some implementations, the value of si_addr may be inaccurate.

// The following shall be declared as functions and may also be defined
// as macros. Function prototypes shall be provided.


int kill(pid_t, int);


int killpg(pid_t, int);


void psiginfo(siginfo_t const *, char const *);
void psignal(int, char const *);
int pthread_kill(pthread_t, int);
int pthread_sigmask(int, sigset_t const *restrict,
           sigset_t *restrict);

int raise(int);

int sigaction(int, struct sigaction const * restrict action,
           struct sigaction * restrict old_action);
int sigaddset(sigset_t *, int);


int sigaltstack(stack_t const *restrict, stack_t *restrict);



// clear the signal set
int sigemptyset(sigset_t *);

// remove the signal from the set
int sigdelset(sigset_t *, int);

// fill the set
int sigfillset(sigset_t *);

// Check whether signal is member of set
int sigismember(sigset_t const *, int);


int sighold(int);
int sigignore(int);
int siginterrupt(int, int);



void (*signal(int, void (*)(int)))(int);

int sigpause(int);


int sigpending(sigset_t *);
int sigprocmask(int, sigset_t const *restrict, sigset_t *restrict);
int sigqueue(pid_t, int, union sigval);


int sigrelset(int);
void (*sigset(int, void (*)(int)))(int);


int sigsuspend(sigset_t const *);
int sigtimedwait(sigset_t const *restrict, siginfo_t *restrict,
           const struct timespec *restrict);
int sigwait(sigset_t const *restrict, int *restrict);
int sigwaitinfo(sigset_t const *restrict, siginfo_t *restrict);

/// Each signal has a current disposition
/// Term  Terminate process
/// Ign   Ignore
/// Core  Dump core
/// Stop  Stop the process
/// Cont  Continue the process if it is stopped
///
/// By default, uses the normal process stack, another
/// stack may be used for signal handling by calling
/// sigaltstack
///
/// Signal disposition is a per-process attribute. The disposition
/// of a signal is the same for all threads
///
/// Children created by fork inherit a copy of the parent's
/// signal dispositions. Processes executed with execve have
/// their signal dispositions reset to default.
///
/// Sending signals
///
/// raise  Send a signal to the calling thread
/// kill   Send a signal to the specified pid or all pids
/// killpg Send a signal to all processes in a process group
/// pthread_kill Send a signal to a specific pthread
/// tgkill Send a system to a specific thread in a specific process
/// sigqueue Send a realtime signal with data to a specific process
///
///

__END_DECLS
