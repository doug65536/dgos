#include "eainstrument.h"
#include "arch/x86_64/cpu/asm_constants.h"
#include "arch/x86_64/cpu/control_regs.h"
#include "likely.h"

#include "irq.h"
#include "cpu/interrupts.h"
#include "cpu/apic.h"
#include "thread.h"
#include "printk.h"

// Using hacky hardcoded stubs to avoid _no_instrument proliferation

static constexpr unsigned trace_io_port = 0xEA;

static int volatile eainst_lock;

int eainst_flush_ready;

_no_instrument
static uintptr_t eainst_lock_acquire()
{
    uintptr_t flags;
    __asm__ __volatile__ (
        "pushfq\n\t"
        "popq %q[flags]\n\t"
        "cli\n\t"
        : [flags] "=r" (flags)
    );
    return flags;
}

_no_instrument
static void eainst_lock_release(uintptr_t flags)
{
    __asm__ __volatile__ (
        "pushq %[flags]\n\t"
        "popfq\n\t"
        :
        : [flags] "r" (flags)
    );
}

struct eainst_scoped_irq_dis {
    _no_instrument
    eainst_scoped_irq_dis() { flags = eainst_lock_acquire(); }

    _no_instrument
    ~eainst_scoped_irq_dis() { eainst_lock_release(flags); }
    uintptr_t flags;
};

// Return thread id in low 32 bits, apicid in high 32 bits
_no_instrument
static uintptr_t eainst_get_current_thread(uint64_t* tsc)
{
    intptr_t ids;
    __asm__ __volatile__ (
        "rdtscp\n\t"

        // rdtscp puts tsc in edx:eax and CPU_MSR_TSC_AUX (0xC0000103) into ecx
        "shlq $32,%%rdx\n\t"
        "orq %%rdx,%%rax\n\t"
        "movq %%rax,(%[tsc_ptr])\n\t"

        // Get APIC ID into high 32 bits of rcx
        "shlq $32,%%rcx\n\t"

        // Get GSBASE into RAX, to handle null GS (and give 0 TID if too early)
        "rdgsbase %q[ids]\n\t"
        "testq %q[ids],%q[ids]\n\t"
        "jz 0f\n\t"

        // GSBASE was nonzero...

        // Get current thread from CPU local storage
        "movq %c[cur_thread_ofs](%[ids]),%q[ids]\n\t"
        "testq %q[ids],%q[ids]\n\t"
        "jz 0f\n\t"

        // We got the thread pointer, load thread ID from current thread
        "movl %c[thread_id_ofs](%[ids]),%k[ids]\n\t"
        "0:\n\t"

        // Merge the APIC ID into the result
        "orq %%rcx,%[ids]\n\t"

        : [ids] "=&a" (ids)
        : [cur_thread_ofs] "n" (CPU_INFO_CURTHREAD_OFS)
        , [thread_id_ofs] "n" (THREAD_THREAD_ID_OFS)
        , [tsc_ptr] "D" (tsc)
        : "memory", "rdx", "rcx"
    );
    return ids;
}

// This gets tweaked
static size_t trace_queue_pool_capacity =
        16384 / sizeof(trace_item);

// Statically reserved buffer, so traces can be collected instantly
static _section(".instr")
trace_item trace_queue[16384 / sizeof(trace_item)];

// Cache line sized count, to avoid false sharing
struct alignas(64) eainst_count_t {
    size_t count;
};

// Pointer to array of per-cpu cache line aligned counts
static _section(".instr")
eainst_count_t *trace_queue_counts;

static _section(".instr")
eainst_count_t early_trace_count;

static _section(".instr")
eainst_count_t early_trace_lock;

// Number of entries per cpu
static _section(".instr")
size_t trace_queue_stride;

_no_instrument
static void emit_trace_output(
        trace_item const *items, size_t byte_count)
{
    __asm__ __volatile__ (
        "rep outsb\n\t"
        : "+S" (items)
        , "+c" (byte_count)
        : "d" (trace_io_port)
        : "memory"
    );

}

