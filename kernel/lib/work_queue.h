#pragma once
#include "types.h"
#include "heap.h"
#include <utility.h>
#include "mutex.h"

class workq_impl;
class workq;

// Abstract work queue work item
class workq_work {
public:
    workq_work()
        : next(nullptr)
    {
    }

    virtual void invoke() = 0;

private:
    workq_work *next;
    workq_impl *owner;

    friend class workq_impl;
    friend class workq;
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

    //template<typename T, typename... Args>
    //static void emplace(Args&& ...args)
    //{
    //    T* item = construct(std::forward<Args>(args)...);
    //    enqueue(item);
    //}

    static void init(int cpu_count);

protected:
    static void free_item(workq_work *item);

    // Array of queues, one per CPU
    static workq_impl* percpu;

private:
    // Allocate memory on the per-cpu heap
    static workq_work *allocate(workq_impl *queue, size_t size);
};

class workq_impl : public workq {
public:
    workq_impl()
        : head(nullptr)
        , tail(nullptr)
    {
        heap = heap_create();
        tid = thread_create(worker, this, 0, false);
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
    using lock_type = std::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;
    lock_type lock;
    std::condition_variable not_empty;

    heap_t *heap;
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
        for (;;) {
            workq_work *item = dequeue_work();

            item->invoke();
            free_item(item);
        }
    }
};

template<typename T>
void workq::enqueue(T&& functor)
{
    int current_cpu = thread_cpu_number();
    workq_impl *queue = percpu + current_cpu;
    void *mem = workq::allocate(queue, sizeof(T));
    workq_wrapper<T> *item = new (mem) workq_wrapper<T>(
                std::forward<T>(functor));
    item->owner = queue;
    item->next = nullptr;
    percpu[current_cpu].enqueue(item);
}
