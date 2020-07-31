#include "physmem.h"
#include <stddef.h>
#include "bootefi.h"
#include "malloc.h"
#include "screen.h"
#include "physmap.h"
#include "halt.h"
#include "inttypes.h"
#include "assert.h"

static tchar const *efi_mem_types[] = {
    TSTR "EfiReservedMemoryType",
    TSTR "EfiLoaderCode",
    TSTR "EfiLoaderData",
    TSTR "EfiBootServicesCode",
    TSTR "EfiBootServicesData",
    TSTR "EfiRuntimeServicesCode",
    TSTR "EfiRuntimeServicesData",
    TSTR "EfiConventionalMemory",
    TSTR "EfiUnusableMemory",
    TSTR "EfiACPIReclaimMemory",
    TSTR "EfiACPIMemoryNVS",
    TSTR "EfiMemoryMappedIO",
    TSTR "EfiMemoryMappedIOPortSpace",
    TSTR "EfiPalCode",
    TSTR "EfiPersistentMemory",
    TSTR "EfiMaxMemoryType"
};

bool get_ram_regions()
{
    UINTN mapsz = 0;
    UINTN mapkey = 0;
    UINTN descsz = 0;
    UINT32 descver = 0;
    EFI_STATUS status;

    status = efi_systab->BootServices->GetMemoryMap(
                &mapsz, nullptr, &mapkey, &descsz, nullptr);

    EFI_MEMORY_DESCRIPTOR *memdesc_buf =
            (EFI_MEMORY_DESCRIPTOR*)malloc(mapsz);
    EFI_MEMORY_DESCRIPTOR const *in = memdesc_buf;

    status = efi_systab->BootServices->GetMemoryMap(
                &mapsz, memdesc_buf, &mapkey, &descsz, &descver);

    if (unlikely(EFI_ERROR(status)))
        return false;

    size_t count = mapsz / descsz;

    for (size_t i = 0; i < count;
         ++i, in = (EFI_MEMORY_DESCRIPTOR*)((char const*)in + descsz)) {
        PRINT("paddr=%" PRIx64 ", len=%" PRIx64
              ", attr=%" PRIx64 ", type=%x (%s)",
              in->PhysicalStart, in->NumberOfPages << 12,
              in->Attribute, in->Type,
              in->Type < sizeof(efi_mem_types) / sizeof(*efi_mem_types) ?
                  efi_mem_types[in->Type] : TSTR "<app defined!>");

        physmem_range_t entry{};

        entry.base = in->PhysicalStart;
        entry.size = in->NumberOfPages << 12;
        entry.valid = 1;

        switch (in->Type) {
        case EfiConventionalMemory:
            entry.type = PHYSMEM_TYPE_NORMAL;
            break;

        case EfiReservedMemoryType:
            entry.type = PHYSMEM_TYPE_UNUSABLE;
            break;

        case EfiLoaderCode: // fall thru
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiRuntimeServicesCode:
        case EfiRuntimeServicesData:
        case EfiACPIReclaimMemory:
            entry.type = PHYSMEM_TYPE_RECLAIMABLE;
            break;

        case 0x80000000:
            entry.type = PHYSMEM_TYPE_BOOTLOADER;
            break;

        /// Memory in which errors have been detected.
        ///
        case EfiUnusableMemory:
            entry.type = PHYSMEM_TYPE_BAD;
            break;

        ///
        /// Address space reserved for use by the firmware.
        ///
        case EfiACPIMemoryNVS:
        case EfiPersistentMemory:
            entry.type = PHYSMEM_TYPE_NVS;
            break;

        case EfiMemoryMappedIO:
        case EfiMemoryMappedIOPortSpace:
        case EfiPalCode:
            entry.type = PHYSMEM_TYPE_UNUSABLE;
            break;

        default:
            entry.type = PHYSMEM_TYPE_UNUSABLE;
            break;

        }

        physmap_insert(entry);
    }

    free(memdesc_buf);

    return true;
}

// Returns -1 on error
void take_pages(uint64_t phys_addr, uint64_t size)
{
    EFI_STATUS status;

    EFI_PHYSICAL_ADDRESS addr = phys_addr;

    assert(size > 0);

    //PRINT("Taking %" PRIx64 " at %" PRIx64, size, phys_addr);

    status = efi_systab->BootServices->AllocatePages(
                AllocateAddress, EfiLoaderData,
                size >> 12, &addr);

    if (unlikely(EFI_ERROR(status)))
        PANIC("Could not take pages, need more memory");
}
