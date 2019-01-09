#pragma once

#ifdef __DGOS_KERNEL__
#include "types.h"
#endif

#ifndef _packed
#define _packed __attribute__((__packed__))
#endif

struct trace_item {
    uint64_t fn:48; // Full virtual address range
    unsigned cid:6; // Up to 64 CPUs
    unsigned tid:9; // Up to 512 threads
    bool irq_en:1;  // EFLAGS.IF

    // <- 64 bit boundary
    uint64_t tsc;

    unsigned sync:7;// sync recovery
    bool call:1;    // true for fn entry, false for fn exit

    __attribute__((__no_instrument_function__))
    constexpr trace_item()
        : fn{0}
        , cid{0}
        , tid{0}
        , irq_en{false}
        , tsc{0}
        , sync{0x55}
        , call{0}
    {
    }

    __attribute__((__no_instrument_function__))
    inline constexpr bool valid() const { return sync == 0x55; }

    __attribute__((__no_instrument_function__))
    inline constexpr void *get_ip() const
    {
        return (void*)(int64_t(uint64_t(fn) << 16) >> 16);
    }

    __attribute__((__no_instrument_function__))
    inline constexpr int get_tid() const { return tid; }

    __attribute__((__no_instrument_function__))
    inline constexpr int get_cid() const { return cid; }

    __attribute__((__no_instrument_function__))
    inline constexpr void set_ip(void *p) { fn = uint64_t(p); }

    __attribute__((__no_instrument_function__))
    inline constexpr void set_tid(unsigned t) { tid = t <= 255 ? t : 255; }

    __attribute__((__no_instrument_function__))
    inline constexpr void set_cid(unsigned c) { cid = c <= 63 ? c : 63; }
} _packed;

static_assert(sizeof(trace_item) == 17, "Unexpected size");

// The same as a trace_item with the thread id and sync field omitted
// Intended for use in when the data has been filtered by thread id
struct trace_record {
    uint64_t fn:48; // Full virtual address range
    bool show:1;
    bool showable:1;
    bool expanded:1;
    bool expandable:1;
    bool unused:4;
    unsigned cid:6; // Up to 64 CPUs
    bool irq_en:1;  // EFLAGS.IF
    bool call:1;    // true for fn entry, false for fn exit

    uint64_t tsc;

    uint64_t total_time;
    uint64_t child_time;

    constexpr trace_record()
        : fn(0)
        , show(true)
        , showable(true)
        , expanded(true)
        , expandable(true)
        , unused(0)
        , cid(0)
        , irq_en(0)
        , call(0)
        , tsc(0)
        , total_time(0)
        , child_time(0)
    {
    }

    constexpr trace_record(trace_item const& rhs)
        : fn(rhs.fn)
        , show(true)
        , showable(true)
        , expanded(true)
        , expandable(true)
        , unused(0)
        , cid(rhs.cid)
        , irq_en(rhs.irq_en)
        , call(rhs.call)
        , tsc(rhs.tsc)
        , total_time(0)
        , child_time(0)
    {
    }

    __attribute__((__no_instrument_function__))
    inline constexpr void *get_ip() const
    {
        return (void*)(int64_t(fn << 16) >> 16);
    }
    __attribute__((__no_instrument_function__))
    inline constexpr int get_cid() const { return cid < 63 ? cid : -1; }
} _packed;

static_assert(sizeof(trace_record) == 32, "Unexpected size");

#ifdef __DGOS_KERNEL__
extern "C" _no_instrument
void __cyg_profile_func_enter(void *this_fn, void *call_site);

extern "C" _no_instrument
void __cyg_profile_func_exit(void *this_fn, void *call_site);

extern int eainst_flush_ready;
#endif
