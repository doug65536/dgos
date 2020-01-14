#include <stdlib.h>
//#include "../../kernel/lib/cc/algorithm.h"
#include <sys/mman.h>
#include <sys/likely.h>

void *malloc(size_t sz)
{
    void *mem = mmap(nullptr, sz + 64, PROT_READ | PROT_WRITE,
                MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (unlikely(mem == MAP_FAILED))
        return nullptr;

    *(size_t*)mem = sz;

    return (char*)mem+64;
}






/*
#include <stdint.h>

/// Buckets
///  Size Log
///    64   6
///   128   7
///   256   8
///   512   9
///  1024  10
///  2048  11
///  4096  12
///  8192  13
/// 16384  14
/// 32768  15
/// 65536  16

// A 64KB arena contains 1024 cache lines of 64 bytes

/// bucket  slot sz item sz items efficiency
/// [ 0] ->      32      16  2048     50.00%
/// [ 1] ->      64      48  1024     75.00%
/// [ 2] ->     128     112   512     87.50%
/// [ 3] ->     256     240   256     93.75%
/// [ 4] ->     512     496   128     96.88%
/// [ 5] ->    1024    1008    64     98.44%
/// [ 6] ->    2048    2032    32     99.22%
/// [ 7] ->    4096    4080    16     99.61%
/// [ 8] ->    8192    8176     8     99.80%
/// [ 9] ->   16384   16368     4     99.90%
/// [10] ->   32768   32752     2     99.95%
/// [11] ->   65536   65520     1     99.98%
/// .... -> use mmap

struct arena_t {
    // Pointer to the memory
    void *mem;

    // Use a 2-level bitmap to perform O(1) first fit
    // allocation of up to 2048 items 64*32b
    uint64_t top;
    uint32_t free_count;
    uint32_t scale;
    uint32_t map[32];

    // Scale is 6 for smallest (64 byte) bucket
    // Scale is 16 for largest (64KB) bucket
    arena_t(void *mem, uint8_t scale)
        : mem(mem)
        , scale(scale)
    {
        // Calculate how many slots at this scale
        uint8_t log2_capacity = 16 - scale;
        free_count = 1 << log2_capacity;

        // Calculate how many 32 bit bitmaps to have a bit per slot
        uint8_t used_top_bits = free_count >> 5;

        // Initialize the top mask to mark all nonexistent slots as allocated
        top = used_top_bits < 64
                ? ~uint64_t(0) << used_top_bits
                : 0;

        // Mark all bitmaps as fully free
        for (size_t i = 0; i < used_top_bits; ++i)
            map[i] = 0;

        if (log2_capacity < 6) {
            top &= ~uint64_t(1);
            map[0] |= ~uint64_t(1) << log2_capacity;
        }
    }
};

*/
//#include "../../kernel/lib/cc/vector.h"
