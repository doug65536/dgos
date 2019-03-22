#pragma once
#include "types.h"
#include "heap.h"
#include <utility.h>
#include "mutex.h"
#include "stdlib.h"
#include "mm.h"
#include "printk.h"
#include "bitsearch.h"
#include "cpu/control_regs.h"

class workq_impl;
class workq;

// Abstract work queue work item
class workq_work {
public:
    workq_work();

    virtual ~workq_work();

    virtual void invoke() = 0;

private:
    workq_work *next = nullptr;
    workq_impl *owner = nullptr;

    friend class workq_impl;
    friend class workq;
};

class workq_alloc {
public:
    static constexpr const size_t capacity = 32 * 32;
    static constexpr const size_t item_sz = 128;

    using value_type = std::aligned_storage<item_sz, 8>::type;

    workq_alloc();

    workq_alloc(workq_alloc const&) = delete;

    void *alloc();

    void free(void *p);

private:
    int alloc_slot();

    void free_slot(size_t i);

    std::aligned_storage<item_sz, 8>::type items[capacity];
    uint32_t top;
    uint32_t map[32];
    static_assert(sizeof(map) <= 32*32/8, "Must be within allocator limits");
};

class workq {
private:
    template<typename T>
    class workq_wrapper : public workq_work
    {
    public:
        workq_wrapper(T&& functor)
            : functor(std::forward<T>(functor))
        {
        }

    private:
        void invoke() override final
        {
            functor();
        }

        T functor;
    };

public:
    template<typename T>
    static void enqueue(T&& functor);

    template<typename T>
    static void enqueue_on_cpu(size_t cpu_nr, T&& functor);

    // Run functor once on every cpu and block until every cpu is finished
    template<typename T>
    static void enqueue_on_all_barrier(T&& functor);

    static void init(int cpu_count);

    static void slih_startup(void *);

    // Allocate memory on a fixed size lockless allocator
    workq_work *allocate(workq_impl *queue, size_t size);

protected:
    void free_item(workq_work *item);

    // Array of queues, one per CPU
    static workq_impl* percpu;
};

class workq_impl : public workq {
public:
    workq_impl()
    {
        tid = thread_create(worker, this, 0, false);
    }

    ~workq_impl()
    {
        if (tid > 0)
            thread_close(tid);
    }

    workq_impl(workq_impl const&) = delete;
    workq_impl& operator=(workq_impl) = delete;

    void enqueue(workq_work *work);
    void set_affinity(int cpu);

private:
    friend class workq;

    struct work {
        typedef void (*callback_t)(uintptr_t arg);
        callback_t callback;
        uintptr_t arg;
    };

    int tid;
    using lock_type = ext::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;
    lock_type queue_lock;
    std::condition_variable not_empty;

    workq_alloc alloc;

    void enqueue_locked(workq_work *work, scoped_lock& lock);

    workq_work *head = nullptr;
    workq_work *tail = nullptr;

    void free(workq_work *work)
    {
        cpu_scoped_irq_disable irq_dis;
        scoped_lock lock(queue_lock);
        alloc.free(work);
    }

    static int worker(void *arg)
    {
        ((workq_impl*)arg)->worker();
        return 0;
    }

    workq_work *dequeue_work_locked(scoped_lock& lock)
    {
        while (!head)
            not_empty.wait(lock);
        workq_work *item = head;
        head = item->next;
        tail = head ? tail : nullptr;
        return item;
    }

    workq_work *dequeue_work()
    {
        cpu_scoped_irq_disable irq_dis;
        scoped_lock lock(queue_lock);
        workq_work *result = dequeue_work_locked(lock);
        return result;
    }

    void worker()
    {
        for (;;) {
            workq_work *item = dequeue_work();

            item->invoke();
            free(item);
        }
    }
};

template<typename T>
void workq::enqueue_on_all_barrier(T &&functor)
{
    size_t cpu_count = thread_get_cpu_count();
    ext::mcslock wait_lock;
    std::condition_variable wait_done;
    size_t done_count = 0;

    for (size_t i = 0; i != cpu_count; ++i) {
        functor(i);

        std::unique_lock<ext::mcslock> lock(wait_lock);
        if (++done_count == cpu_count) {
            lock.unlock();
            wait_done.notify_all();
        }
    }

    std::unique_lock<ext::mcslock> lock(wait_lock);
    while (done_count != cpu_count)
        wait_done.wait(lock);
}

template<typename T>
void workq::enqueue(T&& functor)
{
    size_t cpu_nr = thread_cpu_number();
    enqueue_on_cpu(cpu_nr, std::forward<T>(functor));
}

template<typename T>
_hot
void workq::enqueue_on_cpu(size_t cpu_nr, T&& functor)
{
    workq_impl *queue = percpu + cpu_nr;
    cpu_scoped_irq_disable irq_dis;
    workq_impl::scoped_lock lock(queue->queue_lock);
    void *mem = queue->allocate(queue, sizeof(workq_wrapper<T>));
    workq_wrapper<T> *item = new (mem) workq_wrapper<T>(
                std::forward<T>(functor));
    item->owner = queue;
    item->next = nullptr;
    percpu[cpu_nr].enqueue_locked(item, lock);
}
