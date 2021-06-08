#include "cpu/thread_impl.h"
#include "debug.h"
#include "process.h"
#include "types.h"
#include "interrupts.h"
#include "string.h"
#include "assert.h"
#include "likely.h"

#include "thread.h"
#include "asm_constants.h"
#include "main.h"
#include "halt.h"
#include "cpu.h"
#include "gdt.h"
#include "mm.h"
#include "time.h"
#include "export.h"
#include "printk.h"
#include "apic.h"
#include "callout.h"
#include "vector.h"
#include "mutex.h"
#include "except.h"
#include "work_queue.h"
#include "cxxexcept.h"
#include "basic_set.h"
#include "engunit.h"
#include "idt.h"
#include "user_mem.h"
#include "thread_info.h"

#include "cpu_info.h"

// Implements platform independent thread.h

#define DEBUG_THREAD    0
#if DEBUG_THREAD
#define THREAD_TRACE(...) printdbg("thread: " __VA_ARGS__)
#else
#define THREAD_TRACE(...) ((void)0)
#endif

#define DEBUG_THREAD_STACK    0
#if DEBUG_THREAD_STACK
#define THREAD_STK_TRACE(...) printdbg("thread_stk: " __VA_ARGS__)
#else
#define THREAD_STK_TRACE(...) ((void)0)
#endif


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

//struct link_t {
//    link_t *next;
//    link_t *prev;

//    template<typename T, size_t offset = offsetof(T, link)>
//    T *container(link_t *link)
//    {
//        return (T*)((char*)link - offset);
//    }
//};

// Keeps them sorted and second member deduplicates in case
// of identical timestamps, the lower address thread runs first
// Backed by bump allocator backed by page allocator with very large block
using timestamp_t = uint64_t;
// second here should be uintptr_t
//#ifndef NDEBUG
//// debugger displays thread nicely
//using ready_set_t = ext::fast_set<ext::pair<uintptr_t, thread_info_t*>>;
//#else
//#endif


static void thread_free_stacks(thread_info_t *thread);

C_ASSERT_ISPO2(sizeof(thread_info_t));

// Verify asm_constants.h values
C_ASSERT(offsetof(thread_info_t, fsbase) == THREAD_INFO_FSBASE_OFS);
C_ASSERT(offsetof(thread_info_t, gsbase) == THREAD_INFO_GSBASE_OFS);
C_ASSERT(offsetof(thread_info_t, process) == THREAD_INFO_PROCESS_OFS);
C_ASSERT(offsetof(thread_info_t, fpusave_ptr) == THREAD_INFO_FPUSAVE_PTR_OFS);
C_ASSERT(offsetof(thread_info_t, priv_chg_stack) ==
         THREAD_INFO_PRIV_CHG_STACK_OFS);
C_ASSERT(offsetof(thread_info_t, thread_id) == THREAD_INFO_THREAD_ID_OFS);
C_ASSERT(offsetof(thread_info_t, syscall_mxcsr) ==
         THREAD_INFO_SYSCALL_MXCSR_OFS);
C_ASSERT(offsetof(thread_info_t, syscall_fcw87) ==
         THREAD_INFO_SYSCALL_FCW87_OFS);
C_ASSERT(sizeof(thread_info_t) == THREAD_INFO_SIZE);

// Save FPU context when interrupted in user mode
// (always on for threads that execute user programs)
#define THREAD_FLAGS_USER_FPU   (1U<<0)

// Save FPU context when interrupted in kernel mode
#define THREAD_FLAGS_KERNEL_FPU (1U<<1)

#define THREAD_FLAGS_ANY_FPU ( \
    THREAD_FLAGS_KERNEL_FPU | \
    THREAD_FLAGS_USER_FPU)

// Store in a big array, for now
#define MAX_THREADS 65536
HIDDEN thread_info_t threads[MAX_THREADS];

// Separate to avoid constantly sharing dirty lines
static uint8_t run_cpu[MAX_THREADS];

static size_t volatile thread_count;

uint32_t volatile thread_aps_running;
uint32_t volatile thread_booting_ap_index;
int thread_idle_ready;
int spincount_mask;
int use_mwait;
size_t storage_next_slot;
bool thread_cls_ready;

static size_t constexpr xsave_stack_size = (size_t(8) << 10);

void dump_scheduler_list(char const *prefix, ready_set_t &list);

// Make sure pointer arithmetic would never do a divide
C_ASSERT_ISPO2(sizeof(cpu_info_t));

// Verify asm_constants.h values
C_ASSERT(offsetof(cpu_info_t, self) == CPU_INFO_SELF_OFS);
C_ASSERT(offsetof(cpu_info_t, cur_thread) == CPU_INFO_CUR_THREAD_OFS);
C_ASSERT(offsetof(cpu_info_t, apic_id) == CPU_INFO_APIC_ID_OFS);
C_ASSERT(offsetof(cpu_info_t, tss_ptr) == CPU_INFO_TSS_PTR_OFS);
C_ASSERT(offsetof(cpu_info_t, syscall_flags) == CPU_INFO_SYSCALL_FLAGS_OFS);
C_ASSERT(offsetof(cpu_info_t, in_irq) == CPU_INFO_IN_IRQ_OFS);
C_ASSERT(offsetof(cpu_info_t, pf_count) == CPU_INFO_PF_COUNT_OFS);
C_ASSERT(offsetof(cpu_info_t, syscall_ctx) == CPU_INFO_SYSCALL_CTX_OFS);
C_ASSERT(offsetof(cpu_info_t, locks_held) == CPU_INFO_LOCKS_HELD_OFS);
C_ASSERT(offsetof(cpu_info_t, csw_deferred) == CPU_INFO_CSW_DEFERRED_OFS);
C_ASSERT(offsetof(cpu_info_t, after_csw_fn) == CPU_INFO_AFTER_CSW_FN_OFS);
C_ASSERT(offsetof(cpu_info_t, after_csw_vp) == CPU_INFO_AFTER_CSW_VP_OFS);

C_ASSERT((offsetof(cpu_info_t, self) & ~-64) == 0);
C_ASSERT((offsetof(cpu_info_t, apic_id) & ~-64) == 0);
C_ASSERT((offsetof(cpu_info_t, storage) & ~-64) == 0);
C_ASSERT((offsetof(cpu_info_t, resume_ring) & ~-64) == 0);
//C_ASSERT((offsetof(cpu_info_t, queue_lock) & ~-64) == 0);
C_ASSERT(sizeof(cpu_info_t) == CPU_INFO_SIZE);

_section(".data.cpus")
cpu_info_t cpus[MAX_CPUS];
size_t const cpus_sizeof = sizeof(*cpus);
cpu_info_t * const cpus_end = cpus + MAX_CPUS;

_constructor(ctor_cpu_init_cpus) static void cpus_init_ctor()
{
    // Do some basic initializations on the first CPU
    // without having a big initializer list of unreadable
    // nonsense above
    cpus[0].self = cpus;
    cpus[0].cur_thread = threads;
    cpus[0].tss_ptr = tss_list;
}

// Ticks up as each AP starts up
uint32_t cpu_count;

// Holds full count
uint32_t total_cpus;

static void thread_destruct(thread_info_t *thread,
                            thread_info_t::scoped_lock &lock);

static void thread_signal_completion_locked(
        thread_info_t *thread,
        thread_info_t::scoped_lock &thread_lock);

// Get executing APIC ID (the slow expensive way, for early initialization)
static uint32_t get_apic_id_slow()
{
    cpuid_t cpuid_info;

    cpuid(&cpuid_info, CPUID_INFO_FEATURES, 0);
    uint32_t apic_id = cpuid_info.ebx >> 24;
    return apic_id;
}

// Get executing CPU by APIC ID (slow, only used in early init)
cpu_info_t *this_cpu_by_apic_id_slow()
{
    uint32_t apic_id = get_apic_id_slow();
    for (size_t i = 0, e = total_cpus; i != e; ++i) {
        if (cpus[i].apic_id == apic_id)
            return cpus + i;
    }
    return nullptr;
}

_hot
static _always_inline cpu_info_t *this_cpu()
{
    return cpu_gs_read<cpu_info_t*, 0>();
}

_hot
static _always_inline thread_info_t *this_thread()
{
    return cpu_gs_read<thread_info_t*, offsetof(cpu_info_t, cur_thread)>();
}

_noreturn
static void thread_cleanup()
{
    thread_info_t *thread = this_thread();

    cpu_irq_disable();

    thread_info_t::scoped_lock lock(thread->lock);

    assert(thread->state == THREAD_IS_RUNNING);

    thread->priority = 0;

    assert(thread->state == THREAD_IS_RUNNING);

    thread->state = THREAD_IS_EXITING_BUSY;

    lock.unlock();

    thread_yield();

    __builtin_unreachable();
}

void thread_startup(thread_fn_t fn, void *p, thread_t id)
{
    threads[id].exit_code = fn(p);
    thread_cleanup();
}

static constexpr size_t stack_guard_size = (8<<10);

