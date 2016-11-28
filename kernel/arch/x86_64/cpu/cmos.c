#include "cmos.h"
#include "bios_data.h"
#include "ioport.h"
#include "bios_data.h"
#include "types.h"

#define CMOS_ADDR_PORT  0x70
#define CMOS_DATA_PORT  0x71

#define CMOS_REG_SHUTDOWN_STATUS    0x0F

#define CMOS_SHUTDOWN_STATUS_AP     0x5
#define CMOS_SHUTDOWN_STATUS_NORMAL 0x4

void cmos_init(void)
{
}

void cmos_prepare_ap(void)
{
    // Read vector immediately after boot sector structure
    uint32_t ap_entry_point = *(uint32_t*)0x7C40;
    *BIOS_DATA_AREA(uint32_t, 0x467) = ap_entry_point;

    outb(CMOS_ADDR_PORT, CMOS_REG_SHUTDOWN_STATUS);
    outb(CMOS_DATA_PORT, CMOS_SHUTDOWN_STATUS_AP);


}
