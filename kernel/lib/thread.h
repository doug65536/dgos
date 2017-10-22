#pragma once
#include "types.h"
#include "cpu/spinlock.h"
#include "errno.h"

struct process_t;

// Platform independent thread API

typedef int thread_t;

typedef int16_t thread_priority_t;

typedef int (*thread_fn_t)(void*);

// Holds 0 if single cpu, otherwise holds -1
// This allows branchless setting of spincounts to zero
extern int spincount_mask;

// Implemented in arch
thread_t thread_create(thread_fn_t fn, void *userdata,
                       void *stack, size_t stack_size);

void thread_yield(void);
void thread_sleep_until(uint64_t expiry);
void thread_sleep_for(uint64_t ms);

void thread_set_affinity(int id, uint64_t affinity);
uint64_t thread_get_affinity(int id);

size_t thread_get_cpu_count();
int thread_cpu_number();

thread_t thread_get_id(void);

// Suspend the thread, then release the lock,
// reacquire lock before returning
void thread_suspend_release(spinlock_t *lock, thread_t *thread_id);

void thread_resume(thread_t thread);

thread_priority_t thread_get_priority(thread_t thread_id);
void thread_set_priority(thread_t thread_id, thread_priority_t priority);

int thread_wait(thread_t thread_id);

void thread_idle_set_ready(void);

void *thread_get_exception_top(void);
void *thread_set_exception_top(void *chain);

process_t *thread_current_process();

// Get the TLB shootdown counter for the specified CPU
uint64_t thread_shootdown_count(int cpu_nr);

// Increment the TLB shootdown counter for the current CPU
void thread_shootdown_notify();

void thread_set_error(errno_t errno);
errno_t thread_get_error();
