#include "code16gcc.h"
#include "malloc.h"
#include "screen.h"
#include "bootsect.h"

// Simple allocator

// This must be small enough to fit before first header
// Maximum 13 bytes
typedef struct {
    uint8_t alloc_id;
    uint8_t heap_ready;
} alloc_state_t;

extern alloc_state_t __heap;
extern char __heap_end[];

#ifdef __GNUC__
#define always_inline inline __attribute__((always_inline))
#endif

#ifndef NDEBUG
#define DEBUG_ONLY(e) e;
#else
#define DEBUG_ONLY(e)
#endif

static void debug_break()
{
    __asm__ __volatile__ (
        "int $3\n"
    );
}

static void check_valid_header(uint16_t v)
{
    if ((v & 0x0F) != 0x0D)
        debug_break();
}

static always_inline uint16_t addr_of(void *p)
{
    return (uint16_t)(uint32_t)p;
}

static always_inline uint16_t payload_of(uint16_t addr)
{
    DEBUG_ONLY(check_valid_header(addr))
    return addr + 3;
}

static always_inline char *payload_ptr_of(uint16_t addr)
{
    DEBUG_ONLY(check_valid_header(addr))
    return (void*)(uint32_t)payload_of(addr);
}

static always_inline uint16_t id_of(uint16_t addr)
{
    DEBUG_ONLY(check_valid_header(addr))
    return *(uint8_t*)(uint32_t)addr;
}

static always_inline uint16_t size_of(uint16_t addr)
{
    DEBUG_ONLY(check_valid_header(addr))
    return *(uint16_t*)(uint32_t)(addr + 1);
}

static always_inline uint8_t set_id_of(uint16_t addr, uint8_t id)
{
    DEBUG_ONLY(check_valid_header(addr))
    return *(uint8_t*)(uint32_t)addr = id;
}

static always_inline uint16_t set_size_of(uint16_t addr, uint16_t size)
{
    DEBUG_ONLY(check_valid_header(addr))
    return *(uint16_t*)(uint32_t)(addr + 1) = size;
}

static always_inline uint16_t inc_size_of(uint16_t addr, uint16_t inc)
{
    DEBUG_ONLY(check_valid_header(addr))
    return *(uint16_t*)(uint32_t)(addr + 1) += inc;
}

// This controls alignment
static uint16_t malloc_next_header(uint16_t addr)
{
    // Is there enough room to squeeze header in
    // after previous block and before this block?
    if ((addr & 0x0F) <= 0x0D) {
        // Nice, header will fit after previous block
        addr = (addr & ~15) | 0x0D;
    } else {
        // Won't fit, move forward to next 16-byte aligned area
        addr = ((addr + 16) & ~15) | 0x0D;
    }

    return addr;
}

static uint8_t malloc_take_id()
{
    uint8_t id = ++__heap.alloc_id;

    // Don't let next allocation id be 0xFF or 0x00
    __heap.alloc_id += (__heap.alloc_id == 0xFE) << 1;

    return id;
}

void *malloc(uint16_t bytes)
{
    uint16_t addr = addr_of(&__heap);
    uint16_t best_size = 0;
    uint16_t best_addr;

    // Initialize heap if necessary
    if (!__heap.heap_ready) {
        __heap.heap_ready = 1;

        // Mark end of heap with special id 0xFF
        addr = addr_of(__heap_end);
        addr = malloc_next_header(addr);
        set_id_of(addr, 0xFF);
        set_size_of(addr, 0);

        // Create free block for entire size of heap
        addr = malloc_next_header(addr_of(&__heap));
        set_id_of(addr, 0);
        set_size_of(addr, addr_of(__heap_end) - payload_of(addr));
    }

    for (;;) {
        addr = malloc_next_header(addr);

        // See if this block is free
        uint8_t id = id_of(addr);
        uint16_t size = size_of(addr);

        // See if we reached the end of the heap
        if (id == 0xFF)
            break;

        if (id == 0) {
            // Yes, it is free

            // See if we can coalesce this block with the next
            uint16_t next_header = malloc_next_header(addr + size + 3);
            uint16_t next_size = size_of(next_header);

            if (next_size && id_of(next_header) == 0) {
                // Coalesce
                inc_size_of(addr, next_size + 3);
                continue;
            }

            // If it is big enough and is better fit
            if (size >= bytes && (best_size == 0 || best_size > size)) {
                // Found better fit
                best_addr = addr;
                best_size = size;
            }

            addr += size + 3;
        } else {
            // Not free, skip forward to next header
            addr += size + 3;
        }
    }

    // See if a sufficient free block was found
    if (best_size == 0)
        return 0;

    // Mark taken block as not free
    set_id_of(best_addr, malloc_take_id());

    // Figure out where the header would be immediately after needed space
    uint16_t new_header = malloc_next_header(best_addr + 3 + bytes);

    // Figure out where the next header is, after currently free space
    uint16_t next_header = malloc_next_header(best_addr + 3 + best_size);

    // Insert a new free block, if there is sufficient space
    if (next_header - new_header >= 32)
    {
        // Make free block after space we took
        set_id_of(new_header, 0);
        set_size_of(new_header, next_header - new_header - 3);

        // Reduce the size of the block we took
        set_size_of(best_addr, new_header - best_addr - 3);
    }

    return payload_ptr_of(best_addr);
}

