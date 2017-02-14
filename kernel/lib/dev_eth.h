#pragma once

// Ethernet interface

#include "dev_registration.h"
#include "eth_q.h"

#ifdef ETH_DEV_NAME
#define ETH_EXPAND_2(p,s) p ## s
#define ETH_EXPAND(p,s) ETH_EXPAND_2(p,s)

#define ETH_EXPAND1_3(t) t
#define ETH_EXPAND1_2(t) ETH_EXPAND1_3(t)
#define ETH_EXPAND1(t) ETH_EXPAND1_2(t)

#define ETH_DEV_T ETH_EXPAND(ETH_DEV_NAME, _dev_t)

typedef struct ETH_DEV_T ETH_DEV_T;
#endif

typedef struct storage_dev_vtbl_t storage_dev_vtbl_t;

typedef struct eth_dev_t eth_dev_t;

typedef struct eth_dev_vtbl_t eth_dev_vtbl_t;

typedef struct eth_dev_base_t {
    eth_dev_vtbl_t *vtbl;
} eth_dev_base_t;

struct eth_dev_vtbl_t {
    int (*detect)(eth_dev_base_t ***result);

    // Set/get dimensions
    int (*send)(eth_dev_base_t *, ethq_pkt_t *pkt);

    void (*get_mac)(eth_dev_base_t *, void *mac_addr);
    void (*set_mac)(eth_dev_base_t *, void const *mac_addr);

    int (*get_promiscuous)(eth_dev_base_t *);
    void (*set_promiscuous)(eth_dev_base_t *, int promiscuous);
};

#define MAKE_eth_dev_VTBL(type, name) { \
    name##_detect,              \
    name##_send,                \
    name##_get_mac,             \
    name##_set_mac,             \
    name##_get_promiscuous,     \
    name##_set_promiscuous      \
}

void register_eth_dev_device(char const *name,
                             eth_dev_vtbl_t *dev);

#define DECLARE_eth_dev_DEVICE(name) \
    DECLARE_DEVICE(eth_dev, name)

#define REGISTER_eth_dev_DEVICE(name) \
    REGISTER_DEVICE(eth_dev, name, 'N')

#ifdef ETH_DEV_NAME
#define ETH_DEV_PTR(dev) ETH_DEV_T *self = (void*)dev

#define ETH_DEV_PTR_UNUSED(dev) (void)dev
#endif
