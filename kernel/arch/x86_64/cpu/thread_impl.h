#pragma once
#include "types.h"
#include "cpu/idt.h"
#include "segrw.h"
#include "asm_constants.h"

struct process_t;

extern uint32_t volatile thread_smp_running;

isr_context_t *thread_schedule(isr_context_t *ctx);
isr_context_t *thread_schedule_if_idle(isr_context_t *ctx);
void thread_init(int ap);
uint32_t thread_cpu_count(void);
uint32_t thread_cpus_started(void);
uint32_t thread_get_cpu_apic_id(int cpu);

// CPU-local storage
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
                             thread_cls_each_handler_t handler,
                             void *arg, size_t size);

void thread_send_ipi(int cpu, int intr);

uintptr_t thread_get_fsbase(int thread);
uintptr_t thread_get_gsbase(int thread);

__const
static __always_inline process_t *fast_cur_process()
{
    void *thread_info = cpu_gs_read_ptr<CPU_INFO_CURTHREAD_OFS>();
    return *(process_t**)((char*)thread_info + THREAD_PROCESS_PTR_OFS);
}

void thread_set_process(int thread, process_t *process);