static eainst_count_t trace_output_lock;

_no_instrument
static inline void eainst_acquire_output_lock()
{
    while (atomic_ld_acq(&trace_output_lock.count) != 0 ||
           atomic_cmpxchg(&trace_output_lock.count, 0, 1) != 0)
        __builtin_ia32_pause();
}

_no_instrument
static inline bool eainst_try_output_lock()
{
    for (size_t tries = 1000; tries > 0; --tries, __builtin_ia32_pause()) {
        if (likely(atomic_ld_acq(&trace_output_lock.count) == 0))
            if (likely(atomic_cmpxchg(&trace_output_lock.count, 0, 1) == 0))
                return true;
    }
    return false;
}

_no_instrument
static void eainst_release_output_lock()
{
    atomic_st_rel(&trace_output_lock.count, 0);
}

struct eainst_scoped_output_lock {
    struct try_lock_t {};

    _no_instrument
    eainst_scoped_output_lock() : locked(false)
    {
        eainst_acquire_output_lock();
        locked = true;
    }

    _no_instrument
    eainst_scoped_output_lock(bool acquire)
        : locked(acquire ? (eainst_acquire_output_lock(), true) : false)
    {
    }

    _no_instrument
    eainst_scoped_output_lock(try_lock_t)
        : locked(eainst_try_output_lock())
    {
    }

    _no_instrument
    ~eainst_scoped_output_lock()
    {
        if (locked)
            eainst_release_output_lock();
    }

    bool lock()
    {
        if (!locked) {
            eainst_acquire_output_lock();
            locked = true;
        }
        return locked;
    }

    void unlock()
    {
        if (locked)
            eainst_release_output_lock();
        locked = false;
    }

    bool is_locked() const
    {
        return locked;
    }

    operator bool() const
    {
        return is_locked();
    }

    bool locked;
};

_no_instrument
static void eainst_write_record_early(trace_item const& item);

_no_instrument
void eainst_flush_cpu(int cid, size_t &cpu_queue_count, bool weak)
{
    if (weak) {
        eainst_scoped_output_lock lock{eainst_scoped_output_lock::try_lock_t()};
        if (lock.is_locked()) {
            emit_trace_output(&trace_queue[cid * trace_queue_stride],
                sizeof(trace_item) * cpu_queue_count);
            cpu_queue_count = 0;
        }
    } else {
        eainst_scoped_output_lock lock;
        emit_trace_output(&trace_queue[cid * trace_queue_stride],
                sizeof(trace_item) * cpu_queue_count);
        cpu_queue_count = 0;
    }
}

// Runs on all CPUs to periodically flush trace output for threads
// with low activity
_no_instrument
static isr_context_t *eainst_intr_handler(int intr, isr_context_t *ctx)
{
    apic_eoi_noinst(intr);

    int cpu_nr = thread_cpu_number();
    size_t &cpu_queue_count = trace_queue_counts[cpu_nr].count;

    if (cpu_queue_count) {
        //printdbg("cpu %d: Flushing approx %zu trace records\n",
        //         cpu_nr, cpu_queue_count);
        eainst_flush_cpu(cpu_nr, cpu_queue_count, true);
    } else {
        //printdbg("cpu %d: Nothing to flush\n", cpu_nr);
    }

    return ctx;
}

_hot _no_instrument
static void eainst_write_record(trace_item const& item)
{
    if (unlikely(!trace_queue_counts))
        return eainst_write_record_early(item);

    // Lookup the queued item count for this cpu
    size_t &cpu_queue_count = trace_queue_counts[item.cid].count;

    size_t slot = cpu_queue_count++;

    // Append record
    trace_queue[item.cid * trace_queue_stride + slot] = item;

    size_t effective_stride =
            trace_queue_counts
            ? trace_queue_stride
            : trace_queue_pool_capacity;

    // If not full, fastpath, return early
    if (likely(slot < effective_stride))
        return;

    eainst_flush_cpu(item.cid, cpu_queue_count, false);
}

