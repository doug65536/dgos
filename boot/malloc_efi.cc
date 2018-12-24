#include "malloc.h"
#include "ctors.h"
#include "bootefi.h"
#include "likely.h"

_constructor(ctor_malloc) void malloc_init_auto()
{
    EFI_STATUS status;

    // Initialize malloc (320KB heap, below 0x90000)
    static constexpr size_t heap_sz = (320 << 10);
    EFI_PHYSICAL_ADDRESS heap_physaddr = 0x90000;
    status = efi_systab->BootServices->AllocatePages(
                AllocateMaxAddress, EFI_MEMORY_TYPE(0x80000000),
                heap_sz >> 12, &heap_physaddr);

    if (unlikely(EFI_ERROR(status)))
        halt(TSTR "Could not allocate pages for low memory heap");

    malloc_init((void*)heap_physaddr,
                (void*)(int64_t(heap_physaddr) + heap_sz));
}
