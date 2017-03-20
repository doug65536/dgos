#include "cpu/thread_impl.h"
#include "types.h"
#include "interrupts.h"
#include "atomic.h"
#include "string.h"
#include "assert.h"
#include "likely.h"

#include "thread.h"
#include "control_regs.h"
#include "main.h"
#include "halt.h"
#include "idt.h"
#include "irq.h"
#include "gdt.h"
#include "cpuid.h"
#include "mm.h"
#include "time.h"
#include "threadsync.h"
#include "export.h"
#include "printk.h"
#include "isr_constants.h"
#include "priorityqueue.h"

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

struct thread_info_t {
    isr_context_t * volatile ctx;

    void *xsave_ptr;
    void *xsave_stack;

    void *syscall_stack;

    // Higher numbers are higher priority
    thread_priority_t volatile priority;
    thread_priority_t volatile priority_boost;
    thread_state_t volatile state;

    uint32_t flags;
    uint32_t stack_size;

    uint64_t volatile wake_time;

    uint64_t cpu_affinity;

    void *exception_chain;

    void *stack;

    int exit_code;

    mutex_t lock;
    condition_var_t done_cond;

    size_t queue_slot;
    uint64_t next_run;

    void *align[4];
};

#define THREAD_FLAG_OWNEDSTACK_BIT  1
#define THREAD_FLAG_OWNEDSTACK      (1<<THREAD_FLAG_OWNEDSTACK_BIT)

C_ASSERT(offsetof(thread_info_t, xsave_ptr) == THREAD_XSAVE_PTR_OFS);
C_ASSERT(offsetof(thread_info_t, xsave_stack) == THREAD_XSAVE_STACK_OFS);

C_ASSERT(sizeof(thread_info_t) == 64*3);

// Store in a big array, for now
#define MAX_THREADS 512
static thread_info_t threads[MAX_THREADS] __attribute__((aligned(64)));
static size_t volatile thread_count;
uint32_t volatile thread_smp_running;
int thread_idle_ready;
int spincount_mask;

struct cpu_info_t {
    cpu_info_t *self;
    thread_info_t * volatile cur_thread;
    uint32_t apic_id;
    int online;
    thread_info_t *goto_thread;

    // Used for lazy TLB shootdown
    uint64_t mmu_seq;

    priqueue_t *queue;
    spinlock_t queue_lock;

    void *align[1];
};

C_ASSERT(offsetof(cpu_info_t, cur_thread) == CPU_INFO_CURTHREAD_OFS);
C_ASSERT_ISPO2(sizeof(cpu_info_t));

#define MAX_CPUS    64
static cpu_info_t cpus[MAX_CPUS] __attribute__((aligned(64)));

static volatile uint32_t cpu_count;

// Set in startup code (entry.s)
uint32_t default_mxcsr_mask;

// Get executing APIC ID
static uint32_t get_apic_id(void)
{
    cpuid_t cpuid_info;
    cpuid_nocache(&cpuid_info, CPUID_INFO_FEATURES, 0);
    uint32_t apic_id = cpuid_info.ebx >> 24;
    return apic_id;
}

static inline cpu_info_t *this_cpu(void)
{
    cpu_info_t *cpu = cpu_gs_read_ptr();
    assert(cpu >= cpus && cpu < cpus + countof(cpus));
    assert(cpu->self == cpu);
    return cpu;
}

static inline thread_info_t *this_thread(void)
{
    cpu_info_t *cpu = this_cpu();
    return cpu->cur_thread;
}

