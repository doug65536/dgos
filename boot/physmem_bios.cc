#include "physmem.h"
#include "malloc.h"
#include "farptr.h"
#include "screen.h"
#include "bioscall.h"
#include "likely.h"
#include "halt.h"
#include "assert.h"
#include "physmap.h"

static uint16_t get_e820_region(physmem_range_t *range,
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
    regs.edi = uint32_t(range) & 0xF;
    regs.es = uint32_t(range) >> 4;

    bioscall(&regs, 0x15);

    *continuation_ptr = regs.ebx;

    return !regs.ah_if_carry();
}

bool get_ram_regions()
{
    // Count them
    uint32_t continuation = 0;
    uint64_t total_memory = 0;
    for (int i = 0; continuation || i == 0; ++i) {
        physmem_range_t entry{};

        if (!get_e820_region(&entry, &continuation))
            break;

        // Drop invalid entries
        if (unlikely(!entry.valid))
            continue;

        if (entry.type == PHYSMEM_TYPE_NORMAL)
            total_memory += entry.size;

        physmap_insert(entry);

        PRINT("base=%llx length=%llx type=%lx valid=%lx",
                   entry.base,
                   entry.size,
                   entry.type,
                   entry.valid);
    }

    PRINT("Usable memory: %dMB", (int)(total_memory >> 20));

    return true;
}

void take_pages(uint64_t, uint64_t)
{
}
