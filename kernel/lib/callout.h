#pragma once
#include "types.h"

// Types:
//  'M': VMM initialized
//  'S': Initialize SMP CPU
//  'E': Early initialized device
//  'T': SMP online
//  'L': Late initialized device
//  'F': Filesystem implementation
//  'H': Storage interface
//  'P': Partition scanner
//  'N': Network interface
//  'U': USB interface

struct callout_t {
    void (*fn)(void*);
    void *userarg;
    int32_t type;
    int32_t reserved;
    int64_t reserved2;
};

#define REGISTER_CALLOUT3(a,n)   a##n
#define REGISTER_CALLOUT2(a,n)   REGISTER_CALLOUT3(a,n)

#define REGISTER_CALLOUT(fn, arg, type, order) \
    __attribute__((section(".callout_array." order), used, aligned(16))) \
    static callout_t REGISTER_CALLOUT2(callout_, __COUNTER__) = { \
        (fn), (arg), (type), 0, 0 \
    }

extern "C" size_t callout_call(int32_t type);
