#include "bootinfo.h"
#include "bios_data.h"
#include "main.h"

uintptr_t bootinfo_parameter(bootparam_t param)
{
    auto data = (kernel_params_t const *)&zero_page[uintptr_t(kernel_params)];

    if ((uintptr_t)data < 0x1000)
        data = (kernel_params_t*)(zero_page + (uintptr_t)data);

    switch (param) {
    case bootparam_t::ap_entry_point:
        return data->mp_entry;

    case bootparam_t::boot_device:
        return data->boot_device_info;

    case bootparam_t::vbe_mode_info:
        return data->vbe_selected_mode;

    case bootparam_t::boot_serial:
        return data->boot_serial;

    default:
        return 0;
    }
}
