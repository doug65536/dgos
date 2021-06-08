#include "cmos.h"
#include "bios_data.h"
#include "ioport.h"
#include "bios_data.h"
#include "string.h"
#include "isr.h"
#include "irq.h"
#include "mutex.h"
#include "bootinfo.h"
#include "printk.h"
#include "thread_impl.h"
#include "apic.h"
#include "cpu/interrupts.h"
#include "device/eainstrument.h"

#define CMOS_TEST 0

#define CMOS_ADDR_PORT  0x70
#define CMOS_DATA_PORT  0x71

#define CMOS_REG_RTC_SECOND         0x00
#define CMOS_REG_RTC_SECOND_ALARM   0x01
#define CMOS_REG_RTC_MINUTE         0x02
#define CMOS_REG_RTC_MINUTE_ALARM   0x03
#define CMOS_REG_RTC_HOUR           0x04
#define CMOS_REG_RTC_HOUR_ALARM     0x05
#define CMOS_REG_RTC_WEEKDAY        0x06
#define CMOS_REG_RTC_DAY            0x07
#define CMOS_REG_RTC_MONTH          0x08
#define CMOS_REG_RTC_YEAR           0x09

#define CMOS_REG_STATUS_A           0x0A
#define CMOS_REG_STATUS_B           0x0B
#define CMOS_REG_STATUS_C           0x0C
#define CMOS_REG_STATUS_D           0x0D

#define CMOS_REG_SHUTDOWN_STATUS    0x0F

//
// Status register A

#define CMOS_STATUS_A_RATE_BIT      0
#define CMOS_STATUS_A_RATE_BITS     4
#define CMOS_STATUS_A_CRYSTAL_BIT   4
#define CMOS_STATUS_A_CRYSTAL_BITS  3
#define CMOS_STATUS_A_UPDATING_BIT  7

#define CMOS_STATUS_A_RATE_MASK     ((1<<CMOS_STATUS_A_RATE_BITS)-1)
#define CMOS_STATUS_A_RATE \
    (CMOS_STATUS_A_RATE_MASK<<CMOS_STATUS_A_RATE_BIT)
#define CMOS_STATUS_A_RATE_n(n)     ((n)<<CMOS_STATUS_A_RATE_BIT)
#define CMOS_STATUS_A_RATE_8192     3
#define CMOS_STATUS_A_RATE_4096     4
#define CMOS_STATUS_A_RATE_2048     5
#define CMOS_STATUS_A_RATE_1024     6
#define CMOS_STATUS_A_RATE_512      7
#define CMOS_STATUS_A_RATE_256      8
#define CMOS_STATUS_A_RATE_128      9
#define CMOS_STATUS_A_RATE_64       10
#define CMOS_STATUS_A_RATE_32       11
#define CMOS_STATUS_A_RATE_16       12
#define CMOS_STATUS_A_RATE_8        13
#define CMOS_STATUS_A_RATE_4        14
#define CMOS_STATUS_A_RATE_2        15

#define CMOS_STATUS_A_CRYSTAL_MASK  ((1<<CMOS_STATUS_A_CRYSTAL_BITS)-1)
#define CMOS_STATUS_A_CRYSTAL \
    (CMOS_STATUS_A_CRYSTAL_MASK<<CMOS_STATUS_A_CRYSTAL_BIT)
#define CMOS_STATUS_A_CRYSTAL_n(n)  ((n)<<CMOS_STATUS_A_CRYSTAL_BIT)
#define CMOS_STATUS_A_CRYSTAL_32768 2

#define CMOS_STATUS_A_UPDATING      (1U<<CMOS_STATUS_A_UPDATING_BIT)

//
// Status register B

#define CMOS_STATUS_B_DST_BIT       0
#define CMOS_STATUS_B_24HR_BIT      1
#define CMOS_STATUS_B_BIN_BIT       2
#define CMOS_STATUS_B_SQWF_BIT      3
#define CMOS_STATUS_B_UEI_BIT       4
#define CMOS_STATUS_B_AI_BIT        5
#define CMOS_STATUS_B_PI_BIT        6
#define CMOS_STATUS_B_CLKD_BIT      7

// 1=Enable daylight savings
#define CMOS_STATUS_B_DST           (1U<<CMOS_STATUS_B_DST_BIT)

// 1=Enable squarewave frequency
#define CMOS_STATUS_B_SQWF          (1U<<CMOS_STATUS_B_SQWF_BIT)

// 1=Enable Update Ended Interrupt
#define CMOS_STATUS_B_UEI           (1U<<CMOS_STATUS_B_UEI_BIT)

// 1=Enable Alarm Interrupt
#define CMOS_STATUS_B_AI            (1U<<CMOS_STATUS_B_AI_BIT)

