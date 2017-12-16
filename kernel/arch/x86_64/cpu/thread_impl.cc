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
#include "asm_constants.h"
#include "apic.h"
#include "unique_ptr.h"
#include "rbtree.h"
#include "process.h"

// Implements platform independent thread.h

#define DEBUG_THREAD    1
#if DEBUG_THREAD
#define THREAD_TRACE(...) printdbg("thread: " __VA_ARGS__)
#else
#define THREAD_TRACE(...) ((void)0)
#endif

enum thread_state_t : uint32_t {
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
    THREAD_BUSY = 0x80000000U,
    THREAD_IS_SUSPENDED_BUSY = THREAD_IS_SUSPENDED | THREAD_BUSY,
    THREAD_IS_READY_BUSY = THREAD_IS_READY | THREAD_BUSY,
    THREAD_IS_FINISHED_BUSY = THREAD_IS_FINISHED | THREAD_BUSY,
    THREAD_IS_SLEEPING_BUSY = THREAD_IS_SLEEPING | THREAD_BUSY,
    THREAD_IS_DESTRUCTING_BUSY = THREAD_IS_DESTRUCTING | THREAD_BUSY
};

struct thread_info_t;
struct cpu_info_t;

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

    uintptr_t fsbase;
    uintptr_t gsbase;

    void *syscall_stack;

    process_t *process;

    // Higher numbers are higher priority
    thread_priority_t volatile priority;
    thread_priority_t volatile priority_boost;
    thread_state_t volatile state;

    // --- cache line ---

    uint32_t flags;
    uint32_t stack_size;

    uint64_t volatile wake_time;

    uint64_t volatile cpu_affinity;

    void *exception_chain;

    void *stack;

    int exit_code;
    errno_t errno;

    // 3 bytes...

    mutex_t lock;
    condition_var_t done_cond;

    // Total cpu time used
    uint64_t used_time;

    // Timestamp at moment thread was resumed
    uint64_t sched_timestamp;

    void *align[1];
};

C_ASSERT(offsetof(thread_info_t, flags) == 64);

// Verify asm_constants.h values
C_ASSERT(offsetof(thread_info_t, process) == THREAD_PROCESS_PTR_OFS);
C_ASSERT(offsetof(thread_info_t, syscall_stack) == THREAD_SYSCALL_STACK_OFS);
C_ASSERT(offsetof(thread_info_t, xsave_ptr) == THREAD_XSAVE_PTR_OFS);
C_ASSERT(offsetof(thread_info_t, xsave_stack) == THREAD_XSAVE_STACK_OFS);
C_ASSERT(offsetof(thread_info_t, fsbase) == THREAD_FSBASE_OFS);
C_ASSERT(offsetof(thread_info_t, gsbase) == THREAD_GSBASE_OFS);

#define THREAD_FLAG_OWNEDSTACK_BIT  1
#define THREAD_FLAG_OWNEDSTACK      (1<<THREAD_FLAG_OWNEDSTACK_BIT)

// Verify that thread_info_t is a multiple of the cache line size
C_ASSERT((sizeof(thread_info_t) & 63) == 0);

// Store in a big array, for now
#define MAX_THREADS 512
static thread_info_t threads[MAX_THREADS] __aligned(64);
static size_t volatile thread_count;
uint32_t volatile thread_smp_running;
int thread_idle_ready;
int spincount_mask;
size_t storage_next_slot;

static size_t constexpr syscall_stack_size = (size_t(1) << 16);
static size_t constexpr xsave_stack_size = (size_t(1) << 16);

struct cpu_info_t {
    cpu_info_t *self;
    thread_info_t * volatile cur_thread;
    tss_t *tss_ptr;
    uint32_t apic_id;
    int online;
    thread_info_t *goto_thread;

    // Used for lazy TLB shootdown
    uint64_t mmu_seq;
    uint64_t volatile tlb_shootdown_count;

    spinlock_t queue_lock;

    void *storage[8];
};
C_ASSERT_ISPO2(sizeof(cpu_info_t));

