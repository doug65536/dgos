#pragma once
#include "types.h"

//
// Some other concurrent code helpers

static _always_inline void compiler_barrier(void)
{
    __asm__ __volatile__ ("" ::: "memory");
}

// Technically not atomic but needed in cmpxchg loops
static _always_inline _no_instrument void pause()
{
#if defined(__i386__) || defined(__x86_64__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    __asm__("yield");
#endif
}

#define atomic_fence() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define atomic_acquire_fence() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define atomic_release_fence() __atomic_thread_fence(__ATOMIC_RELEASE)

#define atomic_add(value, rhs) \
    __atomic_add_fetch((value), (rhs), __ATOMIC_SEQ_CST)

#define atomic_xadd(value, rhs) \
    __atomic_fetch_add((value), (rhs), __ATOMIC_SEQ_CST)

#define atomic_inc(value) \
    __atomic_add_fetch((value), 1, __ATOMIC_SEQ_CST)

#define atomic_dec(value) \
    __atomic_sub_fetch((value), 1, __ATOMIC_SEQ_CST)

#define atomic_sub(value, rhs) \
    __atomic_sub_fetch((value), (rhs), __ATOMIC_SEQ_CST)

#define atomic_and(value, rhs) \
    __atomic_and_fetch((value), (rhs), __ATOMIC_SEQ_CST)

#define atomic_xor(value, rhs) \
    __atomic_xor_fetch((value), (rhs), __ATOMIC_SEQ_CST)

#define atomic_or(value, rhs)  \
    __atomic_or_fetch((value), (rhs), __ATOMIC_SEQ_CST)

#define atomic_xchg(value, rhs) \
    __atomic_exchange_n((value), (rhs), __ATOMIC_SEQ_CST)

// Returns old value
#define atomic_cmpxchg(value, expect, replacement) \
    __sync_val_compare_and_swap((value), (expect), (replacement))

// Bit test and reset
#define atomic_btr(value, bit) __extension__ ({\
    __typeof__(value) value_ = (value); \
    __typeof__(*value) mask_ = uint64_t(1) << bit; \
    (0 != (__atomic_fetch_and(value_, ~mask_, __ATOMIC_SEQ_CST) & mask_)); \
    })

// Bit test and set
#define atomic_bts(value, bit) \
    (0 != (__atomic_fetch_or((value), \
        ((decltype(*(value)))1 << (bit))) & \
        (decltype(*(value))1 << (bit))))

// Bit test and complement
#define atomic_btc(value, bit) \
    (0 != (__atomic_fetch_xor((value), \
        ((decltype(*(value)))1 << (bit))) & \
        (decltype(*(value))1 << (bit))))

#define atomic_ld_acq(value) \
    __atomic_load_n(value, __ATOMIC_ACQUIRE)

#define atomic_st_rel(value, rhs) \
    __atomic_store_n((value), (rhs), __ATOMIC_RELEASE)

// Returns true if the exchange was successful. Otherwise, returns
// false and updates expect. Expect is a pointer to a variable. Expect
// value is updated from previous value at value pointer
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
    compiler_barrier(); \
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

#ifdef __cplusplus

__BEGIN_NAMESPACE_EXT

template<typename T>
struct atomic;

template<typename U>
struct atomic<U*>;