// Allocate a stack with a large guard region at both ends and
// return a pointer to the end of the middle committed part
static char *thread_allocate_stack(
        thread_t tid, size_t stack_size, char const *noun, int fill)
{
    // Whole thing is guard pages at first
    char *stack;
    stack = (char*)mmap(nullptr, stack_guard_size +
                        stack_size + stack_guard_size, PROT_NONE,
                        MAP_UNINITIALIZED);

    if (unlikely(stack == MAP_FAILED))
        return nullptr;

    THREAD_STK_TRACE("Allocated %s stack"
                     ", size=%#zx"
                     ", tid=%d"
                     ", filled=%#x\n",
                     noun, stack_size, tid, fill);

    //
    // Guard pages

    // Mark guard region address ranges
    char *guard0_st = stack;
    char *guard0_en = guard0_st + stack_guard_size;
    char *guard1_st = guard0_en + stack_size;
    mprotect(guard0_en, stack_size, PROT_READ | PROT_WRITE);
    madvise(guard0_en, stack_size, MADV_WILLNEED);

    THREAD_TRACE("Thread %d %s stack range=%p-%p,"
                 " stack=%p-%p\n", tid, noun,
                 (void*)stack,
                 (void*)(stack + stack_guard_size + stack_size +
                         stack_guard_size),
                 (void*)(guard0_en),
                 (void*)(guard1_st));

    memset(guard0_en, fill, stack_size);

    return guard1_st;
}

// Returns threads array index or 0 on error
// Minimum allowable stack space is 4KB
cpu_info_t & schedule_thread_on_cpu(
        thread_info_t *thread,
        uint64_t timeslice_timestamp,
        size_t cpu_nr, uint64_t preempt_time)
{
    cpu_info_t& cpu = cpus[cpu_nr];
    cpu_info_t::scoped_lock lock(cpu.queue_lock);
    thread->timeslice_timestamp = timeslice_timestamp;
    thread->preempt_time = preempt_time;
    thread->schedule_node = cpu.ready_list
            .emplace(thread->timeslice_timestamp,
                     ready_set_t::value_type::second_type(thread))
            .first;
    //dump_scheduler_list("ready list:", cpu.ready_list);
    lock.unlock();

    return cpu;
}

static thread_t thread_create_with_state(
        thread_t *ret_tid,
        thread_fn_t fn, void *userdata, char const *name, size_t stack_size,
        thread_state_t state, thread_cpu_mask_t const &affinity,
        thread_priority_t priority, bool user, bool is_float)
{
    if (stack_size == 0)
        stack_size = 16 << 10;
    else if (stack_size < (16 << 10))
        return -1;

    thread_info_t *thread = nullptr;
    size_t i;

    for (i = 0; ; ++i) {
        if (unlikely(i >= MAX_THREADS)) {
//            printdbg("Out of threads, yielding\n");
//            thread_yield();
            panic("Out of threads");
            i = 0;
        }

        thread = threads + i;

        thread_info_t::scoped_lock thread_lock(
                    thread->lock, ext::defer_lock_t());

        if (unlikely(!thread_lock.try_lock()))
            continue;

        if (likely(thread->state != THREAD_IS_UNINITIALIZED))
            continue;

        // Atomically grab the thread
        thread->state = THREAD_IS_INITIALIZING;

        if (ret_tid)
            *ret_tid = i;

        break;
    }

    thread_info_t::scoped_lock thread_lock(thread->lock);

    thread->name = name;

    // Pick CPU based on thread id for now
    size_t cpu_nr;

    cpu_nr = i % apic_cpu_count();

    thread->cpu_affinity = affinity;

    if (!thread->cpu_affinity) {
        thread->cpu_affinity += cpu_nr;
    } else {
        if (!thread->cpu_affinity[cpu_nr])
            cpu_nr = thread->cpu_affinity.lsb_set();
    }

    run_cpu[i] = cpu_nr;

    thread->thread_flags = 0;

    thread->wake_time = UINT64_MAX;

    thread->ref_count = 1;

    char *stack;

    if (i) {
        stack = thread_allocate_stack(i, stack_size,
                                      "create_with_state", 0xA0);
    } else {
        stack = kernel_stack + kernel_stack_size;
        stack_size = kernel_stack_size;
    }

    if (unlikely(stack == nullptr))
        panic_oom();

    thread->stack = stack;
    thread->stack_size = stack_size;

    char *xsave_stack = nullptr;
    if (user || is_float) {
        // XSave stack

        thread->thread_flags |= THREAD_FLAGS_KERNEL_FPU;

        xsave_stack = thread_allocate_stack(
                    i, xsave_stack_size, "xsave", 0);

        assert(sse_context_size >= 512);
        thread->fpusave_ptr = xsave_stack - sse_context_size;
    } else {
        thread->fpusave_ptr = nullptr;
    }

    thread->xsave_stack = xsave_stack;

    // Empty affinity mask selects

    thread->priority = priority;

    thread->preempt_time = 16000000;
    thread->fsbase = nullptr;
    thread->gsbase = nullptr;

    // Created thread inherits creator's process
    thread->process = thread_current_process();

    uintptr_t stack_addr = uintptr_t(stack);
    uintptr_t stack_end = stack_addr;

    constexpr size_t ctx_size = sizeof(isr_context_t);

    // Adjust start of context to make room for context
    uintptr_t ctx_addr = stack_end - ctx_size;

    isr_context_t *ctx = (isr_context_t*)ctx_addr;
    memset(ctx, 0, ctx_size);

    ISR_CTX_FPU(ctx) = (isr_xsave_context_t*)thread->fpusave_ptr;

    ISR_CTX_REG_RSP(ctx) = stack_end;

    ISR_CTX_REG_SS(ctx) = GDT_SEL_KERNEL_DATA;

    ISR_CTX_REG_RFLAGS(ctx) = CPU_EFLAGS_IF | CPU_EFLAGS_ALWAYS;

    ISR_CTX_REG_RIP(ctx) = thread_fn_t(uintptr_t(thread_entry));

    ISR_CTX_REG_CS(ctx) = GDT_SEL_KERNEL_CODE64;
    ISR_CTX_REG_DS(ctx) = GDT_SEL_USER_DATA | 3;
    ISR_CTX_REG_ES(ctx) = GDT_SEL_USER_DATA | 3;
    ISR_CTX_REG_FS(ctx) = GDT_SEL_USER_DATA | 3;
    ISR_CTX_REG_GS(ctx) = GDT_SEL_USER_DATA | 3;

    ISR_CTX_REG_RBP(ctx) = uintptr_t(&ISR_CTX_REG_RBP(ctx));

    ISR_CTX_REG_RDI(ctx) = uintptr_t(fn);
    ISR_CTX_REG_RSI(ctx) = uintptr_t(userdata);
    ISR_CTX_REG_RDX(ctx) = i;
    ISR_CTX_REG_CR3(ctx) = cpu_page_directory_get();

    if (thread->thread_flags & THREAD_FLAGS_ANY_FPU) {
        // All FPU registers empty
        ISR_CTX_FPU_FSW(ctx) = CPU_FPUSW_TOP_n(7);

        // SSE initial config (init optimization possible)
        ISR_CTX_SSE_MXCSR(ctx) = CPU_MXCSR_ELF_INIT &
                default_mxcsr_mask;

        // x87 initial config (init optimization possible)
        ISR_CTX_SSE_MXCSR_MASK(ctx) = default_mxcsr_mask;

        // 64 bit FPU precision (because x87 is used for long double)
        ISR_CTX_FPU_FCW(ctx) = CPU_FPUCW_ELF_INIT;

        if (cpuid_has_xsave()) {
            isr_xsave_context_t *xsave_ctx = ISR_CTX_FPU(ctx);

            // If the x87 is not in its initial configuration
            // Data registers cannot be initialized on thread creation
            if (unlikely(xsave_ctx->legacy_area.fcw != 0x37F))
                xsave_ctx->xstate_bv = CPU_XCOMPBV_X87;

            // Restore SSE state if initial MXCSR
            // is not the initial configuration
            // Data registers cannot be initialized on thread creation
            if (unlikely(ISR_CTX_SSE_MXCSR(ctx) != 0x1F80))
                xsave_ctx->xstate_bv |= CPU_XCOMPBV_SSE;

            // Enable compact format if supported
            if (cpuid_has_xsavec())
                xsave_ctx->xcomp_bv = CPU_XCOMPBV_COMPACT;
        }
    }

    thread->ctx = ctx;

    thread->state = state;

    // Atomically make sure thread_count > i
    atomic_max(&thread_count, i + 1);

    // See if outrageously low or high priority is wanted
    uint64_t now;

    if (likely(i >= cpu_count * 2))
        // Normal thread
        now = time_ns();
    else if (likely(i >= cpu_count))
        // IRQ worker, highest possible priority
        now = 1;
    else
        // Idle, lowest possible priority
        now = UINT64_MAX;

    THREAD_TRACE("cpu %zu initially scheduling thread %u\n",
             cpu_nr, thread->thread_id);

    uint64_t timeslice_timestamp = now;
    uint64_t preempt_time = thread->used_time + 64000000;

    thread_lock.unlock();

    cpu_info_t& cpu = schedule_thread_on_cpu(
                thread, timeslice_timestamp, cpu_nr, preempt_time);

    thread_info_t *parent = this_thread();

    // Kick other cpu if not this cpu
    if (thread_cpu_number() != cpu_nr)
        apic_send_ipi(cpu.apic_id, INTR_IPI_RESCHED);
    // Hand over the cpu if new thread should run sooner than running thread
    else if (//thread->sched_timestamp > 0 &&
             parent->sched_timestamp > thread->sched_timestamp)
        thread_yield();

    return i;
}

