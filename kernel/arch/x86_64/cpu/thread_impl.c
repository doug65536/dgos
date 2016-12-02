#include "cpu/thread_impl.h"
#include "thread.h"
#include "tls.h"
#include "main.h"
#include "halt.h"
#include "idt.h"
#include "gdt.h"
#include "cpuid.h"
#include "atomic.h"
#include "mm.h"
#include "types.h"
#include "string.h"
#include "time.h"
#include "likely.h"

// Implements platform independent thread.h

typedef struct link_t link_t;
struct link_t {
    link_t *next;
    link_t *prev;
};

#define LINK_NEXT(T, curr) ((T*)((link_t*)curr->next))

typedef enum thread_state_t {
    THREAD_IS_UNINITIALIZED,
    THREAD_IS_INITIALIZING,
    THREAD_IS_SUSPENDED,
    THREAD_IS_READY,
    THREAD_IS_RUNNING,
    THREAD_IS_SLEEPING,
    THREAD_IS_DESTRUCTING,
    THREAD_IS_FINISHED
} thread_state_t;

typedef struct thread_info_t thread_info_t;
typedef struct cpu_info_t cpu_info_t;

struct thread_info_t {
    void *ctx;
    cpu_info_t *cpu;
    void *stack;
    size_t stack_size;

    uint64_t wake_time;

    // When state is equal to one of these:
    //  THREAD_IS_READY
    // any CPU can transition it to THREAD_IS_RUNNING.
    //
    // When state is equal to one of these:
    //  THREAD_IS_SLEEPING
    // any CPU can transition it to THREAD_IS_READY
    //
    // When state is equal to one of these:
    //  THREAD_IS_UNINITIALIZED
    // any CPU can transition it to THREAD_IS_INITIALIZING
    //
    // Put another way, another CPU can only change
    // the state if it is currently one of these:
    //  THREAD_IS_READY
    //  THREAD_IS_SLEEPING
    //  THREAD_IS_UNINITIALIZED
    //
    thread_state_t volatile state;

    // Higher numbers are higher priority
    int priority;
};

// Store in a big array, for now
#define MAX_THREADS 16
static thread_info_t threads[MAX_THREADS];
static size_t volatile thread_count;

struct cpu_info_t {
    thread_info_t *cur_thread;
    uint64_t apic_id;
    int online;
    thread_info_t *goto_thread;
};

#define MAX_CPUS    64
static cpu_info_t cpus[MAX_CPUS];

static volatile uint32_t cpu_count;

// Get executing APIC ID
static uint64_t get_apic_id(void)
{
    cpuid_t cpuid_info;
    cpuid(&cpuid_info, CPUID_INFO_FEATURES, 0);
    uint64_t apic_id = cpuid_info.ebx >> 24;
    return apic_id;
}

static cpu_info_t *cpu_from_apic_id(uint64_t apic_id)
{
    for (cpu_info_t *cpu = cpus; cpu < cpus + MAX_CPUS; ++cpu) {
        if (!cpu->online)
            continue;
        if (cpu->apic_id == apic_id)
            return cpu;
    }
    // Failed
    return 0;
}

static cpu_info_t *this_cpu(void)
{
    uint64_t apic_id = get_apic_id();
    return cpu_from_apic_id(apic_id);
}

static thread_info_t *this_thread(void)
{
    cpu_info_t *cpu = this_cpu();
    return cpu->cur_thread;
}

void thread_yield(void)
{
    __asm__ __volatile__ ("int $72\n\t");
}

static void thread_cleanup(void)
{
    thread_info_t *thread = this_thread();
    thread->state = THREAD_IS_DESTRUCTING;
    thread->cpu = 0;
    thread->priority = 0;
    thread->stack = 0;
    thread->stack_size = 0;
    thread->state = THREAD_IS_FINISHED;
    thread_yield();
}

