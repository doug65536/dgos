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
class EXPORT workq_work {
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

    uint32_t top;
    uint32_t map[32];
    std::aligned_storage<item_sz, sizeof(void*)>::type items[capacity];
    static_assert(sizeof(map) <= 32*32/8, "Must be within allocator limits");
};

class workq {
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

private:
    template<typename T>
    class workq_wrapper : public workq_work
    {
    public:
        workq_wrapper(T&& functor);

    private:
        void invoke() override final;

        T functor;
    };
};

template<typename T>
workq::workq_wrapper<T>::workq_wrapper(T&& functor)
    : functor(std::forward<T>(functor))
{
}

template<typename T>
void workq::workq_wrapper<T>::invoke()
{
    functor();
}

class workq_impl : public workq {
public:
    workq_impl() = delete;
    workq_impl(uint32_t cpu_nr);

    ~workq_impl();

    workq_impl(workq_impl const&) = delete;
    workq_impl& operator=(workq_impl) = delete;

    void enqueue(workq_work *work);
    void set_affinity(int cpu);

private:
    friend class workq;
    using lock_type = ext::spinlock;
    using scoped_lock = std::unique_lock<lock_type>;

    struct work {
        typedef void (*callback_t)(uintptr_t arg);
        callback_t callback;
        uintptr_t arg;
    };

    void enqueue_and_unlock(workq_work *work, scoped_lock& lock);

    void free(workq_work *work);

    _noreturn
    static int worker(void *arg);

    workq_work *dequeue_work_locked(scoped_lock& lock);

    workq_work *dequeue_work();

    _noreturn
    void worker();

    lock_type queue_lock;
    std::condition_variable not_empty;
    int tid;

    workq_alloc alloc;

    workq_work *head = nullptr;
    workq_work *tail = nullptr;
};

template<typename T>
void workq::enqueue_on_all_barrier(T &&functor)
{
    size_t cpu_count = thread_get_cpu_count();
    workq_impl::lock_type wait_lock;
    std::condition_variable wait_done;
    size_t done_count = 0;

    for (size_t i = 0; i != cpu_count; ++i) {
        enqueue_on_cpu(i, [&, i] {
            functor(i);

            workq_impl::scoped_lock lock(wait_lock);
            if (++done_count == cpu_count) {
                lock.unlock();
                wait_done.notify_all();
            }
        });
    }

    workq_impl::scoped_lock lock(wait_lock);
    while (done_count != cpu_count)
        wait_done.wait(lock);
}

template<typename T>
void workq::enqueue(T&& functor)
{
    cpu_scoped_irq_disable irq_dis;
    uint32_t cpu_nr = thread_cpu_number();
    enqueue_on_cpu(cpu_nr, std::forward<T>(functor));
}

template<typename T>
_hot
void workq::enqueue_on_cpu(size_t cpu_nr, T&& functor)
{
    workq_impl *queue = percpu + cpu_nr;
    workq_impl::scoped_lock lock(queue->queue_lock);
    void *mem = queue->allocate(queue, sizeof(workq_wrapper<T>));
    workq_wrapper<T> *item = new (mem)
            workq_wrapper<T>(std::forward<T>(functor));
    percpu[cpu_nr].enqueue_and_unlock(item, lock);
}
