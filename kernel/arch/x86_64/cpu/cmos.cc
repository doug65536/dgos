#include "cmos.h"
#include "bios_data.h"
#include "ioport.h"
#include "bios_data.h"
#include "string.h"

#define CMOS_ADDR_PORT  0x70
#define CMOS_DATA_PORT  0x71

#define CMOS_REG_RTC_SECOND     0x00
#define CMOS_REG_RTC_MINUTE     0x02
#define CMOS_REG_RTC_HOUR       0x04
#define CMOS_REG_RTC_DAY        0x07
#define CMOS_REG_RTC_MONTH      0x08
#define CMOS_REG_RTC_YEAR       0x09

#define CMOS_REG_STATUS_A       0x0A
#define CMOS_REG_STATUS_B       0x0B

#define CMOS_REG_SHUTDOWN_STATUS    0x0F

#define CMOS_STATUS_B_24HR_BIT  1
#define CMOS_STATUS_B_BIN_BIT   2

#define CMOS_STATUS_B_24HR      (1<<CMOS_STATUS_B_24HR_BIT)
#define CMOS_STATUS_B_BIN       (1<<CMOS_STATUS_B_BIN_BIT)

#define CMOS_SHUTDOWN_STATUS_AP     0x5
#define CMOS_SHUTDOWN_STATUS_NORMAL 0x4

static uint8_t cmos_status_b;

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDR_PORT, reg);
    return inb(CMOS_DATA_PORT);
}

void cmos_prepare_ap(void)
{
    // Read vector immediately after boot sector structure
    uint32_t ap_entry_point = *(uint32_t*)0x7C40;
    *BIOS_DATA_AREA(uint32_t, 0x467) = ap_entry_point;

    outb(CMOS_ADDR_PORT, CMOS_REG_SHUTDOWN_STATUS);
    outb(CMOS_DATA_PORT, CMOS_SHUTDOWN_STATUS_AP);
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
        t.centisec = cmos_bcd_to_binary(t.centisec);
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

static time_of_day_t cmos_read_gettimeofday(void)
{
    time_of_day_t result;

    result.centisec = 0;
    result.second = cmos_read(CMOS_REG_RTC_SECOND);
    result.minute = cmos_read(CMOS_REG_RTC_MINUTE);
    result.hour = cmos_read(CMOS_REG_RTC_HOUR);
    result.day = cmos_read(CMOS_REG_RTC_DAY);
    result.month = cmos_read(CMOS_REG_RTC_MONTH);
    result.year = cmos_read(CMOS_REG_RTC_YEAR);

    return cmos_fixup_timeofday(result);
}

time_of_day_t cmos_gettimeofday(void)
{
    time_of_day_t last;
    time_of_day_t curr;

    // Keep reading time until we get
    // the same values twice
    memset(&last, 0xFF, sizeof(last));
    do {
        last = cmos_read_gettimeofday();
        curr = cmos_read_gettimeofday();
    } while (memcmp(&last, &curr, sizeof(last)));

    return curr;
}

void cmos_init(void)
{
    cmos_status_b = cmos_read(CMOS_REG_STATUS_B);
    time_ofday_set_handler(cmos_gettimeofday);
}
