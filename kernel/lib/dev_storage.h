#pragma once

// Storage device interface

#include "dev_registration.h"

//
// Storage Device

typedef struct storage_dev_t storage_dev_t;

typedef struct storage_dev_vtbl_t storage_dev_vtbl_t;

typedef struct storage_dev_base_t {
    storage_dev_vtbl_t *vtbl;
} storage_dev_base_t;

struct storage_dev_vtbl_t {
    int (*detect)(storage_dev_base_t **result);

    // Startup/shutdown
    void (*init)(storage_dev_base_t *);
    void (*cleanup)(storage_dev_base_t *);

};

//
// Storage Interface

typedef struct storage_if_t storage_if_t;

typedef struct storage_if_vtbl_t storage_if_vtbl_t;

typedef struct storage_if_base_t {
    storage_if_vtbl_t *vtbl;
} storage_if_base_t;

struct storage_if_vtbl_t {
    int (*detect)(storage_if_base_t **result);

    void (*init)(storage_if_base_t *);
    void (*cleanup)(storage_if_base_t *);

    int (*detect_devices)(storage_dev_base_t **result);
};

#define MAKE_storage_if_VTBL(name) { \
    name##_detect, \
    name##_init, \
    name##_cleanup, \
    name##_detect_devices \
}

void register_storage_if_device(char const *name,
                                storage_if_vtbl_t *vtbl);

#define DECLARE_storage_if_DEVICE(name) \
    DECLARE_DEVICE(storage_if, name)

#define REGISTER_storage_if_DEVICE(name) \
    REGISTER_DEVICE(storage_if, name)

#define STORAGE_IF_DEV_PTR(dev) DEVICE_PTR(storage_if, dev)

#define STORAGE_IF_DEV_PTR_UNUSED(dev) (void)dev

