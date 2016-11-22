#pragma once
#include "types.h"

// Platform independent thread API

typedef int thread_t;

typedef int (*thread_fn_t)(void*);

// Implemented in arch
thread_t thread_create(thread_fn_t fn, void *userdata,
                       void *stack, size_t stack_size);

