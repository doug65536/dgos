#include "unittest.h"
#include "stdlib.h"
#include "mm.h"
#include "likely.h"
#include "memory.h"
#include "vector.h"

#include "contig_alloc.h"
#include "unique_ptr.h"
#include "cpu/phys_alloc.h"

UNITTEST(test_physalloc)
{
    // Simulate 16GB of RAM
    ext::unique_mmap<uint32_t> mem;
    size_t mem_sz = UINT64_C(0x400000);
    size_t mem_base = 0x100000;
    size_t page_cnt = (mem_sz - mem_base) >> PAGE_SIZE_BIT;
    size_t sz = mmu_phys_allocator_t::size_from_highest_page(page_cnt);
    eq(true, mem.mmap(sz));

    mmu_phys_allocator_t uut;

    uut.init(mem.get(), mem_base, page_cnt);

    uut.add_free_space(mem_base, mem_sz - mem_base);

    ext::vector<uintptr_t> pages;
    eq(true, pages.reserve(uut.get_free_page_count()));

    for (size_t pass = 0; pass < 2; ++pass){
        for (;;) {
            uintptr_t addr = uut.alloc_one();
            if (!addr)
                break;

            //printdbg("Allocated %#zx\n", addr);

            eq(true, pages.push_back(addr));
        }

        for (uintptr_t page: pages) {
            //printdbg("Freed %#zx\n", page);
            uut.release_one(page);
        }

        pages.clear();
    }
}

UNITTEST(test_contig_init)
{
    contiguous_allocator_t uut;

    // 1MB of free space at 4MB line
    uut.init(0x400000, 0x100000, "test");

    size_t range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        eq(size_t(0x400000), range.base);
        eq(size_t(0x100000), range.size);
        ++range_count;
        return true;
    });

    eq(size_t(1), range_count);
}

UNITTEST(test_contig_alloc_free)
{
    contiguous_allocator_t uut;

    //                                                          +--+B
    //                                                          |  |
    //                                                         >+--+A
    //
    //                                                   +--+9  +--+9
    //                                                   |  |   |  |
    //  +--+8  +--+8  +--+8  +--+8  +--+8  +--+8  +--+8 >|..|8  |  |
    //  |  |   |  |   |  |   |  |   |  |   |  |   |  |   |  |   |  |
    //  |  |   +--+7  +--+7  +--+7  +--+7  +--+7  +--+7  +--+7  +--+7
    //  |  |   .  .
    //  |  |   .  .                        +--+6  +--+6  +--+6  +--+6
    //  |  |   .  .                        |  |   |  |   |  |   |  |
    //  |  |   .  .          +--+5  +--+5 >|..|5  |  |   |  |   |  |
    //  |  |   .  .          |  |   |  |   |  |   |  |   |  |   |  |
    //  |  |   .  .         >+--+4  |..|4  |  |   |  |   |  |   |  |
    //  |  |   .  .                 |  |   |  |   |  |   |  |   |  |
    //  |  |   .  .                >+--+3  +--+3  |..|   |  |   |  |
    //  |  |   .  .                               |  |   |  |   |  |
    //  |  |   .  .   +--+2  +--+2  +--+2  +--+2 >|..|2  |  |   |  |
    //  |  |   .  .   |  |   |  |   |  |   |  |   |  |   |  |   |  |
    //  |  |   .  .  >+--+1  +--+1  +--+1  +--+1  +--+1  +--+1  +--+1
    //  |  |   .  .
    // >+--+0 <....
    //  IA@0     A7   F1@1   F1@4   F1@3   F1@5   F1@1   F1@8   F1@8
    //   a       b     c      d      e      f      g      h      i
    //   |       |     |      |      |      |      |      |      |
    //  Init   Alloc   | non-adjacent| end-adjacent|   adjacent  |
    //                 |   in hole   |    in hole  |    at end   |
    //                 |             |             |             |
    //           non-adjacent  start-adjacent  close hole  non-adjacent
    //             at start       in hole                     at end
    //

    // a) Space at 4MB line
    uut.init(0x400000, 0x8000, "test");

    // b) Allocate
    uintptr_t addr = uut.alloc_linear(0x7000);

    eq(uintptr_t(0x400000), addr);

    size_t range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        eq(size_t(0x407000), range.base);
        eq(size_t(0x001000), range.size);
        ++range_count;
        return true;
    });

    eq(size_t(1), range_count);

    // c) Free non-adjacent block
    uut.release_linear(addr + 0x1000, 0x1000);

    range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x401000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        case 1:
            eq(size_t(0x407000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(2), range_count);

    // d) not adjacent in hole
    uut.release_linear(addr + 0x4000, 0x1000);

    range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x401000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        case 1:
            eq(size_t(0x404000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        case 2:
            eq(size_t(0x407000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(3), range_count);

    // e) extend start in hole
    uut.release_linear(addr + 0x3000, 0x1000);

    range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x401000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        case 1:
            eq(size_t(0x403000), range.base);
            eq(size_t(0x002000), range.size);
            break;
        case 2:
            eq(size_t(0x407000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(3), range_count);

    // f) extend end in hole
    uut.release_linear(addr + 0x5000, 0x1000);

    range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x401000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        case 1:
            eq(size_t(0x403000), range.base);
            eq(size_t(0x003000), range.size);
            break;
        case 2:
            eq(size_t(0x407000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(3), range_count);

    // g) close hole
    uut.release_linear(addr + 0x2000, 0x1000);

    range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x401000), range.base);
            eq(size_t(0x005000), range.size);
            break;
        case 1:
            eq(size_t(0x407000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(2), range_count);

    // h) adjacent at end
    uut.release_linear(addr + 0x8000, 0x1000);

    range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x401000), range.base);
            eq(size_t(0x005000), range.size);
            break;
        case 1:
            eq(size_t(0x407000), range.base);
            eq(size_t(0x002000), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(2), range_count);

    // i) non-adjacent at end
    uut.release_linear(addr + 0xA000, 0x1000);

    range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x401000), range.base);
            eq(size_t(0x005000), range.size);
            break;
        case 1:
            eq(size_t(0x407000), range.base);
            eq(size_t(0x002000), range.size);
            break;
        case 2:
            eq(size_t(0x40A000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(3), range_count);

    // Free whole thing, extending start and end
    uut.release_linear(addr + 0x800, 0xB000);

    range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x400800), range.base);
            eq(size_t(0x00B000), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(1), range_count);
}