EXPORT void thread_yield(void)
{
#if 1
    __asm__ __volatile__ (
        "mfence\n\t"
        "movq %%rsp,%%rax\n\t"
        "pushq $0x10\n\t"
        "push %%rax\n\t"
        "pushfq\n\t"
        "cli\n\t"
        "push $0x8\n\t"
        "call isr_entry_%c[yield]\n\t"
        :
        : [yield] "i" (INTR_THREAD_YIELD)
        : "%rax", "memory"
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

    cpu_irq_disable();

    assert(thread->state == THREAD_IS_RUNNING);

    atomic_barrier();
    thread->priority = 0;
    thread->priority_boost = 0;
    thread->state = THREAD_IS_DESTRUCTING_BUSY;
    thread_yield();
}

static void thread_startup(thread_fn_t fn, void *p, thread_t id)
{
    threads[id].exit_code = fn(p);
    thread_cleanup();
}

// Returns threads array index or 0 on error
// Minimum allowable stack space is 4KB
static thread_t thread_create_with_state(
        thread_fn_t fn, void *userdata,
        void *stack, size_t stack_size,
        thread_state_t state,
        uint64_t affinity,
        thread_priority_t priority,
        void *fpu_context)
{
    if (stack_size == 0)
        stack_size = 16384;
    else if (stack_size < 16384)
        return -1;

    for (size_t i = 0; ; ++i) {
        if (unlikely(i >= MAX_THREADS)) {
            printdbg("Out of threads, yielding\n");
            thread_yield();
            i = 0;
        }

        thread_info_t *thread = threads + i;

        if (thread->state != THREAD_IS_UNINITIALIZED)
            continue;

        // Atomically grab the thread
        if (unlikely(atomic_cmpxchg(
                         &thread->state,
                         THREAD_IS_UNINITIALIZED,
                         THREAD_IS_INITIALIZING) !=
                     THREAD_IS_UNINITIALIZED)) {
            pause();
            continue;
        }

        atomic_barrier();

        thread->flags = 0;

        if (!stack) {
            stack = mmap(0, stack_size,
                         PROT_READ | PROT_WRITE,
                         MAP_STACK, -1, 0);
            thread->flags |= THREAD_FLAG_OWNEDSTACK;
        }

        // Syscall stack
        thread->syscall_stack = (char*)mmap(
                    0, 1 << 14, PROT_READ | PROT_WRITE,
                    MAP_STACK, -1, 0) + (1 << 14);

        // XSave stack
        thread->xsave_stack = mmap(
                    0, 1 << 14, PROT_READ | PROT_WRITE,
                    MAP_STACK, -1, 0);
        thread->xsave_ptr = thread->xsave_stack;

        thread->stack = stack;
        thread->stack_size = stack_size;
        thread->priority = priority;
        thread->priority_boost = 0;
        thread->cpu_affinity = affinity ? affinity : ~0UL;

        uintptr_t stack_addr = (uintptr_t)stack;
        uintptr_t stack_end = stack_addr +
                stack_size;

        size_t ctx_size = sizeof(isr_gpr_context_t) +
                sizeof(isr_context_t);

        // Adjust start of context to make room for context
        uintptr_t ctx_addr = stack_end - ctx_size - 16;

        assert((ctx_addr & 0x0F) == 0);

        isr_context_t *ctx = (isr_context_t*)ctx_addr;
        memset(ctx, 0, ctx_size);
        ctx->fpr = thread->xsave_ptr;
        ctx->gpr = (void*)(ctx + 1);

        ctx->gpr->iret.rsp = (uintptr_t)
                (((ctx_addr + ctx_size + 15) & -16) + 8);
        ctx->gpr->iret.ss = GDT_SEL_KERNEL_DATA64;
        ctx->gpr->iret.rflags = EFLAGS_IF;
        ctx->gpr->iret.rip = (thread_fn_t)(uintptr_t)thread_startup;
        ctx->gpr->iret.cs = GDT_SEL_KERNEL_CODE64;
        ctx->gpr->s[0] = GDT_SEL_KERNEL_DATA64;
        ctx->gpr->s[1] = GDT_SEL_KERNEL_DATA64;
        ctx->gpr->s[2] = GDT_SEL_KERNEL_DATA64;
        ctx->gpr->s[3] = GDT_SEL_KERNEL_DATA64;
        ctx->gpr->r[0] = (uintptr_t)fn;
        ctx->gpr->r[1] = (uintptr_t)userdata;
        ctx->gpr->r[2] = (uintptr_t)i;
        ctx->gpr->fsbase = 0;

        ctx->fpr->mxcsr = MXCSR_MASK_ALL;
        ctx->fpr->mxcsr_mask = default_mxcsr_mask;

        if ((uintptr_t)fpu_context == 1) {
            // All FPU registers empty
            ctx->fpr->fsw = FPUSW_TOP_n(7);

            // 64 bit FPU precision
            ctx->fpr->fcw = FPUCW_PC_n(FPUCW_PC_64) | FPUCW_IM |
                    FPUCW_DM | FPUCW_ZM | FPUCW_OM | FPUCW_UM | FPUCW_PM;
        } else if (sse_context_size == 512) {
            cpu_fxsave(ctx->fpr);
        } else {
            assert(sse_context_size > 512);
            cpu_xsave(ctx->fpr);
        }

        thread->ctx = ctx;

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

EXPORT thread_t thread_create(thread_fn_t fn, void *userdata,
                       void *stack,
                       size_t stack_size)
{
    return thread_create_with_state(
                fn, userdata,
                stack, stack_size,
                THREAD_IS_READY, 0, 0, 0);
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
    // Enable spinning
    spincount_mask = -1;

    printdbg("SMP thread running\n");
    atomic_inc(&thread_smp_running);
    (void)arg;
    thread_check_stack();
    while (1)
        halt();
    return 0;
}

static int thread_priority_cmp(uintptr_t a, uintptr_t b, void *ctx)
{
    (void)ctx;
    thread_info_t *ap = threads + a;
    thread_info_t *bp = threads + b;
    return ((ap->next_run > bp->next_run) << 1) - 1;
}

static void thread_priority_swapped(uintptr_t a, uintptr_t b, void *ctx)
{
    (void)ctx;
    uintptr_t tmp = threads[a].queue_slot;
    threads[a].queue_slot = threads[b].queue_slot;
    threads[b].queue_slot = tmp;
}

void thread_init(int ap)
{
    uint32_t cpu_number = atomic_xadd(&cpu_count, 1);

    if (cpu_number > 0)
        gdt_load_tr(cpu_number);

    // First CPU is the BSP
    cpu_info_t *cpu = cpus + cpu_number;

    assert(thread_count == cpu_number);

    // First thread is this boot thread
    thread_info_t *thread = threads + cpu_number;

    cpu->self = cpu;
    cpu->apic_id = get_apic_id();
    cpu->online = 1;

    cpu_set_gs(GDT_SEL_KERNEL_DATA64);
    cpu_set_gsbase(cpu);

    cpu->queue = priqueue_create(
                0, thread_priority_cmp, thread_priority_swapped, cpu);

    if (!ap) {
        cpu->cur_thread = thread;
        thread->ctx = 0;
        thread->priority = -256;
        thread->stack = kernel_stack;
        thread->stack_size = kernel_stack_size;
        thread->xsave_stack = mmap(0, 1 << 14, PROT_READ | PROT_WRITE,
                                   MAP_STACK, -1, 0);
        thread->xsave_ptr = thread->xsave_stack;
        thread->cpu_affinity = 1;
        atomic_barrier();
        thread->state = THREAD_IS_RUNNING;
        thread_count = 1;
    } else {
        thread = threads + thread_create_with_state(
                    smp_thread, 0, 0, 0,
                    THREAD_IS_INITIALIZING,
                    1 << cpu_number,
                    -256, (void*)1);

        cpu->goto_thread = thread;

        if (sse_context_size == 512)
            cpu_fxsave(thread->xsave_ptr);
        else
            cpu_xsave(thread->xsave_ptr);

        atomic_barrier();
        thread_yield();
    }
}

static thread_info_t *thread_choose_next(
        cpu_info_t *cpu,
        thread_info_t * const outgoing)
{
    size_t cpu_number = cpu - cpus;
    size_t i = outgoing - threads;
    thread_info_t *best = 0;
    thread_info_t *candidate;
    uint64_t now = 0;

    assert(i < countof(threads));

    size_t count = thread_count;

    for (size_t checked = 0; ++i, checked <= count; ++checked) {
        // Wrap
        if (unlikely(i >= count))
            i = 0;

        candidate = threads + i;

        // If this thread is not allowed to run on this CPU
        // then skip it
        if (unlikely(!(candidate->cpu_affinity & (1 << cpu_number))))
            continue;

        //
        // Expect states to have busy bit set if it is the outgoing thread

        thread_state_t expected_sleep = (outgoing != threads + i)
                ? THREAD_IS_SLEEPING
                : THREAD_IS_SLEEPING_BUSY;

        thread_state_t expected_ready = (outgoing != threads + i)
                ? THREAD_IS_READY
                : THREAD_IS_READY_BUSY;

        if (unlikely(candidate->state == expected_sleep)) {
            // The thread is sleeping, see if it should wake up yet

            // If we didn't get current time yet, get it
            if (now == 0)
                now = time_ms();

            if (now < candidate->wake_time)
                continue;

            // Race to transition it to ready
            if (unlikely(atomic_cmpxchg(
                             &candidate->state,
                             expected_sleep,
                             expected_ready) !=
                         expected_sleep)) {
                // Another CPU beat us to it
                continue;
            }
        } else if (unlikely(candidate->state != expected_ready))
            continue;

        if (likely(best)) {
            // Must be better than best
            if (candidate->priority + candidate->priority_boost >
                    best->priority + best->priority_boost)
                best = candidate;
        } else if (outgoing->state == THREAD_IS_READY_BUSY) {
            // Must be at least the same priority as outgoing
            if (candidate->priority + candidate->priority_boost >=
                    outgoing->priority + outgoing->priority_boost)
                best = candidate;
        } else {
            // Outgoing thread is not ready, any thread is better
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
    atomic_and(&thread->state, ~THREAD_BUSY);
}

void *thread_schedule(void *ctx)
{
    cpu_info_t *cpu = this_cpu();
    thread_info_t *thread = cpu->cur_thread;

    thread_info_t * const outgoing = thread;

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
    if (likely(thread->state == THREAD_IS_RUNNING)) {
        atomic_barrier();
        thread->state = THREAD_IS_READY_BUSY;
        atomic_barrier();
    } else if (unlikely(thread->state == THREAD_IS_DESTRUCTING)) {
        if (thread->flags & THREAD_FLAG_OWNEDSTACK)
            munmap(thread->stack, thread->stack_size);

        munmap(thread->xsave_stack, 1 << 14);

        mutex_lock_noyield(&thread->lock);
        thread->state = THREAD_IS_FINISHED;
        mutex_unlock(&thread->lock);
        condvar_wake_all(&thread->done_cond);
    }

    // Retry because another CPU might steal this
    // thread after it transitions from sleeping to
    // ready
    int retries = 0;
    for ( ; ; ++retries) {
        thread = thread_choose_next(cpu, outgoing);

        assert(thread >= threads &&
               thread < threads + countof(threads));

        if (thread == outgoing &&
                thread->state == THREAD_IS_READY_BUSY) {
            // This doesn't need to be atomic because the
            // outgoing thread is still marked busy
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
    if (unlikely(retries > 0))
        printdbg("Scheduler retries: %d\n", retries);
    atomic_barrier();

    // Thread loses its priority boost when it is scheduled
    thread->priority_boost = 0;

    assert(thread->state == THREAD_IS_RUNNING);

    ctx = thread->ctx;
    cpu->cur_thread = thread;

    isr_context_t *isrctx = (isr_context_t*)ctx;

    assert(isrctx->gpr->iret.cs == 0x8);
    assert(isrctx->gpr->iret.ss == 0x10);
    assert(isrctx->gpr->s[0] == 0x10);
    assert(isrctx->gpr->s[1] == 0x10);
    assert(isrctx->gpr->s[2] == 0x10);
    assert(isrctx->gpr->s[3] == 0x10);

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

static void thread_early_sleep(uint64_t expiry)
{
    while (time_ms() < expiry)
        halt();
}

EXPORT void thread_sleep_until(uint64_t expiry)
{
    if (thread_idle_ready) {
        cpu_info_t *cpu = this_cpu();
        thread_info_t *thread = cpu->cur_thread;

        thread->wake_time = expiry;
        atomic_barrier();
        thread->state = THREAD_IS_SLEEPING_BUSY;
        //thread->priority_boost = 100;
        thread_yield();
    } else {
        thread_early_sleep(expiry);
    }
}

EXPORT void thread_sleep_for(uint64_t ms)
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

EXPORT void thread_resume(thread_t thread)
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

    //threads[thread].priority_boost = 128;
    threads[thread].state = THREAD_IS_READY;
}

EXPORT int thread_wait(thread_t thread_id)
{
    thread_info_t *thread = threads + thread_id;
    mutex_lock(&thread->lock);
    while (thread->state != THREAD_IS_FINISHED)
        condvar_wait(&thread->done_cond, &thread->lock);
    mutex_unlock(&thread->lock);
    return thread->exit_code;
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

EXPORT thread_t thread_get_id(void)
{
    if (thread_count) {
        thread_t thread_id;

        int was_enabled = cpu_irq_disable();

        cpu_info_t *cpu = this_cpu();
        thread_id = cpu->cur_thread - threads;

        cpu_irq_toggle(was_enabled);

        return thread_id;
    }

    // Too early to get a thread ID
    return 0;
}

EXPORT uint64_t thread_get_affinity(int id)
{
    return threads[id].cpu_affinity;
}

EXPORT void thread_set_affinity(int id, uint64_t affinity)
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

EXPORT thread_priority_t thread_get_priority(thread_t thread_id)
{
    return threads[thread_id].priority;
}

EXPORT void thread_set_priority(thread_t thread_id,
                                thread_priority_t priority)
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

void thread_idle_set_ready(void)
{
    thread_idle_ready = 1;
}

void *thread_get_exception_top(void)
{
    thread_info_t *thread = this_thread();
    return thread->exception_chain;
}

void *thread_set_exception_top(void *chain)
{
    thread_info_t *thread = this_thread();
    void *old = thread->exception_chain;
    thread->exception_chain = chain;
    return old;
}
