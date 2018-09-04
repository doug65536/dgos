#include "work_queue.h"
#include "thread.h"
#include "vector.h"
#include "mutex.h"
#include "heap.h"
#include "callout.h"
#include "cpu/thread_impl.h"

workq_impl* workq::percpu;

void workq::init(int cpu_count)
{
    percpu = new workq_impl[cpu_count];

    for (int i = 0; i < cpu_count; ++i)
        percpu[i].set_affinity(i);
}

void workq_impl::enqueue(workq_work *work)
{
    scoped_lock hold(lock);
    if (head == nullptr) {
        head = tail = work;
    } else {
        tail->next = work;
        tail = work;
    }
    not_empty.notify_one();
}

void workq_impl::set_affinity(int cpu)
{
    thread_set_affinity(tid, UINT64_C(1) << cpu);
}

void workq::free_item(workq_work *item)
{
    item->~workq_work();
    heap_free(item->owner->heap, item);
}

workq_work *workq::allocate(workq_impl *queue, size_t size)
{
    workq_work *item = (workq_work *)heap_alloc(queue->heap, size);
    return item;
}

void workq::slih_startup(void*)
{
    printk("Initializing kernel threadpool\n");
    init(thread_cpu_count());
}

REGISTER_CALLOUT(workq::slih_startup, nullptr,
                 callout_type_t::smp_online, "900");
