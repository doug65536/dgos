#pragma once
#include "types.h"
#include "heap.h"
#include <utility.h>

class workq_impl;
class workq;

// Abstract work queue work item
class workq_work {
public:
    virtual void invoke() = 0;

private:
    workq_work *next;
    workq_impl *owner;

    friend class workq_impl;
    friend class workq;
};

class workq {
public:
    template<typename T, typename... Args>
    static T* construct(Args&& ...args)
    {
        void *mem = workq::allocate(sizeof(T));
        return new (mem) T(forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    static void emplace(Args&& ...args)
    {
        T* item = construct(forward<Args>(args)...);
        enqueue(item);
    }

    static void init(int cpu_count);

protected:
    static void free_item(workq_work *item);

    // Per CPU heap
    heap_t *heap;

    // Array of queues, one per CPU
    static workq_impl* percpu;

private:
    // Allocate memory on the per-cpu heap
    static workq_work *allocate(size_t size);
};
