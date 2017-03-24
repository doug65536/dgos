#pragma once
#include "types.h"

extern uint32_t volatile thread_smp_running;

void *thread_schedule(void *ctx);
void thread_init(int ap);
uint32_t thread_cpu_count(void);
uint32_t thread_cpus_started(void);
int thread_cpu_number(void);

size_t thread_cls_alloc(void);
void *thread_cls_get(size_t slot);
void thread_cls_set(size_t slot, void *value);

uint64_t thread_get_cpu_mmu_seq(void);
void thread_set_cpu_mmu_seq(uint64_t seq);

void thread_check_stack(void);

typedef void *(*thread_cls_init_handler_t)(void *);
typedef void (*thread_cls_each_handler_t)(int cpu ,void *, void *, size_t);

void thread_cls_init_each_cpu(
        size_t slot, thread_cls_init_handler_t handler, void *arg);

void thread_cls_for_each_cpu(size_t slot, int other_only,
                             thread_cls_each_handler_t handler, void *arg, size_t size);

void thread_send_ipi(int cpu, int intr);
