#include "unittest.h"
#include "stdlib.h"
#include "mm.h"
#include "likely.h"

DISABLED_UNITTEST(test_mmap_oom)
{
    struct item {
        size_t *addr;
        size_t sz;
    };

    constexpr size_t max_items = 256;

    item items[max_items];
    size_t item_count;

    // Start with 1MB chunks
    size_t chunk_sz = 1 << 20;

    for (item_count = 0; item_count < max_items; ++item_count) {
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
                         chunk_sz);
                continue;
            } else {
                break;
            }
        }

        // Verify that it is all zeros

        // Fill it with check values
        for (size_t i = 0, count = chunk_sz / sizeof(size_t);
             i < count; ++i)
            mem[i] = item_count + 1;

    }

    // Make sure we had enough items[] capacity to run out of memory
    lt(item_count, max_items);

    // Ran out of memory altogether

    for (size_t i = 0; i < item_count; ++i) {
        // Verify no corruption
        size_t *mem = items[i].addr;
        chunk_sz = items[i].sz;

        for (size_t c = 0, count = chunk_sz / sizeof(size_t);
             c < count; ++c) {
            eq(mem[c], i + 1);
        }

        munmap(items[i].addr, items[i].sz);
    }
}