thread_t thread_create(thread_t *ret_tid,
                       thread_fn_t fn, void *userdata, char const *name,
                       size_t stack_size, bool user, bool is_float,
                       thread_cpu_mask_t const& affinity, int priority)
{
    thread_create_info_t info{};
    info.fn = fn;
    info.userdata = userdata;
    info.name = name;
    info.stack_size = stack_size;
    info.affinity = affinity;
    info.ret_tid = ret_tid;
    info.priority = priority;
    info.user = user;
    info.is_float = is_float;

    return thread_create_with_info(&info);
}

thread_t thread_create_with_info(thread_create_info_t const* info)
{
    return thread_create_with_state(
                info->ret_tid,
                info->fn, info->userdata, info->name, info->stack_size,
                info->suspended ? THREAD_IS_SLEEPING : THREAD_IS_READY,
                info->affinity, info->priority, info->user, info->is_float);
}

#if 0
static void thread_monitor_mwait()
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

static intptr_t smp_idle_thread(void *)
{
    // Enable spinning
    spincount_mask = -1;

    apic_config_cpu();

    printdbg("SMP thread running\n");
    atomic_inc(&thread_aps_running);
    thread_check_stack(-1);

    cpu_irq_enable();

    thread_idle();
}

_hot
void thread_idle()
{
    assert(cpu_irq_is_enabled());
    for(;;)
        halt();
}

// Hardware interrupt handler (an IPI) to provoke other CPUs to reschedule
static isr_context_t *thread_ipi_resched(int intr, isr_context_t *ctx)
{
    cpu_info_t::resume_ent_t resumed_data[4];
    size_t resumed_count = 0;

    cpu_info_t *cpu = this_cpu();
    cpu_info_t::scoped_lock cpu_lock(cpu->queue_lock);

    while (cpu->resume_tail != cpu->resume_head) {
        resumed_data[resumed_count++] = cpu->resume_ring[cpu->resume_tail];
        cpu->resume_tail = cpu_info_t::resume_next(cpu->resume_tail);
    }
    cpu_lock.unlock();

    size_t i;
    for (i = 0; i < resumed_count; ++i) {
        uint32_t exitcode = resumed_data[i].ret_lo |
                (uint64_t(resumed_data[i].ret_hi) << 32);
        thread_t tid = resumed_data[i].tid;
        thread_resume(tid, exitcode);
    }
    assert(i == resumed_count);

    return thread_schedule(ctx);
}

void thread_set_timer(uint8_t& apic_dcr, uint64_t ns)
{
    uint64_t ticks = apic_ns_to_ticks(ns);
    uint64_t actual = apic_timer_hw_oneshot(apic_dcr, ticks);
//    printdbg("Requested oneshot ns=%zu, ticks=%zu, actual=%zu\n",
//             ns, ticks, actual);
}

_constructor(ctor_thread_init_bsp) static void thread_init_bsp()
{
    thread_init(0);
}

void thread_set_cpu_count(size_t new_cpu_count)
{
    total_cpus = new_cpu_count;
}

void dump_scheduler_list(char const *prefix, ready_set_t &list)
{
    char ready_list_debug[64];
    size_t ready_used = 0;
    ready_list_debug[ready_used] = 0;

    for (ready_set_t::value_type const& item: list) {
        if (unlikely(ready_used + 6 >= sizeof(ready_list_debug))) {
            strcpy(ready_list_debug + ready_used, "...");
            break;
        }

        thread_info_t *thread = (thread_info_t*)item.second;

        int emitted = snprintf(ready_list_debug + ready_used,
                       sizeof(ready_list_debug) - ready_used,
                       " %u", thread->thread_id);
        ready_used += emitted >= 0 ? emitted : 0;
    }

    printdbg("%s %s\n", prefix, ready_list_debug);
}

void thread_init(int ap)
{
    if (!thread_cls_ready)
        thread_cls_ready = true;

    uint32_t cpu_nr = atomic_xadd(&cpu_count, 1);
    cpu_info_t *cpu = cpus + cpu_nr;

    cpu->ready_list.get_allocator().create();

    cpu->cpu_nr = cpu_nr;
    cpu->sleep_list.share_allocator(cpu->ready_list);

    if (cpu_nr > 0) {
        // Load hidden tssbase, point to cpu-local TSS
        gdt_load_tr(cpu_nr);
        cpu->tss_ptr = tss_list + cpu_nr;
    }

    assert(thread_count == cpu_nr);

    thread_info_t *thread = threads + cpu_nr;

    cpu->self = cpu;
    cpu->apic_id = get_apic_id_slow();
    cpu->online = true;

    // Initialize kernel gsbase and other gsbase
    cpu_gsbase_set(cpu);
    cpu_altgsbase_set(nullptr);

    // Remember CR0 so we can avoid changing it
    cpu->cr0_shadow = uint32_t(cpu_cr0_get());

    if (!ap) {
        //
        // Perform BSP-only initializations

        thread->process = process_t::init(cpu_page_directory_get());

        for (size_t i = 0; i < countof(threads); ++i) {
            // Initialize every thread ID so pointer tricks aren't needed
            threads[i].thread_id = i;

            // First 2N threads, 1N for idle threads, 1N for per cpu workers
            if (i < total_cpus * 2)
                threads[i].process = threads[0].process;
        }

        // Hook handler that performs a reschedule requested by another CPU
        intr_hook(INTR_IPI_RESCHED, thread_ipi_resched,
                  "hw_ipi_resched", eoi_lapic);

        // The current thread for this CPU is this thread
        cpu->cur_thread = thread;

        mm_init_process(thread->process, true);

        // 0x00 is minimum priority, 0xFF is maximum priority
        thread->priority = 0;
        thread->sched_timestamp = (uint64_t(thread->priority ^ 0xFF) << 56) |
                time_ns();
        thread->used_time = 0;
        thread->preempt_time = 64000000;
        thread->ctx = nullptr;

        // BSP stack (grows down)
        thread->stack = kernel_stack + kernel_stack_size;
        thread->stack_size = kernel_stack_size;

        thread->xsave_stack = nullptr;
        thread->fpusave_ptr = nullptr;
        thread->cpu_affinity = thread_cpu_mask_t(0);
        thread->name = "Idle(BSP)";
        atomic_st_rel(&thread->state, THREAD_IS_RUNNING);

        size_t cpu_nr = run_cpu[thread->thread_id];

        THREAD_TRACE("cpu %zu initially scheduling idle thread %u\n",
                 cpu_nr, thread->thread_id);

        schedule_thread_on_cpu(thread, UINT64_MAX, cpu_nr, UINT64_MAX);

        thread->preempt_time = UINT64_MAX;
        thread->timeslice_timestamp = UINT64_MAX;
        thread->schedule_node = cpus[cpu_nr].ready_list
                .emplace(thread->timeslice_timestamp,
                         ready_set_t::value_type::second_type(thread))
                .first;

        thread_count = 1;
    } else {
        cpu_irq_disable();

        thread = threads + thread_create_with_state(
                    nullptr,
                    smp_idle_thread, nullptr, "Idle(AP)", 0,
                    THREAD_IS_INITIALIZING,
                    thread_cpu_mask_t(cpu_nr),
                    0, false, false);

        thread->process = threads[0].process;

        thread->used_time = 0;

        cpu->goto_thread = thread;

        thread_yield();
        __builtin_unreachable();
    }
}

static thread_info_t *thread_choose_next(
        cpu_info_t *cpu,
        thread_info_t * const outgoing,
        uint64_t now)
{
    // If we have not created all of the idle threads yet...
    if (unlikely(thread_count < cpu_count))
        // ...don't even think about context switching
        return outgoing;

    // Service the sleep queue
    for (ready_set_t::const_iterator en = cpu->sleep_list.cend(),
         sleeping = cpu->sleep_list.cbegin(), next;
         sleeping != en && sleeping->first <= now;
         sleeping = next) {
        thread_info_t *sleeping_thread = (thread_info_t*)sleeping->second;

        thread_info_t::scoped_lock thread_lock(sleeping_thread->lock);

        THREAD_TRACE("Sleep ended for tid=%d\n", sleeping_thread->thread_id);

        thread_state_t expect_state = (outgoing != sleeping_thread)
                ? THREAD_IS_SLEEPING
                : THREAD_IS_SLEEPING_BUSY;

        thread_state_t ready_state = (outgoing != sleeping_thread)
                ? THREAD_IS_READY
                : THREAD_IS_READY_BUSY;

        assert(expect_state == sleeping_thread->state);

        sleeping_thread->state = ready_state;

        ready_set_t::node_type node = cpu->sleep_list.extract(sleeping);

        // ready list keyed on timeslice timestamp
        node.value().first = sleeping_thread->timeslice_timestamp;

        sleeping_thread->schedule_node = cpu->ready_list
                .insert(ext::move(node))
                .first;

        thread_lock.unlock();

        next = cpu->sleep_list.cbegin();
    }

    assert(!cpu->ready_list.empty());
    ready_set_t::const_iterator it = cpu->ready_list.cbegin();
    thread_info_t *ft = reinterpret_cast<thread_info_t *>(it->second);
    assert(ft->state == THREAD_IS_READY ||
           (ft->state == THREAD_IS_READY_BUSY && outgoing == ft));
    return ft;
}

