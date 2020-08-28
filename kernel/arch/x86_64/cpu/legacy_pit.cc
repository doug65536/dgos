#include "legacy_pit.h"
#include "assert.h"
#include "ioport.h"
#include "irq.h"
#include "atomic.h"
#include "time.h"
#include "cpu/thread_impl.h"
#include "interrupts.h"
#include "mutex.h"
#include "inttypes.h"
#include "work_queue.h"

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

#define PIT_READBACK_CNT_BIT    5
#define PIT_READBACK_STS_BIT    4
#define PIT_READBACK_CNT2_BIT   3
#define PIT_READBACK_CNT1_BIT   2
#define PIT_READBACK_CNT0_BIT   1

#define PIT_READBACK_CNT        (1 << PIT_READBACK_CNT_BIT)
#define PIT_READBACK_STS        (1 << PIT_READBACK_STS_BIT)
#define PIT_READBACK_CNT2       (1 << PIT_READBACK_CNT2_BIT)
#define PIT_READBACK_CNT1       (1 << PIT_READBACK_CNT1_BIT)
#define PIT_READBACK_CNT0       (1 << PIT_READBACK_CNT0_BIT)

#define PIT_CHANNEL(n)      ((n) << PIT_CHANNEL_BIT)
#define PIT_READBACK        PIT_CHANNEL(3)

#define PIT_READBACK_CH0    PIT_READBACK | \
                            PIT_READBACK_CNT | \
                            PIT_READBACK_CNT0_BIT
#define PIT_READBACK_CH2    PIT_READBACK | \
                            PIT_READBACK_CNT | \
                            PIT_READBACK_CNT2_BIT

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

#define PIT_IRQ     0

// Timer crystal runs at 1193181 and 2/3 MHz
// Freq * 3 = 3579545 even
#define PIT_FREQ_NUMER  3579545
#define PIT_FREQ_DENOM  3

using pit8254_lock_type = ext::noirq_lock<ext::spinlock>;
using pit8254_scoped_lock = ext::unique_lock<pit8254_lock_type>;
static pit8254_lock_type pit8254_lock;

// When the timer ticks, this value is loaded into the timer
static uint_fast16_t divisor;

// Total timer interrupts, no time relationship
static uint64_t volatile timer_ticks;

// Exceeds 2^63 in about 292 years
static int64_t volatile timer_ns;

static unsigned rate_hz;
static uint64_t tick_ns;
static uint64_t accumulator;

static void pit8254_set_rate(unsigned hz)
{
    // Minimum divisor: 19, or 62kHz rate
    if (hz > 62500U)
        hz = 62500U;

    // Clamp to valid range
    if (hz <= 18U)
        divisor = 0xFFFFU;
    else
        divisor = PIT_FREQ_NUMER / (hz * PIT_FREQ_DENOM);

    // Calculate nanoseconds per tick for the given timer frequency
    // The worst case divisor is under 48 bit
    // The divisor is constant 3.5 million
    // The highest possible result is just over 54 million
    tick_ns = uint64_t(PIT_FREQ_DENOM * 1000000000U) *
            divisor / PIT_FREQ_NUMER;

    rate_hz = hz;
    accumulator = 0U;

    cpu_scoped_irq_disable irq_dis;
    pit8254_scoped_lock lock(pit8254_lock);

    outb(PIT_CMD,
         PIT_CHANNEL(0) |
         PIT_ACCESS_BOTH |
         PIT_MODE_RATEGEN |
         PIT_FORMAT_BINARY);
    outb(PIT_DATA(0), divisor & 0xFF);
    outb(PIT_DATA(0), (divisor >> 8) & 0xFF);
}

static isr_context_t *pit8254_irq_handler(int irq, isr_context_t *ctx)
{
    (void)irq;
    assert(irq == PIT_IRQ);

    atomic_inc(&timer_ticks);
    atomic_add(&timer_ns, tick_ns);

    // Test
    static int64_t last_time;
    if (last_time + 1000000000 <= timer_ns) {
        last_time += 1000000000;

//        workq::enqueue([=] {
//            printdbg("PIT Time: %8" PRId64 "\n", timer_ns);
//        });
    }

    return ctx;
}

