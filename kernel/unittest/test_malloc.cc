#include "unittest.h"
#include "stdlib.h"
#include "mm.h"
#include "likely.h"

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

    item items[max_items] = {};
    size_t item_count;

    for (size_t pass = 0; pass < 2; ++pass) {
        // Start with 1GB chunks
        size_t chunk_sz = size_t(1) << 30;

        for (item_count = 0; item_count < max_items; ++item_count) {
            printdbg("Attempting to allocate %zu KB\n", chunk_sz >> 10);

            size_t *mem = (size_t*)mmap(nullptr, chunk_sz,
                                        PROT_READ | PROT_WRITE,
                                        MAP_POPULATE, -1, 0);

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
