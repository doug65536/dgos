#include "legacy_pit.h"
#include "ioport.h"
#include "irq.h"
#include "atomic.h"
#include "time.h"
#include "cpu/thread_impl.h"
#include "interrupts.h"
#include "assert.h"
#include "spinlock.h"

#include "conio.h"
#include "printk.h"

#define DEBUG_PIT   1
#if DEBUG_PIT
#define PIT_TRACE(...) printdbg("pit: " __VA_ARGS__)
#else
#define PIT_TRACE(...) ((void)0)
#endif

//
// Command byte

#define PIT_CHANNEL_BIT     6
#define PIT_ACCESS_BIT      4
#define PIT_MODE_BIT        1
#define PIT_FORMAT_BIT      0

#define PIT_CHANNEL(n)      ((n) << PIT_CHANNEL_BIT)
#define PIT_READBACK        PIT_CHANNEL(3)

#define PIT_ACCESS(n)       ((n) << PIT_ACCESS_BIT)
#define PIT_ACCESS_LATCH    PIT_ACCESS(0)
#define PIT_ACCESS_LO       PIT_ACCESS(1)
#define PIT_ACCESS_HI       PIT_ACCESS(2)
#define PIT_ACCESS_BOTH     PIT_ACCESS(3)

#define PIT_MODE(n)         ((n) << PIT_MODE_BIT)
#define PIT_MODE_IOTC       PIT_MODE(0)
#define PIT_MODE_ONESHOT    PIT_MODE(1)
#define PIT_MODE_RATEGEN    PIT_MODE(2)
#define PIT_MODE_SQUARE     PIT_MODE(3)
#define PIT_MODE_SOFTSTROBE PIT_MODE(4)
#define PIT_MODE_HARDSTROBE PIT_MODE(5)
#define PIT_MODE_RATE2      PIT_MODE(6)
#define PIT_MODE_SQUARE2    PIT_MODE(7)

#define PIT_FORMAT(n)       ((n) << PIT_FORMAT_BIT)
#define PIT_FORMAT_BINARY   PIT_FORMAT(0)
#define PIT_FORMAT_BCD      PIT_FORMAT(1)

//
// Ports

#define PIT_BASE            0x40

// Write only
#define PIT_CMD             (PIT_BASE + 3)

// Read/write
#define PIT_DATA(channel)   (PIT_BASE + (channel))

// Timer crystal runs at 1.193181666... MHz
// Freq * 6 = 7159090.0 even

static spinlock_t pit8253_lock;

// Total timer interrupts, no time relationship
static uint64_t volatile timer_ticks;

// Exceeds 2^63 in about 292 years
static uint64_t volatile timer_ns;

static unsigned rate_hz;
static uint64_t tick_ns;
static uint64_t accumulator;

static void pit8254_set_rate(unsigned hz)
{
    // Minimum divisor: 19, or 62kHz rate
    if (hz > 62500U)
        hz = 62500U;

    // Clamp to valid range
    uint16_t divisor;
    if (hz <= 18U)
        divisor = 0xFFFFU;
    else
        divisor = 7159090U / (hz * 6U);

    tick_ns = 600000000UL * divisor / 715909;

    rate_hz = hz;
    accumulator = 0U;

    spinlock_lock_noirq(&pit8253_lock);

    outb(PIT_CMD,
         PIT_CHANNEL(0) |
         PIT_ACCESS_BOTH |
         PIT_MODE_RATEGEN |
         PIT_FORMAT_BINARY);
    outb(PIT_DATA(0), divisor & 0xFF);
    outb(PIT_DATA(0), (divisor >> 8) & 0xFF);

    spinlock_unlock_noirq(&pit8253_lock);
}

static isr_context_t *pit8254_irq_handler(int irq, isr_context_t *ctx)
{
    (void)irq;
    assert(irq == 0);

    atomic_inc(&timer_ticks);
    atomic_add(&timer_ns, tick_ns);

    // Test
    static uint64_t last_time;
    if (last_time + 1000000000 <= timer_ns) {
        last_time += 1000000000;

        printdbg("PIT Time: %8ld\n", timer_ns);

        //con_draw_xy(70, 0, buf, 7);
    }

    return ctx;
}

static uint64_t pit8254_time_ns()
{
    return timer_ns;
}

static void pit8254_time_ns_stop()
{
    PIT_TRACE("Disabling PIT timer tick\n");

    irq_setmask(0, 0);

    spinlock_lock_noirq(&pit8253_lock);
    outb(PIT_CMD,
         PIT_CHANNEL(0) |
         PIT_ACCESS_BOTH |
         PIT_MODE_ONESHOT |
         PIT_FORMAT_BINARY);
    outb(PIT_DATA(0), 0);
    outb(PIT_DATA(0), 0);
    spinlock_unlock_noirq(&pit8253_lock);

    irq_unhook(0, pit8254_irq_handler);
}

#if __amd64__
// Multiply two unsigned 64 bit values, giving an intermediate 128 bit result,
// then divide that by an unsigned 64 bit value, and return the quotient
__const
static inline uint64_t mul_64_64_div_64(uint64_t m1, uint64_t m2, uint64_t d)
{
    __asm__ (
        "mul %[m2]\n\t"
        "div %[d]\n\t"
        : [m1] "+a" (m1)
        , [m2] "+d" (m2)
        : [d] "r" (d)
        : "cc"
    );
    return m1;
}
#else
#error Need 32x32->64/32 implementation
#endif

// Returns number of elapsed nanoseconds
static uint64_t pit8254_nsleep(uint64_t nanosec)
{
    uint64_t count = mul_64_64_div_64(nanosec, 1193182, 1000000000);

    count += 64;

    if (count > 0xFFFF)
        count = 0xFFFF;

    spinlock_lock_noirq(&pit8253_lock);

    outb(PIT_CMD, PIT_CHANNEL(2) | PIT_ACCESS_BOTH |
         PIT_MODE_ONESHOT | PIT_FORMAT_BINARY);
    outb(PIT_DATA(2), count & 0xFF);
    outb(PIT_DATA(2), (count >> 8) & 0xFF);

    uint32_t readback;
    do {
        outb(PIT_CMD,
            PIT_CHANNEL(2) |
            PIT_ACCESS_LATCH);
        readback = inb(PIT_DATA(2));
        readback |= inb(PIT_DATA(2)) << 8;
        pause();
    } while (readback > 64);

    spinlock_unlock_noirq(&pit8253_lock);

    return mul_64_64_div_64(count - readback, 1000000000, 1193182);
}

void pit8253_init()
{
    nsleep_set_handler(pit8254_nsleep, nullptr, false);
}

void pit8254_enable()
{
    if (!time_ns_set_handler(pit8254_time_ns, pit8254_time_ns_stop, false))
        return;

    PIT_TRACE("Starting PIT timer\n");

    pit8254_set_rate(60);
    irq_hook(0, pit8254_irq_handler);
    irq_setmask(0, true);
}
