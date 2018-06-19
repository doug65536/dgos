#include "malloc.h"
#include "ctors.h"

static __pure uintptr_t get_top_of_low_memory() {
    // Read top of memory from BIOS data area
    return *(uint16_t*)0x40E << 4;
}

extern "C" char ___heap_st[];
__constructor(ctor_malloc) void malloc_init_auto()
{
    // Memory map
    //
    // +-------------------------+ <- Start of BIOS data area (usually 0x9FC00)
    // |  blk_hdr_t with 0 size  |
    // +-------------------------+ <- 16 byte aligned
    // |         blocks          |
    // +-------------------------+ <- ___heap_st, 16 byte aligned
    // |         .bss            |
    // |         .data           |
    // |         .text           |
    // +-------------------------+ <- 0x1000

    malloc_init(___heap_st, (void*)get_top_of_low_memory());
}
