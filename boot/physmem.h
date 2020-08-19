#pragma once

#include "types.h"
#include "physmem_data.h"
#include "paging.h"

bool get_ram_regions();

void take_pages(uint64_t phys_addr, uint64_t size);

// Allocate the first free range, and take up to the specified amount,
// or, take the entire first free block if it is smaller than size
phys_alloc_t alloc_phys(uint64_t size, uint64_t for_addr = 0,
                        bool insist = false);
void free_phys(phys_alloc_t freed, size_t hint = -1);

class alloc_page_factory_t : public page_factory_t {
public:
    // You can tell it which linear address you are mapping and use
    // that information to choose an optimal page
    // Optimize for AMD's 16KB TLB page optimization
    // (a 16KB aligned run of contiguous 16KB pages with the same permission
    // takes a single 4KB TLB entry)
    phys_alloc_t alloc(size64_t size, uint64_t for_addr)
            noexcept override final
    {
        return alloc_phys(size, for_addr);
    }
};
