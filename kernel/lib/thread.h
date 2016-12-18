#pragma once
#include "types.h"
#include "cpu/spinlock.h"

// Platform independent thread API

typedef int thread_t;

typedef int (*thread_fn_t)(void*);

// Implemented in arch
thread_t thread_create(thread_fn_t fn, void *userdata,
                       void *stack, size_t stack_size);

void thread_yield(void);
void thread_sleep_until(uint64_t expiry);
void thread_sleep_for(uint64_t ms);

void thread_set_affinity(int id, uint64_t affinity);
uint64_t thread_get_affinity(int id);

thread_t thread_get_id(void);

// Suspend the thread, then release the lock,
// reacquire lock before returning
void thread_suspend_release(spinlock_t *lock, thread_t *thread_id);

void thread_resume(thread_t thread);
