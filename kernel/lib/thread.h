#pragma once
#include "types.h"

typedef int thread_t;

typedef int (*thread_fn_t)(void);

thread_t thread_create(thread_fn_t fn, void *context, size_t stack_size);
