#pragma once
#include "types.h"

typedef void (*stacktrace_xlat_fn_t)(void * const *ips, size_t count);

stacktrace_xlat_fn_t perf_set_stacktrace_xlat_fn(stacktrace_xlat_fn_t fn);
void perf_stacktrace_xlat(void * const *ips, size_t count);

void perf_init();

uint64_t perf_gather_samples(
        void (*callback)(int, int, char const *, void *), void *arg);

void perf_set_event(uint32_t event, uint8_t event_scale);
