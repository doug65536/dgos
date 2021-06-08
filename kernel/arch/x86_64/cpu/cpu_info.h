#pragma once
#include "types.h"
#include "isr.h"
#include "gdt.h"
#include "mutex.h"
#include "basic_set.h"

// might as well use the exact same one as contig_alloc
using ready_set_t = ext::fast_set<ext::pair<uintptr_t, uintptr_t>>;

enum fpu_state_t : uint8_t {
    // FPU is discarded at entry to syscall
    discarded,
    saved,
    restored
};

struct alignas(512) cpu_info_t {
    constexpr cpu_info_t() = default;
    cpu_info_t(cpu_info_t const&) = delete;
    ~cpu_info_t() = default;

    //
    // exclusive use by this cpu

    cpu_info_t *self = nullptr;

    thread_info_t * volatile cur_thread = nullptr;

    // Accessed every syscall
    tss_t *tss_ptr = nullptr;
    uintptr_t syscall_flags = 0;

    bool online = false;
    fpu_state_t fpu_state = discarded;
    uint8_t apic_dcr = 0;

    // in_irq is incremented after irq entry and decremented before irq exit
    // you can expect setting should_reschedule to work when in_irq is > 0
    // thread_entering_irq() and thread_finishing_irq() adjust the value
    // of in_irq
    uint8_t in_irq = 0;

    // Which thread's context is in the FPU, or -1 if discarded
    thread_t fpu_owner = -1;

    thread_info_t *goto_thread = nullptr;

    isr_syscall_context_t *syscall_ctx = nullptr;

    // Context switch is prevented when this is nonzero
    uint32_t locks_held = 0;
    // When locks_held transitions to zero, a context switch is forced
    // when this is true. Deferring a context switch because locks_held
    // is nonzero sets this to true
    uint32_t csw_deferred = 0;

    // --- cache line ---

    // Scheduler/context switch data

    uint32_t apic_id = ~uint32_t(0);
    uint32_t time_ratio = 0;

    uint32_t busy_ratio = 0;
    uint32_t busy_percent_x1M = 0;

    uint32_t cr0_shadow = 0;
    uint32_t cpu_nr = 0;

    uint64_t irq_count = 0;

    uint64_t irq_time = 0;

    uint64_t pf_count = 0;

    // Cleanup to be run after switching stacks on a context switch
    void (*after_csw_fn)(void*) = nullptr;
    void *after_csw_vp = nullptr;

    // --- cache line ---

    static constexpr const size_t max_cls = 8;
    void *storage[max_cls] = {};

    // --- cache line ---

    // Resume ring. Cross-CPU thread_resume will enqueue a thread_id
    // here and kick the target CPU with a reschedule IPI

    // Each resumed thread takes 3 slots, thread id, exitcode low, exitcode hi
    // Room enough for 4 items
    struct resume_ent_t {
        thread_t tid;
        uint32_t ret_lo;
        uint32_t ret_hi;
    };

    static constexpr size_t resume_ring_sz = 14;
    resume_ent_t resume_ring[resume_ring_sz];
    ext::atomic_uint8_t resume_head;
    ext::atomic_uint8_t resume_tail;
    bool should_reschedule;
    uint8_t reserved5[5];
    ext::irq_spinlock resume_lock;

    // --- ^ 3 cache lines ---

    using lock_type = ext::irq_spinlock;
    using scoped_lock = ext::unique_lock<lock_type>;

    // This lock protects ready_list and sleep_list
    lock_type queue_lock;

    // Oldest timeslice first
    // Tree sorted in order of when timestamp was issued.
    // Only threads that are ready to run appear in this tree.
    // Threads that block on I/O and keep an old timeslice are
    // selected to run before threads that keep using up their
    // timeslice and therefore have newer timeslices.
    // The threaded interrupt handler permanently has a timestamp
    // equal to 1, making it utterly overrule every thread
    ready_set_t ready_list;

    // Sleeping threads are keyed here in the order they wake up,
    // using (thread->wake_time). Threads waiting forever are here too.
    ready_set_t sleep_list;

    uint64_t volatile tlb_shootdown_count = 0;

    static inline constexpr uint32_t resume_next(uint32_t curr)
    {
        return curr + 1 < sizeof(resume_ring) / sizeof(*resume_ring)
                ? curr + 1 : 0;
    }

    bool enqueue_resume(thread_t tid, uintptr_t value)
    {
        size_t head = resume_head;

        if (unlikely(resume_next(head) == resume_tail)) {
            printdbg("Resume ring was full!");
            return false;
        }

        //THREAD_TRACE("Enqueuing cross cpu resume, tid=%d\n", tid);

        resume_ring[head].tid = tid;
        resume_ring[head].ret_lo = uint32_t(value & 0xFFFFFFFFU);
        resume_ring[head].ret_hi = uint32_t((value >> 32) & 0xFFFFFFFFU);

        resume_head = resume_next(resume_head);

        return true;
    }
};
