#pragma once
#include "types.h"
#include "cpu/spinlock.h"
#include "cpu/spinlock_arch.h"
#include "errno.h"
#include "algorithm.h"
#include "bitsearch.h"

__BEGIN_DECLS

struct process_t;

// Platform independent thread API

typedef int  thread_t;

typedef int16_t thread_priority_t;

typedef int (*thread_fn_t)(void*);

struct thread_create_info_t
{
    // Thread startup function and argument
    thread_fn_t fn;
    void *userdata;

    size_t stack_size;
    thread_priority_t priority;
    bool user;
    bool suspended;

    uintptr_t tls_base;
    uint64_t affinity;
};

// 9 == 512 CPUs max
#define thread_cpu_mask_t_log2_max 9
#define thread_max_cpu (1 << thread_cpu_mask_t_log2_max)
struct thread_cpu_mask_t
{
#ifdef __cplusplus
    static constexpr size_t log2_max = thread_cpu_mask_t_log2_max;
    static_assert(log2_max >= 6, "Minimum is 64 CPUs");
    static constexpr size_t bitmap_entries = size_t(1) << (log2_max - 6);
#endif

    uint64_t bitmap[bitmap_entries];

#ifdef __cplusplus
    // In-place, all zeros
    constexpr thread_cpu_mask_t()
        : bitmap{}
    {
    }

    // In-place, bit=-1 to set all bits, bit=6 to set bit 6 only, others clear
    constexpr thread_cpu_mask_t(int bit)
        : thread_cpu_mask_t()
    {
        if (bit >= 0)
            *this += bit;
        else
            std::fill_n(bitmap, countof(bitmap), ~UINT64_C(0));
    }

    // += 4 sets bit 4. If bit 7 wasn't clear, writes value unchanged
    constexpr thread_cpu_mask_t& operator+=(size_t bit)
    {
        bitmap[(bit >> 6)] |= (UINT64_C(1) << (bit & 63));
        return *this;
    }

    constexpr thread_cpu_mask_t operator+(size_t bit)
    {
        thread_cpu_mask_t result{*this};
        result += bit;
        return result;
    }

    // += 4 sets bit 4. If bit 7 wasn't clear, writes value unchanged
    thread_cpu_mask_t& atom_set(size_t bit)
    {
        atomic_or(&bitmap[(bit >> 6)], (UINT64_C(1) << (bit & 63)));
        return *this;
    }

    // -= 7 clears bit 7. If bit 7 wasn't set, writes value unchanged
    constexpr thread_cpu_mask_t& operator-=(size_t bit)
    {
        bitmap[(bit >> 6)] &= ~(UINT64_C(1) << (bit & 63));
        return *this;
    }

    constexpr thread_cpu_mask_t operator-(size_t bit)
    {
        thread_cpu_mask_t result{*this};
        result -= bit;
        return result;
    }

    constexpr thread_cpu_mask_t operator-(
            thread_cpu_mask_t const& rhs) const
    {
        thread_cpu_mask_t result{*this};
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            result.bitmap[i] = bitmap[i] & ~rhs.bitmap[i];
        return result;
    }

    constexpr thread_cpu_mask_t operator+(
            thread_cpu_mask_t const& rhs) const
    {
        thread_cpu_mask_t result{*this};
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            result.bitmap[i] = bitmap[i] | rhs.bitmap[i];
        return result;
    }

    // -= 7 clears bit 7. If bit 7 wasn't set, writes value unchanged
    void atom_clr(size_t bit) volatile
    {
        atomic_and(&bitmap[(bit >> 6)], ~(UINT64_C(1) << (bit & 63)));
    }

    // produce rvalue
    constexpr thread_cpu_mask_t operator&(
            thread_cpu_mask_t const& rhs) const
    {
        thread_cpu_mask_t result;
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            result.bitmap[i] = bitmap[i] & rhs.bitmap[i];
        return result;
    }

    // modify in place
    constexpr thread_cpu_mask_t& operator&=(
            thread_cpu_mask_t const& rhs)
    {
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            bitmap[i] &= rhs.bitmap[i];
        return *this;
    }

    // modify in place
    thread_cpu_mask_t atom_and(thread_cpu_mask_t const& rhs) volatile
    {
        thread_cpu_mask_t result;
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            result.bitmap[i] = atomic_and(&bitmap[i], rhs.bitmap[i]);
        return result;
    }

    // produce rvalue
    constexpr thread_cpu_mask_t operator|(
            thread_cpu_mask_t const& rhs) const
    {
        thread_cpu_mask_t result;
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            result.bitmap[i] = bitmap[i] | rhs.bitmap[i];
        return result;
    }

