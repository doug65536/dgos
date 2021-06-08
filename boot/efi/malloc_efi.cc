#include "malloc.h"
#include "ctors.h"
#include "bootefi.h"
#include "likely.h"
#include "halt.h"

_constructor(ctor_malloc) static void malloc_init_auto()
{
    EFI_STATUS status;

    // Initialize malloc (320KB heap, below 0x90000)
    static constexpr size_t heap_sz = (320 << 10);

    // Ask for 320KB below 1M
    EFI_PHYSICAL_ADDRESS heap_physaddr = 0x100000;
    status = efi_systab->BootServices->AllocatePages(
                AllocateMaxAddress, EfiLoaderData,
                heap_sz >> PAGE_SIZE_BIT, &heap_physaddr);

    if (unlikely(EFI_ERROR(status)))
        PANIC("Could not allocate pages for low memory heap");

    malloc_init((void*)heap_physaddr,
                (void*)(int64_t(heap_physaddr) + heap_sz));
}