// Returns threads array index or 0 on error
// Minumum allowable stack space is 4KB
static thread_t thread_create_with_state(
        thread_fn_t fn, void *userdata,
        void *stack,
        size_t stack_size,
        thread_state_t state)
{
    if (stack_size < 4096)
        return 0;

    for (size_t i = 0; ; ++i) {
        if (i >= MAX_THREADS)
            i = 0;

        thread_info_t *thread = threads + i;

        if (thread->state != THREAD_IS_UNINITIALIZED)
            continue;

        // Atomically grab the thread
        if (atomic_cmpxchg(
                    &thread->state,
                    THREAD_IS_UNINITIALIZED,
                    THREAD_IS_INITIALIZING) !=
                THREAD_IS_UNINITIALIZED) {
            pause();
            continue;
        }

        thread->stack = stack;
        thread->stack_size = stack_size;
        thread->priority = 0;

        uintptr_t stack_addr = (uintptr_t)stack;
        uintptr_t stack_end = stack_addr +
                stack_size;

        size_t tls_data_size = tls_size();
        uintptr_t thread_env_addr = stack_end -
                tls_data_size;

        // Align thread environment block
        thread_env_addr &= -16;

        thread_env_t *teb = (thread_env_t*)thread_env_addr;
        teb->self = teb;

        // Make room for TLS
        uintptr_t tls_end = thread_env_addr -
                tls_data_size;

        memcpy((void*)tls_end, tls_init_data(), tls_data_size);

        uintptr_t ctx_addr = tls_end -
                sizeof(isr_start_context_t);

        size_t misalignment = (ctx_addr +
                offsetof(isr_start_context_t, fpr))
                & 0x0F;

        ctx_addr -= misalignment;

        isr_start_context_t *ctx =
                (isr_start_context_t*)ctx_addr;
        memset(ctx, 0, sizeof(*ctx));
        ctx->ret.ret_rip = thread_cleanup;
        ctx->gpr.iret.rsp = (uint64_t)&ctx->ret;
        ctx->gpr.iret.rflags = EFLAGS_IF;
        ctx->gpr.iret.rip = fn;
        ctx->gpr.iret.cs = GDT_SEG_KERNEL_CS;
        ctx->gpr.iret.ss = GDT_SEG_KERNEL_DS;
        ctx->gpr.s[0] = GDT_SEG_KERNEL_DS;
        ctx->gpr.s[1] = GDT_SEG_KERNEL_DS;
        ctx->gpr.s[2] = GDT_SEG_KERNEL_DS;
        ctx->gpr.s[3] = GDT_SEG_KERNEL_DS;
        ctx->gpr.rdi = (uint64_t)userdata;
        ctx->gpr.fsbase = teb;

        ctx->fpr.mxcsr = MXCSR_MASK_ALL;
        ctx->fpr.mxcsr_mask = MXCSR_MASK_ALL;

        ctx->mc.gpr = &ctx->gpr;
        ctx->mc.fpr = &ctx->fpr;

        thread->ctx = ctx;

        thread->state = state;

        // Atomically make sure thread_count > i
        size_t old_count = thread_count;
        for (;;) {
            if (old_count > i)
                break;
            size_t latest_count = atomic_cmpxchg(
                        &thread_count, old_count, old_count + 1);

            if (latest_count == old_count)
                break;
            pause();
            old_count = latest_count;
        }

        return i;
    }
}

thread_t thread_create(thread_fn_t fn, void *userdata,
                       void *stack,
                       size_t stack_size)
{
    return thread_create_with_state(
                fn, userdata,
                stack, stack_size,
                THREAD_IS_READY);
}

#if 0
static void thread_monitor_mwait(void)
{
    uint64_t rax = 0;
    __asm__ __volatile__ (
        "lea timer_ms(%%rip),%%rax\n\t"
        "monitor\n\t"
        "mwait\n\t"
        : "+a" (rax)
        : "d" (0), "c" (0)
        : "memory"
    );
}
#endif

static int smp_thread(void *arg)
{
    (void)arg;
    while (1)
        halt();
    return 0;
}

