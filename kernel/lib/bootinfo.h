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
    testrun_port,
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
    gdb_port,
    phys_mem_table,
    phys_mem_table_size,
    phys_mapping,
    phys_mapping_sz
};

KERNEL_API uintptr_t bootinfo_parameter(bootparam_t param);
KERNEL_API void bootinfo_drop_initrd();

__END_DECLS