void thread_clear_busy(void *outgoing)
{
    thread_info_t *thread = (thread_info_t*)outgoing;

    if (unlikely(atomic_and(&thread->state, (thread_state_t)~THREAD_BUSY) ==
                 THREAD_IS_FINISHED)) {
        // The exiting thread has finished getting off the CPU, immediately
        // delete the thread from the owning process
        thread_info_t::scoped_lock lock(thread->lock);
        thread_destruct(thread, lock);
        //thread_free_stacks(thread);
    }
}

static void thread_free_stacks(thread_info_t *thread)
{
    char *stk;
    size_t stk_sz;

    assert(thread->state == THREAD_IS_FINISHED);

    // The thread stack
    if (thread->stack != nullptr && thread->stack_size > 0) {
        stk = thread->stack - thread->stack_size - stack_guard_size;
        stk_sz = stack_guard_size + thread->stack_size + stack_guard_size;
        thread->stack = nullptr;
        thread->stack_size = 0;

        THREAD_STK_TRACE("Freeing %s stack"
                         ", addr=%#zx"
                         ", size=%#zx"
                         ", tid=%d\n",
                         "thread", uintptr_t(stk),
                         stk_sz, thread->thread_id);

        assert(stk != nullptr);
        assert(stk_sz != 0);
        munmap(stk, stk_sz);
    }

    // The xsave stack
    if (thread->thread_flags & THREAD_FLAGS_ANY_FPU) {
        stk = thread->xsave_stack - xsave_stack_size - stack_guard_size;
        stk_sz = stack_guard_size + xsave_stack_size + stack_guard_size;
        thread->xsave_stack = nullptr;
        thread->thread_flags &= ~THREAD_FLAGS_ANY_FPU;

        THREAD_STK_TRACE("Freeing %s stack"
                         ", addr=%#zx"
                         ", size=%#zx"
                         ", tid=%d\n",
                         "xsave", uintptr_t(stk),
                         stk_sz, thread->thread_id);

        assert(stk != nullptr);
        assert(stk_sz != 0);
        munmap(stk, stk_sz);
    }
}

static void thread_signal_completion_locked(
        thread_info_t *thread,
        thread_info_t::scoped_lock &thread_lock)
{
    // Wake up any threads waiting for this thread to exit ASAP
    thread->state = THREAD_IS_FINISHED_BUSY;
    thread_lock.unlock();
    thread->done_cond.notify_all();
}

static void thread_add_ref(thread_info_t *thread,
                           thread_info_t::scoped_lock &lock)
{
    assert(lock.is_locked());
    assert(thread->ref_count > 0);
    ++thread->ref_count;
}

static void thread_release_ref(thread_info_t *thread,
                               thread_info_t::scoped_lock &lock)
{
    assert(lock.is_locked());
    //assert(thread->ref_count > 0);
    thread->ref_count -= (thread->ref_count > 0);
    if (thread->ref_count == 0)
        thread->state = (thread_state_t)
                (THREAD_IS_UNINITIALIZED | (thread->state & THREAD_BUSY));
}

// Thread is not running anymore, destroy things only needed when it runs
void thread_destruct(thread_info_t *thread,
                     thread_info_t::scoped_lock &lock)
{
    thread_free_stacks(thread);

    // If everybody has closed their handle to this thread,
    // then mark it for recycling immediately
    //thread_release_ref(thread, lock);
}

int thread_close(thread_t tid)
{
    thread_info_t* thread = threads + tid;

    thread_info_t::scoped_lock lock(thread->lock);

    if (thread->ref_count == 0)
        return 0;

    --thread->ref_count;

    if (thread->state == THREAD_IS_FINISHED) {
        assert(thread->ref_count == 0);
        //mutex_destroy(&thread.lock);
        thread_destruct(thread, lock);
        return 1;
    }

    return 0;
}

isr_context_t *bootstrap_idle_thread(cpu_info_t *cpu, thread_info_t *thread,
                                     isr_context_t *ctx)
{
    thread = cpu->goto_thread;
    cpu->cur_thread = thread;
    cpu->goto_thread = nullptr;
    thread->state = THREAD_IS_RUNNING;
    ctx = thread->ctx;
    thread->ctx = nullptr;
    return ctx;
}

void accumulate_time(cpu_info_t *cpu, thread_info_t *thread, uint64_t elapsed)
{
    thread->used_time += elapsed;

    // If it is not an idle thread
    if (uint32_t(thread->thread_id) > cpu_count)
        cpu->busy_ratio += elapsed;

    cpu->time_ratio += elapsed;

    // Normalize ratio to < 32768
    uint8_t time_scale = bit_msb_set(cpu->time_ratio);
    if (time_scale > 14) {
        time_scale -= 14;
        cpu->time_ratio >>= time_scale;
        cpu->busy_ratio >>= time_scale;
    }

    if (likely(cpu->time_ratio)) {
        // 1032 == 1.032%
        int busy_percent = (int)(UINT64_C(100000000) *
                                 cpu->busy_ratio / cpu->time_ratio);
        cpu->busy_percent_x1M = busy_percent;
    } else {
        cpu->busy_percent_x1M = 0;
    }
}

void thread_move_to_other_cpu(cpu_info_t *cpu, thread_info_t *thread)
{
    cpu_info_t::scoped_lock cpu_lock(cpu->queue_lock);

    ready_set_t::node_type node = cpu->ready_list
            .extract(thread->schedule_node);

    cpu_lock.unlock();

    size_t first_enabled = thread->cpu_affinity.lsb_set();

    cpu_info_t *other_cpu = cpus + first_enabled;

    cpu_info_t::scoped_lock other_cpu_lock(other_cpu->queue_lock);

    thread->state = THREAD_IS_SLEEPING_BUSY;
    thread->wake_time = 0;

    thread->schedule_node = other_cpu->sleep_list
            .insert(ext::move(node)).first;

    other_cpu_lock.unlock();

    apic_send_ipi(other_cpu->apic_id, INTR_IPI_RESCHED);
}

static void thread_csw_fpu(isr_context_t *ctx, cpu_info_t *cpu,
                           thread_info_t* const outgoing,
                           thread_info_t *incoming)
{
    fpu_state_t& fpu_state = cpu->fpu_state;

    assume(incoming != outgoing);

    //
    // FPU context switch save and/or restore, lockout/unlock

    unsigned from_cs = ISR_CTX_REG_CS(ctx);
    unsigned to_cs = ISR_CTX_REG_CS(ctx);

    bool from_kern = GDT_SEL_RPL_IS_KERNEL(from_cs);
    bool to_kern = GDT_SEL_RPL_IS_KERNEL(to_cs);

    bool from_cs64 = GDT_SEL_IS_C64(from_cs);
    bool to_cs64 = GDT_SEL_IS_C64(to_cs);

    bool from_fpu = outgoing->thread_flags & THREAD_FLAGS_ANY_FPU;
    bool to_fpu = incoming->thread_flags & THREAD_FLAGS_ANY_FPU;

    bool from_kern_fpu = outgoing->thread_flags & THREAD_FLAGS_KERNEL_FPU;
    bool to_kern_fpu = incoming->thread_flags & THREAD_FLAGS_KERNEL_FPU;

    enum action_t { ignore, ctrlwords, zeros, dataregs };

    action_t save = ignore;

    if (from_fpu) {
        // Only save control words when fpu is discarded
        if (fpu_state == discarded)
            save = ctrlwords;

        // Don't save FPU again on nested context save
        else if (fpu_state == saved)
            save = ignore;

        // Possibly save FPU registers from kernel mode
        else if (!from_kern || from_kern_fpu)
            save = dataregs;
    }

    action_t restore = ignore;

    if (to_fpu) {
        // In decreasing order of likelihood
        if (!to_kern)
            restore = dataregs;
        else if (!to_kern_fpu)
            restore = ctrlwords;
        else
            restore = dataregs;
    }

    switch (save) {
    case dataregs:
        //
        // The outgoing FPU context needs to be saved

        THREAD_TRACE("Saving fpu context of thread %#x\n",
                     outgoing->thread_id);

        // Save it correctly
        // Don't bother saving FPU for voluntary context switch
        if (ISR_CTX_INTR(ctx) != INTR_THREAD_YIELD) {
            ISR_CTX_FPU(outgoing->ctx) = from_cs64
                    ? isr_save_fpu_ctx64(outgoing)
                    : isr_save_fpu_ctx32(outgoing);
        } else {
            // Only save FPU control words, not data registers
            outgoing->syscall_mxcsr = cpu_mxcsr_get();
            outgoing->syscall_fcw87 = cpu_fcw_get();
            ISR_CTX_FPU(outgoing->ctx) = nullptr;
        }

        fpu_state = saved;
        break;

    case ctrlwords:
        outgoing->syscall_mxcsr = cpu_mxcsr_get();
        outgoing->syscall_fcw87 = cpu_fcw_get();
        ISR_CTX_FPU(outgoing->ctx) = nullptr;
        fpu_state = saved;
        break;

    case zeros:
    case ignore:
        assert(ISR_CTX_FPU(outgoing->ctx) == nullptr);
        break;
    }

    switch (restore) {
    case dataregs:
        //
        // The incoming FPU context needs to be restored

        // If FPU is blocked...
        if (cpu->cr0_shadow & CPU_CR0_TS) {
            // Clear TS flag to unblock access to FPU
            cpu->cr0_shadow &= ~CPU_CR0_TS;
            cpu_cr0_clts();
        }

        THREAD_TRACE("Restoring fpu context of thread %#x\n",
                     incoming->thread_id);

        if (ISR_CTX_FPU(ctx)) {
            // Use the correct one
            if (likely(to_cs64))
                isr_restore_fpu_ctx64(incoming);
            else
                isr_restore_fpu_ctx32(incoming);

        } else if (ISR_CTX_INTR(ctx) == INTR_THREAD_YIELD) {
            // Restore just control words
            cpu_mxcsr_set(incoming->syscall_mxcsr);
            cpu_fcw_set(incoming->syscall_fcw87);
        }

        fpu_state = restored;
        break;

    case ctrlwords:
    case zeros:
        if (!(cpu->cr0_shadow & CPU_CR0_TS))
            cpu_clear_fpu();
        fpu_state = saved;
        break;

    case ignore:
        break;
    }

    if (to_fpu) {
        if (cpu->cr0_shadow & CPU_CR0_TS) {
            // Allow use of FPU
            THREAD_TRACE("FPU allowed\n");
            cpu->cr0_shadow &= ~CPU_CR0_TS;
            cpu_cr0_clts();
        }
    } else if ((cpu->cr0_shadow & CPU_CR0_TS) == 0) {
        // Lock out the FPU
        // Set TS flag to block access to FPU
        THREAD_TRACE("FPU locked out\n");
        cpu->cr0_shadow |= CPU_CR0_TS;
        cpu_cr0_set(cpu->cr0_shadow);
    }
}

