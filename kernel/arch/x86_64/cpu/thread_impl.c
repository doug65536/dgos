#include "cpu/thread_impl.h"
#include "types.h"
#include "interrupts.h"
#include "atomic.h"
#include "string.h"
#include "assert.h"
#include "likely.h"

#include "thread.h"
#include "control_regs.h"
#include "tls.h"
#include "main.h"
#include "halt.h"
#include "idt.h"
#include "gdt.h"
#include "cpuid.h"
#include "mm.h"
#include "time.h"

#include "printk.h"

// Implements platform independent thread.h

typedef enum thread_state_t {
    THREAD_IS_UNINITIALIZED = 0,
    THREAD_IS_INITIALIZING,
    THREAD_IS_SUSPENDED,
    THREAD_IS_READY,
    THREAD_IS_RUNNING,
    THREAD_IS_SLEEPING,
    THREAD_IS_DESTRUCTING,
    THREAD_IS_FINISHED,

    // Flag keeps other cpus from taking thread
    // until after stack switch
    THREAD_BUSY = (int)0x80000000,
    THREAD_IS_SUSPENDED_BUSY = THREAD_IS_SUSPENDED | THREAD_BUSY,
    THREAD_IS_READY_BUSY = THREAD_IS_READY | THREAD_BUSY,
    THREAD_IS_FINISHED_BUSY = THREAD_IS_FINISHED | THREAD_BUSY,
    THREAD_IS_SLEEPING_BUSY = THREAD_IS_SLEEPING | THREAD_BUSY,
    THREAD_IS_DESTRUCTING_BUSY = THREAD_IS_DESTRUCTING | THREAD_BUSY
} thread_state_t;

typedef struct thread_info_t thread_info_t;
typedef struct cpu_info_t cpu_info_t;

struct thread_info_t {
    isr_context_t * volatile ctx;
    uint64_t volatile wake_time;

    uint64_t cpu_affinity;

    void *stack;
    size_t stack_size;

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
    thread_priority_t volatile priority;
    thread_priority_t volatile priority_boost;

    uint64_t flags;

    uint64_t align;
};

#define THREAD_FLAG_OWNEDSTACK_BIT  1
#define THREAD_FLAG_OWNEDSTACK      (1<<THREAD_FLAG_OWNEDSTACK_BIT)

C_ASSERT(sizeof(thread_info_t) == 64);

// Store in a big array, for now
#define MAX_THREADS 64
static thread_info_t threads[MAX_THREADS];
static size_t volatile thread_count;
uint32_t volatile thread_smp_running;

struct cpu_info_t {
    cpu_info_t *self;
    thread_info_t * volatile cur_thread;
    uint64_t apic_id;
    int online;
    thread_info_t *goto_thread;

