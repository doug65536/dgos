#include "work_queue.h"
#include "thread.h"
#include "vector.h"
#include "mutex.h"
#include "heap.h"
#include "callout.h"
#include "cpu/thread_impl.h"

EXPORT workq_impl* workq::percpu;

void workq::init(int cpu_count)
{
    percpu = new workq_impl[cpu_count];

    for (int i = 0; i < cpu_count; ++i)
        percpu[i].set_affinity(i);
}

EXPORT void workq_impl::enqueue(workq_work *work)
{
    cpu_scoped_irq_disable irq_dis;
    scoped_lock lock(queue_lock);
    enqueue_locked(work, lock);
}

EXPORT void workq_impl::enqueue_locked(workq_work *work, scoped_lock& lock)
{
    workq_work **prev_ptr = head ? &tail->next : &head;
    *prev_ptr = work;
    tail = work;
    not_empty.notify_one();
}

EXPORT void workq_impl::set_affinity(int cpu)
{
    thread_set_affinity(tid, cpu);
}

_hot
EXPORT workq_work *workq::allocate(workq_impl *queue, size_t size)
{
    assert(size <= workq_alloc::item_sz);
    workq_work *item = (workq_work *)queue->alloc.alloc();
    return item;
}

void workq::slih_startup(void*)
{
    printk("Initializing kernel threadpool\n");
    init(thread_cpu_count());
}

REGISTER_CALLOUT(workq::slih_startup, nullptr,
                 callout_type_t::smp_online, "900");

EXPORT workq_work::workq_work()
{
}

EXPORT workq_work::~workq_work()
{
    __asm__ __volatile__ ("":::"memory");
}

workq_alloc::workq_alloc()
    : top{}
    , map{}
{
}

void *workq_alloc::alloc()
{
    int slot;
    if (likely((slot = alloc_slot()) >= 0))
        return items + alloc_slot();
    panic("Ran out of workq memory");
}

void workq_alloc::free(void *p)
{
    size_t i = (value_type*)p - items;
    free_slot(i);
}

int workq_alloc::alloc_slot()
{
    // The top map will be all 1 bits when all are taken
    if (unlikely(~top == 0))
        return -1;

    // Find the first qword with a 0 bit
    size_t word = bit_lsb_set(~top);

    // Find the first 0 bit in that qword
    uint8_t bit = bit_lsb_set(~map[word]);
    uint64_t upd = map[word] | (UINT64_C(1) << bit);
    map[word] = upd;

    // Build a mask that will set the top bit to 1
    // if all underlying bits are now 1
    uint64_t top_mask = (UINT64_C(1) << word) & -(~upd == 0);

    top |= top_mask;

    return int(word << 6) + bit;
}

void workq_alloc::free_slot(size_t i)
{
    assert(i < capacity);

    size_t word = unsigned(i) >> 6;

    uint8_t bit = word & 63;

    // Clear that bit
    map[word] &= ~(UINT64_C(1) << bit);

    // Since we freed one, we know that the bit of level 0 must become 0
    top &= ~(UINT64_C(1) << word);
}
