#pragma once
#include "types.h"

// Types:
//  'M': VMM initialized
//  'S': Initialize SMP CPU
//  'E': Early initialized device
//  'T': SMP online
// from init thread, in this order:
//  'D': Facilities needed by drivers
//  'L': Late initialized device
//  'F': Filesystem implementation
//  'H': Storage interface
//  'P': Partition scanner
//  'N': Network interface
//  'U': USB interface

enum struct callout_type_t : uint32_t {
    // bootstrap
    vmm_ready,  // 'M'
    smp_start,  // 'S'
    early_dev,  // 'E'
    smp_online, // 'T'

    // from init_thread
    driver_base,    // 'D'
    late_dev,       // 'L'
    reg_filesys,    // 'F'
    storage_dev,    // 'H'
    partition_probe,// 'P'
    nic,            // 'N'
    usb             // 'U'
};

struct callout_t {
    void (*fn)(void*);
    void *userarg;
    callout_type_t type;
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

extern "C" size_t callout_call(callout_type_t type);
