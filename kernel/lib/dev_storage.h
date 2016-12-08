#pragma once

// Storage device interface

//
// Storage Device

typedef struct storage_dev_t storage_dev_t;

typedef struct storage_dev_vtbl_t storage_dev_vbtl_t;

typedef struct storage_dev_base_t {
    storage_dev_vbtl_t *vtbl;
} storage_dev_base_t;

struct storage_dev_vbtl_t {
    int (*detect)(storage_dev_base_t **result);

    void (*init)(storage_dev_base_t *);
    void (*cleanup)(storage_dev_base_t *);


};

//
// Storage Interface

typedef struct storage_if_t storage_if_t;

typedef struct storage_if_vtbl_t storage_if_vbtl_t;

typedef struct storage_if_base_t {
    storage_if_vbtl_t *vtbl;
} storage_if_base_t;

struct storage_if_vbtl_t {
    int (*detect)(storage_if_base_t **result);

    void (*init)(storage_if_base_t *);
    void (*cleanup)(storage_if_base_t *);

    int (*detect_devices)(storage_dev_base_t **result);
};