void free(void *p)
{
    if (!p)
        return;

    uint16_t addr = addr_of(p);

    // Mark as free
    set_id_of(addr - 3, 0);
}

static uint32_t seed_z1 = 12345;
static uint32_t seed_z2 = 12345;
static uint32_t seed_z3 = 12345;
static uint32_t seed_z4 = 12345;

void lfsr113_seed(uint32_t seed)
{
    seed_z1 = seed;
    seed_z2 = seed;
    seed_z3 = seed;
    seed_z4 = seed;
}

uint32_t lfsr113_rand(void)
{

   unsigned int b;
   b  = ((seed_z1 << 6) ^ seed_z1) >> 13;
   seed_z1 = ((seed_z1 & 0xFFFFFFFEU) << 18) ^ b;
   b  = ((seed_z2 << 2) ^ seed_z2) >> 27;
   seed_z2 = ((seed_z2 & 0xFFFFFFF8U) << 2) ^ b;
   b  = ((seed_z3 << 13) ^ seed_z3) >> 21;
   seed_z3 = ((seed_z3 & 0xFFFFFFF0U) << 7) ^ b;
   b  = ((seed_z4 << 3) ^ seed_z4) >> 12;
   seed_z4 = ((seed_z4 & 0xFFFFFF80U) << 13) ^ b;
   return (seed_z1 ^ seed_z2 ^ seed_z3 ^ seed_z4);
}

uint32_t rand_range(uint32_t st, uint32_t en)
{
    uint32_t n = lfsr113_rand();
    n %= (en - st);
    n += st;
    return n;
}

uint16_t *alloc_random_block()
{
    uint16_t size = rand_range(8, 1024) >> 1;
    uint16_t *block = malloc(size * sizeof(uint16_t));
    block[0] = size;
    for (uint16_t i = 1; i < size; ++i)
        block[i] = 0x8000 + i;
    return block;
}

void check_random_block(uint16_t *block)
{
    uint16_t size = block[0];
    if (size < 4)
    {
        print_line("check_random_block failed, size=%d", size);
        debug_break();
    }

    for (uint16_t i = 1; i < size; ++i)
    {
        uint16_t expect = 0x8000 + i;
        uint16_t got = block[i];
        if (got != expect) {
            print_line("check_random_block failed,"
                       " expected=%d, value=%d", expect, got);
            debug_break();
        }
    }
}

#define TEST_SIZE 64
void test_malloc(void)
{
    uint16_t *ptrs[TEST_SIZE];
    const uint32_t iters = 0xFFFFFF;

    // Populate with random blocks
    for (uint16_t i = 0; i < TEST_SIZE; ++i)
        ptrs[i] = alloc_random_block();

    for (uint32_t i = 0; i < iters; ++i)
    {
        // Pick something to free
        uint16_t f = rand_range(0, TEST_SIZE);
        check_random_block(ptrs[f]);
        free(ptrs[f]);
        ptrs[f] = alloc_random_block();

        if (!(i & 0xFFFF))
            print_line("Iter %d", i);
    }

    // Free everything
    for (uint16_t i = 0; i < TEST_SIZE; ++i)
        free(ptrs[i]);
}
