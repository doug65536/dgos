#include "bootdev.h"
#include "printk.h"
#include "bootinfo.h"
#include "bootdev_info.h"

void bootdev_info(uint8_t *bus, uint8_t *slot, uint8_t *func)
{
    (void)bus;
    (void)slot;
    (void)func;

    bootdev_drive_params_t *p = (bootdev_drive_params_t*)
            bootinfo_parameter(bootparam_t::boot_device);

    printdbg("Host bus %4.4s %8.8s\n", p->host_bus, p->interface_type);
}