void arch_thread_cswitch(thread_info_t* const outgoing,
                         isr_context_t *ctx, thread_info_t *incoming)
{
    assume(outgoing != incoming);

    outgoing->fsbase = cpu_fsbase_get();
    outgoing->gsbase = cpu_altgsbase_get();

    if (outgoing->fsbase != incoming->fsbase)
        cpu_fsbase_set((void*)incoming->fsbase);

    // User threads are expected to always have null gsbase
    if (unlikely(outgoing->gsbase != incoming->gsbase))
        cpu_altgsbase_set((void*)incoming->gsbase);

    // If segments changed
    if (unlikely(ISR_CTX_REG_SEG_IMG(outgoing->ctx) !=
                 ISR_CTX_REG_SEG_IMG(ctx))) {
        if (unlikely(ISR_CTX_REG_DS(outgoing->ctx) !=
                     ISR_CTX_REG_DS(ctx)))
            cpu_ds_set(ISR_CTX_REG_DS(ctx));

        if (unlikely(ISR_CTX_REG_ES(outgoing->ctx) !=
                     ISR_CTX_REG_ES(ctx)))
            cpu_es_set(ISR_CTX_REG_ES(ctx));

        if (unlikely(ISR_CTX_REG_FS(outgoing->ctx) !=
                     ISR_CTX_REG_FS(ctx))) {
            cpu_fs_set(ISR_CTX_REG_FS(ctx));

            cpu_fsbase_set((void*)incoming->fsbase);
        }

        if (unlikely(ISR_CTX_REG_GS(outgoing->ctx) !=
                     ISR_CTX_REG_GS(ctx))) {
            // Move the kernel gsbase to safety
            cpu_swapgs();

            // Load the incoming gs
            cpu_gs_set(ISR_CTX_REG_GS(ctx));

            cpu_gsbase_set((void*)incoming->gsbase);

            // Restore kernel gsbase
            cpu_swapgs();
        }
    }

    // Update CR3
    if (unlikely(ISR_CTX_REG_CR3(outgoing->ctx) !=
                 ISR_CTX_REG_CR3(ctx)))
        cpu_page_directory_set(ISR_CTX_REG_CR3(ctx));
}

_hot
isr_context_t *thread_schedule(isr_context_t *ctx, bool was_timer)
{
    assert(!(cpu_eflags_get() & CPU_EFLAGS_IF));

    cpu_info_t *cpu = this_cpu();

    thread_info_t *thread = cpu->cur_thread;

    thread_info_t * const outgoing = thread;

    // Bootstrap AP idle thread
    if (unlikely(cpu->goto_thread))
        return bootstrap_idle_thread(cpu, thread, ctx);

    // Defer reschedule if locks are held
    if (unlikely(cpu->locks_held)) {
        cpu->csw_deferred = true;
        return ctx;
    }

    // Store context pointer for resume later
    assert(thread->ctx == nullptr ||
           thread->thread_id < thread_t(cpu_count));
    thread->ctx = ctx;

    uint64_t now = time_ns();
    uint64_t elapsed = now - thread->sched_timestamp;
    thread->sched_timestamp = 0;

    //
    // Accumulate used and busy time on this CPU

    accumulate_time(cpu, thread, elapsed);

    cpu_info_t::scoped_lock cpu_lock(cpu->queue_lock);

    // If the thread moved to a different CPU
    if (unlikely(thread_cpu_count() > 1 &&
                 !thread->cpu_affinity[cpu->cpu_nr])) {
        thread_move_to_other_cpu(cpu, thread);
    } else if ((thread->state != THREAD_IS_RUNNING) ||
            (thread->used_time >= thread->preempt_time &&
             thread->thread_id >= thread_t(cpu_count * 2))) {
        // A new timeslice is needed
        // and the thread is eligible for timestamp changes
        assert(thread->schedule_node != ready_set_t::const_iterator());

        // Remove, modify, reinsert tree node

        ready_set_t::node_type sched_node = cpu->ready_list
                .extract(thread->schedule_node);
        thread->schedule_node = ready_set_t::const_iterator();

        if (thread->state == THREAD_IS_RUNNING) {
            // Replenish timeslice and reinsert it into ready queue
            thread->state = THREAD_IS_READY_BUSY;
            thread->preempt_time = thread->used_time + 10000000;
            thread->timeslice_timestamp = now;
            sched_node.value().first = now;
            thread->schedule_node = cpu->ready_list
                    .insert(ext::move(sched_node)).first;
        } else if (thread->state == THREAD_IS_SLEEPING_BUSY) {
            // Insert into sleep queue, keyed on wake time
            sched_node.value().first = thread->wake_time;
            thread->schedule_node = cpu->sleep_list
                    .insert(ext::move(sched_node)).first;
        } else if (thread->state == THREAD_IS_EXITING_BUSY) {
            // Let node destruct
            thread_info_t::scoped_lock thread_lock(thread->lock);

            cpu_lock.unlock();
            thread_signal_completion_locked(thread, thread_lock);
            cpu_lock.lock();
        } else {
            assert(!"?");
        }
    }

    now = time_ns();

    thread_state_t state = thread->state;

    // Change to ready if running
    if (likely(state == THREAD_IS_RUNNING))
        thread->state = THREAD_IS_READY_BUSY;

    // Retry because another CPU might steal this
    // thread after it transitions from sleeping to
    // ready
    int retries = 0;
    for ( ; ; ++retries) {
        thread = thread_choose_next(cpu, outgoing, now);

        assert((thread >= threads + cpu_count &&
                thread < threads + countof(threads)) ||
               thread == threads + cpu->cpu_nr);

        if (thread == outgoing && thread->state == THREAD_IS_READY_BUSY) {
            // This doesn't need to be cmpxchg because the
            // outgoing thread is still marked busy
            thread->state = THREAD_IS_RUNNING;
            break;
        } else if (thread->state == THREAD_IS_READY &&
                atomic_cmpxchg(&thread->state,
                               THREAD_IS_READY, THREAD_IS_RUNNING) ==
                   THREAD_IS_READY) {

            break;
        }

        cpu_debug_break();
        pause();
    }

    ctx = thread->ctx;
    thread->ctx = nullptr;

    // Program rescheduling interrupt for remainder of timeslice
    uint64_t timeslice = thread->preempt_time > thread->used_time
            ? thread->preempt_time - thread->used_time
            : 10000000;

    // If only idle thread or only one thread in addition to idle thread
    // then grant infinite timeslice
    if (cpu->ready_list.size() <= 2 && thread_idle_ready)
        timeslice = UINT64_MAX;

    // Infinite if nothing sleeping
    uint64_t next_sleep_expiry = UINT64_MAX;

    // See the next sleep expiry in sleep queue
    if (!cpu->sleep_list.empty())
        next_sleep_expiry = cpu->sleep_list.cbegin()->first;

    // If the sleep queue requires preempting earlier,
    // then preempt earlier
    if (next_sleep_expiry < UINT64_MAX) {
        assert(next_sleep_expiry > now);
        if (timeslice > next_sleep_expiry - now)
            timeslice = next_sleep_expiry - now;
    }

    // Update CPU usage accounting at least once per second
    if (timeslice > 1000000000)
        timeslice = 1000000000;

    // At least 200 microseconds
    if (timeslice < 200000)
        timeslice = 200000;

    //if (timeslice < UINT64_MAX)
    thread_set_timer(cpu->apic_dcr, timeslice);


    if (thread != outgoing) {
        // Swap context

        thread_csw_fpu(ctx, cpu, outgoing, thread);
        arch_thread_cswitch(outgoing, ctx, thread);
    }

    thread->sched_timestamp = (uint64_t(thread->priority ^ 0xFF) << 56) | now;

    assert(thread->state == THREAD_IS_RUNNING);

    cpu->tss_ptr->rsp[0] = uintptr_t(thread->priv_chg_stack);

    thread->ctx = nullptr;
    assert(ctx != nullptr);
    cpu->cur_thread = thread;

    assert(ctx->gpr.s.r[0] == (GDT_SEL_USER_DATA | 3));
    assert(ctx->gpr.s.r[1] == (GDT_SEL_USER_DATA | 3));
    assert(ctx->gpr.s.r[2] == (GDT_SEL_USER_DATA | 3));
    assert(ctx->gpr.s.r[3] == (GDT_SEL_USER_DATA | 3));

    if (thread != outgoing) {
        cpu->after_csw_fn = thread_clear_busy;
        cpu->after_csw_vp = outgoing;
    } else {
        assert(thread->state == THREAD_IS_RUNNING);
    }

    assert(ctx);
    assert((outgoing->state == THREAD_IS_RUNNING && thread == outgoing) ||
           (outgoing->state & THREAD_BUSY));
    cpu->should_reschedule = false;

    return ctx;
}

