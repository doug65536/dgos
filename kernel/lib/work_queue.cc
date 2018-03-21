#include "work_queue.h"
#include "thread.h"
#include "vector.h"
#include "mutex.h"
#include "heap.h"

workq_impl* workq::percpu;

class workq_impl : public workq {
public:
    workq_impl()
    {
        heap_create();
        tid = thread_create(worker, this, 0, false);
    }

    workq_impl(workq_impl const&) = delete;
    workq_impl& operator=(workq_impl) = delete;

    void enqueue(workq_work *work);

private:
    struct work {
        typedef void (*callback_t)(uintptr_t arg);
        callback_t callback;
        uintptr_t arg;
    };

    int tid;
    using lock_type = mcslock;
    using scoped_lock = unique_lock<lock_type>;
    lock_type lock;
    condition_variable not_empty;

    workq_work *head;
    workq_work *tail;

    static int worker(void *arg)
    {
        ((workq_impl*)arg)->worker();
        return 0;
    }

    workq_work *dequeue_work()
    {
        scoped_lock hold(lock);
        while (!head)
            not_empty.wait(hold);
        workq_work *item = head;
        head = item->next;
        return item;
    }

    void worker()
    {
        workq_work *item = dequeue_work();

        item->invoke();
        free_item(item);
    }
};

void workq::init(int cpu_count)
{
    percpu = new workq_impl[cpu_count];
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

void workq::free_item(workq_work *item)
{
    heap_free(item->owner->heap, item);
}

workq_work *workq::allocate(size_t size)
{
    size_t current_cpu = thread_cpu_number();
    workq_impl *queue = percpu + current_cpu;
    auto item = (workq_work *)heap_alloc(queue->heap, size);
    item->owner = queue;
    return item;
}