    // Used for lazy TLB shootdown
    uint64_t mmu_seq;
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

static cpu_info_t *this_cpu(void)
{
    cpu_info_t *cpu = cpu_gs_read_ptr();
    assert(cpu >= cpus && cpu < cpus + countof(cpus));
    return cpu;

    //uint64_t apic_id = get_apic_id();
    //return cpu_from_apic_id(apic_id);
}

static thread_info_t *this_thread(void)
{
    cpu_info_t *cpu = this_cpu();
    return cpu->cur_thread;
}

void thread_yield(void)
{
#if 1
    __asm__ __volatile__ (
        "movq %%rsp,%%rax\n\t"
        "pushq $0x10\n\t"
        "push %%rax\n\t"
        "pushfq\n\t"
        "cli\n\t"
        "push $0x8\n\t"
        "call isr_entry_%c[yield]\n\t"
        :
        : [yield] "i" (INTR_THREAD_YIELD)
        : "%rax"
    );
#else
    __asm__ __volatile__ (
        "int %[yield_intr]\n\t"
        :
        : [yield_intr] "i" (INTR_THREAD_YIELD)
    );
#endif
}

static void thread_cleanup(void)
{
    thread_info_t *thread = this_thread();

    assert(thread->state == THREAD_IS_RUNNING);

    atomic_barrier();
    thread->priority = 0;
    thread->priority_boost = 0;
    thread->stack = 0;
    thread->stack_size = 0;
    atomic_barrier();
    thread->state = THREAD_IS_DESTRUCTING_BUSY;
    thread_yield();
}

// Returns threads array index or 0 on error
// Minimum allowable stack space is 4KB
static thread_t thread_create_with_state(
        thread_fn_t fn, void *userdata,
        void *stack, size_t stack_size,
        thread_state_t state,
        uint64_t affinity,
        thread_priority_t priority)
{
    if (stack_size == 0)
        stack_size = 16384;
    else if (stack_size < 16384)
        return -1;

    for (size_t i = 0; ; ++i) {
        if (i >= MAX_THREADS) {
            thread_yield();
            i = 0;
        }

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

        thread->flags = 0;

        if (!stack) {
            stack = mmap(0, stack_size,
                         PROT_READ | PROT_WRITE,
                         MAP_STACK, -1, 0);
            thread->flags |= THREAD_FLAG_OWNEDSTACK;
        }

        thread->stack = stack;
        thread->stack_size = stack_size;
        thread->priority = priority;
        thread->priority_boost = 0;
        thread->cpu_affinity = affinity ? affinity : ~0UL;

        uintptr_t stack_addr = (uintptr_t)stack;
        uintptr_t stack_end = stack_addr +
                stack_size;

        size_t tls_data_size = tls_size();
        uintptr_t thread_env_addr = stack_end -
                sizeof(thread_env_t);

        // Align thread environment block
        thread_env_addr &= -16;

        thread_env_t *teb = (thread_env_t*)thread_env_addr;
        teb->self = teb;

        // Make room for TLS
        uintptr_t tls_end = thread_env_addr - tls_data_size;

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
        ctx->gpr.iret.ss = GDT_SEL_KERNEL_DATA64;
        ctx->gpr.iret.rflags = EFLAGS_IF;
        ctx->gpr.iret.rip = fn;
        ctx->gpr.iret.cs = GDT_SEL_KERNEL_CODE64;
        ctx->gpr.s[0] = GDT_SEL_KERNEL_DATA64;
        ctx->gpr.s[1] = GDT_SEL_KERNEL_DATA64;
        ctx->gpr.s[2] = GDT_SEL_KERNEL_DATA64;
        ctx->gpr.s[3] = GDT_SEL_KERNEL_DATA64;
        ctx->gpr.r[0] = (uint64_t)userdata;
        ctx->gpr.fsbase = teb;

        ctx->fpr.mxcsr = MXCSR_MASK_ALL;
        ctx->fpr.mxcsr_mask = MXCSR_MASK_ALL;

        ctx->ctx.gpr = &ctx->gpr;
        ctx->ctx.fpr = &ctx->fpr;

        thread->ctx = &ctx->ctx;

        ptrdiff_t free_stack_space = (char*)thread->ctx -
                (char*)thread->stack;

        //printk("New thread free stack space: %zd\n", free_stack_space);

        assert(free_stack_space > 4096);

        atomic_barrier();
        thread->state = state;

        // Atomically make sure thread_count > i
        atomic_max(&thread_count, i + 1);

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
                THREAD_IS_READY, 0, 0);
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
    printk("SMP thread running\n");
    atomic_inc_uint32(&thread_smp_running);
    (void)arg;
    thread_check_stack();
    while (1)
        halt();
    return 0;
}

void thread_init(int ap)
{
    uint32_t cpu_number = atomic_xadd_uint32(&cpu_count, 1);

    // First CPU is the BSP
    cpu_info_t *cpu = cpus + cpu_number;

    assert(thread_count == cpu_number);

    // First thread is this boot thread
    thread_info_t *thread = threads + cpu_number;

    cpu->self = cpu;
    cpu->apic_id = get_apic_id();
    cpu->online = 1;

    cpu_set_gsbase(cpu);

    if (!ap) {
        cpu->cur_thread = thread;
        thread->ctx = 0;
        thread->priority = -256;
        thread->stack = kernel_stack;
        thread->stack_size = kernel_stack_size;
        thread->cpu_affinity = 1;
        atomic_barrier();
        thread->state = THREAD_IS_RUNNING;
        thread_count = 1;
    } else {
        thread = threads + thread_create_with_state(
                    smp_thread, 0, 0, 0,
                    THREAD_IS_INITIALIZING,
                    1 << cpu_number,
                    -256);

        cpu->goto_thread = thread;
        atomic_barrier();
        thread_yield();
    }
}

static thread_info_t *thread_choose_next(
        thread_info_t * const outgoing)
{
    cpu_info_t *cpu = this_cpu();
    size_t cpu_number = cpu - cpus;
    size_t i = outgoing - threads;
    thread_info_t *best = 0;
    thread_info_t *candidate;
    uint64_t now = 0;

    assert(i < countof(threads));

    size_t count = thread_count;

    for (size_t checked = 0; ++i, checked <= count; ++checked) {
        // Wrap
        if (i >= count)
            i = 0;

        candidate = threads + i;

        // If this thread is not allowed to run on this CPU
        // then skip it
        if (!(candidate->cpu_affinity & (1 << cpu_number)))
            continue;

        //
        // Expect states to have busy bit set if it is the outgoing thread

        thread_state_t expected_sleep = (outgoing != threads + i)
                ? THREAD_IS_SLEEPING
                : THREAD_IS_SLEEPING_BUSY;

        thread_state_t expected_ready = (outgoing != threads + i)
                ? THREAD_IS_READY
                : THREAD_IS_READY_BUSY;

        if (candidate->state == expected_sleep) {
            // The thread is sleeping, see if it should wake up yet

            // If we didn't get current time yet, get it
            if (now == 0)
                now = time_ms();

            if (now < candidate->wake_time)
                continue;

            // Race to transition it to ready
            if (atomic_cmpxchg(
                        &candidate->state,
                        expected_sleep,
                        expected_ready) !=
                    expected_sleep) {
                // Another CPU beat us to it
                continue;
            }
        } else if (candidate->state != expected_ready)
            continue;

        if (best) {
            // Must be better than best
            if (candidate->priority + candidate->priority_boost >
                    best->priority + best->priority_boost)
                best = candidate;
        } else {
            best = candidate;
        }
    }

    assert(best
           ? best >= threads && best <= threads + countof(threads)
           : outgoing >= threads && outgoing <= threads + countof(threads));

    return best ? best : outgoing;
}

static void thread_clear_busy(void *outgoing)
{
    thread_info_t *thread = outgoing;
    atomic_and_int32(&thread->state, ~THREAD_BUSY);
}

void *thread_schedule(void *ctx)
{
    cpu_info_t *cpu = this_cpu();
    thread_info_t *thread = cpu->cur_thread;

    thread_info_t *outgoing = thread;

    if (unlikely(cpu->goto_thread)) {
        thread = cpu->goto_thread;
        cpu->cur_thread = thread;
        cpu->goto_thread = 0;
        atomic_barrier();
        thread->state = THREAD_IS_RUNNING;
        return thread->ctx;
    }

    // Store context pointer for resume later
    thread->ctx = ctx;

    // Change to ready if running
    if (thread->state == THREAD_IS_RUNNING) {
        //thread->cpu = 0;
        atomic_barrier();
        thread->state = THREAD_IS_READY_BUSY;
        atomic_barrier();
    } else if (thread->state == THREAD_IS_DESTRUCTING) {
        if (thread->flags & THREAD_FLAG_OWNEDSTACK)
            munmap(thread->stack, thread->stack_size);
        thread->state = THREAD_IS_FINISHED;
    }

    // Retry because another CPU might steal this
    // thread after it transitions from sleeping to
    // ready
    int retries = 0;
    for ( ; ; ++retries) {
        thread = thread_choose_next(outgoing);

        assert(thread >= threads &&
               thread < threads + countof(threads));

        if (thread == outgoing &&
                thread->state == THREAD_IS_READY_BUSY) {
            atomic_barrier();
            thread->state = THREAD_IS_RUNNING;
            break;
        } else if (thread->state == THREAD_IS_READY &&
                atomic_cmpxchg(&thread->state,
                           THREAD_IS_READY,
                           THREAD_IS_RUNNING) ==
                THREAD_IS_READY) {
            break;
        }
        pause();
    }
    if (retries > 0)
        printdbg("Scheduler retries: %d\n", retries);
    atomic_barrier();

    // Thread loses its priority boost when it is scheduled
    thread->priority_boost = 0;

    assert(thread->state == THREAD_IS_RUNNING);

    ctx = thread->ctx;
    cpu->cur_thread = thread;

    if (1) {
        size_t cpu_number = cpu - cpus;
        uint16_t *addr = (uint16_t*)0xb8000 + 80 + 70;
        addr[cpu_number] = ((addr[cpu_number] + 1) & 0xFF) | 0x0700;
    }

    isr_context_t *isrctx = (isr_context_t*)ctx;

    assert(isrctx->gpr->iret.rsp >= (uintptr_t)thread->stack);
    assert(isrctx->gpr->iret.rsp <
           (uintptr_t)thread->stack + thread->stack_size);

    if (thread != outgoing) {
        // Add outgoing cleanup data at top of context
        isr_resume_context_t *cleanup = &isrctx->resume;

        cleanup->cleanup = thread_clear_busy;
        cleanup->cleanup_arg = outgoing;
    } else {
        assert(thread->state == THREAD_IS_RUNNING);
    }

    return ctx;
}

void thread_sleep_until(uint64_t expiry)
{
    cpu_info_t *cpu = this_cpu();
    thread_info_t *thread = cpu->cur_thread;

    thread->wake_time = expiry;
    atomic_barrier();
    thread->state = THREAD_IS_SLEEPING_BUSY;
    thread->priority_boost = 100;
    thread_yield();
}

void thread_sleep_for(uint64_t ms)
{
    thread_sleep_until(time_ms() + ms);
}

void thread_suspend_release(spinlock_t *lock, thread_t *thread_id)
{
    cpu_info_t *cpu = this_cpu();
    thread_info_t *thread = cpu->cur_thread;

    *thread_id = thread - threads;
    atomic_barrier();

    thread->state = THREAD_IS_SUSPENDED_BUSY;
    atomic_barrier();
    spinlock_unlock(lock);
    thread_yield();
    assert(thread->state == THREAD_IS_RUNNING);
    spinlock_lock(lock);
}

void thread_resume(thread_t thread)
{
    // Wait for it to reach suspended state in case of race
    int wait_count = 0;
    for ( ; threads[thread].state != THREAD_IS_SUSPENDED;
          ++wait_count)
        pause();

    if (wait_count > 0) {
        printdbg("Resuming thread %d with old state %x, waited %d\n",
                 thread, threads[thread].state, wait_count);
    }

    threads[thread].priority_boost = 128;
    threads[thread].state = THREAD_IS_READY;
}

uint32_t thread_cpu_count(void)
{
    return cpu_count;
}

uint32_t thread_cpus_started(void)
{
    return thread_smp_running + 1;
}

uint64_t thread_get_cpu_mmu_seq(void)
{
    cpu_info_t *cpu = this_cpu();
    return cpu->mmu_seq;
}

void thread_set_cpu_mmu_seq(uint64_t seq)
{
    cpu_info_t *cpu = this_cpu();
    cpu->mmu_seq = seq;
}

thread_t thread_get_id(void)
{
    thread_t thread_id;

    int was_enabled = cpu_irq_disable();

    cpu_info_t *cpu = this_cpu();
    thread_id = cpu->cur_thread - threads;

    cpu_irq_toggle(was_enabled);

    return thread_id;
}

uint64_t thread_get_affinity(int id)
{
    return threads[id].cpu_affinity;
}

void thread_set_affinity(int id, uint64_t affinity)
{
    cpu_info_t *cpu = this_cpu();
    size_t cpu_number = cpu - cpus;

    threads[id].cpu_affinity = affinity;

    // Are we changing current thread affinity?
    if (cpu->cur_thread == threads + id &&
            !(affinity & (1 << cpu_number))) {
        // Get off this CPU
        thread_yield();
    }
}

thread_priority_t thread_get_priority(thread_t thread_id)
{
    return threads[thread_id].priority;
}

void thread_set_priority(thread_t thread_id, thread_priority_t priority)
{
    threads[thread_id].priority = priority;
}

void thread_check_stack(void)
{
    thread_info_t *thread = this_thread();

    void *sp = cpu_get_stack_ptr();

    if (sp < thread->stack ||
            (char*)sp > (char*)thread->stack + thread->stack_size)
        cpu_crash();
}
