#include "irq.h"
#include "types.h"
#include "string.h"
#include "mutex.h"
#include "atomic.h"
#include "assert.h"
#include "printk.h"
#include "cpu/interrupts.h"
#include "cpu/control_regs.h"
#include "thread.h"

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
static intr_link_t intr_first[INTR_COUNT];

// Unmask count for each interrupt
static uint8_t intr_unmask_count[INTR_COUNT];

// Interrupt handler vectors, could be large from IRQ sharing
#define MAX_INTR_HANDLERS   256
static intr_link_t intr_first_free;
static intr_link_t intr_handlers_count;
static intr_handler_reg_t intr_handlers[MAX_INTR_HANDLERS];

using intr_handler_reg_lock_type = ext::noirq_lock<ext::spinlock>;
using intr_handler_reg_scoped_lock =
    ext::unique_lock<intr_handler_reg_lock_type>;
static intr_handler_reg_lock_type intr_handler_reg_lock;

// Vectors
static irq_setmask_handler_t irq_setmask_vec;
static irq_islevel_handler_t irq_islevel_vec;
static irq_hook_handler_t irq_hook_vec;
static irq_unhook_handler_t irq_unhook_vec;
static msi_irq_alloc_handler_t msi_irq_alloc_vec;
static irq_setcpu_handler_t irq_setcpu_vec;
static irq_unhandled_irq_handler_t irq_unhandled_irq_vec;

void irq_setmask_set_handler(irq_setmask_handler_t handler)
{
    irq_setmask_vec = handler;
}

void irq_islevel_set_handler(irq_islevel_handler_t handler)
{
    irq_islevel_vec = handler;
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

static bool irq_mask_if_not_unmasked(int irq, bool unmask)
{
    if (unlikely(!intr_unmask_count[irq] && !unmask)) {
        printdbg("Got unexpected unmask=%d of IRQ %d, masking...\n",
                 unmask, irq);

        // Caller is masking something never hooked (this happens
        // when an IRQ is attempted to be dispatched and cannot be)
        // Tell the hardware to mask it just in case
        irq_setmask_vec(irq, false);
        return false;
    }

    return true;
}

void irq_setmask(int irq, bool unmask)
{
    cpu_scoped_irq_disable irq_dis;
    intr_handler_reg_scoped_lock lock(intr_handler_reg_lock);

    if (unlikely(!irq_mask_if_not_unmasked(irq, unmask)))
        return;

    // Unmask when unmask count transitions from 0 to 1
    // Mask when unmask count transitions from 1 to 0
    assert(intr_unmask_count[irq] != (unmask ? 255U : 0U));
    if ((intr_unmask_count[irq] += (unmask ? 1 : -1)) == (unmask ? 1 : 0))
        irq_setmask_vec(irq, unmask);
}

bool irq_islevel(int irq)
{
    return irq_islevel_vec(irq);
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

void intr_hook(int intr, intr_handler_t handler,
               char const *name, intr_eoi_t eoi_handler)
{
    cpu_scoped_irq_disable irq_dis;
    intr_handler_reg_scoped_lock lock(intr_handler_reg_lock);

    assert((intr >= INTR_APIC_DSP_BASE && intr <= INTR_APIC_DSP_LAST &&
           eoi_handler == eoi_lapic) ||
           (intr >= INTR_PIC_DSP_BASE && intr <= INTR_PIC_DSP_LAST &&
           eoi_handler == eoi_i8259) ||
           eoi_handler == eoi_none);

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
        entry->eoi_handler = eoi_handler;
        entry->handler = handler;

        if (eoi_handler == eoi_auto) {
            if (intr < 32)
                eoi_handler = eoi_none;
            else if (intr <= INTR_APIC_IRQ_END)
                eoi_handler = eoi_lapic;
            else
                eoi_handler = eoi_i8259;
        }

        // There is no such thing as software interrupts.
        // Every non-exception interrupt is an IPI or APIC timer or MSI IRQ,
        // if it is less than the beginning of the 8259 range

        assert(eoi_handler != eoi_lapic || (intr >= 32 &&
                                            (intr < (INTR_APIC_IRQ_BASE +
                                                     INTR_APIC_IRQ_COUNT))));

        assert(eoi_handler != eoi_i8259 || (intr >= INTR_PIC1_IRQ_BASE &&
                                            (intr < (INTR_PIC2_IRQ_BASE + 8))));

        assert(eoi_handler != eoi_none || (intr < INTR_PIC1_IRQ_BASE));

        atomic_st_rel(prev_link, entry - intr_handlers);
    }
}

void intr_handler_names(int intr)
{
    cpu_scoped_irq_disable irq_dis;
    intr_handler_reg_scoped_lock lock(intr_handler_reg_lock);

    if (intr_handlers_count == 0)
        return;

    intr_link_t *prev_link = &intr_first[intr];

    intr_handler_reg_t *entry = nullptr;
    while (*prev_link >= 0) {
        entry = intr_handlers + *prev_link;

        printdbg("vector %u handler: %s\n", entry->intr, entry->name);

        prev_link = &entry->next;

        entry = nullptr;
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
    cpu_scoped_irq_disable irq_dis;
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
    thread_check_stack(intr);

    intr_handler_reg_t *entry;
    intr_link_t i = intr_first[intr];
    for ( ; i >= 0 && ctx; i = entry->next) {
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

int irq_msi_alloc(msi_irq_mem_t *results, int count,
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
