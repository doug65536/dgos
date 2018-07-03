#include "malloc.h"
#include "ctors.h"

//static void *set_top_of_low_memory(void *addr)
//{
//    // Read original top of low memory
//    uintptr_t top = uintptr_t(*(uint16_t const*)0x413) << 10;
//    
//    // Adjust it
//    *(uint16_t*)0x413 = uintptr_t(addr) >> 10;
//    
//    return (void*)top;
//}

static void *get_top_of_low_memory(void *addr)
{
    // Read original top of low memory
    uintptr_t top = uintptr_t(*(uint16_t const*)0x413) << 10;
    
    return (void*)top;
}

extern "C" char ___heap_st[];
_constructor(ctor_malloc) void malloc_init_auto()
{
    // Memory map
    //
    // +-------------------------+ <- End of free base memory
    // |  blk_hdr_t with 0 size  |
    // +-------------------------+ <- 16 byte aligned
    // |         blocks          |
    // +-------------------------+ <- start of heap, 4KB byte aligned
    // | /////// unused //////// |
    // +-------------------------+ <- ___heap_st
    // |         .bss            |
    // |         .data           |
    // |         .text           |
    // +-------------------------+ <- 0x1000
    
    void *st = (void*)((uintptr_t(___heap_st) + 4095) & -4096);
    void *en = get_top_of_low_memory(st);

    malloc_init(st, en);
}
