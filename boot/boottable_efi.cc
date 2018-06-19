#include "boottable.h"
#include "boottable_decl.h"
#include "bootefi.h"
#include "string.h"

static EFI_GUID const efi_acpi_20_guid = ACPI_20_TABLE_GUID;
static EFI_GUID const efi_acpi_guid = ACPI_TABLE_GUID;

boottbl_acpi_info_t boottbl_find_acpi_rsdp()
{
    boottbl_acpi_info_t result{};

    EFI_CONFIGURATION_TABLE const *tbl = efi_systab->ConfigurationTable;

    result.rsdt_addr = 0;

    for (UINTN i = 0; i < efi_systab->NumberOfTableEntries; ++i) {
        if (!memcmp(&tbl[i].VendorGuid, &efi_acpi_20_guid,
                    sizeof(tbl[i].VendorGuid))) {
            acpi_rsdp2_t *rsdp2 = (acpi_rsdp2_t*)tbl[i].VendorTable;
            result.rsdt_addr = rsdp2->xsdt_addr_lo |
                    (uint64_t(rsdp2->xsdt_addr_hi) << 32);
            result.rsdt_size = rsdp2->length;
            result.ptrsz = sizeof(uint64_t);
            break;
        }
    }

    for (UINTN i = 0; i < efi_systab->NumberOfTableEntries &&
         !result.rsdt_addr; ++i) {
        if (!memcmp(&tbl[i].VendorGuid, &efi_acpi_guid,
                    sizeof(tbl[i].VendorGuid))) {
            acpi_rsdp_t *rsdp = (acpi_rsdp_t*)tbl[i].VendorTable;
            result.rsdt_addr = uint64_t(rsdp->rsdt_addr);
            result.rsdt_size = 0;
            result.ptrsz = sizeof(uint32_t);
            break;
        }
    }

    return result;
}

boottbl_mptables_info_t boottbl_find_mptables()
{
    boottbl_mptables_info_t result{};
    return result;
}
