#include "bootinfo.h"
#include "bios_data.h"
#include "main.h"
#include "callout.h"
#include "export.h"
#include "mm.h"
#include "printk.h"

#ifdef _ASAN_ENABLED
_constructor(ctor_asan_init) static void bootinfo_init()
{
    __asan_storeN_noabort(uintptr_t(kernel_params), sizeof(*kernel_params));
    __asan_storeN_noabort(uintptr_t(kernel_params->vbe_selected_mode),
                          sizeof(*kernel_params->vbe_selected_mode));
}
#endif

static void bootinfo_remap(void*)
{
    kernel_params = (kernel_params_t*)mmap(
                kernel_params, sizeof(*kernel_params),
                PROT_READ | PROT_WRITE, MAP_PHYSICAL);
    if (unlikely(kernel_params == MAP_FAILED))
        panic_oom();

    vbe_selected_mode_t *mode = kernel_params->vbe_selected_mode;

    kernel_params->vbe_selected_mode = (vbe_selected_mode_t*)
            mmap((void*)mode, sizeof(*mode), PROT_READ, MAP_PHYSICAL);
}
REGISTER_CALLOUT(bootinfo_remap, nullptr, callout_type_t::vmm_ready, "000");

EXPORT uintptr_t bootinfo_parameter(bootparam_t param)
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

    case bootparam_t::initrd_base:
        return data->initrd_st;

    case bootparam_t::initrd_size:
        return data->initrd_sz;

    case bootparam_t::smp_enable:
        return data->smp_enable;

    case bootparam_t::acpi_enable:
        return data->acpi_enable;

    case bootparam_t::mps_enable:
        return data->mps_enable;

    case bootparam_t::msi_enable:
        return data->msi_enable;

    case bootparam_t::msix_enable:
        return data->msix_enable;

    case bootparam_t::port_e9_debug:
        return data->e9_enable;

    }

    return 0;
}

EXPORT void bootinfo_drop_initrd()
{
    auto data = kernel_params;

    if (uintptr_t(data) < 0x1000)
        data = (kernel_params_t*)(zero_page + (uintptr_t)data);

    data->initrd_st = 0;
    data->initrd_sz = 0;
}
