#pragma once
#include "types.h"

void perf_init();

uint64_t perf_gather_samples(
        void (*callback)(int, int, char const *, void *), void *arg);

void perf_set_event(uint32_t event, uint8_t event_scale);