// 1=Enable Periodic Interrupt
#define CMOS_STATUS_B_PI            (1U<<CMOS_STATUS_B_PI_BIT)

// 1=Disable Clock update
#define CMOS_STATUS_B_CLKD          (1U<<CMOS_STATUS_B_CLKD_BIT)

// 1=24 hour mode, 0=12 hour mode
#define CMOS_STATUS_B_24HR          (1<<CMOS_STATUS_B_24HR_BIT)

// 1=Binary format, 0=BCD format
#define CMOS_STATUS_B_BIN           (1<<CMOS_STATUS_B_BIN_BIT)

//
// Status register C

#define CMOS_STATUS_C_UEI_BIT       CMOS_STATUS_B_UEI_BIT
#define CMOS_STATUS_C_AI_BIT        CMOS_STATUS_B_AI_BIT
#define CMOS_STATUS_C_PI_BIT        CMOS_STATUS_B_PI_BIT
#define CMOS_STATUS_C_IRQF_BIT      7

// Update ended interrupt occurred
#define CMOS_STATUS_C_UEI           (1U<<CMOS_STATUS_C_UEI_BIT)
// Alarm interrupt occurred
#define CMOS_STATUS_C_AI            (1U<<CMOS_STATUS_C_AI_BIT)
// Periodic interrupt occurred
#define CMOS_STATUS_C_PI            (1U<<CMOS_STATUS_C_PI_BIT)
// ?
#define CMOS_STATUS_C_IRQF          (1U<<CMOS_STATUS_C_IRQF_BIT)

//
// Status register D

#define CMOS_STATUS_D_BATT_BIT      7

// 1=Battery working
#define CMOS_STATUS_D_BATT          (1U<<CMOS_STATUS_D_BATT_BIT)

//
// Shutdown status byte

// Unexpected shutdown
#define CMOS_SHUTDOWN_STATUS_US     0x0

// Shutdown when determining memory size
#define CMOS_SHUTDOWN_STATUS_MS     0x1

// Shutdown when running memory test
#define CMOS_SHUTDOWN_STATUS_MT     0x2

// Shutdown with memory error
#define CMOS_SHUTDOWN_STATUS_ME     0x3

// Run bootloader
#define CMOS_SHUTDOWN_STATUS_NORMAL 0x4

// JMP DWORD with INT init
#define CMOS_SHUTDOWN_STATUS_AP     0xB

using cmos_lock_type = ext::spinlock;
using cmos_scoped_lock = ext::unique_lock<cmos_lock_type>;
static cmos_lock_type cmos_lock;
static time_of_day_t time_of_day;
static uint64_t time_of_day_timestamp;

static uint8_t cmos_status_b;

static uint8_t cmos_read(uint8_t reg, cmos_scoped_lock const&)
{
    outb(CMOS_ADDR_PORT, reg);
    return inb(CMOS_DATA_PORT);
}

static void cmos_write(uint8_t reg, uint8_t val, cmos_scoped_lock const&)
{
    outb(CMOS_ADDR_PORT, reg);
    outb(CMOS_DATA_PORT, val);
}

void cmos_prepare_ap(void)
{
//obsolete
//    cmos_scoped_lock lock(cmos_lock);

//    // Read vector immediately after boot sector structure
//    uint32_t ap_entry_point = //*(uint32_t*)0x7C40;
//            (uint32_t)
//            bootinfo_parameter(bootparam_t::ap_entry_point);
//    *BIOS_DATA_AREA(uint32_t, 0x467) = ap_entry_point;

//    cmos_write(CMOS_REG_SHUTDOWN_STATUS, CMOS_SHUTDOWN_STATUS_AP, lock);
//    //cmos_write(CMOS_REG_SHUTDOWN_STATUS, CMOS_SHUTDOWN_STATUS_US, lock);
}

static uint8_t cmos_bcd_to_binary(uint8_t n)
{
    return ((n >> 4) * 10) + (n & 0x0F);
}

static time_of_day_t cmos_fixup_timeofday(time_of_day_t t)
{
    uint8_t pm_bit = t.hour & 0x80;

    if ((cmos_status_b & CMOS_STATUS_B_BIN) == 0) {
        // BCD
        t.second = cmos_bcd_to_binary(t.second);
        t.minute = cmos_bcd_to_binary(t.minute);
        t.hour = cmos_bcd_to_binary(t.hour & 0x7F);
        t.day = cmos_bcd_to_binary(t.day);
        t.month = cmos_bcd_to_binary(t.month);
        t.year = cmos_bcd_to_binary(t.year);
    }

    // Pivot year, values < 17 are in 22nd century
    t.century = 20 + (t.year < 17);

    if (!(cmos_status_b & CMOS_STATUS_B_24HR)) {
        if (t.hour >= 12)
            t.hour -= 12;
        if (pm_bit)
            t.hour += 12;
    }

    return t;
}