static void thread_early_sleep(uint64_t timeout_time)
{
    while (time_ns() < timeout_time)
        halt();
}

void thread_sleep_until(uint64_t timeout_time)
{

    if (thread_idle_ready) {
        thread_info_t *thread = this_thread();

        // Mask timer while transitioning to sleeping
        cpu_scoped_irq_disable irq_dis;
        thread_info_t::scoped_lock thread_lock(thread->lock);

        thread->wake_time = timeout_time;
        thread->state = THREAD_IS_SLEEPING_BUSY;

        thread_lock.unlock();

        thread_yield();
    } else {
        thread_early_sleep(timeout_time);
    }
}

void thread_sleep_for(uint64_t ms)
{
    thread_sleep_until(time_ns() + ms * 1000000);
}

uint64_t thread_get_usage(int id)
{
    if (unlikely(unsigned(id) >= unsigned(countof(threads))))
        return -1;

    thread_info_t *thread = id < 0 ? this_thread() : (threads + id);
    return thread->used_time;
}

// specify UINT64_MAX in timeout_time for infinite timeout
uintptr_t thread_sleep_release(
        spinlock_t *lock, thread_t *thread_id, uint64_t timeout_time)
{
    thread_info_t *thread = this_thread();

    // Idle threads should never try to block and context switch!
    assert(thread->thread_id >= thread_t(cpu_count));

    *thread_id = thread->thread_id;

    thread->wake_time = timeout_time;

    assert(thread->state == THREAD_IS_RUNNING);

    thread_state_t old_state;
    old_state = atomic_cmpxchg(&thread->state,
                               THREAD_IS_RUNNING, THREAD_IS_SLEEPING_BUSY);
    assert(old_state == THREAD_IS_RUNNING);

    spinlock_unlock(lock);

    assert(thread_locks_held() == 0);
    uintptr_t result = thread_yield();
    assert(thread->state == THREAD_IS_RUNNING);

    //!!!not locked on return anymore!!! spinlock_lock(lock);

    return result;
}

void thread_request_reschedule_noirq()
{
    cpu_info_t *cpu = this_cpu();

    // If this weren't true, then someone is setting it when it will be missed
    assert(cpu->in_irq > 0 || !cpu->should_reschedule);

    cpu->should_reschedule = true;
}

void thread_request_reschedule()
{
    cpu_scoped_irq_disable irq_dis;

    thread_request_reschedule_noirq();
}

isr_context_t *thread_reschedule_if_requested_noirq(isr_context_t *ctx)
{
    cpu_info_t *cpu = this_cpu();

    if (cpu->should_reschedule) {
        cpu->should_reschedule = false;
        return thread_schedule(ctx);
    }

    return ctx;
}

isr_context_t *thread_reschedule_if_requested(isr_context_t *ctx)
{
    cpu_scoped_irq_disable irq_dis;

    return thread_reschedule_if_requested_noirq(ctx);
}

_hot
void thread_resume(thread_t tid, intptr_t exit_code)
{
    thread_info_t *resumed_thread = threads + tid;

    for (;;) {
        thread_info_t::scoped_lock lock(resumed_thread->lock);

        if (resumed_thread->state != THREAD_IS_SLEEPING) {
            uint64_t wait_sleeping_st = time_ns();
            cpu_wait_value(&resumed_thread->state, THREAD_IS_SLEEPING);
            uint64_t wait_sleeping_en = time_ns();
            uint64_t wait_sleeping = wait_sleeping_en - wait_sleeping_st;
            THREAD_TRACE("Waited %" PRIu64 "ns to wake thread from sleep\n",
                     wait_sleeping);
        }

        // Transition it to sleeping+busy so another cpu won't touch it
        if (resumed_thread->state == THREAD_IS_SLEEPING &&
            atomic_cmpxchg(&resumed_thread->state, THREAD_IS_SLEEPING,
                           THREAD_IS_SLEEPING_BUSY) == THREAD_IS_SLEEPING) {

            size_t cpu_nr = run_cpu[tid];
            cpu_info_t& cpu = cpus[cpu_nr];
            cpu_info_t::scoped_lock cpu_lock(cpu.queue_lock);

            uint32_t this_cpu_nr = thread_cpu_number();

            if (this_cpu_nr != cpu_nr) {
                // Alternate algorithm for cross-cpu wakeup

                uint64_t wait_st = 0;
                while (!cpu.enqueue_resume(tid, exit_code)) {
                    if (!wait_st)
                        wait_st = time_ns();

                    pause();
                }

                // tattletale
                if (unlikely(wait_st)) {
                    uint64_t wait_en = time_ns();

                    THREAD_TRACE("Waited %ss for resume ring"
                             " of cpu %zu from cpu %u\n",
                             engineering_t<uint64_t>(
                                 wait_en - wait_st, -3).ptr(),
                             cpu_nr, this_cpu_nr);
                }
                resumed_thread->state = THREAD_IS_SLEEPING;
                apic_send_ipi(cpu.apic_id, INTR_IPI_RESCHED);
                cpu_lock.unlock();

                return;
            }

            // Remove node from the sleep queue for reuse in ready queue
            ready_set_t::node_type node = cpu.sleep_list.extract(
                        resumed_thread->schedule_node);

            node.value().first = resumed_thread->timeslice_timestamp;

//            printdbg("Waking sleeping thread %u\n",
//                     resumed_thread->thread_id);
            resumed_thread->schedule_node = cpu.ready_list
                    .insert(ext::move(node)).first;

            //dump_scheduler_list("ready list:", cpu.ready_list);

            // Should be a fast, voluntarily yielded context
            assert(ISR_CTX_CTX_FLAGS(resumed_thread->ctx) &
                   (1<<ISR_CTX_CTX_FLAGS_FAST_BIT));

            // Set return value
            ISR_CTX_ERRCODE(resumed_thread->ctx) = exit_code;

            // Done manipulating it, mark it ready
            resumed_thread->state = THREAD_IS_READY;

            // True if the resumed thread should run immediately
            bool need_resched = (resumed_thread->schedule_node ==
                            cpu.ready_list.cbegin());

            if (need_resched)
                thread_request_reschedule_noirq();

            cpu_lock.unlock();

            return;
        }

        if (resumed_thread->state == THREAD_IS_SLEEPING_BUSY &&
                atomic_cmpxchg(&resumed_thread->state, THREAD_IS_SLEEPING_BUSY,
                               THREAD_IS_READY_BUSY) == THREAD_IS_SLEEPING_BUSY)
            return;

        THREAD_TRACE("Did not resume %d! Retrying I guess\n", tid);
    }
}

intptr_t thread_wait(thread_t thread_id)
{
    thread_info_t *thread = threads + thread_id;

    thread_info_t::scoped_lock lock(thread->lock);

    thread_add_ref(thread, lock);

    while (thread->state != THREAD_IS_FINISHED &&
           thread->state != THREAD_IS_FINISHED_BUSY)
        thread->done_cond.wait(lock);

    thread_release_ref(thread, lock);

    return thread->exit_code;
}

uint32_t thread_cpus_started()
{
    return thread_aps_running + 1;
}

