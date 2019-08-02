#pragma once
#include "types.h"

enum struct bootparam_t {
    ap_entry_point,
    vbe_mode_info,
    boot_drv_serial,
    boot_debugger,
    boot_serial_log,
    boot_acpi_rsdp,
    boot_mptables,
    initrd_base,
    initrd_size
};

extern "C" uintptr_t bootinfo_parameter(bootparam_t param);
