#pragma once

// Storage device interface

#include "dev_registration.h"

#define STORAGE_EXPAND2(p,s) p ## s
#define STORAGE_EXPAND(p,s) STORAGE_EXPAND2(p,s)

#define STORAGE_IF_T STORAGE_EXPAND(STORAGE_DEV_NAME, _if_t)
#define STORAGE_DEV_T STORAGE_EXPAND(STORAGE_DEV_NAME, _dev_t)

//
// Storage Device

typedef struct STORAGE_DEV_T STORAGE_DEV_T;

typedef struct storage_dev_vtbl_t storage_dev_vtbl_t;

typedef struct storage_dev_base_t {
    storage_dev_vtbl_t *vtbl;
} storage_dev_base_t;

typedef struct storage_dev_list_t {
    void *base;
    unsigned stride;
    unsigned count;
} storage_dev_list_t;

struct storage_dev_vtbl_t {
    storage_dev_list_t (*detect)(void);

    // Startup/shutdown
    void (*cleanup)(storage_dev_base_t *);

};

//
// Storage Interface

typedef struct STORAGE_IF_T STORAGE_IF_T;

typedef struct storage_if_vtbl_t storage_if_vtbl_t;

typedef struct storage_if_base_t {
    storage_if_vtbl_t *vtbl;
} storage_if_base_t;

typedef struct storage_if_list_t {
    void *base;
    unsigned stride;
    unsigned count;
} storage_if_list_t;

struct storage_if_vtbl_t {
    storage_if_list_t (*detect)(void);

    void (*cleanup)(storage_if_base_t *);

    storage_dev_list_t (*detect_devices)(storage_if_base_t *if_);
};

#define MAKE_storage_if_VTBL(name) { \
    name##_detect, \
    name##_cleanup, \
    name##_detect_devices \
}

void register_storage_if_device(char const *name, storage_if_vtbl_t *vtbl);

#define DECLARE_storage_if_DEVICE(name) \
    DECLARE_DEVICE(storage_if, name)

#define REGISTER_storage_if_DEVICE(name) \
    REGISTER_DEVICE(storage_if, name, 'L')

#define STORAGE_IF_DEV_PTR(dev) DEVICE_PTR(storage_if, dev)

#define STORAGE_IF_DEV_PTR_UNUSED(dev) (void)dev
