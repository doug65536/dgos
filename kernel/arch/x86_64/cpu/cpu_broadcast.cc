#include "cpu_broadcast.h"
#include "string.h"
#include "stdlib.h"
#include "mutex.h"
#include "thread_impl.h"
#include "apic.h"
#include "printk.h"
#include "hash_table.h"

// Queued work item
struct cpu_broadcast_work_t {
    struct cpu_broadcast_work_t *next;
    cpu_broadcast_handler_t handler;
    void *data;
    int intr;
    int unique;
};

// Per-CPU message queue
struct cpu_broadcast_queue_t {
    cpu_broadcast_work_t *head;
    cpu_broadcast_work_t *tail;
    cpu_broadcast_work_t *free;

    ticketlock lock;
};

void cpu_broadcast_service(int intr, size_t slot)
{
    cpu_broadcast_queue_t *queue =
            (cpu_broadcast_queue_t *)thread_cls_get(slot);
    cpu_broadcast_work_t *work;
    int eoi_done = 0;

    unique_lock<ticketlock> lock(queue->lock);
    for (;;) {
        // Take whole list and release lock ASAP
        cpu_broadcast_work_t *head = queue->head;
        cpu_broadcast_work_t *tail = queue->head;
        if (!eoi_done) {
            eoi_done = 1;
            apic_eoi(intr);
        }
        queue->head = 0;
        queue->tail = 0;
        lock.unlock();

        // End loop when no
        if (!head)
            break;

        for (work = head; work; work = work->next)
            work->handler(work->data);

        // Insert into free list
        lock.lock();
        tail->next = queue->free;
        queue->free = head;
    }
}

static void *cpu_broadcast_init_cpu(void *arg)
{
    (void)arg;

    cpu_broadcast_queue_t *queue;
    void *mem = calloc(1, sizeof(*queue));
    queue = new (mem) cpu_broadcast_queue_t();

    return queue;
}

size_t cpu_broadcast_create(void)
{
    // Allocate a CPU-local storage slot
    size_t slot = thread_cls_alloc();

    thread_cls_init_each_cpu(slot, cpu_broadcast_init_cpu, 0);

    return slot;
}

static void cpu_broadcast_add_work(
        int cpu, void *slot_data, void *arg, size_t size)
{
    cpu_broadcast_queue_t *queue = (cpu_broadcast_queue_t *)slot_data;

    unique_lock<ticketlock> lock(queue->lock);

    cpu_broadcast_work_t *incoming_work = (cpu_broadcast_work_t *)arg;

    if (incoming_work->unique && queue->head)
        return;

    cpu_broadcast_work_t *queued_work;
    void *mem = malloc(sizeof(*queued_work) + size);

    queued_work = new (mem) cpu_broadcast_work_t();
    memcpy(queued_work, arg, sizeof(*queued_work));
    memcpy(queued_work + 1, queued_work->data, size);
    queued_work->data = queued_work + 1;

    while (queue->free) {
        cpu_broadcast_work_t *next = queue->free->next;
        free(queue->free);
        queue->free = next;
    }
    if (!queue->head) {
        queue->head = queue->tail = queued_work;
        thread_send_ipi(cpu, queued_work->intr);
    } else {
        queue->tail->next = queued_work;
        queue->tail = queued_work;
    }
    queued_work->next = nullptr;
}

void cpu_broadcast_message(int intr, size_t slot, int other_only,
                           cpu_broadcast_handler_t handler,
                           void *data, size_t size, int unique)
{
    cpu_broadcast_work_t work;
    work.handler = handler;
    work.data = data;
    work.intr = intr;
    work.unique = unique;
    thread_cls_for_each_cpu(slot, other_only,
                            cpu_broadcast_add_work, &work, size);
}