// Verify asm_constants.h values
C_ASSERT(offsetof(cpu_info_t, cur_thread) == CPU_INFO_CURTHREAD_OFS);
C_ASSERT(offsetof(cpu_info_t, tss_ptr) == CPU_INFO_TSS_PTR_OFS);

#define MAX_CPUS    64
static cpu_info_t cpus[MAX_CPUS] __aligned(64);

static volatile uint32_t cpu_count;

// Set in startup code (entry.s)
uint32_t default_mxcsr_mask;

// Per-CPU scheduling queue
class cpu_queue_t {
public:
    cpu_queue_t();
    ~cpu_queue_t();

    thread_info_t *choose_next();

private:
    rbtree_t<uint64_t, thread_info_t*> queue;

    static uint32_t quantum_from_priority(thread_info_t const *thread);

    spinlock_t lock;
};

// Get executing APIC ID
static uint32_t get_apic_id(void)
{
    cpuid_t cpuid_info;
    cpuid(&cpuid_info, CPUID_INFO_FEATURES, 0);
    uint32_t apic_id = cpuid_info.ebx >> 24;
    return apic_id;
}

static __always_inline cpu_info_t *this_cpu(void)
{
    cpu_info_t *cpu = (cpu_info_t *)cpu_gs_read_ptr();
    assert(cpu >= cpus && cpu < cpus + countof(cpus));
    assert(cpu->self == cpu);
    return cpu;
}

static __always_inline thread_info_t *this_thread(void)
{
    cpu_scoped_irq_disable intr_was_enabled;
    cpu_info_t *cpu = this_cpu();
    return cpu->cur_thread;
}

