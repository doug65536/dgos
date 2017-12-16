#include "bootinfo.h"
#include "bios_data.h"

struct bootinfo_data_t {
    uint32_t ap_entry_addr;
    uint32_t vbe_info_addr;
    uint32_t bootdev_info_addr;
};

uintptr_t bootinfo_parameter(bootparam_t param)
{
    bootinfo_data_t const *data =
            (bootinfo_data_t*)(uintptr_t)
            *(uint16_t*)&zero_page[0x9A0];

    if ((uintptr_t)data < 0x1000)
        data = (bootinfo_data_t*)(zero_page + (uintptr_t)data);

    switch (param) {
    case bootparam_t::ap_entry_point:
        return data->ap_entry_addr;

    case bootparam_t::boot_device:
        return data->bootdev_info_addr;

    case bootparam_t::vbe_mode_info:
        return data->vbe_info_addr;

    default:
        return 0;
    }
}
