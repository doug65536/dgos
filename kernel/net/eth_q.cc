#include "eth_q.h"
#include "mm.h"
#include "assert.h"
#include "cpu/atomic.h"
#include "printk.h"
#include "mutex.h"

#define ETHQ_DEBUG  1
#if ETHQ_DEBUG
#define ETHQ_TRACE(...) printdbg("ethq: " __VA_ARGS__)
#else
#define ETHQ_TRACE(...) ((void)0)
#endif

// A half-page (exactly) packet
struct alignas(2048) ethq_pkt2K_t {
    ethq_pkt_t pkt;
};

C_ASSERT(sizeof(ethq_pkt2K_t) == (PAGESIZE >> 1));

static ethq_pkt2K_t *ethq_pkts;
static ethq_pkt_t ** ethq_pkt_list;
static size_t ethq_pkt_count;
static uint16_t volatile *ethq_free_chain;
static uint32_t volatile ethq_first_free_aba;
static ethq_pkt_t *ethq_first_free;

// Redundant calls are tolerated and ignored
int ethq_init(void)
{
    if (ethq_pkts)
        return 1;

    // Allocate 64KB of packets
    //  + 64KB of packet pointers
    //  + 8KB of free chain indices
    ethq_pkt_count = 1 << 4;
    size_t ethq_pool_size = ethq_pkt_count *
            (sizeof(ethq_pkt2K_t) +
             sizeof(ethq_pkt2K_t*) +
             sizeof(uint16_t));

    ethq_pkts = (ethq_pkt2K_t*)mmap(nullptr, ethq_pool_size,
                          PROT_READ | PROT_WRITE,
                          MAP_POPULATE | MAP_32BIT, -1, 0);
    if (!ethq_pkts || ethq_pkts == MAP_FAILED)
        return 0;

    ethq_pkt_list = (ethq_pkt_t**)(ethq_pkts + ethq_pkt_count);
    ethq_free_chain = (uint16_t*)(ethq_pkt_list + ethq_pkt_count);

    uintptr_t physaddr = 0;
    for (size_t i = 0; i < ethq_pkt_count; ++i) {
        if (!(i & 1))
            physaddr = mphysaddr(&ethq_pkts[i].pkt);
        else
            physaddr += sizeof(ethq_pkt2K_t);

        ethq_pkts[i].pkt.next = (i+1) < ethq_pkt_count
                ? &ethq_pkts[i+1].pkt
                : nullptr;

        ethq_pkts[i].pkt.physaddr = physaddr;

        ethq_pkt_list[i] = &ethq_pkts[i].pkt;
    }
    ethq_first_free = &ethq_pkts[0].pkt;

    // Initialize lock free MRU allocator
    ethq_free_chain[ethq_pkt_count-1] = 0xFFFF;
    for (size_t i = ethq_pkt_count-1; i > 0; --i) {
        ethq_free_chain[i-1] = i;
    }
    ethq_first_free_aba = 0;

#ifndef NDEBUG
    // Self test
    ethq_pkt_t *test1 = ethq_pkt_acquire();
    ethq_pkt_t *test2 = ethq_pkt_acquire();
    ethq_pkt_t *test3 = ethq_pkt_acquire();
    ethq_pkt_t *test4 = ethq_pkt_acquire();
    ethq_pkt_release(test1);
    ethq_pkt_release(test2);
    ethq_pkt_release(test3);
    ethq_pkt_release(test4);
    ethq_pkt_t *chk1 = ethq_pkt_acquire();
    ethq_pkt_t *chk2 = ethq_pkt_acquire();
    ethq_pkt_t *chk3 = ethq_pkt_acquire();
    ethq_pkt_t *chk4 = ethq_pkt_acquire();
    assert(chk4 == test1);
    assert(chk3 == test2);
    assert(chk2 == test3);
    assert(chk1 == test4);
    ethq_pkt_release(test4);
    ethq_pkt_release(test3);
    ethq_pkt_release(test2);
    ethq_pkt_release(test1);
#endif

    return 1;
}

