#include "eth_q.h"
#include "mm.h"
#include "assert.h"
#include "cpu/atomic.h"

// A half-page (exactly) packet
typedef struct ethq_pkt2K_t {
    ethq_pkt_t pkt;
    uint8_t align1[(PAGESIZE >> 1) - sizeof(ethq_pkt_t)];
} ethq_pkt2K_t;

C_ASSERT(sizeof(ethq_pkt2K_t) == (PAGESIZE >> 1));

static ethq_pkt2K_t *ethq_pkts;
static ethq_pkt_t **ethq_pkt_list;
static size_t ethq_pkt_count;
static uint16_t *ethq_free_chain;
static uint32_t volatile ethq_first_free_aba;

// Redundant calls are tolerated and ignored
int ethq_init(void)
{
    if (ethq_pkts)
        return 1;

    // Allocate 16MB of packets
    //  + 64KB of packet pointers
    //  + 8KB of free chain indices
    ethq_pkt_count = 1 << 13;
    size_t ethq_pool_size = ethq_pkt_count *
            (sizeof(ethq_pkt2K_t) +
             sizeof(ethq_pkt2K_t*) +
             sizeof(uint16_t));

    ethq_pkts = mmap(0, ethq_pool_size,
                          PROT_READ | PROT_WRITE,
                          MAP_POPULATE | MAP_32BIT, -1, 0);
    if (!ethq_pkts || ethq_pkts == MAP_FAILED)
        return 0;

    ethq_pkt_list = (void*)(ethq_pkts + ethq_pkt_count);
    ethq_free_chain = (void*)(ethq_pkt_list + ethq_pkt_count);

    uintptr_t physaddr;
    for (size_t i = 0; i < ethq_pkt_count; ++i) {
        if (!(i & 1))
            physaddr = mphysaddr(&ethq_pkts->pkt);
        else
            physaddr += sizeof(ethq_pkt2K_t);

        ethq_pkts[i].pkt.pkt_physaddr = physaddr;

        ethq_pkt_list[i] = &ethq_pkts[i].pkt;
    }

    // Initialize lock free MRU allocator
    ethq_free_chain[ethq_pkt_count-1] = 0xFFFF;
    for (size_t i = ethq_pkt_count-1; i > 0; --i) {
        ethq_free_chain[i-1] = i;
    }
    ethq_first_free_aba = 0;

    return 1;
}

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
        if (old_first_free == first_free_aba)
            return ethq_pkt_list[first_free];

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

        if (old_first_free == first_free_aba)
            return;

        // Try again with fresh value of ethq_first_free_aba
        first_free_aba = old_first_free;
    }
}

void ethq_enqueue(ethq_queue_t *queue, ethq_pkt_t *pkt)
{
    pkt->next = 0;

    spinlock_hold_t hold = spinlock_lock_noirq(&queue->lock);

    ethq_pkt_t *head = queue->head;
    ethq_pkt_t *tail = queue->tail;

    if (!head && !tail) {
        queue->head = pkt;
        queue->tail = pkt;
    } else {
        head->next = pkt;
        queue->head = pkt;
    }

    ++queue->count;

    spinlock_unlock_noirq(&queue->lock, &hold);
}

ethq_pkt_t *ethq_dequeue(ethq_queue_t *queue)
{
    spinlock_hold_t hold = spinlock_lock_noirq(&queue->lock);

    ethq_pkt_t *head = queue->head;
    ethq_pkt_t *tail = queue->tail;

    if (tail && (head != tail)) {
        queue->tail = tail->next;
        --queue->count;
    } else if (tail) {
        queue->tail = 0;
        queue->head = 0;
        --queue->count;
    }

    spinlock_unlock_noirq(&queue->lock, &hold);

    return tail;
}