typedef enum memory_order : int {
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

template<typename _T>
struct atomic
{
    using value_type = _T;

    constexpr atomic() noexcept = default;

    constexpr atomic(_T __v)
        : __v(__v)
    {
    }

    atomic(atomic const&) = delete;

    _T operator=(_T __new_value) noexcept
    {
        __atomic_store_n(&__v, __new_value, memory_order_seq_cst);
        return __new_value;
    }

    _T operator=(_T __new_value) volatile noexcept
    {
        __atomic_store_n(&__v, __new_value, memory_order_seq_cst);
        return __new_value;
    }

    operator _T() const noexcept
    {
        return __atomic_load_n(&__v, memory_order_seq_cst);
    }

    operator _T() volatile const noexcept
    {
        return __atomic_load_n(&__v, memory_order_seq_cst);
    }

    _T exchange(_T __new_value,
                memory_order order = memory_order_seq_cst) noexcept
    {
        __atomic_exchange_n(&__v, __new_value, order);
    }

    _T exchange(_T __new_value,
                memory_order order = memory_order_seq_cst) volatile noexcept
    {
        __atomic_exchange_n(&__v, __new_value, order);
    }

    _T load(memory_order order = memory_order_seq_cst) const noexcept
    {
        return __atomic_load_n(&__v, order);
    }

    _T load(memory_order order = memory_order_seq_cst) const volatile noexcept
    {
        return __atomic_load_n(&__v, order);
    }

    _T store(value_type new_value,
             memory_order order = memory_order_seq_cst) noexcept
    {
        return __atomic_store_n(&__v, new_value, order);
    }

    _T store(value_type new_value,
             memory_order order = memory_order_seq_cst) volatile noexcept
    {
        return __atomic_store_n(&__v, new_value, order);
    }

    _T fetch_add(_T rhs,
                 memory_order order = memory_order_seq_cst) noexcept
    {
        return __atomic_fetch_add(&__v, rhs, order);
    }

    _T fetch_add(_T rhs,
                 memory_order order = memory_order_seq_cst) volatile noexcept
    {
        return __atomic_fetch_add(&__v, rhs, order);
    }

    _T fetch_sub(_T rhs,
                 memory_order order = memory_order_seq_cst) noexcept
    {
        return __atomic_fetch_sub(&__v, rhs, order);
    }

    _T fetch_sub(_T rhs,
                 memory_order order = memory_order_seq_cst) volatile noexcept
    {
        return __atomic_fetch_sub(&__v, rhs, order);
    }

    _T fetch_and(_T rhs,
                 memory_order order = memory_order_seq_cst) noexcept
    {
        return __atomic_fetch_and(&__v, rhs, order);
    }

    _T fetch_and(_T rhs,
                 memory_order order = memory_order_seq_cst) volatile noexcept
    {
        return __atomic_fetch_and(&__v, rhs, order);
    }

    _T fetch_or(_T rhs,
                memory_order order = memory_order_seq_cst) noexcept
    {
        return __atomic_fetch_or(&__v, rhs, order);
    }

    _T fetch_or(_T rhs,
                memory_order order = memory_order_seq_cst) volatile noexcept
    {
        return __atomic_fetch_or(&__v, rhs, order);
    }

    _T fetch_xor(_T rhs,
                 memory_order order = memory_order_seq_cst) noexcept
    {
        return __atomic_fetch_xor(&__v, rhs, order);
    }

    _T fetch_xor(_T rhs,
                 memory_order order = memory_order_seq_cst) volatile noexcept
    {
        return __atomic_fetch_xor(&__v, rhs, order);
    }

    _T operator++() noexcept
    {
        return __atomic_add_fetch(&__v, 1, memory_order_seq_cst);
    }

    _T operator++() volatile noexcept
    {
        return __atomic_add_fetch(&__v, 1, memory_order_seq_cst);
    }

    _T operator++(int) noexcept
    {
        return fetch_add(1);
    }

    _T operator++(int) volatile noexcept
    {
        return fetch_add(1);
    }

    _T operator--() noexcept
    {
        return __atomic_sub_fetch(&__v, 1, memory_order_seq_cst);
    }

    _T operator--() volatile noexcept
    {
        return __atomic_sub_fetch(&__v, 1, memory_order_seq_cst);
    }

    _T operator--(int) noexcept
    {
        return fetch_sub(1);
    }

    _T operator--(int) volatile noexcept
    {
        return fetch_sub(1);
    }

    _T operator+=(_T rhs) noexcept
    {
        return __atomic_add_fetch(&__v, rhs, memory_order_seq_cst);
    }

    _T operator+=(_T rhs) volatile noexcept
    {
        return __atomic_add_fetch(&__v, rhs, memory_order_seq_cst);
    }

    _T operator-=(_T rhs) noexcept
    {
        return __atomic_sub_fetch(&__v, rhs, memory_order_seq_cst);
    }

    _T operator-=(_T rhs) volatile noexcept
    {
        return __atomic_sub_fetch(&__v, rhs, memory_order_seq_cst);
    }

    _T operator&=(_T rhs) noexcept
    {
        return __atomic_and_fetch(&__v, rhs, memory_order_seq_cst);
    }

    _T operator&=(_T rhs) volatile noexcept
    {
        return __atomic_and_fetch(&__v, rhs, memory_order_seq_cst);
    }

    _T operator|=(_T rhs) noexcept
    {
        return __atomic_or_fetch(&__v, rhs, memory_order_seq_cst);
    }

    _T operator|=(_T rhs) volatile noexcept
    {
        return __atomic_or_fetch(&__v, rhs, memory_order_seq_cst);
    }

    _T operator^=(_T rhs) noexcept
    {
        return __atomic_xor_fetch(&__v, rhs, memory_order_seq_cst);
    }

    _T operator^=(_T rhs) volatile noexcept
    {
        return __atomic_xor_fetch(&__v, rhs, memory_order_seq_cst);
    }

private:
    _T __v;
};

using atomic_bool = atomic<bool>;
using atomic_char = atomic<char>;
using atomic_schar = atomic<signed char>;
using atomic_uchar = atomic<unsigned char>;
using atomic_short = atomic<short>;
using atomic_ushort = atomic<unsigned short>;
using atomic_int = atomic<int>;
using atomic_uint = atomic<unsigned int>;
using atomic_long = atomic<long>;
using atomic_ulong = atomic<unsigned long>;
using atomic_llong = atomic<long long>;
using atomic_ullong = atomic<unsigned long long>;
using atomic_char16_t = atomic<char16_t>;
using atomic_char32_t = atomic<char32_t>;
using atomic_wchar_t = atomic<wchar_t>;
using atomic_int8_t = atomic<int8_t>;
using atomic_uint8_t = atomic<uint8_t>;
using atomic_int16_t = atomic<int16_t>;
using atomic_uint16_t = atomic<uint16_t>;
using atomic_int32_t = atomic<int32_t>;
using atomic_uint32_t = atomic<uint32_t>;
using atomic_int64_t = atomic<int64_t>;
using atomic_uint64_t = atomic<uint64_t>;
using atomic_int_least8_t = atomic<int_least8_t>;
using atomic_uint_least8_t = atomic<uint_least8_t>;
using atomic_int_least16_t = atomic<int_least16_t>;
using atomic_uint_least16_t = atomic<uint_least16_t>;
using atomic_int_least32_t = atomic<int_least32_t>;
using atomic_uint_least32_t = atomic<uint_least32_t>;
using atomic_int_least64_t = atomic<int_least64_t>;
using atomic_uint_least64_t = atomic<uint_least64_t>;
using atomic_int_fast8_t = atomic<int_fast8_t>;
using atomic_uint_fast8_t = atomic<uint_fast8_t>;
using atomic_int_fast16_t = atomic<int_fast16_t>;
using atomic_uint_fast16_t = atomic<uint_fast16_t>;
using atomic_int_fast32_t = atomic<int_fast32_t>;
using atomic_uint_fast32_t = atomic<uint_fast32_t>;
using atomic_int_fast64_t = atomic<int_fast64_t>;
using atomic_uint_fast64_t = atomic<uint_fast64_t>;
using atomic_intptr_t = atomic<intptr_t>;
using atomic_uintptr_t = atomic<uintptr_t>;
using atomic_size_t = atomic<size_t>;
using atomic_ptrdiff_t = atomic<ptrdiff_t>;
using atomic_intmax_t = atomic<intmax_t>;
using atomic_uintmax_t = atomic<uintmax_t>;

__END_NAMESPACE_EXT

#endif
