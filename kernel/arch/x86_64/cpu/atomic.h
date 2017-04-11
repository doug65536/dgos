#pragma once
#include "types.h"

//
// Some other concurrent code helpers

__attribute__((always_inline))
static inline void atomic_barrier(void)
{
    __asm__ __volatile__ ( "" : : : "memory" );
}

// Technically not atomic but needed in cmpxchg loops
#define pause() __builtin_ia32_pause()

#define atomic_fence() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define atomic_lfence() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define atomic_sfence() __atomic_thread_fence(__ATOMIC_RELEASE)

#define atomic_add(value, rhs) __atomic_add_fetch((value), (rhs), __ATOMIC_SEQ_CST)
#define atomic_xadd(value, rhs) __atomic_fetch_add((value), (rhs), __ATOMIC_SEQ_CST)
#define atomic_inc(value) __atomic_add_fetch((value), 1, __ATOMIC_SEQ_CST)
#define atomic_dec(value) __atomic_sub_fetch((value), 1, __ATOMIC_SEQ_CST)
#define atomic_sub(value, rhs) __atomic_sub_fetch((value), (rhs), __ATOMIC_SEQ_CST)
#define atomic_and(value, rhs) __atomic_and_fetch((value), (rhs), __ATOMIC_SEQ_CST)
#define atomic_xor(value, rhs) __atomic_xor_fetch((value), (rhs), __ATOMIC_SEQ_CST)
#define atomic_or(value, rhs)  __atomic_or_fetch((value), (rhs), __ATOMIC_SEQ_CST)
#define atomic_xchg(value, rhs) __atomic_exchange_n((value), (rhs), __ATOMIC_SEQ_CST)

#define atomic_cmpxchg(value, expect, replacement) \
    __sync_val_compare_and_swap((value), (expect), (replacement))

// Returns true if the exchange was successful. Otherwise, returns
// false and updates expect. Expect is a pointer to a variable.
#define atomic_cmpxchg_upd(value, expect, replacement) \
    __atomic_compare_exchange_n((value), (expect), (replacement), \
        0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

//
// Atomic update helpers

// Replace the value with n if the value is > n
// Returns n if replacement occurred, otherwise
// returns latest value (which is < n)
#define atomic_min(value_ptr, n) __extension__ ({\
    atomic_barrier(); \
    __typeof__(value_ptr) _value_ptr = (value_ptr); \
    __typeof__(n) _n = (n); \
    \
    __typeof__(*_value_ptr) _last_value = *_value_ptr; \
    \
    if (_last_value > _n) { \
        for (;; pause()) { \
            __typeof__(*_value_ptr) _curr_value = atomic_cmpxchg( \
                        _value_ptr, _last_value, _n); \
            \
            /* If it got updated, return n */ \
            if (_curr_value == _last_value) { \
                _last_value = _n; \
                break; \
            } \
            /* If it is already greater, return what it is now */ \
            if (_curr_value < _n) { \
                _last_value = _curr_value; \
                break; \
            } \
            \
            _last_value = _curr_value; \
        } \
    } \
    _last_value; \
})

// Replace the value with n if the value is < n
// Returns n if replacement occurred, otherwise
// returns latest value (which is > n)
#define atomic_max(value_ptr, n) __extension__ ({\
    atomic_barrier(); \
    __typeof__(value_ptr) _value_ptr = (value_ptr); \
    __typeof__(n) _n = (n); \
    \
    __typeof__(*_value_ptr) _last_value = *_value_ptr; \
    \
    if (_last_value < _n) { \
        for (;; pause()) { \
            __typeof__(*_value_ptr) _curr_value = atomic_cmpxchg( \
                        _value_ptr, _last_value, _n); \
            \
            /* If it got updated, return n */ \
            if (_curr_value == _last_value) { \
                _last_value = _n; \
                break; \
            } \
            /* If it is already less, return what it is now */ \
            if (_curr_value > _n) { \
                _last_value = _curr_value; \
                break; \
            } \
            \
            _last_value = _curr_value; \
        } \
    } \
    _last_value; \
})
