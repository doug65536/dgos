#pragma once
#include "types.h"
#include "cpu/spinlock.h"
#include "errno.h"
#include "algorithm.h"
#include "bitsearch.h"

__BEGIN_DECLS

struct process_t;

// Platform independent thread API

typedef int thread_t;

typedef int16_t thread_priority_t;

typedef int (*thread_fn_t)(void*);

// 10 == 1024 CPUs max
#define thread_cpu_affinity_t_log2_max 10
#define thread_max_cpu (1 << thread_cpu_affinity_t_log2_max)
struct thread_cpu_affinity_t
{
#ifdef __cplusplus
    static constexpr size_t log2_max = thread_cpu_affinity_t_log2_max;
    static_assert(log2_max >= 6, "Minimum is 64 CPUs");
    static constexpr size_t bitmap_entries = size_t(1) << (log2_max - 6);
#endif

    uint64_t bitmap[bitmap_entries];

#ifdef __cplusplus
    constexpr thread_cpu_affinity_t()
        : bitmap{}
    {
    }

    constexpr thread_cpu_affinity_t(int bit)
        : thread_cpu_affinity_t()
    {
        if (bit >= 0)
            *this *= bit;
        else
            std::fill_n(bitmap, countof(bitmap), ~UINT64_C(0));
    }

    // *= 4 sets bit 4
    constexpr thread_cpu_affinity_t& operator*=(size_t bit)
    {
        bitmap[(bit >> 6)] |= (UINT64_C(1) << (bit & 63));
        return *this;
    }

    // /= 7 clears bit 7
    constexpr thread_cpu_affinity_t& operator/=(size_t bit)
    {
        bitmap[(bit >> 6)] &= ~(UINT64_C(1) << (bit & 63));
        return *this;
    }

    constexpr thread_cpu_affinity_t(thread_cpu_affinity_t const&) = default;
    ~thread_cpu_affinity_t() = default;

    constexpr bool operator[](size_t bit) const
    {
        return bitmap[(bit >> 6)] & (UINT64_C(1) << (bit & 63));
    }

    constexpr size_t lsb_set() const
    {
        for (size_t i = 0; i < bitmap_entries; ++i) {
            if (bitmap[i]) {
                return bit_lsb_set(bitmap[i]) +
                        i * (sizeof(*bitmap) * CHAR_BIT);
            }
        }
        return ~size_t(0);
    }
#endif
};

// Holds 0 if single cpu, otherwise holds -1
// This allows branchless setting of spincounts to zero
extern int spincount_mask;

// Implemented in arch
thread_t thread_create(thread_fn_t fn, void *userdata,
                       size_t stack_size, bool user);

void thread_yield(void);
void thread_sleep_until(uint64_t expiry);
void thread_sleep_for(uint64_t ms);
uint64_t thread_get_usage(int id);

void thread_set_affinity(int id, uint64_t affinity);
uint64_t thread_get_affinity(int id);

void thread_set_affinity(int id, thread_cpu_affinity_t const& affinity);
thread_cpu_affinity_t const* thread_get_affinity(int id);

size_t thread_get_cpu_count();
int thread_cpu_number();

thread_t thread_get_id(void);

// Suspend the thread, then release the lock,
// reacquire lock before returning
void thread_suspend_release(spinlock_t *lock, thread_t *thread_id);

void thread_resume(thread_t thread);

thread_priority_t thread_get_priority(thread_t thread_id);
void thread_set_priority(thread_t thread_id, thread_priority_t priority);

int thread_wait(thread_t thread_id);

void thread_idle_set_ready(void);

void *thread_get_exception_top(void);
void *thread_set_exception_top(void *chain);

process_t *thread_current_process();

// Get the TLB shootdown counter for the specified CPU
uint64_t thread_shootdown_count(int cpu_nr);

// Increment the TLB shootdown counter for the current CPU
void thread_shootdown_notify();

_noreturn
void thread_idle();

__END_DECLS
