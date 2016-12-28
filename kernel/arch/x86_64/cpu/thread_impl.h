#pragma once
#include "types.h"

extern uint32_t volatile thread_smp_running;

void *thread_schedule(void *ctx);
void thread_init(int ap);
uint32_t thread_cpu_count(void);
uint32_t thread_cpus_started(void);

uint64_t thread_get_cpu_mmu_seq(void);
void thread_set_cpu_mmu_seq(uint64_t seq);

void thread_check_stack(void);
