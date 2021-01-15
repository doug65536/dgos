#include "x86/cpu_x86.h"
#include "bootefi.h"
#include "paging.h"
#include "malloc.h"
#include "assert.h"
#include "halt.h"
#include "elf64decl.h"
#include "string.h"

extern "C" _noreturn void arch_run_kernel(
        uint64_t entry, void *params, uint64_t cr3, bool nx_available);

extern char arch_run_kernel_end[];

// This only runs on the BSP
void run_kernel(uint64_t *entry, void *param)
{
    assert(param != nullptr);

    UINTN mapsz = 0;
    UINTN mapkey = 0;
    UINTN descsz = 0;

    EFI_STATUS status;

    paging_map_physical(uint64_t(arch_run_kernel),
                        uint64_t(arch_run_kernel),
                        uint64_t(arch_run_kernel_end) -
                        uint64_t(arch_run_kernel),
                        PTE_PRESENT | PTE_ACCESSED);

    status = efi_systab->BootServices->GetMemoryMap(
                &mapsz, nullptr, &mapkey, &descsz, nullptr);

    if (unlikely(EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL))
        PANIC("Error getting EFI memory map");

    status = efi_systab->BootServices->ExitBootServices(
                efi_image_handle, mapkey);

    if (unlikely(EFI_ERROR(status)))
        PANIC("Error exiting boot services");

    uint64_t numeric_entry = 0;
    memcpy(&numeric_entry, entry, sizeof(numeric_entry));
    arch_run_kernel(numeric_entry, param,
        paging_root_addr(), nx_available);
}
