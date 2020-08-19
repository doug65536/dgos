#pragma once
#include "types.h"

__BEGIN_DECLS

typedef void (*stacktrace_xlat_fn_t)(
        void *arg, void * const *ips, size_t count);

void perf_set_stacktrace_xlat_fn(stacktrace_xlat_fn_t fn, void *arg);
void perf_stacktrace_xlat(void * const *ips, size_t count);

void perf_init();

uint64_t perf_gather_samples(
        void (*callback)(void *, int, int, char const *), void *arg);

uint32_t perf_set_event(uint32_t event);
bool perf_set_invert(bool invert);
bool perf_set_edge(bool edge);
uint32_t perf_set_event_mask(uint32_t event_mask);
uint32_t perf_set_count_mask(uint32_t count_mask);
uint64_t perf_set_divisor(uint64_t event_divisor);
uint64_t perf_set_all(uint64_t value);

uint32_t perf_get_event();
bool perf_get_invert();
bool perf_get_edge();
uint32_t perf_get_unit_mask();
uint32_t perf_get_count_mask();
uint64_t perf_get_divisor();
uint64_t perf_get_all();

uint64_t perf_adj_divisor(int64_t adjustment);

void perf_stacktrace_decoded();
void perf_set_zeroing(bool zeroing_enabled);
bool perf_get_zeroing();

__END_DECLS
