
#define CONSTRUCTOR_PRIORITY_DEVICE 1000

// Forward declare the vtbl
#define DECLARE_DEVICE(type, name) \
    extern type##_vtbl_t name##_vtbl

// Define the vtbl and define the constructor which
// registers the device driver
#define REGISTER_DEVICE(type, name) \
    type##_vtbl_t name##_vtbl =\
            MAKE_##type##_VTBL(name); \
    __attribute__((constructor(CONSTRUCTOR_PRIORITY_DEVICE))) \
    static void name##_register_##type##_device(void) \
    { \
        register_##type##_device( #name, & name##_vtbl); \
    }

#define DEVICE_PTR(type, dev) type##_t *self = (type##_t*)dev
