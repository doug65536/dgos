#pragma once
#include "types.h"
#include "ethernet.h"

__BEGIN_DECLS

// Circular dependency
struct ethq_pkt_t;
struct eth_dev_base_t;

#include "dev_eth.h"

struct ethq_pkt_t {
    // Ethernet packet
    ethernet_pkt_t pkt;

    // physical address of packet
    uintptr_t physaddr;

    // Next packet in queue
    ethq_pkt_t *next;

    // Completion callback
    void (*callback)(ethq_pkt_t*, int error, uintptr_t);
    uintptr_t callback_arg;

    // Source network interface
    eth_dev_base_t *nic;

    // Size of packet
    uint16_t size;
};

int ethq_init(void);
ethq_pkt_t *ethq_pkt_acquire(void);
void ethq_pkt_release(ethq_pkt_t *pkt);

struct ethq_queue_t {
    ethq_pkt_t * head;
    ethq_pkt_t * tail;
    size_t count;
};

void ethq_enqueue(ethq_queue_t *queue, ethq_pkt_t *pkt);
ethq_pkt_t *ethq_dequeue(ethq_queue_t *queue);
ethq_pkt_t *ethq_dequeue_all(ethq_queue_t *queue);

__END_DECLS
