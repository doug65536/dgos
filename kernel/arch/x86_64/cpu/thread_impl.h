#pragma once
#include "types.h"
#include "cpu/isr.h"
#include "segrw.h"
#include "cpu_info.ofs.h"
#include "thread_info.ofs.h"
#include "atomic.h"

__BEGIN_DECLS

struct process_t;

extern "C" uint32_t volatile thread_aps_running;
extern "C" uint32_t volatile thread_booting_ap_index;

struct thread_info_t;
struct cpu_info_t;

isr_context_t *thread_schedule(isr_context_t *ctx, bool was_timer = false);
KERNEL_API isr_context_t *thread_schedule_postirq(isr_context_t *ctx);
void thread_init(int ap);
cpu_info_t *thread_set_cpu_gsbase(int ap);
void thread_init_cpu_count(int count);
void thread_init_cpu(size_t cpu_nr, uint32_t apic_id);
uint32_t thread_cpus_started(void);

isr_context_t *thread_reschedule_if_requested(isr_context_t *ctx);
isr_context_t *thread_reschedule_if_requested_noirq(isr_context_t *ctx);

_pure
uint32_t thread_get_cpu_apic_id(uint32_t cpu);

_noreturn
void thread_exit(int exitcode);

void thread_clear_busy(void *outgoing);

// CPU-local storage
size_t thread_cls_alloc(void);
void *thread_cls_get(size_t slot);
void thread_cls_set(size_t slot, void *value);

typedef void *(*thread_cls_init_handler_t)(void *);
typedef void (*thread_cls_each_handler_t)(int cpu ,void *, void *, size_t);

void thread_cls_init_each_cpu(
        size_t slot, thread_cls_init_handler_t handler, void *arg);

void thread_cls_for_each_cpu(size_t slot, int other_only,
                             thread_cls_each_handler_t handler,
                             void *arg, size_t size);

void thread_send_ipi(int cpu, int intr);

void *thread_get_fsbase(int thread);
void *thread_get_gsbase(int thread);

_const
static _always_inline process_t *fast_cur_process()
{
    void *thread_info = cpu_gs_read<void*, CPU_INFO_CUR_THREAD_OFS>();
    return *(process_t**)((char*)thread_info + THREAD_INFO_PROCESS_OFS);
}

void thread_set_process(int tid, process_t *process);

extern uint32_t cpu_count;

static _always_inline int thread_cpu_count()
{
    return cpu_count;
}

void thread_add_cpu_irq_time(uint64_t tsc_ticks);

uint32_t thread_locks_held();

__END_DECLS

template<typename C>
static void *thread_cls_init_each_cpu_wrap(void *a)
{
    C &callback = *(C*)a;
    return callback();
}

template<typename C>
void thread_cls_init_each_cpu(size_t slot, int other_only, C callback)
{
    return thread_cls_init_each_cpu(
                slot, thread_cls_init_each_cpu_wrap<C>, &callback);
}