EXPORT void thread_yield(void)
{
#if 1
    __asm__ __volatile__ (
        // Emulate behavior of software interrupt instruction
        // int is a serializing instruction
        "mfence\n\t"

        // Push ss:rsp
        "movq %%rsp,%%rax\n\t"
        "pushq $0x10\n\t"
        "push %%rax\n\t"

        // Push rflags
        "pushfq\n\t"

        // Disable interrupts
        "cli\n\t"

        // Push cs:rip and jump to isr_entry
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
        thread_priority_t priority)
{
    if (stack_size == 0)
        stack_size = 65536;
    else if (stack_size < 65536)
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
            // Allocate one extra page for guard page
            stack = mmap(0, stack_size + PAGE_SIZE,
                         PROT_READ | PROT_WRITE,
                         MAP_STACK, -1, 0);
            // Guard page
            madvise(stack, PAGE_SIZE, MADV_DONTNEED);
            mprotect(stack, PAGE_SIZE, PROT_NONE);
            memset((char*)stack + PAGE_SIZE, 0xcc, stack_size);
            thread->flags |= THREAD_FLAG_OWNEDSTACK;

            THREAD_TRACE("Thread %zd stack guard=0x%zx,"
                         " stack=0x%zx-0x%zx\n",
                         i,
                         uintptr_t(stack),
                         uintptr_t(stack) + PAGE_SIZE,
                         uintptr_t(stack) + PAGE_SIZE + stack_size);
        }

        // Syscall stack
        // Allocate one extra page for guard page
        thread->syscall_stack = (char*)mmap(
                    0, syscall_stack_size + PAGE_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_STACK, -1, 0) + syscall_stack_size;
        madvise(thread->syscall_stack, PAGE_SIZE, MADV_DONTNEED);
        mprotect(thread->syscall_stack, PAGE_SIZE, PROT_NONE);
        THREAD_TRACE("Thread %zd syscall stack guard=0x%zx,"
                     " stack=0x%zx-0x%zx\n",
                     i,
                     uintptr_t(thread->syscall_stack),
                     uintptr_t(thread->syscall_stack) + PAGE_SIZE,
                     uintptr_t(thread->syscall_stack) + PAGE_SIZE +
                     syscall_stack_size);

        // XSave stack
        // Allocate one extra page for guard page
        thread->xsave_stack = mmap(
                    0, xsave_stack_size + PAGE_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_STACK, -1, 0);
        thread->xsave_ptr = thread->xsave_stack;
        madvise((char*)thread->xsave_stack + xsave_stack_size,
                PAGE_SIZE, MADV_DONTNEED);
        mprotect((char*)thread->xsave_stack + xsave_stack_size,
                 PAGE_SIZE, PROT_NONE);
        THREAD_TRACE("Thread %zd xsave stack guard=0x%zx,"
                     " stack=0x%zx-0x%zx\n",
                     i,
                     uintptr_t(thread->xsave_stack) + stack_size,
                     uintptr_t(thread->xsave_stack),
                     uintptr_t(thread->xsave_stack) + stack_size);

        thread->stack = stack;
        thread->stack_size = stack_size;
        thread->priority = priority;
        thread->priority_boost = 0;
        thread->cpu_affinity = affinity ? affinity : ~0UL;
        thread->fsbase = 0;
        thread->gsbase = 0;

        // APs inherit BSP's process
        thread->process = cpus[0].cur_thread->process;

        uintptr_t stack_addr = (uintptr_t)stack;
        uintptr_t stack_end = stack_addr +
                stack_size + PAGE_SIZE;

        size_t ctx_size = sizeof(isr_gpr_context_t) +
                sizeof(isr_context_t);

        // Adjust start of context to make room for context
        uintptr_t ctx_addr = stack_end - ctx_size - 32;

        ctx_addr &= -16;

        assert((ctx_addr & 0x0F) == 0);

        isr_context_t *ctx = (isr_context_t*)ctx_addr;
        memset(ctx, 0, ctx_size);
        ctx->fpr = (isr_fxsave_context_t*)thread->xsave_ptr;
        ctx->gpr = (isr_gpr_context_t*)(ctx + 1);

        ISR_CTX_REG_RSP(ctx) = (uintptr_t)
                ((ctx_addr + ctx_size + 15) & -16) + 8;
        assert((ISR_CTX_REG_RSP(ctx) & 0xF) == 0x8);
        ISR_CTX_REG_SS(ctx) = GDT_SEL_KERNEL_DATA;
        ISR_CTX_REG_RFLAGS(ctx) = CPU_EFLAGS_IF;
        ISR_CTX_REG_RIP(ctx) = (thread_fn_t)(uintptr_t)thread_startup;
        ISR_CTX_REG_CS(ctx) = GDT_SEL_KERNEL_CODE64;
        ISR_CTX_REG_DS(ctx) = GDT_SEL_USER_DATA | 3;
        ISR_CTX_REG_ES(ctx) = GDT_SEL_USER_DATA | 3;
        ISR_CTX_REG_FS(ctx) = GDT_SEL_USER_DATA | 3;
        ISR_CTX_REG_GS(ctx) = GDT_SEL_USER_DATA | 3;
        ISR_CTX_REG_RDI(ctx) = (uintptr_t)fn;
        ISR_CTX_REG_RSI(ctx) = (uintptr_t)userdata;
        ISR_CTX_REG_RDX(ctx) = (uintptr_t)i;
        ISR_CTX_REG_CR3(ctx) = cpu_get_page_directory();

        memset(ctx->fpr, 0, sse_context_size);

        ISR_CTX_SSE_MXCSR(ctx) = (CPU_MXCSR_MASK_ALL |
                CPU_MXCSR_RC_n(CPU_MXCSR_RC_NEAREST)) &
                default_mxcsr_mask;
        ISR_CTX_SSE_MXCSR_MASK(ctx) = default_mxcsr_mask;

        // All FPU registers empty
        ISR_CTX_FPU_FSW(ctx) = CPU_FPUSW_TOP_n(7);

        // 53 bit FPU precision
        ISR_CTX_FPU_FCW(ctx) = CPU_FPUCW_PC_n(CPU_FPUCW_PC_53) | CPU_FPUCW_IM |
                CPU_FPUCW_DM | CPU_FPUCW_ZM | CPU_FPUCW_OM |
                CPU_FPUCW_UM | CPU_FPUCW_PM;

        thread->ctx = ctx;

        // Check available stack space
        assert((char*)thread->ctx - (char*)thread->stack > 4096);

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

static int smp_idle_thread(void *arg)
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

static isr_context_t *thread_context_switch_handler(
        int intr, isr_context_t *ctx)
{
    (void)intr;
    assert(intr == INTR_THREAD_YIELD);
    return thread_schedule(ctx);
}

void thread_init(int ap)
{
    uint32_t cpu_number = atomic_xadd(&cpu_count, 1);

    if (cpu_number > 0)
        gdt_load_tr(cpu_number);

    // First CPU is the BSP
    cpu_info_t *cpu = cpus + cpu_number;
    cpu->tss_ptr = tss_list + cpu_number;

    assert(thread_count == cpu_number);

    // First thread is this boot thread
    thread_info_t *thread = threads + cpu_number;

    cpu->self = cpu;
    cpu->apic_id = get_apic_id();
    cpu->online = 1;

    cpu_set_gs(GDT_SEL_USER_DATA | 3);
    cpu_set_gsbase(cpu);

    if (!ap) {
        intr_hook(INTR_THREAD_YIELD, thread_context_switch_handler);

        thread->process = process_init(cpu_get_page_directory());

        cpu->cur_thread = thread;

        mm_init_process();

        thread->sched_timestamp = cpu_rdtsc();
        thread->used_time = 0;
        thread->ctx = 0;
        thread->priority = -256;

        thread->stack = kernel_stack;
        thread->stack_size = kernel_stack_size;
        madvise((char*)thread->stack, PAGE_SIZE, MADV_DONTNEED);
        mprotect((char*)thread->stack, PAGE_SIZE, PROT_NONE);
        THREAD_TRACE("Thread %d stack guard=0x%zx,"
                     " stack=0x%zx-0x%zx\n",
                     0,
                     uintptr_t(thread->stack),
                     uintptr_t(thread->stack) + PAGE_SIZE,
                     uintptr_t(thread->stack) + PAGE_SIZE + kernel_stack_size);

        thread->xsave_stack = mmap(0, xsave_stack_size + PAGE_SIZE,
                                   PROT_READ | PROT_WRITE,
                                   MAP_STACK, -1, 0);
        madvise((char*)thread->xsave_stack + xsave_stack_size,
                PAGE_SIZE, MADV_DONTNEED);
        mprotect((char*)thread->xsave_stack + xsave_stack_size,
                 PAGE_SIZE, PROT_NONE);
        THREAD_TRACE("Thread %d stack guard=0x%zx,"
                     " stack=0x%zx-0x%zx\n",
                     0,
                     uintptr_t(thread->stack),
                     uintptr_t(thread->stack) + PAGE_SIZE,
                     uintptr_t(thread->stack) + PAGE_SIZE + xsave_stack_size);

        madvise(kernel_stack, PAGE_SIZE, MADV_DONTNEED);
        mprotect(kernel_stack, PAGE_SIZE, PROT_NONE);

        thread->xsave_ptr = thread->xsave_stack;
        thread->cpu_affinity = 1;
        atomic_barrier();
        thread->state = THREAD_IS_RUNNING;
        thread_count = 1;
    } else {
        cpu_irq_disable();

        thread = threads + thread_create_with_state(
                    smp_idle_thread, 0, 0, 0,
                    THREAD_IS_INITIALIZING,
                    1 << cpu_number,
                    -256);

        thread->used_time = 0;

        cpu->goto_thread = thread;

        atomic_barrier();
        thread_yield();
        __builtin_unreachable();
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

    // If this thread is not allowed on this CPU, best is the idle thread
    // until proven otherwise
    if (unlikely(!(outgoing->cpu_affinity & (1 << cpu_number))))
        best = threads + cpu_number;

    for (size_t checked = 0; ++i, checked <= count; ++checked) {
        // Wrap
        if (unlikely(i >= count))
            i = 0;

        atomic_barrier();

        candidate = threads + i;

        // If this thread is not allowed to run on this CPU
        // then skip it
        if (unlikely(!(candidate->cpu_affinity & (1U << cpu_number))))
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
                now = time_ns();

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
    thread_info_t *thread = (thread_info_t*)outgoing;
    atomic_and(&thread->state, (thread_state_t)~THREAD_BUSY);
}

isr_context_t *thread_schedule(isr_context_t *ctx)
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
    thread->ctx = (isr_context_t*)ctx;
    uint64_t now = cpu_rdtsc();
    thread->used_time += now - thread->sched_timestamp;

    // Change to ready if running
    if (likely(thread->state == THREAD_IS_RUNNING)) {
        atomic_barrier();
        thread->state = THREAD_IS_READY_BUSY;
        atomic_barrier();
    } else if (unlikely(thread->state == THREAD_IS_DESTRUCTING)) {
        if (thread->flags & THREAD_FLAG_OWNEDSTACK)
            munmap(thread->stack, thread->stack_size);

        munmap(thread->xsave_stack, 1 << 16);

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

    thread->sched_timestamp = now;

    // Thread loses its priority boost when it is scheduled
    thread->priority_boost = 0;

    assert(thread->state == THREAD_IS_RUNNING);

    ctx = thread->ctx;
    cpu->cur_thread = thread;

    isr_context_t *isrctx = (isr_context_t*)ctx;

    assert(isrctx->gpr->iret.cs == 0x8);
    assert(isrctx->gpr->iret.ss == 0x10);
    assert(isrctx->gpr->s[0] == (GDT_SEL_USER_DATA | 3));
    assert(isrctx->gpr->s[1] == (GDT_SEL_USER_DATA | 3));
    assert(isrctx->gpr->s[2] == (GDT_SEL_USER_DATA | 3));
    assert(isrctx->gpr->s[3] == (GDT_SEL_USER_DATA | 3));

    assert(isrctx->gpr->iret.rsp >= (uintptr_t)thread->stack);
    assert(isrctx->gpr->iret.rsp <
           (uintptr_t)thread->stack + thread->stack_size + PAGE_SIZE);

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
    while (time_ns() < expiry)
        halt();
}

EXPORT void thread_sleep_until(uint64_t expiry)
{
    if (thread_idle_ready) {
        cpu_scoped_irq_disable intr_was_enabled;
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
    thread_sleep_until(time_ns() + ms * 1000000);
}

EXPORT uint64_t thread_get_usage(int id)
{
    if (id >= int(countof(threads)))
        return -1;

    thread_info_t *thread = id < 0 ? this_thread() : (threads + id);
    return thread->used_time;
}

void thread_suspend_release(spinlock_t *lock, thread_t *thread_id)
{
    cpu_scoped_irq_disable intr_was_enabled;
    cpu_info_t *cpu = this_cpu();
    thread_info_t *thread = cpu->cur_thread;

    *thread_id = thread - threads;
    atomic_barrier();

    thread->state = THREAD_IS_SUSPENDED_BUSY;
    atomic_barrier();
    spinlock_t saved_lock = spinlock_unlock_save(lock);
    thread_yield();
    assert(thread->state == THREAD_IS_RUNNING);
    spinlock_lock_restore(lock, saved_lock);
}

EXPORT void thread_resume(thread_t thread)
{
    // Wait for it to reach suspended state in case of race
    int wait_count = 0;
    for ( ; threads[thread].state != THREAD_IS_SUSPENDED;
          ++wait_count)
        pause();

    if (wait_count > 2) {
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
    cpu_scoped_irq_disable intr_was_enabled;
    cpu_info_t *cpu = this_cpu();
    return cpu->mmu_seq;
}

void thread_set_cpu_mmu_seq(uint64_t seq)
{
    cpu_scoped_irq_disable intr_was_enabled;
    cpu_info_t *cpu = this_cpu();
    cpu->mmu_seq = seq;
}

EXPORT thread_t thread_get_id(void)
{
    if (thread_count) {
        thread_t thread_id;

        cpu_scoped_irq_disable intr_was_enabled;

        cpu_info_t *cpu = this_cpu();
        thread_id = cpu->cur_thread - threads;

        return thread_id;
    }

    // Too early to get a thread ID
    return 0;
}

EXPORT uint64_t thread_get_affinity(int id)
{
    return threads[id].cpu_affinity;
}

EXPORT size_t thread_get_cpu_count()
{
    return cpu_count;
}

EXPORT void thread_set_affinity(int id, uint64_t affinity)
{
    cpu_scoped_irq_disable intr_was_enabled;
    cpu_info_t *cpu = this_cpu();
    size_t cpu_number = cpu - cpus;

    threads[id].cpu_affinity = affinity;

    // Are we changing current thread affinity?
    while (cpu->cur_thread == threads + id &&
            !(affinity & (1 << cpu_number))) {
        // Get off this CPU
        thread_yield();

        // Check again, a racing thread may have picked
        // up this thread without seeing change
        cpu = this_cpu();
        cpu_number = cpu - cpus;
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
            (char*)sp > (char*)thread->stack +
            thread->stack_size + PAGE_SIZE) {
        cpu_debug_break();
        cpu_crash();
    }
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

size_t thread_cls_alloc(void)
{
    return atomic_xadd(&storage_next_slot, 1);
}

void *thread_cls_get(size_t slot)
{
    cpu_info_t *cpu = this_cpu();
    return cpu->storage[slot];
}

void thread_cls_set(size_t slot, void *value)
{
    cpu_info_t *cpu = this_cpu();
    cpu->storage[slot] = value;
}

void thread_cls_init_each_cpu(
        size_t slot, thread_cls_init_handler_t handler, void *arg)
{
    for (size_t c = 0; c < cpu_count; ++c)
        cpus[c].storage[slot] = handler(arg);
}

void thread_cls_for_each_cpu(
        size_t slot, int other_only,
        thread_cls_each_handler_t handler, void *arg, size_t size)
{
    cpu_info_t *cpu = other_only ? this_cpu() : 0;
    for (size_t c = 0; c < cpu_count; ++c) {
        if (cpus + c != cpu)
            handler(c, cpus[c].storage[slot], arg, size);
    }
}

void thread_send_ipi(int cpu, int intr)
{
    assert(cpu >= 0 && (unsigned)cpu < cpu_count);
    apic_send_ipi(cpus[cpu].apic_id, intr);
}

int thread_cpu_number()
{
    cpu_info_t *cpu = this_cpu();
    return cpu - cpus;
}

isr_context_t *thread_schedule_if_idle(isr_context_t *ctx)
{
    cpu_info_t *cpu = this_cpu();
    if (cpu->cur_thread - threads < cpu_count)
        return thread_schedule(ctx);
    return ctx;
}

// linearly map [-20,0] -> [800,100], and map [0,20] -> [100,5]
uint32_t cpu_queue_t::quantum_from_priority(thread_info_t const *thread)
{
    uint8_t n = thread->priority;
    return n <= 0 ? n * -35 + 100 : n * -19 / 4 + 100;
}

process_t *thread_current_process()
{
    thread_info_t *thread = this_thread();
    return thread->process;
}


uint32_t thread_get_cpu_apic_id(int cpu)
{
    return cpus[cpu].apic_id;
}

uint64_t thread_shootdown_count(int cpu_nr)
{
    cpu_info_t const *cpu = cpus + cpu_nr;
    return cpu->tlb_shootdown_count;
}

void thread_shootdown_notify()
{
    cpu_info_t *cpu = this_cpu();
    atomic_inc(&cpu->tlb_shootdown_count);
}

void thread_set_error(errno_t errno)
{
    thread_info_t *info = this_thread();
    info->errno = errno;
}

errno_t thread_get_error()
{
    thread_info_t *info = this_thread();
    return info->errno;
}

uintptr_t thread_get_fsbase(int thread)
{
    thread_info_t *info = thread >= 0 ? threads + thread : this_thread();
    return info->fsbase;
}

uintptr_t thread_get_gsbase(int thread)
{
    thread_info_t *info = thread >= 0 ? threads + thread : this_thread();
    return info->gsbase;
}
