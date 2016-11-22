#include "legacy_pit.h"
#include "ioport.h"
#include "irq.h"
#include "atomic.h"
#include "time.h"
#include "cpu/thread_impl.h"

#include "conio.h"
#include "printk.h"

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

// Total timer interrupts, no time relationship
uint64_t volatile timer_ticks;

// Exceeds 2^63 in about 292000 years
uint64_t volatile timer_ms;

static unsigned divisor;
static unsigned rate_hz;
static unsigned accumulator;

static void pit8254_set_rate(unsigned hz)
{
    // Reasonable upper limit, prevent 32 bit overflow
    // This puts the minimum timeslice at 16 microseconds
    // which is ridiculously short
    // The divisor will never be below 19
    if (hz > 62500U)
        hz = 62500U;

    // Clamp to valid range
    if (hz <= 18U)
        divisor = 0xFFFFU;
    else
        divisor = 1193181666U / (hz * 1000U);

    rate_hz = hz;
    accumulator = 0U;

    outb(PIT_CMD,
         PIT_CHANNEL(0) |
         PIT_ACCESS_BOTH |
         PIT_MODE_RATEGEN |
         PIT_FORMAT_BINARY);
    outb(PIT_DATA(0), divisor & 0xFF);
    outb(PIT_DATA(0), (divisor >> 8) & 0xFF);
}

static void *pit8254_handler(int irq, void *ctx)
{
    (void)irq;

    atomic_inc_uint64(&timer_ticks);

    // Accumulate crystal clock cycles
    accumulator += divisor * 1000;

    // Accumulated milliseconds

    if (accumulator >= 1193181U) {
        unsigned accum_ms = accumulator / 1193181U;
        accumulator -= 1193181U * accum_ms;
        atomic_add_uint64(&timer_ms, accum_ms);
    }

    // Test
    static uint64_t last_time;
    if (last_time + 1000 <= timer_ms) {
        last_time = timer_ms;

        char buf[10];
        snprintf(buf, sizeof(buf), "%8ld ", last_time);

        con_draw_xy(70, 0, buf, 7);

    }

    return thread_schedule(ctx);
}

static uint64_t pit8254_time_ms(void)
{
    return timer_ms;
}

void pit8254_enable(void)
{
    time_ms = pit8254_time_ms;

    pit8254_set_rate(60);
    irq_hook(0, pit8254_handler);
    irq_setmask(0, 1);
}