thread_t thread_get_id()
{
    if (likely(thread_cls_ready)) {
        thread_t thread_id;

        thread_info_t *cur_thread = this_thread();
        thread_id = cur_thread->thread_id;

        return thread_id;
    }

    // Too early to get a thread ID
    return 0;
}

void thread_set_gsbase(thread_t tid, uintptr_t gsbase)
{
    if (unlikely(!mm_is_user_range((void*)gsbase, 1)))
        panic("Almost set insecure user gsbase!");

    thread_info_t *thread = this_thread();

    if (likely(tid < 0))
        tid = thread->thread_id;

    if (likely(uintptr_t(tid) < thread_count))
        threads[tid].gsbase = (void*)gsbase;

    if (likely(thread->thread_id == tid))
        cpu_altgsbase_set((void*)gsbase);
}

void thread_set_fsbase(thread_t tid, uintptr_t fsbase)
{
    if (unlikely(!mm_is_user_range((void*)fsbase, 1)))
        panic("Almost set insecure user fsbase!");

    thread_info_t *self = this_thread();

    if (likely(tid < 0))
        tid = self->thread_id;

    if (uintptr_t(tid) < countof(threads))
        threads[tid].fsbase = (void*)fsbase;

    if (likely(self->thread_id == tid))
        cpu_fsbase_set((void*)fsbase);
}

thread_cpu_mask_t const* thread_get_affinity(int id)
{
    return &threads[id].cpu_affinity;
}

size_t thread_get_cpu_count()
{
    return cpu_count;
}

void thread_set_affinity(int id, thread_cpu_mask_t const &affinity)
{
    cpu_scoped_irq_disable intr_was_enabled;

    cpu_info_t *cpu = this_cpu();

    size_t cpu_nr = cpu->cpu_nr;

    thread_info_t *thread = threads + (id >= 0 ? id : thread_get_id());

    thread->cpu_affinity = affinity;

    if ((affinity[run_cpu[id]]) == false) {
        // Home CPU is not in the affinity mask
        // Move home to a cpu in the affinity mask
        run_cpu[id] = affinity.lsb_set();
    }

    // Are we changing current thread affinity?
    if (cpu->cur_thread->thread_id == id) {
        while (!(affinity[cpu_nr])) {
            // Get off this CPU
            thread_yield();

            // Check again, a racing CPU may have picked
            // up this thread without seeing change
            cpu = this_cpu();
            cpu_nr = cpu->cpu_nr;
        }
    }
}

thread_priority_t thread_get_priority(thread_t thread_id)
{
    return threads[thread_id].priority;
}

void thread_set_priority(thread_t thread_id,
                                thread_priority_t priority)
{
    threads[thread_id].priority = priority;
}

void thread_check_stack(int intr)
{
    char *sp = (char*)cpu_stack_ptr_get();

    size_t ist_slot = 0;

    switch (intr) {
    case INTR_EX_STACK:
        ist_slot = IDT_IST_SLOT_STACK;
        break;

    case INTR_EX_DBLFAULT:
        ist_slot = IDT_IST_SLOT_DBLFAULT;
        break;

    case INTR_IPI_FL_TRACE:
        ist_slot = IDT_IST_SLOT_FLUSH_TRACE;
        break;

    case INTR_EX_NMI:
        ist_slot = IDT_IST_SLOT_NMI;
        break;

    }

    cpu_scoped_irq_disable irq_dis;

    cpu_info_t *cpu = this_cpu();

    ext::pair<void *, void *> stk;

    if (ist_slot) {
        stk = idt_get_ist_stack(cpu->cpu_nr, ist_slot);
    } else {
        stk.first = cpu->cur_thread->stack - cpu->cur_thread->stack_size;
        stk.second = cpu->cur_thread->stack;
    }

    if (sp > stk.second || sp < stk.first) {
        cpu_debug_break();
        cpu_crash();
    }
}

void thread_idle_set_ready()
{
    thread_idle_ready = 1;
}

void *thread_get_exception_top()
{
    // Ensure that threading is initialized, in case of early exception
    if (likely(thread_get_cpu_count())) {
        thread_info_t *thread = this_thread();
        return thread->exception_chain;
    }
    return nullptr;
}

void *thread_set_exception_top(void *chain)
{
    thread_info_t *thread = this_thread();

    void *old = thread->exception_chain;
    thread->exception_chain = chain;
    return old;
}

