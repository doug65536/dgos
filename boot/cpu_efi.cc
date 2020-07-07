#include "cpu.h"
#include "bootefi.h"
#include "paging.h"
#include "malloc.h"
#include "assert.h"
#include "halt.h"
#include "elf64decl.h"
#include "string.h"

extern "C" _noreturn void code64_run_kernel(
        uint64_t entry, void *params, uint64_t cr3, bool nx_available);

extern char code64_run_kernel_end[];

void reloc_kernel(uint64_t distance, Elf64_Rela const *elf_rela, size_t relcnt)
{
    while (relcnt--) {
        uint64_t offset = elf_rela->r_offset;
        uint64_t addend = elf_rela->r_addend;
        ++elf_rela;
        uint64_t value = distance + addend;
        void *spot = (void*)(offset + distance);
        memcpy(spot, &value, sizeof(value));
    }
}

// This only runs on the BSP
void run_kernel(uint64_t entry, void *param)
{
    assert(param != nullptr);

    UINTN mapsz = 0;
    UINTN mapkey = 0;
    UINTN descsz = 0;

    EFI_STATUS status;

    paging_map_physical(uint64_t(code64_run_kernel),
                        uint64_t(code64_run_kernel),
                        uint64_t(code64_run_kernel_end) -
                        uint64_t(code64_run_kernel),
                        PTE_PRESENT | PTE_ACCESSED);

    status = efi_systab->BootServices->GetMemoryMap(
                &mapsz, nullptr, &mapkey, &descsz, nullptr);

    if (unlikely(EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL))
        PANIC("Error getting EFI memory map");

    status = efi_systab->BootServices->ExitBootServices(
                efi_image_handle, mapkey);

    if (unlikely(EFI_ERROR(status)))
        PANIC("Error exiting boot services");

    code64_run_kernel(entry, param, paging_root_addr(), nx_available);
}

// Not used
void copy_kernel(uint64_t dest_addr, void *src, size_t sz)
{
    assert(!"Should not be called");
}