_no_instrument
static void eainst_acquire_early_trace_lock()
{
    while (atomic_ld_acq(&early_trace_lock.count) != 0 ||
           atomic_cmpxchg(&early_trace_lock.count, 0, 1) != 0)
        __builtin_ia32_pause();
}

_no_instrument
static void eainst_release_early_trace_lock()
{
    atomic_st_rel(&early_trace_lock.count, 0);
}

struct eainst_scoped_early_trace_lock {
    _no_instrument
    eainst_scoped_early_trace_lock() { eainst_acquire_early_trace_lock(); }

    _no_instrument
    ~eainst_scoped_early_trace_lock() { eainst_release_early_trace_lock(); }
};

_no_instrument
static void eainst_write_record_early(trace_item const& item)
{
    eainst_scoped_early_trace_lock early_lock;

    if (unlikely(trace_queue_counts)) {
        // Racing thread transistioned us to efficient trace mode
        // Go back to writing normally, not early anymore
        return eainst_write_record(item);
    }

    // Lookup the queued item count for this cpu
    size_t &cpu_queue_count = early_trace_count.count;

    size_t slot = atomic_ld_acq(&cpu_queue_count);

    // Append record
    trace_queue[item.cid * trace_queue_stride + slot] = item;

    atomic_st_rel(&cpu_queue_count, slot + 1);

    // If not full, fastpath, return early
    if (likely(slot + 1 < trace_queue_pool_capacity))
        return;

    eainst_flush_cpu(0, cpu_queue_count, false);
}

void eainst_set_cpu_count(int count)
{
    {
        eainst_scoped_early_trace_lock early_lock;

        // Flush early events if any
        if (early_trace_count.count > 0) {
            emit_trace_output(&trace_queue[0],
                    early_trace_count.count * sizeof(trace_item));
            early_trace_count.count = 0;
        }

        // Address of count array
        trace_queue_counts = (eainst_count_t*)
                (trace_queue + trace_queue_pool_capacity) - count;

        // Recalculate trace pool capacity with counts space clipped off
        trace_queue_pool_capacity = (trace_item*)trace_queue_counts -
                trace_queue;

        for (auto it = trace_queue_counts, e = it + count; it != e; ++it)
            it->count = 0;

        // Round end of buffering area down to cache line boundary
        char *buffer_en = (char*)(uintptr_t(trace_queue_counts) & -64);
        char *buffer_st = (char*)trace_queue;
        size_t buffer_sz = buffer_en - buffer_st;
        buffer_en = buffer_st + buffer_sz;

        // Compute total number of items possible to buffer
        size_t buffer_cnt = buffer_sz / sizeof(trace_item);

        trace_queue_stride = buffer_cnt / count;
    }

    // Start periodically flushing
    intr_hook(INTR_FLUSH_TRACE, eainst_intr_handler, "eainst-flush");

    atomic_st_rel(&eainst_flush_ready, 1);
}

_hot _no_instrument
void __cyg_profile_func_enter(void *this_fn, void * /*call_site*/)
{
    trace_item item;
    uintptr_t tid = eainst_get_current_thread(&item.tsc);
    eainst_scoped_irq_dis lock;
    item.set_ip(this_fn);
    item.set_tid(tid & 0xFFFFFFFF);
    item.set_cid(tid >> 32);
    item.irq_en = lock.flags & CPU_EFLAGS_IF;
    item.call = true;
    eainst_write_record(item);
}

_hot _no_instrument
void __cyg_profile_func_exit(void *this_fn, void * /*call_site*/)
{
    trace_item item;
    uintptr_t tid = eainst_get_current_thread(&item.tsc);
    eainst_scoped_irq_dis lock;
    item.set_ip(this_fn);
    item.set_tid(tid & 0xFFFFFFFF);
    item.set_cid(tid >> 32);
    item.irq_en = lock.flags & CPU_EFLAGS_IF;
    item.call = false;
    eainst_write_record(item);
}
