#include "boottable.h"
#include "../kernel/lib/acpi_decl.h"

boottbl_nodes_info_t boottbl_find_numa(boottbl_acpi_info_t const& acpi_info)
{
    boottbl_nodes_info_t result{};

    bool is64 = (acpi_info.ptrsz == 8);

    if (acpi_info.rsdt_addr + acpi_info.rsdt_size <= SIZE_MAX) {
        acpi_sdt_hdr_t const* sdt = (acpi_sdt_hdr_t const *)acpi_info.rsdt_addr;

    }

    return result;
}