static time_of_day_t cmos_read_gettimeofday(
        cmos_scoped_lock const& lock)
{
    time_of_day_t result;

    result.centisec = 0;
    result.second = cmos_read(CMOS_REG_RTC_SECOND, lock);
    result.minute = cmos_read(CMOS_REG_RTC_MINUTE, lock);
    result.hour = cmos_read(CMOS_REG_RTC_HOUR, lock);
    result.day = cmos_read(CMOS_REG_RTC_DAY, lock);
    result.month = cmos_read(CMOS_REG_RTC_MONTH, lock);
    result.year = cmos_read(CMOS_REG_RTC_YEAR, lock);

    return cmos_fixup_timeofday(result);
}

static isr_context_t *cmos_irq_handler(int, isr_context_t *ctx)
{
    //printdbg("CMOS IRQ\n");

    cmos_scoped_lock lock(cmos_lock);

    // To clear the irq pin, the processor program normally reads Register C
    // - MC146818 datasheet
    uint8_t intr_cause = cmos_read(CMOS_REG_STATUS_C, lock);

    if (intr_cause & CMOS_STATUS_C_UEI) {
        // Update ended interrupt
        time_of_day = cmos_read_gettimeofday(lock);
        time_of_day_timestamp = time_ns();

        // Flush trace
        if (unlikely(eainst_flush_ready))
            apic_send_ipi_noinst(-2, INTR_IPI_FL_TRACE);
    }

    return ctx;
}

time_of_day_t cmos_gettimeofday()
{
    cmos_scoped_lock lock(cmos_lock);

    time_of_day_t result = time_of_day;
    uint64_t now = time_ns();
    uint64_t since_known_tod = now - time_of_day_timestamp;
    result.centisec = since_known_tod / 10000000;

    if (unlikely(result.centisec >= 100))
        result.centisec = 99;

    return result;
}

#if CMOS_TEST
static int cmos_test_thread(void *)
{
    uint64_t st_ns = time_ns();
    uint64_t st_ut = time_unix_ms(time_ofday());

    for (;;) {
        uint64_t ns = time_ns();
        uint64_t ut = time_unix_ms(time_ofday());

        int64_t d_ns = ns - st_ns;
        int64_t d_ut = ut - st_ut;

        st_ns = ns;
        st_ut = ut;

        printdbg("ns=%+" PRId64
                 ", ut=%+" PRId64
                 "\n", d_ns / 1000000, d_ut);

        thread_sleep_for(1000);
    }
}
#endif

void cmos_init(void)
{
    cmos_scoped_lock lock(cmos_lock);

    irq_hook(8, cmos_irq_handler, "cmos_rtc");

    // Set IRQ rate to 2Hz, just in case
    uint8_t cmos_status_a = cmos_read(CMOS_REG_STATUS_A, lock);
    cmos_status_a = (cmos_status_a & ~CMOS_STATUS_A_RATE) |
            CMOS_STATUS_A_RATE_2;
    cmos_write(CMOS_REG_STATUS_A, cmos_status_a, lock);

    cmos_status_b = cmos_read(CMOS_REG_STATUS_B, lock);
    time_ofday_set_handler(cmos_gettimeofday);

    // Keep rereading the time until we get the same values twice
    time_of_day_t tod1{};
    time_of_day_t tod2{};
    tod1 = cmos_read_gettimeofday(lock);
    for (;;) {
        tod2 = cmos_read_gettimeofday(lock);

        if (!memcmp(&tod1, &tod2, sizeof(tod1)))
            break;

        tod1 = tod2;
    }
    time_of_day = tod1;
    time_of_day_timestamp = time_ns();

    //
    // Program CMOS interrupt

    // Preserve some bits
    cmos_status_b &= (CMOS_STATUS_B_DST |
                 CMOS_STATUS_B_SQWF |
                 CMOS_STATUS_B_24HR |
                 CMOS_STATUS_B_BIN);

    // Enable update ended IRQ, don't disable clock update
    cmos_status_b |= CMOS_STATUS_B_UEI;

    // Disable periodic IRQ
    cmos_status_b &= ~CMOS_STATUS_C_PI;

    // Enable update ended IRQ
    cmos_write(CMOS_REG_STATUS_B, cmos_status_b, lock);

    // EOI just in case
    cmos_read(CMOS_REG_STATUS_C, lock);

#if CMOS_TEST
    thread_create(nullptr, cmos_test_thread, nullptr,
                  "cmos-test", 0, false, false);
#endif

    irq_setcpu(8, -1);
    irq_setmask(8, true);
}
