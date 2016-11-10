#pragma once

#define CONSTRUCTOR_PRIORITY_DEVICE 1000

// Forward declare the vtbl
#define DECLARE_DEVICE(type, name) \
    extern type##_vtbl_t name##_device_vtbl

// Define the vtbl and define the constructor which
// registers the device driver
#define REGISTER_DEVICE(type, name) \
    type##_vtbl_t name##_device_vtbl =\
            MAKE_##type##_VTBL(name); \
    __attribute__((constructor(CONSTRUCTOR_PRIORITY_DEVICE))) \
    void name##_##type##_register_device(void); \
    __attribute__((constructor(CONSTRUCTOR_PRIORITY_DEVICE))) \
    void name##_##type##_register_device(void) \
    { \
        register_##type##_device( #name, & name##_device_vtbl); \
    }

#define DEVICE_PTR(type, dev) type##_t *self = (type##_t*)dev
