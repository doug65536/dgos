#pragma once

#include "types.h"

typedef struct __siginfo_t siginfo_t;
typedef struct __stack_t stack_t;
typedef struct __ucontext_t ucontext_t;

typedef uint64_t sigset_t;

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
};

struct sigaction32 {
    uint32_t sa_handler;
    uint32_t sa_mask;
    uint32_t sa_flags;
    uint32_t sa_sigaction;
};

int sys_sigaction(int signum, sigaction const *user_act, sigaction *user_oldact);
