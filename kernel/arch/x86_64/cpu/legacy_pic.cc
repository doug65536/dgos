#include "legacy_pic.h"
#include "ioport.h"
#include "irq.h"
#include "cpu/halt.h"
#include "idt.h"
#include "control_regs.h"

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

// PIC IRQ0 -> INT 32
#define PIC_IRQ_BASE    32

static uint16_t pic8259_mask;

static void pic8259_init(uint8_t pic1_irq_base,
                         uint8_t pic2_irq_base)
{
    // Expect BIOS has configured master/slave
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);

    // Base IRQs
    outb(PIC1_DATA, pic1_irq_base);
    outb(PIC2_DATA, pic2_irq_base);

    // Slave at IRQ 2
    outb(PIC1_DATA, 1 << 2);

    // Cascade identity
    outb(PIC2_DATA, 2);

    // 8086 mode
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    // Initially all IRQs masked
    pic8259_mask = 0xFFFF;

    outb(PIC1_DATA, pic8259_mask & 0xFF);
    outb(PIC2_DATA, (pic8259_mask >> 8) & 0xFF);
}

// Get command port for master or slave
static uint16_t pic8259_port_cmd(int slave)
{
    return slave ? PIC2_CMD : PIC1_CMD;
}

// Get data port for master or slave
static uint16_t pic8259_port_data(int slave)
{
    return slave ? PIC2_DATA : PIC1_DATA;
}

#if 0   // never used
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

// Detect and discard spurious IRQ
// or call IRQ handler and acknowledge IRQ
static isr_context_t *pic8259_dispatcher(
        int intr, isr_context_t *ctx)
{
    isr_context_t *returned_stack_ctx;

    int irq = intr - PIC_IRQ_BASE;

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
    returned_stack_ctx = irq_invoke(irq + PIC_IRQ_BASE,
                                    irq, ctx);

    if (irq < 16) {
        // Acknowledge IRQ
        pic8259_eoi(is_slave);
    }

    return returned_stack_ctx;
}

void pic8259_disable(void)
{
    // Need to move it to reasonable ISRs even if disabled
    // in case of spurious IRQs
    pic8259_init(PIC_IRQ_BASE, PIC_IRQ_BASE + 8);

    // Mask all IRQs
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

// Gets plugged into irq_setmask
static void pic8259_setmask(int irq, int unmask)
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

// Gets plugged into irq_hook
static void pic8259_hook(int irq, intr_handler_t handler)
{
    intr_hook(irq + PIC_IRQ_BASE, handler);
}

// Gets plugged into irq_unhook
static void pic8259_unhook(int irq, intr_handler_t handler)
{
    intr_unhook(irq + PIC_IRQ_BASE, handler);
}

void pic8259_enable(void)
{
    pic8259_init(PIC_IRQ_BASE, PIC_IRQ_BASE + 8);
    irq_dispatcher_set_handler(pic8259_dispatcher);
    irq_setmask_set_handler(pic8259_setmask);
    irq_hook_set_handler(pic8259_hook);
    irq_unhook_set_handler(pic8259_unhook);
    cpu_irq_enable();
}
