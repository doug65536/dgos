#include "legacy_timer_pic.h"
#include "ioport.h"

// Implements legacy Programmable Interval Timer
// and Programmable Interrupt Controller, used
// if the APIC is not available
//
// The APIC replaces these functions when available

#define PIC1_BASE   0x20
#define PIC2_BASE   0xA0

#define PIC1_CMD    PIC1_BASE
#define PIC2_CMD    PIC2_BASE
#define PIC1_DATA   (PIC1_BASE+1)
#define PIC2_DATA   (PIC2_BASE+1)

#define PIC_EOI     0x20

static void (*irq_handlers[16])(int irq);

static void pic8259_init(uint8_t pic1_irq_base, uint8_t pic2_irq_base)
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

// Read Interrupt Request Register
static uint8_t pic8259_get_IRR(int slave)
{
    uint16_t port = pic8259_port_cmd(slave);
    outb(port, 0x0A);
    return inb(port);
}

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
}

// Detect and discard spurious IRQ
// or call IRQ handler and acknowledge IRQ
static void pic8259_dispatcher(int irq)
{
    uint8_t isr;
    int is_slave = (irq >= 8);

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

            return;
        }
    }

    // If we made it here, it was not a spurious IRQ
    if (irq_handlers[irq])
        irq_handlers[irq](irq);

    pic8259_eoi(is_slave);
}

void timerpic_init(void)
{
    // Reprogram PIC to put IRQ0-15 at INT 0x20-0x2F
    // and mask all IRQs
    pic8259_init(0x20, 0x28);

    // Mask all IRQs except cascade and timer (IRQ0 and IRQ2)
    outb(PIC1_DATA, ~(((1 << 0) | (1 << 2))) & 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void timerpic_disable(void)
{
    // Need to move it to reasonable ISRs even if disabled
    // in case of spurious IRQs
    timerpic_init();

    // Mask all IRQs
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
