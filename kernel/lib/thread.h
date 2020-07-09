#pragma once
#include "types.h"
#include "cpu/spinlock.h"
#include "cpu/spinlock_arch.h"
#include "errno.h"

//#include "algorithm.h"
//#include "bitsearch.h"

__BEGIN_DECLS

struct process_t;
struct cpu_info_t;
struct isr_context_t;

// Platform independent thread API

typedef int  thread_t;

typedef int16_t thread_priority_t;

typedef int (*thread_fn_t)(void*);

void thread_check_stack(int intr);

void thread_startup(thread_fn_t fn, void *p, thread_t id);

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
    inline constexpr thread_cpu_mask_t()
        : bitmap{}
    {
    }

    // In-place, bit=-1 to set all bits, bit=6 to set bit 6 only, others clear
    constexpr explicit thread_cpu_mask_t(int bit);

    // += 4 sets bit 4. If bit 7 wasn't clear, writes value unchanged
    thread_cpu_mask_t& operator+=(size_t bit);

    thread_cpu_mask_t operator+(size_t bit);

    // += 4 sets bit 4. If bit 7 wasn't clear, writes value unchanged
    thread_cpu_mask_t& atom_set(size_t bit);

    // -= 7 clears bit 7. If bit 7 wasn't set, writes value unchanged
    thread_cpu_mask_t& operator-=(size_t bit);

    thread_cpu_mask_t operator-(size_t bit);

    thread_cpu_mask_t operator-(
            thread_cpu_mask_t const& rhs) const;

    thread_cpu_mask_t operator+(
            thread_cpu_mask_t const& rhs) const;

    // -= 7 clears bit 7. If bit 7 wasn't set, writes value unchanged
    void atom_clr(size_t bit) volatile;

    // produce rvalue
    thread_cpu_mask_t operator&(
            thread_cpu_mask_t const& rhs) const;

    // modify in place
    thread_cpu_mask_t& operator&=(
            thread_cpu_mask_t const& rhs);

    // modify in place
    thread_cpu_mask_t atom_and(thread_cpu_mask_t const& rhs) volatile;

    // produce rvalue
    thread_cpu_mask_t operator|(
            thread_cpu_mask_t const& rhs) const;

    // modify in place
    thread_cpu_mask_t& operator|=(
            thread_cpu_mask_t const& rhs);

    thread_cpu_mask_t atom_or(thread_cpu_mask_t const& rhs) volatile;

    // produce rvalue
    thread_cpu_mask_t operator^(
            thread_cpu_mask_t const& rhs) const;

    // modify in place
    thread_cpu_mask_t& operator^=(
            thread_cpu_mask_t const& rhs);

    // modify in place
    thread_cpu_mask_t atom_xor(thread_cpu_mask_t const& rhs) volatile;

    thread_cpu_mask_t(thread_cpu_mask_t const&) = default;
    ~thread_cpu_mask_t() = default;

    thread_cpu_mask_t operator~() const;

    // Returns true if every bit is zero
    bool operator!() const;

    bool operator[](size_t bit) const;

    size_t lsb_set() const;

    thread_cpu_mask_t& set_all();

    bool operator==(thread_cpu_mask_t const& rhs) const;

    bool operator!=(thread_cpu_mask_t const& rhs) const;

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

constexpr thread_cpu_mask_t::thread_cpu_mask_t(int bit)
    : thread_cpu_mask_t()
{
    if (bit >= 0) {
        *this += bit;
    } else {
        for (size_t i = 0, e = countof(bitmap); i != e; ++i)
            bitmap[i] = ~UINT64_C(0);
    }
}

struct thread_create_info_t
{
    // Thread startup function and argument
    thread_fn_t fn;
    void *userdata;

    size_t stack_size;
    thread_priority_t priority;
    bool user;
    bool is_float;
    bool suspended;

    uintptr_t tls_base;
    thread_cpu_mask_t affinity;

    char const *name;

    void *stack_ptr;
};

// Holds 0 if single cpu, otherwise holds -1
// This allows branchless setting of spincounts to zero
extern int spincount_mask;
extern int use_mwait;

// Implemented in arch
thread_t thread_create(thread_fn_t fn, void *userdata, char const *name,
                       size_t stack_size, bool user, bool is_float,
                       thread_cpu_mask_t const& affinity = thread_cpu_mask_t());

thread_t thread_create_with_info(thread_create_info_t const* info);

uintptr_t thread_yield(void);
void thread_sleep_until(uint64_t expiry);
void thread_sleep_for(uint64_t ms);
uint64_t thread_get_usage(int id);

void thread_set_fsbase(thread_t tid, uintptr_t fsbase);
void thread_set_gsbase(thread_t tid, uintptr_t gsbase);

void thread_set_affinity(int id, thread_cpu_mask_t const& affinity);
thread_cpu_mask_t const* thread_get_affinity(int id);

size_t thread_get_cpu_count();
uint32_t thread_cpu_number();
unsigned thread_cpu_usage_x1k(size_t cpu);

thread_t thread_get_id(void);

// Suspend the thread, then release the lock,
// DOES NOT reacquire lock before returning
uintptr_t thread_sleep_release(spinlock_t *lock, thread_t *thread_id,
                               uint64_t timeout_time);

// Request a reschedule on this cpu
void thread_request_reschedule_noirq();
void thread_request_reschedule();

// Perform a reschedule if one was requested
isr_context_t *thread_schedule_if_requested(isr_context_t *ctx);
isr_context_t *thread_schedule_if_requested_noirq(isr_context_t *ctx);

void thread_resume(thread_t thread, intptr_t exit_code);

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
unsigned thread_current_cpu(thread_t tid);

_use_result
thread_t thread_proc_0(void (*fn)());

_use_result
thread_t thread_proc_1(void (*fn)(void *), void *arg);

_use_result
thread_t thread_func_0(int (*fn)());

_use_result
thread_t thread_func_1(int (*fn)(void*), void *arg);

thread_t thread_create_irq_handler();

void thread_set_cpu_count(size_t new_cpu_count);

struct __cxa_exception;

struct __cxa_eh_globals;
__cxa_eh_globals *thread_cxa_get_globals();

void thread_set_timer(uint8_t &apic_dcr, uint64_t ns);

void thread_panic_other_cpus();

isr_context_t *thread_entering_irq(isr_context_t *ctx);

isr_context_t *thread_finishing_irq(isr_context_t *ctx);

__END_DECLS
