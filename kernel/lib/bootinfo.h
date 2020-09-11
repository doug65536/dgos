#pragma once
#include "types.h"
#include "bootloader.h"

__BEGIN_DECLS

enum struct bootparam_t {
    ap_entry_point,
    vbe_mode_info,
    boot_drv_serial,
    boot_debugger,
    boot_serial_log,
    boot_acpi_rsdp,
    boot_mptables,
    initrd_base,
    initrd_size,
    smp_enable,
    acpi_enable,
    mps_enable,
    msi_enable,
    msix_enable,
    port_e9_debug,
    gdb_port
};

uintptr_t bootinfo_parameter(bootparam_t param);
void bootinfo_drop_initrd();

__END_DECLS
