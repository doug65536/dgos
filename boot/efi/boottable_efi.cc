#include "boottable.h"
#include "boottable_decl.h"
#include "bootefi.h"
#include "string.h"
#include "bswap.h"
#include "screen.h"

static EFI_GUID const efi_acpi_20_guid = ACPI_20_TABLE_GUID;
static EFI_GUID const efi_acpi_guid = ACPI_TABLE_GUID;
static EFI_GUID const efi_mps_guid = MPS_TABLE_GUID;

static void format_guid(tchar *buf37, EFI_GUID const& guid)
{
    static_assert(sizeof(EFI_GUID) == 16, "Unexpected GUID size");
    char bytes[16];

    // Strange mixed endianness microsoft variant 2 uuid
    static uint8_t const byte_lookup[] = {
        0x03, 0x02, 0x01, 0x00, 0x05, 0x04, 0x07, 0x06,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };

    memcpy(bytes, &guid, sizeof(bytes));

    uint32_t dash_mask  = 0b0001010101000000;
    uint32_t check_mask = 0b1000000000000000;

    size_t o = 0;

    for (size_t i = 0; i < 16; (check_mask >>= 1), ++i) {
        uint_fast8_t n = bytes[byte_lookup[i]];

        // Write hex digits
        buf37[o++] = hexlookup[(n >> 4) & 0xF];
        buf37[o++] = hexlookup[n & 0xF];

        // Make -1 if we need dash, otherwise zero
        int32_t mask = -(int32_t)(check_mask & dash_mask) >> 31;

        // Write a dash or a zero
        buf37[o] = (mask & '-');

        // Advance o if we wrote a dash
        o -= mask;
    }
}

struct guid_ent_t {
    EFI_GUID const *guid;
    tchar const *name;
};

guid_ent_t guid_names[] = {
    { &efi_acpi_guid, TSTR "ACPI 1.0 RSDP" },
    { &efi_acpi_20_guid, TSTR "ACPI 2.0 RSDP" },
    { &efi_mps_guid, TSTR "MPS table" }
};

void boottbl_dump_guids()
{
    EFI_CONFIGURATION_TABLE const *tbl = efi_systab->ConfigurationTable;

    tchar guid_text[37];

    for (UINTN i = 0; i < efi_systab->NumberOfTableEntries; ++i) {
        format_guid(guid_text, tbl[i].VendorGuid);

        size_t k;
        for (k = countof(guid_names); k > 0; --k) {
            if (!memcmp(guid_names[k-1].guid, &tbl[i].VendorGuid,
                        sizeof(tbl->VendorGuid)))
                break;
        }

        PRINT("GUID %" TFMT " %" TFMT "\n", guid_text,
              k ? guid_names[k-1].name : TSTR "");
    }

}

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

    boottbl_dump_guids();

    EFI_CONFIGURATION_TABLE const *tbl = efi_systab->ConfigurationTable;

    for (UINTN i = 0; i < efi_systab->NumberOfTableEntries; ++i) {
        if (!memcmp(&tbl[i].VendorGuid, &efi_mps_guid,
                    sizeof(tbl[i].VendorGuid))) {
            result.mp_addr = uint64_t(tbl[i].VendorTable);
            break;
        }
    }

    return result;
}