    // modify in place
    constexpr thread_cpu_mask_t& operator|=(
            thread_cpu_mask_t const& rhs)
    {
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            bitmap[i] |= rhs.bitmap[i];
        return *this;
    }

    thread_cpu_mask_t atom_or(thread_cpu_mask_t const& rhs) volatile
    {
        thread_cpu_mask_t result;
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            result.bitmap[i] = atomic_or(&bitmap[i], rhs.bitmap[i]);
        return result;
    }

    // produce rvalue
    constexpr thread_cpu_mask_t operator^(
            thread_cpu_mask_t const& rhs) const
    {
        thread_cpu_mask_t result;
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            result.bitmap[i] = bitmap[i] ^ rhs.bitmap[i];
        return result;
    }

    // modify in place
    constexpr thread_cpu_mask_t& operator^=(
            thread_cpu_mask_t const& rhs)
    {
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            bitmap[i] ^= rhs.bitmap[i];
        return *this;
    }

    // modify in place
    thread_cpu_mask_t atom_xor(thread_cpu_mask_t const& rhs) volatile
    {
        thread_cpu_mask_t result;
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            result.bitmap[i] = atomic_xor(&bitmap[i], rhs.bitmap[i]);
        return result;
    }

    constexpr thread_cpu_mask_t(thread_cpu_mask_t const&) = default;
    ~thread_cpu_mask_t() = default;

    constexpr thread_cpu_mask_t operator~() const
    {
        thread_cpu_mask_t comp{};
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            comp.bitmap[i] = ~bitmap[i];
        return comp;
    }

    // Returns true if every bit is zero
    constexpr bool operator!() const
    {
        uint64_t un = bitmap[0];
        for (size_t i = 1, e = countof(bitmap); i != e; ++i)
            un |= bitmap[i];
        return un == 0;
    }

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

    constexpr thread_cpu_mask_t& set_all()
    {
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            bitmap[i] = ~(UINT64_C(0));
        return *this;
    }

    bool operator==(thread_cpu_mask_t const& rhs) const
    {
        bool is_equal = bitmap[0] == rhs.bitmap[0];
        for (size_t i = 1, e = countof(bitmap); i != e; ++i)
            is_equal = is_equal & (bitmap[i] == rhs.bitmap[i]);
        return is_equal;
    }

    bool operator!=(thread_cpu_mask_t const& rhs) const
    {
        bool not_equal = bitmap[0] != rhs.bitmap[0];
        for (size_t i = 1, e = countof(bitmap); i != e; ++i)
            not_equal = not_equal | (bitmap[i] != rhs.bitmap[i]);
        return not_equal;
    }

    //
    // Bans

    // Ordered comparisons are meaningless
    bool operator<(thread_cpu_mask_t rhs) const = delete;
    bool operator<=(thread_cpu_mask_t rhs) const = delete;
    bool operator>(thread_cpu_mask_t rhs) const = delete;
    bool operator>=(thread_cpu_mask_t rhs) const = delete;

    // Use atom_set
    thread_cpu_mask_t operator+=(thread_cpu_mask_t) volatile = delete;

    // Use atom_clr
    thread_cpu_mask_t operator-=(thread_cpu_mask_t) volatile = delete;

    // Use atom_and
    thread_cpu_mask_t operator&=(thread_cpu_mask_t) volatile = delete;

    // Use atom_or
    thread_cpu_mask_t operator|=(thread_cpu_mask_t) volatile = delete;

    // Use atom_xor
    thread_cpu_mask_t operator^=(thread_cpu_mask_t) volatile = delete;
#endif
};

// Holds 0 if single cpu, otherwise holds -1
// This allows branchless setting of spincounts to zero
extern int spincount_mask;

// Implemented in arch
thread_t thread_create(thread_fn_t fn, void *userdata,
                       size_t stack_size, bool user);

thread_t thread_create_with_info(thread_create_info_t const* info);

void thread_yield(void);
void thread_sleep_until(uint64_t expiry);
void thread_sleep_for(uint64_t ms);
uint64_t thread_get_usage(int id);

void thread_set_fsbase(thread_t tid, uintptr_t fsbase);
void thread_set_gsbase(thread_t tid, uintptr_t gsbase);

void thread_set_affinity(int id, thread_cpu_mask_t const& affinity);
thread_cpu_mask_t const* thread_get_affinity(int id);

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

void thread_cls_init_early(int ap);

_noreturn
void thread_idle();

int thread_close(thread_t tid);

_use_result
thread_t thread_proc_0(void (*fn)());

_use_result
thread_t thread_proc_1(void (*fn)(void *), void *arg);

_use_result
thread_t thread_func_0(int (*fn)());

_use_result
thread_t thread_func_1(int (*fn)(void*), void *arg);

__END_DECLS
