#pragma once
#include "types.h"

// Storage device interface

#include "dev_registration.h"

#define STORAGE_EXPAND_2(p,s) p ## s
#define STORAGE_EXPAND(p,s) STORAGE_EXPAND_2(p,s)

#define STORAGE_EXPAND1_3(t) t
#define STORAGE_EXPAND1_2(t) STORAGE_EXPAND1_3(t)
#define STORAGE_EXPAND1(t) STORAGE_EXPAND1_2(t)

#define STORAGE_IF_T STORAGE_EXPAND(STORAGE_DEV_NAME, _if_t)
#define STORAGE_DEV_T STORAGE_EXPAND(STORAGE_DEV_NAME, _dev_t)

//
// Forward declarations

typedef struct STORAGE_DEV_T STORAGE_DEV_T;
typedef struct storage_dev_vtbl_t storage_dev_vtbl_t;

typedef struct STORAGE_IF_T STORAGE_IF_T;
typedef struct storage_if_vtbl_t storage_if_vtbl_t;

typedef struct storage_dev_base_t storage_dev_base_t;
typedef struct storage_if_base_t storage_if_base_t;

//
// Storage Device

struct storage_dev_base_t {
    storage_dev_vtbl_t *vtbl;
    storage_if_base_t *if_;
};

typedef struct storage_dev_list_t {
    void *base;
    unsigned stride;
    unsigned count;
} storage_dev_list_t;

struct storage_dev_vtbl_t {
    // Startup/shutdown
    void (*cleanup)(storage_dev_base_t *);

    int (*read)(storage_dev_base_t *dev,
                void *data, uint64_t count,
                uint64_t lba);

    int (*write)(storage_dev_base_t *dev,
                 void *data, uint64_t count,
                 uint64_t lba);

    int (*flush)(storage_dev_base_t *dev);
};

//
// Storage Interface

struct storage_if_base_t {
    storage_if_vtbl_t *vtbl;
};

typedef struct storage_if_list_t {
    void *base;
    unsigned stride;
    unsigned count;
} storage_if_list_t;

struct storage_if_vtbl_t {
    storage_if_list_t (*detect)(void);

    void (*cleanup)(storage_if_base_t *if_);

    storage_dev_list_t (*detect_devices)(storage_if_base_t *if_);
};

void register_storage_if_device(char const *name, storage_if_vtbl_t *vtbl);

#define MAKE_storage_if_VTBL(name) { \
    name##_detect, \
    name##_cleanup, \
    name##_detect_devices \
}

#define MAKE_storage_dev_VTBL(name) { \
    name##_cleanup, \
    name##_read, \
    name##_write, \
    name##_flush \
}

#ifdef STORAGE_IMPL
#define DECLARE_storage_if_DEVICE(name) \
    DECLARE_DEVICE(storage_if, name ## _if)

#define REGISTER_storage_if_DEVICE(name) \
    REGISTER_DEVICE(storage_if, name ## _if, 'L')

#define STORAGE_IF_DEV_PTR(dev) STORAGE_IF_T *self = (void*)dev

#define STORAGE_IF_DEV_PTR_UNUSED(dev) (void)dev

//

#define DECLARE_storage_dev_DEVICE(name) \
    DECLARE_DEVICE(storage_dev, name ## _dev)

#define REGISTER_storage_dev_DEVICE(name) \
    REGISTER_DEVICE(storage_dev, name ## _dev, 'L')

#define DEFINE_storage_dev_DEVICE(name) \
    DEFINE_DEVICE(storage_dev, name ## _dev)

#define STORAGE_DEV_DEV_PTR(dev) STORAGE_DEV_T *self = (void*)dev

#define STORAGE_DEV_DEV_PTR_UNUSED(dev) (void)dev

#endif

extern storage_if_base_t *storage_ifs[];
extern int storage_if_count;

extern storage_dev_base_t *storage_devs[];
extern int storage_dev_count;
