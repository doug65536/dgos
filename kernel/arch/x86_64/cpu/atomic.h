#pragma once
#include "types.h"

//
// Some other concurrent code helpers

static inline void atomic_barrier(void)
{
    __asm__ __volatile__ ( "" : : : "memory" );
}

static inline void atomic_fence(void)
{
    __asm__ __volatile__ ( "mfence" : : : "memory" );
}

static inline void atomic_lfence(void)
{
    __asm__ __volatile__ ( "lfence" : : : "memory" );
}

static inline void atomic_sfence(void)
{
    __asm__ __volatile__ ( "sfence" : : : "memory" );
}

/// In the following definitions
///  S is one of
///   int
///   uint
///  T is one of
///   8
///   16
///   32
///   64
///
/// void atomic_<INSN>_<S><T>(<S><T>_t volatile *value)
///  INSN is one of:
///   inc
///   dec
///   not
///   neg
///
/// void atomic_<INSN>_<S><T>(<S><T>_t volatile *value, <S><T>_t rhs)
///  INSN is one of:
///   bts
///   btr
///   btc
///   add
///   sub
///   and
///   or
///   xor
///
/// <S><T>_t atomic_<INSN>_<S><T>(<S><T>_t volatile *value, <S><T>_t rhs)
///  INSN is one of:
///   cmpxchg
///   xadd
///   xchg
///

/// Forgive the macro abuse, this defines 120 functions

#ifndef ATOMIC_USE_BUILTINS
#define ATOMIC_USE_BUILTINS 1
#endif

#define ATOMIC_DEFINE_UNARY(type, name, insn, suffix) \
    static inline void atomic_##name##_##type ( \
        type##_t volatile *value) \
    { \
        __asm__ __volatile__ ( \
            "lock " insn suffix " %[value]\n\t" \
            : [value] "+m" (*value) \
            : \
            : "memory" \
        ); \
    }

#define ATOMIC_DEFINE_BINARY(type, name, insn, suffix) \
    static inline void atomic_##name##_##type ( \
        type##_t volatile *value, type##_t rhs) \
    { \
        __asm__ __volatile__ ( \
            "lock " insn suffix " %[rhs],%[value]\n\t" \
            : [value] "+m" (*value) \
            : [rhs] "r" (rhs) \
            : "memory" \
        ); \
    }

#define ATOMIC_DEFINE_BINARY_RC(type, name, insn, suffix) \
    static inline uint8_t atomic_##name##_##type ( \
        type##_t volatile *value, \
        type##_t rhs) \
    { \
        uint8_t carry; \
        __asm__ __volatile__ ( \
            "lock " insn suffix " %[rhs],%[value]\n\t" \
            "setc %[carry]\n\t" \
            : [value] "+m" (*value), \
              [carry] "=r" (carry) \
            : [rhs] "r" (rhs) \
            : "memory" \
        ); \
        return carry; \
    }

#if ATOMIC_USE_BUILTINS
#define ATOMIC_DEFINE_CMPXCHG(type, suffix) \
    static inline type##_t atomic_cmpxchg_##type( \
        type##_t volatile *value, \
        type##_t expect,\
        type##_t replacement) \
    { \
        atomic_barrier(); \
        return __sync_val_compare_and_swap(value, expect, replacement); \
    }

#define atomic_cmpxchg(value, expect, replacement) \
    __sync_val_compare_and_swap(value, expect, replacement)

#else

#define ATOMIC_DEFINE_CMPXCHG(type, suffix) \
    static inline type##_t atomic_cmpxchg_##type ( \
        type##_t volatile *value, \
        type##_t expect,\
        type##_t replacement) \
    { \
        __asm__ __volatile__ ( \
            "lock cmpxchg" suffix " %[replacement],(%[value])\n\t" \
            : [value] "+m" (*value) \
            : [expect] "a" (expect), \
              [replacement] "r" (replacement) \
            : "memory" \
        ); \
        return expect; \
    }

#endif

#define ATOMIC_DEFINE_XCHG(type, suffix) \
    static inline type##_t atomic_xchg_##type ( \
        type##_t volatile *value, \
        type##_t replacement) \
    { \
        __asm__ __volatile__ ( \
            "xchg" suffix " %[replacement],%[value]\n\t" \
            : [value] "+m" (*value), \
              [replacement] "+r" (replacement) \
            : \
            : "memory" \
        ); \
        return replacement; \
    }

#define ATOMIC_DEFINE_XADD(type, suffix) \
    static inline type##_t atomic_xadd_##type ( \
        type##_t volatile *value, \
        type##_t addend) \
    { \
        __asm__ __volatile__ ( \
            "lock xadd" suffix " %[addend],%[value]\n\t" \
            : [value] "+m" (*value), \
              [addend] "+r" (addend) \
            : \
            : "memory" \
        ); \
        return addend; \
    }