size_t thread_cls_alloc()
{
    size_t next;

    while ((next = storage_next_slot) < cpu_info_t::max_cls) {
        if (atomic_cmpxchg(&storage_next_slot, next + 1, next) == next)
            return next;
    }

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
    cpu_info_t *cpu = other_only ? this_cpu() : nullptr;
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

uint32_t thread_cpu_number()
{
    return cpu_gs_read<uint32_t, offsetof(cpu_info_t, cpu_nr)>();
}

_hot
isr_context_t *thread_schedule_postirq(isr_context_t *ctx)
{
    cpu_info_t *cur_cpu = this_cpu();
    thread_info_t *cur_thread = cur_cpu->cur_thread;
    unsigned tid = cur_thread->thread_id;

    // If idle thread was interrupted,
    // or the SLIH thread is ready and the SLIH thread wasn't running already
    if ((thread_idle_ready && tid < cpu_count) ||
            ((threads[cpu_count + cur_cpu->cpu_nr].state == THREAD_IS_READY) &&
             tid != cpu_count + cur_cpu->cpu_nr))
        return thread_schedule(ctx);

    return ctx;
}

isr_context_t *thread_schedule_if_requested(isr_context_t *ctx)
{
    cpu_scoped_irq_disable irq_dis;
    return thread_schedule_if_requested_noirq(ctx);
}

isr_context_t *thread_schedule_if_requested_noirq(isr_context_t *ctx)
{
    cpu_info_t *cpu = this_cpu();

    if (cpu->should_reschedule) {
        cpu->should_reschedule = false;
        return thread_schedule(ctx);
    }

    return ctx;
}

process_t *thread_current_process()
{
    thread_info_t *thread = this_thread();
    return thread->process;
}

unsigned thread_current_cpu(thread_t tid)
{
    cpu_info_t *cpu = tid < 0 ? this_cpu() : &cpus[run_cpu[tid]];
    return cpu->cpu_nr;
}

uint32_t thread_get_cpu_apic_id(uint32_t cpu)
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

void *thread_get_fsbase(int thread)
{
    if (cpu_count) {
        thread_info_t *info = thread >= 0 ? threads + thread : this_thread();
        return info->fsbase;
    }
    return nullptr;
}

void *thread_get_gsbase(int thread)
{
    if (cpu_count) {
        thread_info_t *info = thread >= 0 ? threads + thread : this_thread();
        return info->gsbase;
    }
    return nullptr;
}

void thread_set_process(int tid, process_t *process)
{
    thread_info_t *thread = tid >= 0 ? threads + tid : this_thread();
    thread->process = process;
}

void thread_tss_ready(void*)
{
    cpus[0].tss_ptr = tss_list;
}

REGISTER_CALLOUT(thread_tss_ready, nullptr,
                 callout_type_t::tss_list_ready, "000");

void thread_terminate(thread_t tid)
{
    printdbg("Fixme: thread_terminate unimplemented\n");
}

void thread_exit(int exit_code)
{
    thread_info_t *info = this_thread();
    thread_t tid = info->thread_id;
    info->exit_code = exit_code;
    info->process->del_thread(tid);
    thread_cleanup();
}

cpu_info_t *thread_set_cpu_gsbase(int ap)
{
    cpu_info_t *cpu = ap ? this_cpu_by_apic_id_slow() : cpus;
    // Assert that the startup code setup gsbase correctly
    assert((cpu_info_t*)cpu_gsbase_get() == cpu);
    //cpu_gsbase_set(cpu);
    return cpu;
}

void thread_init_cpu_count(int count)
{
    for (size_t i = 0, e = countof(run_cpu); i != e; ++i)
        run_cpu[i] = i % count;
}

void thread_init_cpu(size_t cpu_nr, uint32_t apic_id)
{
    cpu_info_t *cpu = cpus + cpu_nr;
    cpu->self = cpu;
    cpu->apic_id = apic_id;
    cpu->cpu_nr = cpu_nr;
    cpu->cur_thread = threads + cpu_nr;
}

// PCID address space is 4096 bits
//  0 bits are free pcids, 1 bits are taken pcids
//  4096 is 64*64, so represent it with a 2-level hierarchy
//  The underlying maps are in pcid_alloc_map 1 thru 64 inclusive
//  The top map is stored in pcid_alloc_map[0]
//  The top map will have 1 bits if all of the underlying 64 bits are 1
//  The top map will have 0 bits if any of the underlying 64 bits are 0
static uint64_t pcid_alloc_map[65];

int thread_pcid_alloc()
{
    // The top map will be all 1 bits when all pcids are taken
    if (unlikely(~pcid_alloc_map[0] == 0))
        return -1;

    // Find the first qword with a 0 bit
    size_t word = bit_lsb_set(~pcid_alloc_map[0]);

    // Find the first 0 bit in that qword
    uint8_t bit = bit_lsb_set(~pcid_alloc_map[word+1]);

    uint64_t upd = pcid_alloc_map[word+1] | (UINT64_C(1) << bit);

    pcid_alloc_map[word+1] = upd;

    // Build a mask that will set the top bit to 1
    // if all underlying bits are now 1
    uint64_t top_mask = (UINT64_C(1) << word) & -(~upd == 0);

    pcid_alloc_map[0] |= top_mask;

    return int(word << 6) + bit;
}

void thread_pcid_free(int pcid)
{
    assert(size_t(pcid) < 4096);

    size_t word = unsigned(pcid) >> 6;

    uint8_t bit = word & 63;

    // Clear that bit
    pcid_alloc_map[word+1] &= ~(UINT64_C(1) << bit);

    // Since we freed one, we know that the bit of level 0 must become 0
    pcid_alloc_map[0] &= ~(UINT64_C(1) << word);
}

unsigned thread_cpu_usage_x1M(size_t cpu)
{
    return cpus[cpu].busy_percent_x1M;
}

void thread_add_cpu_irq_time(uint64_t tsc_ticks)
{
    atomic_add((cpu_gs_ptr<uint64_t, offsetof(cpu_info_t, irq_time)>()),
               tsc_ticks);
}

void thread_cls_init_early(int ap)
{
    cpu_info_t *cpu = thread_set_cpu_gsbase(ap);
    cpu->self = cpu;
    cpu->cpu_nr = cpu - cpus;
}

uint32_t thread_locks_held()
{
    cpu_info_t *cpu = this_cpu();
    return cpu->locks_held;
}

__cxa_eh_globals *thread_cxa_get_globals()
{
    return &this_thread()->cxx_exception_info;
}

void thread_panic_other_cpus()
{
    if (thread_cpu_count() > 1)
        apic_send_ipi(-1, INTR_IPI_PANIC);
}

// =========


//int __gthread_active_p(void)
//{
//  return 0;
//}
//
//int __gthread_once(__gthread_once_t *__once, void(*__func)(void))
//{
//  return 0;
//}
//
//static inline int
//__gthread_key_create(__gthread_key_t *__key, void(*__func)(void *))
//{
//  return 0;
//}
//
//static int
//__gthread_key_delete(__gthread_key_t __key)
//{
//  return 0;
//}
//
//static inline void *
//__gthread_getspecific(__gthread_key_t __key)
//{
//  return 0;
//}
//
//static inline int
//__gthread_setspecific(__gthread_key_t __key, void const *__v)
//{
//  return 0;
//}
//
//static inline int
//__gthread_mutex_destroy(__gthread_mutex_t *__mutex)
//{
//  return 0;
//}
//
//static inline int
//__gthread_mutex_lock(__gthread_mutex_t *__mutex)
//{
//  return 0;
//}
//
//static inline int
//__gthread_mutex_trylock(__gthread_mutex_t *__mutex)
//{
//  return 0;
//}
//
//static inline int
//__gthread_mutex_unlock(__gthread_mutex_t *__mutex)
//{
//  return 0;
//}
//
//static inline int
//__gthread_recursive_mutex_lock(__gthread_recursive_mutex_t *__mutex)
//{
//  return __gthread_mutex_lock(__mutex);
//}
//
//static inline int
//__gthread_recursive_mutex_trylock(__gthread_recursive_mutex_t *__mutex)
//{
//  return __gthread_mutex_trylock(__mutex);
//}
//
//static inline int
//__gthread_recursive_mutex_unlock(__gthread_recursive_mutex_t *__mutex)
//{
//  return __gthread_mutex_unlock(__mutex);
//}
//
//static inline int
//__gthread_recursive_mutex_destroy(__gthread_recursive_mutex_t *__mutex)
//{
//  return __gthread_mutex_destroy(__mutex);
//}

isr_context_t *thread_entering_irq(isr_context_t *ctx)
{
    if (unlikely((!++*cpu_gs_ptr<uint8_t, CPU_INFO_IN_IRQ_OFS>())))
        panic("Excessive IRQ nesting\n");

    return ctx;
}

isr_context_t *thread_finishing_irq(isr_context_t *ctx)
{
    if (!--*cpu_gs_ptr<uint8_t, CPU_INFO_IN_IRQ_OFS>())
        return thread_reschedule_if_requested_noirq(ctx);
    return ctx;
}

void arch_jump_to_user(uintptr_t ip, uintptr_t sp,
                       uintptr_t kernel_sp, bool use64,
                       uintptr_t arg0, uintptr_t arg1, uintptr_t arg2)
{
    cpu_info_t *cpu = this_cpu();
    thread_info_t *thread = cpu->cur_thread;

    // Update TSS kernel stack pointer before entering user mode
    thread->priv_chg_stack = (char*)kernel_sp;
    cpu->tss_ptr->rsp[0] = kernel_sp;
    cpu_fsbase_set(thread->fsbase);
    cpu_altgsbase_set(thread->gsbase);

    isr_sysret(ip, sp, kernel_sp, use64, arg0, arg1, arg2);
}

__BEGIN_DECLS

_noreturn
void sys_sigreturn_impl_32(void *mctx, bool xsave);

_noreturn
void sys_sigreturn_impl_64(void *mctx, bool xsave);

__END_DECLS

int sys_sigreturn(void *mctx)
{
    process_t *process = fast_cur_process();

    char data[3072];

    bool use_xsave = cpuid_has_xsave();

    size_t fpuctx_sz = sizeof(mcontext_x86_fpu_t);

    uintptr_t data_ptr = uintptr_t(data + sizeof(data));
    if (use_xsave) {
        if (unlikely(sse_context_size > sizeof(data) ||
                     ((data_ptr - sse_context_size) & -64) < uintptr_t(data)))
            panic("Cannot handle SSE state, too large, please fix");

        fpuctx_sz = sse_context_size;
    }

    data_ptr -= fpuctx_sz;

    if (cpuid_has_xsave())
        data_ptr &= -64;
    else
        data_ptr &= -16;

    assert(data_ptr >= uintptr_t(data));

    mcontext_x86_fpu_t *fpuctx = (mcontext_x86_fpu_t*)data_ptr;

    if (process->use64) {
        if (unlikely(!mm_is_user_range(mctx, sizeof(mcontext_t))))
            return -int(errno_t::EFAULT);

        mcontext_t ctx{};

        // Copy the restored general register context from userspace
        if (unlikely(!mm_copy_user(&ctx, mctx, sizeof(ctx))))
            return -int(errno_t::EFAULT);

        // Validate the user FPU context pointer points to userspace
        if (unlikely(!mm_is_user_range((void*)ctx.__fpu, sizeof(*fpuctx))))
            return -int(errno_t::EFAULT);

        // Copy the fpu context into kernel space
        if (unlikely(!mm_copy_user(fpuctx, (void*)ctx.__fpu, fpuctx_sz)))
            return -int(errno_t::EFAULT);

        // Point kernel copy of context at kernel copy of FPU context
        ctx.__fpu = uintptr_t(fpuctx);

        assert(ctx.__cs == (GDT_SEL_USER_CODE64|3));
        assert(ctx.__ss == (GDT_SEL_USER_DATA|3));

        assert(!(ctx.__rflags & (CPU_EFLAGS_IOPL | CPU_EFLAGS_NT |
                               CPU_EFLAGS_VIF | CPU_EFLAGS_VIP |
                               CPU_EFLAGS_VM)));

        assert((ctx.__rflags & (CPU_EFLAGS_IF | CPU_EFLAGS_ALWAYS)) ==
               (CPU_EFLAGS_IF | CPU_EFLAGS_ALWAYS));

        sys_sigreturn_impl_64(&ctx, use_xsave);
    } else {
        if (unlikely(!mm_is_user_range(mctx, sizeof(mcontext32_t))))
            return -int(errno_t::EFAULT);

        mcontext32_t ctx{};

        if (unlikely(!mm_copy_user(&ctx, mctx, sizeof(ctx))))
            return -int(errno_t::EFAULT);

        if (unlikely(!mm_is_user_range((void*)uintptr_t(ctx.__fpu),
                                       sizeof(*fpuctx))))
            return -int(errno_t::EFAULT);

        if (unlikely(!mm_copy_user(&fpuctx, (void*)(uintptr_t)ctx.__fpu,
                                   sizeof(*fpuctx))))
            return -int(errno_t::EFAULT);

        ctx.__fpu = uintptr_t(fpuctx);

        assert(ctx.__cs == (GDT_SEL_USER_CODE32|3));
        assert(ctx.__ss == (GDT_SEL_USER_DATA|3));

        assert(!(ctx.__eflags & (CPU_EFLAGS_IOPL | CPU_EFLAGS_NT |
                               CPU_EFLAGS_VIF | CPU_EFLAGS_VIP |
                               CPU_EFLAGS_VM)));

        assert((ctx.__eflags & (CPU_EFLAGS_IF | CPU_EFLAGS_ALWAYS)) ==
               (CPU_EFLAGS_IF | CPU_EFLAGS_ALWAYS));

        sys_sigreturn_impl_32(&ctx, use_xsave);
    }
}
