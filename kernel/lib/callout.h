#pragma once
#include "types.h"

typedef struct callout_t {
    void (*fn)(void*);
    void *userarg;
    int32_t type;
    int32_t reserved;
    int64_t reserved2;
} callout_t;

#define REGISTER_CALLOUT(fn, arg, type, order) \
    __attribute__((section(".callout_array." order), used)) \
    static callout_t callout_##__COUNTER__ = { (fn), (arg), (type), 0, 0 }

size_t callout_call(int32_t type);
