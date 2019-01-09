#include "irq.h"
#include "types.h"
#include "string.h"
#include "mutex.h"
#include "cpu/atomic.h"
#include "assert.h"
#include "printk.h"

typedef int16_t intr_link_t;

struct intr_handler_reg_t {
    char const *name;
    intr_link_t next;
    int16_t refcount;
    int16_t intr;
    int16_t eoi_handler;
    intr_handler_t handler;
};

// Context save vector here for locality
size_t sse_context_size;
uint64_t sse_xsave_mask;

uint16_t sse_avx_offset;
uint16_t sse_avx_size;
uint16_t sse_avx512_opmask_offset;
uint16_t sse_avx512_opmask_size;
uint16_t sse_avx512_upper_offset;
uint16_t sse_avx512_upper_size;
uint16_t sse_avx512_xregs_offset;
uint16_t sse_avx512_xregs_size;

// Singly linked list head for each interrupt
static intr_link_t intr_first[256];

// Unmask count for each interrupt
static uint8_t intr_unmask_count[256];

// Interrupt handler vectors
#define MAX_INTR_HANDLERS   256
static intr_link_t intr_first_free;
static intr_link_t intr_handlers_count;
static intr_handler_reg_t intr_handlers[MAX_INTR_HANDLERS];

using intr_handler_reg_lock_type = std::mcslock;
using intr_handler_reg_scoped_lock =
    std::unique_lock<intr_handler_reg_lock_type>;
static intr_handler_reg_lock_type intr_handler_reg_lock;

// Vectors
static irq_setmask_handler_t irq_setmask_vec;
static irq_hook_handler_t irq_hook_vec;
static irq_unhook_handler_t irq_unhook_vec;
static msi_irq_alloc_handler_t msi_irq_alloc_vec;
static irq_setcpu_handler_t irq_setcpu_vec;
static irq_unhandled_irq_handler_t irq_unhandled_irq_vec;

void irq_setmask_set_handler(irq_setmask_handler_t handler)
{
    irq_setmask_vec = handler;
}

void irq_hook_set_handler(irq_hook_handler_t handler)
{
    irq_hook_vec = handler;
}

void irq_unhook_set_handler(irq_unhook_handler_t handler)
{
    irq_unhook_vec = handler;
}

void msi_irq_alloc_set_handler(msi_irq_alloc_handler_t handler)
{
    msi_irq_alloc_vec = handler;
}

void irq_setcpu_set_handler(irq_setcpu_handler_t handler)
{
    irq_setcpu_vec = handler;
}

void irq_set_unhandled_irq_handler(irq_unhandled_irq_handler_t handler)
{
    irq_unhandled_irq_vec = handler;
}

void irq_setmask(int irq, bool unmask)
{
    intr_handler_reg_scoped_lock lock(intr_handler_reg_lock);

    if (!intr_unmask_count[irq] && !unmask) {
        // Caller is masking something never hooked (this happens
        // when an IRQ is attempted to be dispatched and cannot be)
        // Tell the hardware to mask it just in case
        irq_setmask_vec(irq, unmask);
        return;
    }

    // Unmask when unmask count transitions from 0 to 1
    // Mask when unmask count transitions from 1 to 0
    assert(intr_unmask_count[irq] != (unmask ? 255U : 0U));
    if ((intr_unmask_count[irq] += (unmask ? 1 : -1)) == (unmask ? 1 : 0))
        irq_setmask_vec(irq, unmask);
}

void irq_hook(int irq, intr_handler_t handler, char const *name)
{
    irq_hook_vec(irq, handler, name);
}

void irq_unhook(int irq, intr_handler_t handler)
{
    irq_unhook_vec(irq, handler);
}

static intr_handler_reg_t *intr_alloc(void)
{
    intr_handler_reg_t *entry;

    if (intr_handlers_count == 0 || intr_first_free < 0) {
        entry = intr_handlers + intr_handlers_count++;
    } else {
        entry = intr_handlers + intr_first_free;
        intr_first_free = entry->next;
    }

    return entry;
}

void intr_hook(int intr, intr_handler_t handler, char const *name)
{
    intr_handler_reg_scoped_lock lock(intr_handler_reg_lock);

    if (intr_handlers_count == 0) {
        // First time initialization
        intr_first_free = -1;
        memset(intr_first, -1, sizeof(intr_first));
    }

    intr_link_t *prev_link = &intr_first[intr];

    intr_handler_reg_t *entry = nullptr;
    while (*prev_link >= 0) {
        entry = intr_handlers + *prev_link;

        if (entry->intr == intr &&
                entry->handler == handler) {
            ++entry->refcount;
            break;
        }

        prev_link = &entry->next;

        entry = nullptr;
    }

    if (!entry) {
        entry = intr_alloc();

        entry->name = name;
        entry->next = -1;
        entry->refcount = 1;
        entry->intr = intr;
        entry->eoi_handler = 0;
        entry->handler = handler;

        atomic_st_rel(prev_link, entry - intr_handlers);
    }
}

static void intr_delete(intr_link_t *prev_link,
                        intr_handler_reg_t *entry)
{
    *prev_link = entry->next;
    entry->next = intr_first_free;
    intr_first_free = entry - intr_handlers;
}

void intr_unhook(int intr, intr_handler_t handler)
{
    intr_handler_reg_scoped_lock lock(intr_handler_reg_lock);

    intr_link_t *prev_link = &intr_first[intr];

    intr_handler_reg_t *entry = nullptr;
    while (*prev_link != 0) {
        entry = intr_handlers + *prev_link;

        if (entry->intr == intr && entry->handler == handler) {
            if (--entry->refcount)
                intr_delete(prev_link, entry);
            break;
        }

        prev_link = &entry->next;

        entry = nullptr;
    }
}

_hot
int intr_has_handler(int intr)
{
    return intr_handlers_count > 0 && intr_first[intr] >= 0;
}

isr_context_t *intr_invoke(int intr, isr_context_t *ctx)
{
    intr_handler_reg_t *entry;
    intr_link_t i = intr_first[intr];
    // assert not empty handler list
    assert(i >= 0);
    for ( ; i >= 0; i = entry->next) {
        entry = intr_handlers + i;
        ctx = entry->handler(intr, ctx);
    }
    return ctx;
}

_hot
isr_context_t *irq_invoke(int intr, int irq, isr_context_t *ctx)
{
    if (intr_has_handler(intr)) {
        intr_handler_reg_t *entry;
        for (intr_link_t i = intr_first[intr];
             i >= 0; i = entry->next) {
            entry = intr_handlers + i;
            ctx = entry->handler(irq, ctx);
        }
    } else {
        printdbg("Ignored IRQ %d INTR %d!\n", irq, intr);
        irq_unhandled_irq_vec(intr, irq);
    }
    return ctx;
}

int msi_irq_alloc(msi_irq_mem_t *results, int count,
                  int cpu, int distribute)
{
    if (msi_irq_alloc_vec)
        return msi_irq_alloc_vec(results, count, cpu, distribute);

    // Not possible
    return 0;
}

bool irq_setcpu(int irq, int cpu)
{
    if (irq_setcpu_vec)
        return irq_setcpu_vec(irq, cpu);
    return false;
}