void thread_init(int ap)
{
    uint32_t cpu_number = atomic_xadd_uint32(&cpu_count, 1);

    // First CPU is the BSP
    cpu_info_t *cpu = cpus + cpu_number;

    // First thread is this boot thread
    thread_info_t *thread = threads + cpu_number;

    cpu->apic_id = get_apic_id();
    cpu->online = 1;

    if (!ap) {
        cpu->cur_thread = thread;
        thread->cpu = cpu;
        thread->ctx = 0;
        thread->state = THREAD_IS_RUNNING;
        thread->priority = 0;

        thread->stack = kernel_stack;
        thread->stack_size = kernel_stack_size;
        thread_count = 1;
    } else {
        size_t stack_size = 4096;
        void *stack = mmap(
                    0, stack_size,
                    PROT_READ | PROT_WRITE,
                    MAP_STACK, -1, 0);
        thread = threads + thread_create_with_state(
                    smp_thread, 0, stack, stack_size,
                    THREAD_IS_INITIALIZING);

        cpu->goto_thread = thread;
        thread_yield();
    }
}

static thread_info_t *thread_choose_next(
        thread_info_t *thread)
{
    size_t i = thread - threads;
    thread_info_t *best = 0;
    uint64_t now = 0;

    for (size_t checked = 0; ++i, checked < thread_count; ++checked) {
        // Wrap
        if (i >= thread_count)
            i = 0;

        if (threads[i].state == THREAD_IS_SLEEPING) {
            if (now == 0)
                now = time_ms();

            if (now >= threads[i].wake_time) {
                // Race to transition it to ready
                if (atomic_cmpxchg(
                            &threads[i].state,
                            THREAD_IS_SLEEPING,
                            THREAD_IS_READY) !=
                        THREAD_IS_SLEEPING) {
                    // Another CPU beat us to it
                    continue;
                }
            }
        } else if (threads[i].state != THREAD_IS_READY)
            continue;

        if (best) {
            // Must be better than best
            if (threads[i].priority > best->priority)
                best = threads + i;
        } else {
            // Must be same or better than outgoing
            if (thread->state == THREAD_IS_SLEEPING ||
                    threads[i].priority >= thread->priority)
                best = threads + i;
        }
    }

    return best ? best : thread;
}

void *thread_schedule(void *ctx)
{
    cpu_info_t *cpu = this_cpu();
    thread_info_t *thread = cpu->cur_thread;

    if (unlikely(cpu->goto_thread)) {
        thread = cpu->goto_thread;
        thread->state = THREAD_IS_RUNNING;
        cpu->cur_thread = thread;
        cpu->goto_thread = 0;
        return thread->ctx;
    }

    // Store context pointer for resume later
    if (thread->state != THREAD_IS_DESTRUCTING) {
        thread->ctx = ctx;
    } else {
        thread->state = THREAD_IS_FINISHED;
    }

    // Change to ready if running
    if (thread->state == THREAD_IS_RUNNING) {
        thread->state = THREAD_IS_READY;

    }

    // At this point, any CPU might take thread

    // Retry because another CPU might steal this
    // thread after it transitions from sleeping to
    // ready
    for (;;) {
        thread = thread_choose_next(thread);

        if (thread->state == THREAD_IS_READY &&
                atomic_cmpxchg(&thread->state,
                           THREAD_IS_READY,
                           THREAD_IS_RUNNING) ==
                THREAD_IS_READY)
            break;
        pause();
    }

    cpu->cur_thread = thread;
    thread->cpu = cpu;

    if (1) {
        size_t cpu_number = cpu - cpus;
        uint16_t *addr = (uint16_t*)0xb8000 + 80 + 75;
        addr[cpu_number] = ((addr[cpu_number] + 1) & 0xFF) | 0x0700;
    }

    return thread->ctx;
}

void thread_sleep_until(uint64_t expiry)
{
    cpu_info_t *cpu = this_cpu();
    thread_info_t *thread = cpu->cur_thread;

    thread->wake_time = expiry;
    thread->state = THREAD_IS_SLEEPING;
    thread_yield();
}

void thread_sleep_for(uint64_t ms)
{
    thread_sleep_until(time_ms() + ms);
}
