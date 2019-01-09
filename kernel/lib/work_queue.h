#pragma once
#include "types.h"
#include "heap.h"
#include <utility.h>
#include "mutex.h"
#include "stdlib.h"
#include "mm.h"
#include "printk.h"
#include "bitsearch.h"

class workq_impl;
class workq;

// Abstract work queue work item
class workq_work {
public:
    workq_work()
    {
    }

    virtual ~workq_work() {
        __asm__ __volatile__ ("":::"memory");
    }

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

    workq_alloc()
        : top{}
        , map{}
    {
    }

    workq_alloc(workq_alloc const&) = delete;

    void *alloc()
    {
        return items + alloc_slot();
    }

    void free(void *p)
    {
        size_t i = (value_type*)p - items;
        free_slot(i);
    }

private:
    int alloc_slot()
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

    void free_slot(size_t i)
    {
        assert(i < capacity);

        size_t word = unsigned(i) >> 6;

        uint8_t bit = word & 63;

        // Clear that bit
        map[word] &= ~(UINT64_C(1) << bit);

        // Since we freed one, we know that the bit of level 0 must become 0
        top &= ~(UINT64_C(1) << word);
    }

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
    lock_type queue_lock;
    std::condition_variable not_empty;

    workq_alloc alloc;

    void enqueue_locked(workq_work *work, scoped_lock& lock);

    workq_work *head = nullptr;
    workq_work *tail = nullptr;

    void free(workq_work *work)
    {
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
        scoped_lock lock(queue_lock);
        return dequeue_work_locked(lock);
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
    workq_impl::scoped_lock lock(queue->queue_lock);
    void *mem = queue->allocate(queue, sizeof(workq_wrapper<T>));
    workq_wrapper<T> *item = new (mem) workq_wrapper<T>(
                std::forward<T>(functor));
    item->owner = queue;
    item->next = nullptr;
    percpu[cpu_nr].enqueue_locked(item, lock);
}
