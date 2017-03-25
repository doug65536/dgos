#pragma once
#include "callout.h"

#define CONSTRUCTOR_PRIORITY_DEVICE 1000

// Forward declare the vtbl
#define DECLARE_DEVICE(type, name) \
    extern type##_vtbl_t name##_##type##_device_vtbl

#define DEFINE_DEVICE(type, name) \
    type##_vtbl_t name##_##type##_device_vtbl = \
            MAKE_##type##_VTBL(type, name)

// Define the vtbl and define the constructor which
// registers the device driver
#define REGISTER_DEVICE(type, name, id) \
    DEFINE_DEVICE(type, name); \
    extern "C" void name##_##type##_register_device(void *arg); \
    void name##_##type##_register_device(void *arg) \
    { \
        (void)arg; \
        register_##type##_device( #name, & name##_##type##_device_vtbl); \
    } \
    REGISTER_CALLOUT((name##_##type##_register_device), 0, id, "1000")

#define DEVICE_PTR(type, dev) type##_t *self = (type##_t*)dev
