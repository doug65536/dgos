#pragma once
#include "types.h"

typedef void (*stacktrace_cb_t)(uintptr_t rbp, uintptr_t rip);
size_t stacktrace(void **addresses, size_t max_frames);
void stacktrace(stacktrace_cb_t cb);
