#include "bootinfo.h"
#include "bios_data.h"
#include "main.h"

uintptr_t bootinfo_parameter(bootparam_t param)
{
    auto data = kernel_params;

    if (uintptr_t(data) < 0x1000)
        data = (kernel_params_t*)(zero_page + (uintptr_t)data);

    switch (param) {
    case bootparam_t::ap_entry_point:
        return data->ap_entry;

    case bootparam_t::vbe_mode_info:
        return data->vbe_selected_mode;

    case bootparam_t::boot_drv_serial:
        return data->boot_drv_serial;

    case bootparam_t::boot_serial_log:
        return data->serial_debugout;

    case bootparam_t::boot_debugger:
        return data->wait_gdb;

    case bootparam_t::boot_acpi_rsdp:
        return uintptr_t(&data->acpi_rsdt);

    case bootparam_t::boot_mptables:
        return uintptr_t(&data->mptables);

    default:
        return 0;
    }
}