#define ATOMIC_DEFINE_UNARY_INSN(name, insn) \
    ATOMIC_DEFINE_UNARY(uint8, name, insn, "b") \
    ATOMIC_DEFINE_UNARY(uint16, name, insn, "w") \
    ATOMIC_DEFINE_UNARY(uint32, name, insn, "l") \
    ATOMIC_DEFINE_UNARY(uint64, name, insn, "q") \
    ATOMIC_DEFINE_UNARY(int8, name, insn, "b") \
    ATOMIC_DEFINE_UNARY(int16, name, insn, "w") \
    ATOMIC_DEFINE_UNARY(int32, name, insn, "l") \
    ATOMIC_DEFINE_UNARY(int64, name, insn, "q")

#define ATOMIC_DEFINE_BINARY_INSN(name, insn) \
    ATOMIC_DEFINE_BINARY(uint8, name, insn, "b") \
    ATOMIC_DEFINE_BINARY(uint16, name, insn, "w") \
    ATOMIC_DEFINE_BINARY(uint32, name, insn, "l") \
    ATOMIC_DEFINE_BINARY(uint64, name, insn, "q") \
    ATOMIC_DEFINE_BINARY(int8, name, insn, "b") \
    ATOMIC_DEFINE_BINARY(int16, name, insn, "w") \
    ATOMIC_DEFINE_BINARY(int32, name, insn, "l") \
    ATOMIC_DEFINE_BINARY(int64, name, insn, "q")

#define ATOMIC_DEFINE_BINARY_RC_INSN(name, insn) \
    ATOMIC_DEFINE_BINARY_RC(uint8, name, insn, "b") \
    ATOMIC_DEFINE_BINARY_RC(uint16, name, insn, "w") \
    ATOMIC_DEFINE_BINARY_RC(uint32, name, insn, "l") \
    ATOMIC_DEFINE_BINARY_RC(uint64, name, insn, "q") \
    ATOMIC_DEFINE_BINARY_RC(int8, name, insn, "b") \
    ATOMIC_DEFINE_BINARY_RC(int16, name, insn, "w") \
    ATOMIC_DEFINE_BINARY_RC(int32, name, insn, "l") \
    ATOMIC_DEFINE_BINARY_RC(int64, name, insn, "q")

// Swap
ATOMIC_DEFINE_XCHG(uint8, "b")
ATOMIC_DEFINE_XCHG(uint16, "w")
ATOMIC_DEFINE_XCHG(uint32, "l")
ATOMIC_DEFINE_XCHG(uint64, "q")
ATOMIC_DEFINE_XCHG(int8, "b")
ATOMIC_DEFINE_XCHG(int16, "w")
ATOMIC_DEFINE_XCHG(int32, "l")
ATOMIC_DEFINE_XCHG(int64, "q")

// Compare and swap
ATOMIC_DEFINE_CMPXCHG(uint8, "b")
ATOMIC_DEFINE_CMPXCHG(uint16, "w")
ATOMIC_DEFINE_CMPXCHG(uint32, "l")
ATOMIC_DEFINE_CMPXCHG(uint64, "q")
ATOMIC_DEFINE_CMPXCHG(int8, "b")
ATOMIC_DEFINE_CMPXCHG(int16, "w")
ATOMIC_DEFINE_CMPXCHG(int32, "l")
ATOMIC_DEFINE_CMPXCHG(int64, "q")

// Exchange and add
ATOMIC_DEFINE_XADD(uint8, "b")
ATOMIC_DEFINE_XADD(uint16, "w")
ATOMIC_DEFINE_XADD(uint32, "l")
ATOMIC_DEFINE_XADD(uint64, "q")
ATOMIC_DEFINE_XADD(int8, "b")
ATOMIC_DEFINE_XADD(int16, "w")
ATOMIC_DEFINE_XADD(int32, "l")
ATOMIC_DEFINE_XADD(int64, "q")

// Unary, no return value
ATOMIC_DEFINE_UNARY_INSN(inc, "inc")
ATOMIC_DEFINE_UNARY_INSN(dec, "dec")
ATOMIC_DEFINE_UNARY_INSN(not, "not")
ATOMIC_DEFINE_UNARY_INSN(neg, "neg")

// Binary, returns carry
ATOMIC_DEFINE_BINARY_RC_INSN(bts, "bts")
ATOMIC_DEFINE_BINARY_RC_INSN(btr, "btr")
ATOMIC_DEFINE_BINARY_RC_INSN(btc, "btc")

// Binary, no return value
ATOMIC_DEFINE_BINARY_INSN(add, "add")
ATOMIC_DEFINE_BINARY_INSN(sub, "sub")
ATOMIC_DEFINE_BINARY_INSN(and, "and")
ATOMIC_DEFINE_BINARY_INSN(or, "or")
ATOMIC_DEFINE_BINARY_INSN(xor, "xor")

//
// Technically not atomic but needed in cmpxchg loops

static inline void pause(void)
{
    __asm__ __volatile__ ( "pause" : : : "memory" );
}

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
        for (;;) { \
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
            pause(); \
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
        for (;;) { \
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
            pause(); \
        } \
    } \
    _last_value; \
})
