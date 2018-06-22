#include "cpu.h"
#include "bootefi.h"
#include "paging.h"
#include "malloc.h"
#include "assert.h"

extern "C" _noreturn void code64_run_kernel(
        uint64_t entry, void *params, uint64_t cr3, bool nx_available);
extern char code64_run_kernel_end[];

void reloc_kernel(uint64_t distance, void *elf_rela, size_t relcnt)
{
}

// This only runs on the BSP
void run_kernel(uint64_t entry, void *param)
{
    assert(param != nullptr);

    UINTN mapsz = 0;
    UINTN mapkey = 0;
    UINTN descsz = 0;

    efi_systab->BootServices->GetMemoryMap(
                &mapsz, nullptr, &mapkey, &descsz, nullptr);

    efi_systab->BootServices->ExitBootServices(
                efi_image_handle, mapkey);

    paging_map_physical(uint64_t(code64_run_kernel),
                        uint64_t(code64_run_kernel),
                        uint64_t(code64_run_kernel_end) -
                        uint64_t(code64_run_kernel),
                        PTE_PRESENT | PTE_ACCESSED);

    code64_run_kernel(entry, param, paging_root_addr(), nx_available);
}

// Not used
void copy_kernel(uint64_t dest_addr, void *src, size_t sz)
{
    assert(!"Should not be called");
}
