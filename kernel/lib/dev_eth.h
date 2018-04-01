#pragma once

// Ethernet interface

#include "dev_registration.h"
#include "eth_q.h"

struct eth_dev_factory_t {
    eth_dev_factory_t(char const *name);
    virtual int detect(eth_dev_base_t ***result) = 0;
};

struct eth_dev_base_t {
    // Set/get dimensions
    virtual int send(ethq_pkt_t *pkt) = 0;

    virtual void get_mac(void *mac_addr) = 0;
    virtual void set_mac(void const *mac_addr) = 0;

    virtual int get_promiscuous() = 0;
    virtual void set_promiscuous(int promiscuous) = 0;
};

#define ETH_DEV_IMPL                                        \
    virtual int send(ethq_pkt_t *pkt) override;             \
    virtual void get_mac(void *mac_addr) override;          \
    virtual void set_mac(void const *mac_addr) override;    \
    virtual int get_promiscuous() override;                 \
    virtual void set_promiscuous(int promiscuous) override;

void register_eth_dev_factory(char const *name, eth_dev_factory_t *factory);