#if 1
static ticketlock ethq_lock;
ethq_pkt_t *ethq_pkt_acquire(void)
{
    unique_lock<ticketlock> lock(ethq_lock);

    ethq_pkt_t *pkt = ethq_first_free;

    if (pkt)
        ethq_first_free = pkt->next;

    return pkt;
}

void ethq_pkt_release(ethq_pkt_t *pkt)
{
    unique_lock<ticketlock> lock(ethq_lock);

    pkt->next = ethq_first_free;
    ethq_first_free = pkt;
}

#else

ethq_pkt_t *ethq_pkt_acquire(void)
{
    for (uint32_t first_free_aba = ethq_first_free_aba;
         first_free_aba != 0xFFFF; pause()) {
        // Determine the next ABA protection sequence number
        uint32_t next_seq = (first_free_aba & -65536) + 0x10000;

        // Mask off ABA protection sequence number
        uint32_t first_free = first_free_aba & 0xFFFF;

        // See which packet is next
        uint32_t next_free = ethq_free_chain[first_free];

        // Try to atomically take a packet
        uint32_t old_first_free = atomic_cmpxchg(
                    &ethq_first_free_aba,
                    first_free_aba,
                    next_free | next_seq);

        // If no race, return pointer to packet
        if (old_first_free == first_free_aba) {
            ethq_pkt_t *pkt = ethq_pkt_list[first_free];

            ETHQ_TRACE("Acquired packet %p\n", (void*)pkt);

            return pkt;
        }

        // Try again with fresh value of ethq_first_free_aba
        first_free_aba = old_first_free;
    }

    return 0;
}

void ethq_pkt_release(ethq_pkt_t *pkt)
{
    uint16_t pkt_index = (ethq_pkt2K_t*)pkt - ethq_pkts;

    for (uint32_t first_free_aba = ethq_first_free_aba; ; pause()) {
        // Determine the next ABA protection sequence number
        uint32_t next_seq = (first_free_aba & -65536) + 0x10000;

        // Mask off ABA protection sequence number
        uint16_t first_free = first_free_aba & 0xFFFF;

        // Set next index for this slot
        ethq_free_chain[pkt_index] = first_free;

        uint32_t old_first_free = atomic_cmpxchg(
                    &ethq_first_free_aba,
                    first_free_aba,
                    pkt_index | next_seq);

        if (old_first_free == first_free_aba) {
            ETHQ_TRACE("Released packet %p\n", (void*)pkt);
            return;
        }

        // Try again with fresh value of ethq_first_free_aba
        first_free_aba = old_first_free;
    }
}
#endif

void ethq_enqueue(ethq_queue_t *queue, ethq_pkt_t *pkt)
{
    pkt->next = nullptr;

    ethq_pkt_t *head = queue->head;
    ethq_pkt_t *tail = queue->tail;

    if (!head && !tail) {
        queue->head = pkt;
        queue->tail = pkt;
    } else {
        head->next = pkt;
        queue->head = pkt;
    }
    pkt->next = nullptr;

    ++queue->count;
}

ethq_pkt_t *ethq_dequeue(ethq_queue_t *queue)
{
    ethq_pkt_t *head = queue->head;
    ethq_pkt_t *tail = queue->tail;

    if (tail && (head != tail)) {
        queue->tail = tail->next;
        tail->next = nullptr;
        --queue->count;
    } else if (tail) {
        queue->tail = nullptr;
        queue->head = nullptr;
        tail->next = nullptr;
        --queue->count;
    }

    return tail;
}

ethq_pkt_t *ethq_dequeue_all(ethq_queue_t *queue)
{
    ethq_pkt_t *all = queue->head;
    queue->tail->next = nullptr;
    queue->head = nullptr;
    queue->tail = nullptr;
    queue->count = 0;
    return all;
}
