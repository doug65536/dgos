#pragma once
#include "types.h"
#include "ethernet.h"
#include "cpu/spinlock.h"

typedef struct ethq_pkt_t ethq_pkt_t;
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

    // Size of packet
    uint16_t size;
};

int ethq_init(void);
ethq_pkt_t *ethq_pkt_acquire(void);
void ethq_pkt_release(ethq_pkt_t *pkt);

typedef struct ethq_queue_t {
    ethq_pkt_t * volatile head;
    ethq_pkt_t * volatile tail;
    size_t volatile count;
} ethq_queue_t;

void ethq_enqueue(ethq_queue_t *queue, ethq_pkt_t *pkt);
ethq_pkt_t *ethq_dequeue(ethq_queue_t *queue);
