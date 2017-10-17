#include "physmem.h"
#include "malloc.h"
#include "farptr.h"
#include "screen.h"
#include "bioscall.h"

static uint16_t get_ram_region(physmem_range_t *range,
                               uint32_t *continuation_ptr)
{
    // AX = E820h
    // EAX = 0000E820h
    // EDX = 534D4150h ('SMAP')
    // EBX = continuation value or 00000000h to start at beginning of map
    // ECX = size of buffer for result, in bytes (should be >= 20 bytes)
    // ES:DI -> buffer for result (see #00581)
    
    range->valid = 1;
    
    bios_regs_t regs;
    regs.eax = 0xE820;
    regs.edx = 0x534D4150;
    regs.ebx = *continuation_ptr;
    regs.ecx = sizeof(physmem_range_t);
    regs.edi = (uint32_t)range;
    
    bioscall(&regs, 0x15);
    
    *continuation_ptr = regs.ebx;
    
    return !regs.ah_if_carry();
}

uint16_t get_ram_regions(uint32_t *ret_size)
{
    physmem_range_t temp;

    // Count them
    uint32_t continuation = 0;
    uint16_t count = 0;
    do {
        if (!get_ram_region(&temp, &continuation))
            break;

        ++count;
    } while (continuation != 0);

    physmem_range_t *result = (physmem_range_t*)
            calloc(count + 1, sizeof(*result));

    continuation = 0;
    uint64_t total_memory = 0;
    for (uint16_t i = 0; i < count; ++i) {
        if (!get_ram_region(result + i, &continuation))
            break;

        if (result[i].type == PHYSMEM_TYPE_NORMAL)
            total_memory += result[i].size;

        print_line("base=%llx length=%llx type=%x valid=%x",
                   result[i].base,
                   result[i].size,
                   result[i].type,
                   result[i].valid);
    }

    far_ptr_t far_result;
    // Allocate extra space in case entries get split
    // Worst case is every entry overlaps and becomes 3 entries
    uint16_t size = (count + 1) * sizeof(*result);
    far_result.segment = far_malloc(size * 3);
    far_result.offset = 0;
    far_copy(far_result, far_ptr((uint32_t)result), size);
    free(result);

    *ret_size = count;

    print_line("Usable memory: %dMB", (int)(total_memory >> 20));

    return far_result.segment;
}
