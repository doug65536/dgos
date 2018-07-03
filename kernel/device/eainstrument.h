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
    unsigned sync:7;// sync recovery
    bool call:1;    // true for fn entry, false for fn exit

    __attribute__((__no_instrument_function__))
    trace_item()
        : sync(0x55)
    {
    }

    __attribute__((__no_instrument_function__))
    inline bool valid() const { return sync == 0x55; }
    __attribute__((__no_instrument_function__))
    inline void *get_ip() const { return (void*)(int64_t(fn << 16) >> 16); }
    __attribute__((__no_instrument_function__))
    inline int get_tid() const { return tid < 255 ? tid : -1; }
    __attribute__((__no_instrument_function__))
    inline int get_cid() const { return cid < 63 ? cid : -1; }
    __attribute__((__no_instrument_function__))
    inline void set_ip(void *p) { fn = uint64_t(p); }
    __attribute__((__no_instrument_function__))
    inline void set_tid(unsigned t) { tid = t < 255 ? t : 255; }
    __attribute__((__no_instrument_function__))
    inline void set_cid(unsigned c) { cid = c < 63 ? c : 63; }
} _packed;

static_assert(sizeof(trace_item) == 9, "Unexpected size");

#ifdef __DGOS_KERNEL__
extern "C" _no_instrument
void __cyg_profile_func_enter(void *this_fn, void *call_site);

extern "C" _no_instrument
void __cyg_profile_func_exit(void *this_fn, void *call_site);
#endif
