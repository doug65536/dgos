#pragma once

#include "types.h"
#include "thread.h"
#include "mutex.h"
#include "cpu_info.h"
#include "cxxexcept.h"

struct alignas(256) thread_info_t {
    // Exclusive line, frequently modified items, rarely shared

    isr_context_t * volatile ctx;

    // Modified every context switch and/or syscall
    void * volatile fsbase;
    void * volatile gsbase;
    uint32_t syscall_mxcsr, syscall_fcw87;
    char * volatile fpusave_ptr;

    // Modified every context switch
    uint64_t used_time;

    // When used_time >= preempt_time, get a new timestamp
    uint64_t preempt_time;

    uint8_t reserved[8];

    // --- cache line --- shared line

    // 2 64-bit pointers
    uint64_t reserved3[2];

    // Owning process
    process_t *process;

    char *xsave_stack;

    // Points to place in stack to use when an interrupt occurs at lower priv
    char *priv_chg_stack;

    thread_t thread_id;

    // 1 until closed, then 0
    int ref_count;

    thread_state_t volatile state;

    // Higher numbers are higher priority
    thread_priority_t priority;
    uint8_t reserved5[3];

    uint64_t volatile wake_time;

    // --- cache line ---

    // 4 64-bit values
    using lock_type = ext::irq_spinlock;   // 32 bytes
    using scoped_lock = ext::unique_lock<lock_type>;
    lock_type lock;

    // 3 64-bit values
    ext::condition_variable done_cond;

    // Current CPU exception longjmp container
    void *exception_chain;

    // --- cache line ---

    // Thread current kernel errno
    errno_t errno;
    // 3 bytes...

    // Doesn't include guard pages
    uint32_t stack_size;

    uint32_t thread_flags;

    uint32_t reserved4;

    // Process exit code
    intptr_t exit_code;

    // 2 64-bit values
    __cxa_eh_globals cxx_exception_info;

    // Timestamp at moment thread was resumed
    uint64_t sched_timestamp;

    // Threads that got their timeslice sooner are ones that have slept,
    // so they are implicitly higher priority
    // When their timeslice is used up, they get a new one timestamped now
    // losing their privileged status allowing other threads to have a turn
    uint64_t timeslice_timestamp;

    // Each time a thread context switches, time is removed from this value
    // When it reaches zero, it is replenished, and timeslice_timestamp
    // is set to now. Preemption is set up so the timer will fire when this
    // time elapses (or the earliest timer expiry, whichever is earlier).
    uint64_t timeslice_remaining;

    // --- cache line ---

    char *stack;

    thread_cpu_mask_t cpu_affinity;

    char const *name;

    //bool closed;

    // Iterator points to the node we inserted into
    // scheduler list, otherwise default constructed
    ready_set_t::const_iterator schedule_node;
};
