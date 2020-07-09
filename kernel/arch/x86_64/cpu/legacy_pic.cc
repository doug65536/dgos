#include "legacy_pic.h"
#include "ioport.h"
#include "irq.h"
#include "cpu/halt.h"
#include "interrupts.h"
#include "pic.bits.h"
#include "assert.h"

// Implements legacy Programmable Interrupt Controller,
// used if the APIC is not available
//
// The APIC replaces these functions when available
// This still needs to handle spurious IRQs though

#define PIC1_BASE   0x20
#define PIC2_BASE   0xA0

#define PIC1_CMD    PIC1_BASE
#define PIC2_CMD    PIC2_BASE
#define PIC1_DATA   (PIC1_BASE+1)
#define PIC2_DATA   (PIC2_BASE+1)

#define PIC_EOI     0x20

static uint16_t pic8259_mask;

// Get command port for master or slave
static uint16_t pic8259_port_cmd(int slave)
{
    return slave ? PIC2_CMD : PIC1_CMD;
}

#if 0 // not used
// Read Interrupt Request Register
static uint8_t pic8259_get_IRR(int slave)
{
    uint16_t port = pic8259_port_cmd(slave);
    outb(port, 0x0A);
    return inb(port);
}
#endif

// Read In Service Register
static uint8_t pic8259_get_ISR(int slave)
{
    uint16_t port = pic8259_port_cmd(slave);
    outb(port, 0x0B);
    return inb(port);
}

// Acknowledge IRQ
static void pic8259_eoi(int slave)
{
    uint16_t port = pic8259_port_cmd(slave);
    outb(port, PIC_EOI);

    if (slave)
        outb(PIC1_BASE, PIC_EOI);
}

static void pic8259_init(uint8_t pic1_irq_base,
                         uint8_t pic2_irq_base)
{
    // Base IRQ must be aligned to a multiple of 8
    assert((pic1_irq_base & PIC_ICW2_VECTOR) == pic1_irq_base);
    assert((pic2_irq_base & PIC_ICW2_VECTOR) == pic2_irq_base);

    // It doesn't make sense for them both to be at the same base
    assert(pic1_irq_base != pic2_irq_base);

    // ICW1 - ICW4 needed
    outb(PIC1_BASE + PIC_ICW1, PIC_ICW1_IC4 | PIC_ICW1_MBS);
    outb(PIC2_BASE + PIC_ICW1, PIC_ICW1_IC4 | PIC_ICW1_MBS);

    // Base IRQs
    outb(PIC1_BASE + PIC_ICW2, pic1_irq_base);
    outb(PIC2_BASE + PIC_ICW2, pic2_irq_base);

    // Starting with IBM PC/AT (286), there is a second PIC cascaded on IRQ 2

    // Slave bitmask written to master indicates IRQ 2 has slave attached
    outb(PIC1_BASE + PIC_ICW3_M, PIC_ICW3_M_S2);

    // Indicate that slave should respond when irq 2 cascade occurs
    outb(PIC2_BASE + PIC_ICW3_S, PIC_ICW3_S_ID_n(2));

    // 8086 mode
    outb(PIC1_BASE + PIC_ICW4, PIC_ICW4_8086);
    outb(PIC2_BASE + PIC_ICW4, PIC_ICW4_8086);

    // Initially all IRQs masked
    pic8259_mask = 0xFFFF;
    outb(PIC1_BASE + PIC_OCW1, pic8259_mask & 0xFF);
    outb(PIC2_BASE + PIC_OCW1, (pic8259_mask >> 8) & 0xFF);

    // Clean up the in-service register, EOI everything in service
    for (size_t slave = 0; slave < 2; ++slave) {
        while (pic8259_get_ISR(slave))
            pic8259_eoi(slave);
    }
}

// Get data port for master or slave
static uint16_t pic8259_port_data(int slave)
{
    return slave ? PIC2_DATA : PIC1_DATA;
}

// Detect and discard spurious IRQ
// or call IRQ handler and acknowledge IRQ
isr_context_t *pic8259_dispatcher(
        int intr, isr_context_t *ctx)
{
    ctx = thread_entering_irq(ctx);

    isr_context_t *returned_stack_ctx;

    int irq = intr - INTR_PIC1_IRQ_BASE;

    int is_slave = (irq >= 8);

    if (irq < 16) {
        uint8_t isr;

        // IRQ 7 or 15
        int spurious_irq = (is_slave << 3) + 7;

        if (irq == spurious_irq) {
            isr = pic8259_get_ISR(is_slave);

            if ((isr & (1 << 7)) == 0) {
                // Spurious IRQ!

                // Still need to ack master on
                // spurious slave IRQ
                if (irq >= 8)
                    pic8259_eoi(0);

                return ctx;
            }
        }
    }

    // If we made it here, it was not a spurious IRQ

    // Run IRQ handler
    returned_stack_ctx = irq_invoke(irq + INTR_PIC1_IRQ_BASE, irq, ctx);

    if (irq < 16) {
        // Acknowledge IRQ
        pic8259_eoi(is_slave);
    }

    return thread_finishing_irq(returned_stack_ctx);
}

void pic8259_disable(void)
{
    // Need to move it to reasonable ISRs even if disabled
    // in case of spurious IRQs
    pic8259_init(INTR_PIC1_IRQ_BASE, INTR_PIC2_IRQ_BASE);
}

// Gets plugged into irq_setmask
static void pic8259_setmask(int irq, bool unmask)
{
    if (unmask)
        pic8259_mask &= ~(1 << irq);
    else
        pic8259_mask |= (1 << irq);

    // Update cascade mask
    if (((pic8259_mask & 0xFF00) != 0xFF00) &&
            (pic8259_mask & (1 << 2))) {
        // Need to unmask cascade, IRQ2
        pic8259_mask &= ~(1 << 2);
        outb(PIC1_DATA, pic8259_mask & 0xFF);
    } else if (((pic8259_mask & 0xFF00) == 0xFF00) &&
            !(pic8259_mask & (1 << 2))) {
        // Mask cascade, no upper IRQs enabled
        pic8259_mask |= (1 << 2);
        outb(PIC1_DATA, pic8259_mask & 0xFF);
    }

    uint8_t is_slave = (irq >= 8);
    uint8_t shift = is_slave << 3;
    ioport_t port = pic8259_port_data(is_slave);

    outb(port, (pic8259_mask >> shift) & 0xFF);
}

static bool pic8259_islevel(int irq _unused)
{
    // Always edge on ISA
    return false;
}

// Gets plugged into irq_hook
static void pic8259_hook(int irq, intr_handler_t handler, char const *name)
{
    intr_hook(irq + INTR_PIC1_IRQ_BASE, handler, name, eoi_i8259);
}

// Gets plugged into irq_unhook
static void pic8259_unhook(int irq, intr_handler_t handler)
{
    intr_unhook(irq + INTR_PIC1_IRQ_BASE, handler);
}

void pic8259_enable(void)
{
    pic8259_init(INTR_PIC1_IRQ_BASE, INTR_PIC2_IRQ_BASE);
    irq_setmask_set_handler(pic8259_setmask);
    irq_islevel_set_handler(pic8259_islevel);
    irq_hook_set_handler(pic8259_hook);
    irq_unhook_set_handler(pic8259_unhook);
}
