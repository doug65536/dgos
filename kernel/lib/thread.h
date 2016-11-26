#pragma once
#include "types.h"

// Platform independent thread API

typedef int thread_t;

typedef int (*thread_fn_t)(void*);

// Implemented in arch
thread_t thread_create(thread_fn_t fn, void *userdata,
                       void *stack, size_t stack_size);


void thread_yield(void);
void thread_sleep_until(uint64_t expiry);
void thread_sleep_for(uint64_t ms);