UNITTEST(test_contiguous_alloc_take_taken)
{
    contiguous_allocator_t uut;

    uut.init(0x400000, 0x1000, "test");
    eq(true, uut.take_linear(0x4200, 0x1000, false));
    //uut.dump("took linear");

    size_t range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x400000), range.base);
            eq(size_t(0x001000), range.size);
            break;
        default:
            fail("Should have one entry");
            break;
        }
        return true;
    });

    eq(size_t(1), range_count);
}

UNITTEST(test_contiguous_alloc_take_some_start)
{
    contiguous_allocator_t uut;

    uut.init(0x400000, 0x1000, "test");
    eq(true, uut.take_linear(0x3ff000, 0x1800, false));
    //uut.dump("took linear");

    size_t range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x400800), range.base);
            eq(size_t(0x000800), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(1), range_count);
}

UNITTEST(test_contiguous_alloc_take_some_mid)
{
    contiguous_allocator_t uut;

    uut.init(0x400000, 0x2000, "test");
    eq(true, uut.take_linear(0x400400, 0x0800, false));
    //uut.dump("took linear");

    size_t range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x400000), range.base);
            eq(size_t(0x000400), range.size);
            break;
        case 1:
            eq(size_t(0x400c00), range.base);
            eq(size_t(0x001400), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(2), range_count);
}

UNITTEST(test_contiguous_alloc_take_some_end)
{
    contiguous_allocator_t uut;

    uut.init(0x400000, 0x1000, "test");
    eq(true, uut.take_linear(0x400800, 0x1000, false));
    //uut.dump("took linear");

    size_t range_count = 0;
    uut.each_fw([&](contiguous_allocator_t::mmu_range_t range) {
        switch (range_count++) {
        case 0:
            eq(size_t(0x400000), range.base);
            eq(size_t(0x000800), range.size);
            break;
        default:
            break;
        }
        return true;
    });

    eq(size_t(1), range_count);
}

_const
static size_t mem_fill_value(size_t input)
{
    // Expect them at 8-byte boundaries, make all bits useful
    size_t n = ((input & UINT64_C(0xFFFFFFFFFFFF) >> 3)) *
            UINT64_C(6364136223846793005) +
            UINT64_C(1442695040888963407);
    n = (n << 32) | (n >> 32);
    return n;
}

DISABLED_UNITTEST(test_mmap_oom)
{
    struct item {
        size_t *addr;
        size_t sz;
    };

    constexpr size_t max_items = 256;

    ext::unique_ptr<item[]> items(new (ext::nothrow) item[max_items]());
    size_t item_count;

    for (size_t pass = 0; pass < 2; ++pass) {
        // Start with 1GB chunks
        size_t chunk_sz = size_t(1) << 30;

        for (item_count = 0; item_count < max_items; ++item_count) {
            printdbg("Attempting to allocate %zu KB\n", chunk_sz >> 10);

            size_t *mem = (size_t*)mmap(nullptr, chunk_sz,
                                        PROT_READ | PROT_WRITE,
                                        MAP_POPULATE);

            items[item_count].addr = mem;
            items[item_count].sz = chunk_sz;

            if (unlikely(mem == MAP_FAILED)) {
                --item_count;

                if (chunk_sz > PAGESIZE) {
                    chunk_sz >>= 1;
                    printdbg("Too big, scaled down to %zu KB per mmap\n",
                             chunk_sz >> 10);
                    continue;
                } else {
                    break;
                }
            }

            printdbg("Processing %zuKB chunk\n", chunk_sz >> 10);

            // Verify that it is all zeros
            printdbg("Zero checking\n");
            for (size_t i = 0, count = chunk_sz / sizeof(size_t);
                 i < count; ++i)
                eq(size_t(0), mem[i]);

            // Fill it with check values
            printdbg("Filling with values\n");
            for (size_t i = 0, count = chunk_sz / sizeof(size_t);
                 i < count; ++i) {
                size_t value = mem_fill_value(size_t(&mem[i]));
                mem[i] = value;
            }

            // Verify check values
            printdbg("Fill check\n");
            for (size_t i = 0, count = chunk_sz / sizeof(size_t);
                 i < count; ++i) {
                size_t expect = mem_fill_value(size_t(&mem[i]));
                eq(expect, mem[i]);
            }

        }

        // Make sure we had enough items[] capacity to run out of memory
        lt(item_count, max_items);

        // Ran out of memory altogether

        for (size_t i = 0; i < item_count; ++i) {
            // Verify no corruption
            size_t *mem = items[i].addr;
            chunk_sz = items[i].sz;

            printdbg("Final checking %zu KB\n", items[i].sz >> 10);
            for (size_t c = 0, count = chunk_sz / sizeof(size_t);
                 c < count; ++c) {
                size_t expect = mem_fill_value(size_t(&mem[i]));
                eq(expect, mem[i]);
            }

            printdbg("Freeing %zu KB\n", items[i].sz >> 10);
            munmap(items[i].addr, items[i].sz);
        }

        printdbg("Pass %zu completed\n", pass + 1);
    }
}
