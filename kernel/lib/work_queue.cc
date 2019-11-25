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
    percpu = new (std::nothrow) workq_impl[cpu_count]();

    for (int i = 0; i < cpu_count; ++i)
        percpu[i].set_affinity(i);
}

EXPORT void workq_impl::enqueue(workq_work *work)
{
    cpu_scoped_irq_disable irq_dis;
    scoped_lock lock(queue_lock);
    enqueue_and_unlock(work, lock);
}

EXPORT void workq_impl::enqueue_and_unlock(workq_work *work, scoped_lock& lock)
{
    work->next = nullptr;
    work->owner = this;

    assert((tail != nullptr) == (head != nullptr));
    assert(!tail || tail->next == nullptr);

    // Point tail next to new node if one exists, otherwise point head
    workq_work **prev_ptr = tail ? &tail->next : &head;
    *prev_ptr = work;
    tail = work;
    lock.unlock();
    not_empty.notify_one();
}

void workq_impl::free(workq_work *work)
{
    cpu_scoped_irq_disable irq_dis;
    scoped_lock lock(queue_lock);
    alloc.free(work);
}

int workq_impl::worker(void *arg)
{
    ((workq_impl*)arg)->worker();
    // noreturn
}

workq_work *workq_impl::dequeue_work_locked(workq_impl::scoped_lock &lock)
{
    while (!head)
        not_empty.wait(lock);
    workq_work *item = head;
    head = item->next;
    tail = head ? tail : nullptr;
    return item;
}

workq_work *workq_impl::dequeue_work()
{
    cpu_scoped_irq_disable irq_dis;
    scoped_lock lock(queue_lock);
    workq_work *result = dequeue_work_locked(lock);
    return result;
}

void workq_impl::worker()
{
    for (;;) {
        workq_work *item = dequeue_work();

        item->invoke();
        free(item);
    }
}

EXPORT void workq_impl::set_affinity(int cpu)
{
    thread_set_affinity(tid, thread_cpu_mask_t(cpu));
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
    int cpu_count = thread_cpu_count();
    init(cpu_count);
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
    uint32_t upd = map[word] | (UINT32_C(1) << bit);
    map[word] = upd;

    // Build a mask that will set the top bit to 1
    // if all underlying bits are now 1
    uint32_t top_mask = (UINT32_C(1) << word) & -(~upd == 0);

    top |= top_mask;

    return int(word << 5) + bit;
}

void workq_alloc::free_slot(size_t i)
{
    assert(i < capacity);

    size_t word = i >> 5;

    uint8_t bit = word & 31;

    // Clear that bit
    map[word] &= ~(UINT32_C(1) << bit);

    // Since we freed one, we know that the bit of level 0 must become 0
    top &= ~(UINT64_C(1) << word);
}

workq_impl::workq_impl()
{
    tid = thread_create(worker, this, 0, false, false);
}

workq_impl::~workq_impl()
{
    if (tid > 0)
        thread_close(tid);
}
