#pragma once
#include "types.h"

__BEGIN_DECLS

typedef void (*stacktrace_xlat_fn_t)(
        void *arg, void * const *ips, size_t count);

KERNEL_API void perf_set_stacktrace_xlat_fn(stacktrace_xlat_fn_t fn, void *arg);
KERNEL_API void perf_stacktrace_xlat(void * const *ips, size_t count);

KERNEL_API void perf_init();

KERNEL_API uint64_t perf_gather_samples(
        void (*callback)(void *arg, int percent, int micropercent,
                         char const *file, int line, char const *function),
        void *arg);

KERNEL_API uint32_t perf_set_event(uint32_t event);
KERNEL_API bool perf_set_invert(bool invert);
KERNEL_API bool perf_set_edge(bool edge);
KERNEL_API uint32_t perf_set_event_mask(uint32_t event_mask);
KERNEL_API uint32_t perf_set_count_mask(uint32_t count_mask);
KERNEL_API uint64_t perf_set_divisor(uint64_t event_divisor);
KERNEL_API uint64_t perf_set_all(uint64_t value);

KERNEL_API uint32_t perf_get_event();
KERNEL_API bool perf_get_invert();
KERNEL_API bool perf_get_edge();
KERNEL_API uint32_t perf_get_unit_mask();
KERNEL_API uint32_t perf_get_count_mask();
KERNEL_API uint64_t perf_get_divisor();
KERNEL_API uint64_t perf_get_all();

KERNEL_API uint64_t perf_adj_divisor(int64_t adjustment);

KERNEL_API void perf_stacktrace_decoded();
KERNEL_API void perf_set_zeroing(bool zeroing_enabled);
KERNEL_API bool perf_get_zeroing();

__END_DECLS