static uint64_t pit8254_time_ns()
{
    cpu_scoped_irq_disable irq_dis;
    pit8254_scoped_lock lock(pit8254_lock);

    outb(PIT_CMD,
        PIT_READBACK_CH0);

    outb(PIT_CMD,
        PIT_CHANNEL(0) |
        PIT_ACCESS_LATCH);

    uint16_t readback;
    readback = inb(PIT_DATA(0));
    readback |= inb(PIT_DATA(0)) << 8;

    uint64_t base = timer_ns;

    lock.unlock();

    uint64_t tick_offset = divisor - readback;

    uint64_t result = base + (tick_offset * 600000000 / 715909);

    return result;
}

static void pit8254_time_ns_stop()
{
    PIT_TRACE("Disabling PIT timer tick\n");

    irq_setmask(PIT_IRQ, false);

    cpu_scoped_irq_disable irq_dis;
    pit8254_scoped_lock lock(pit8254_lock);
    outb(PIT_CMD,
         PIT_CHANNEL(0) |
         PIT_ACCESS_BOTH |
         PIT_MODE_ONESHOT |
         PIT_FORMAT_BINARY);
    outb(PIT_DATA(0), 0);
    outb(PIT_DATA(0), 0);
    lock.unlock();

    irq_unhook(PIT_IRQ, pit8254_irq_handler);
}

// Multiply two unsigned 64 bit values, giving an intermediate 128 bit result,
// then divide that by an unsigned 64 bit value, and return the quotient
_const
static _always_inline uint64_t mul_64_64_div_64(
        uint64_t m1, uint64_t m2, uint64_t d)
{
#if __amd64__
    __asm__ (
        "mul %[m2]\n\t"
        "div %[d]\n\t"
        : [m1] "+a" (m1)
        , [m2] "+d" (m2)
        : [d] "r" (d)
        : "cc"
    );
    return m1;
#else
#error Need 32x32->64/32 implementation
#endif
}

// Returns number of elapsed nanoseconds
static uint64_t pit8254_nsleep(uint64_t nanosec)
{
    uint64_t count = mul_64_64_div_64(nanosec, PIT_FREQ_NUMER, 3000000000);

    // Add 64 so we can see how far we have skid past goal at last poll
    count += 64;

    if (count > 0xFFFF)
        count = 0xFFFF;

    cpu_scoped_irq_disable irq_dis;
    pit8254_scoped_lock lock(pit8254_lock);

    outb(PIT_CMD, PIT_CHANNEL(2) | PIT_ACCESS_BOTH |
         PIT_MODE_ONESHOT | PIT_FORMAT_BINARY);
    outb(PIT_DATA(2), count & 0xFF);
    outb(PIT_DATA(2), (count >> 8) & 0xFF);

    uint32_t readback;
    do {
        outb(PIT_CMD, PIT_READBACK_CH2);
        outb(PIT_CMD, PIT_CHANNEL(2) | PIT_ACCESS_LATCH);
        readback = inb(PIT_DATA(2));
        readback |= inb(PIT_DATA(2)) << 8;
        pause();
    } while (readback > 64);

    lock.unlock();

    return mul_64_64_div_64(count - readback, 3000000000, PIT_FREQ_NUMER);
}

void pit8254_init()
{
    nsleep_set_handler(pit8254_nsleep, nullptr, false);
}

void pit8254_enable()
{
    if (!time_ns_set_handler(pit8254_time_ns, pit8254_time_ns_stop, false))
        return;

    PIT_TRACE("Starting PIT timer\n");

    pit8254_set_rate(20);
    irq_hook(PIT_IRQ, pit8254_irq_handler, "pit8254");
    irq_setmask(PIT_IRQ, true);

//    uint64_t now, end = time_ns() + 500000000;

//    while ((now = time_ns()) < end) {
//        printdbg("time: %" PRIx64 "\n", now);
//    }
}
