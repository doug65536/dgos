#include "malloc.h"
#include "screen.h"
#include "string.h"
#include "bootsect.h"
#include "rand.h"
#include "farptr.h"

// Allocate-only (no free) far pointer allocator

// Next free location for aligned far allocations
// Starts at top of first 64KB
static uint16_t free_seg_st = 0x1000;

// Aligned top of memory for far allocations
static uint16_t free_seg_en;

static uint16_t get_top_of_low_memory() {
    // Read top of memory from BIOS data area
    return *(uint16_t*)0x40E;
}

// Allocate from top of memory
uint16_t far_malloc(uint32_t bytes)
{
    if (free_seg_en == 0)
        free_seg_en = get_top_of_low_memory();

    uint16_t paragraphs = (bytes - 1 + (1 << 4)) >> 4;
    uint16_t segment = (free_seg_en -= paragraphs);
    far_zero(far_ptr2(segment, 0), paragraphs);

    return segment;
}

// Returns segment guaranteed aligned on page boundary
uint16_t far_malloc_aligned(uint32_t bytes)
{
    uint16_t segment = free_seg_st;
    uint16_t paragraphs = ((bytes - 1 + (1 << 12)) & -4096) >> 4;
    free_seg_st += paragraphs;
    far_zero(far_ptr2(segment, 0), paragraphs);
    return segment;
}

// Simple heap allocator

// This must be small enough to fit before first header
// Maximum 13 bytes
typedef struct alloc_state_t {
    uint8_t alloc_id;
    uint8_t heap_ready;

    // Simplify starting at the beginning of the heap
    uint16_t first_header;

    // Optimize starting point for free block search
    uint16_t first_free;
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

#ifndef NDEBUG
static void check_valid_header(uint16_t v)
{
    if ((v & 0x0F) != 0x0D)
        debug_break();
}
#endif

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
    return (char*)(uint32_t)payload_of(addr);
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

static void debug_ff(uint16_t addr)
{
    (void)addr;
//    print_line("FF=%x", addr);
}

void *malloc(uint16_t bytes)
{
    uint16_t addr;
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

        __heap.first_header = addr;
        __heap.first_free = addr;
        debug_ff(addr);
    }

    uint16_t seen_free = 0;
    addr = __heap.first_free;

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

            if (seen_free == 0) {
                seen_free = 1;

                // Make first free precise
                if (__heap.first_free != addr) {
                    __heap.first_free = addr;
                    debug_ff(addr);
                }
            }

            // See if we can coalesce this block with the next
            uint16_t next_header = malloc_next_header(addr + size + 3);
            uint16_t next_size = size_of(next_header);

            if (next_size && id_of(next_header) == 0) {
                // Free space rover can't possibly point to the
                // block we coalesce because this block is free
                // and freeing moves it back

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

        // Move free block rover forward if we split first free block
        if (__heap.first_free == best_addr) {
            __heap.first_free = new_header;
            debug_ff(new_header);
        }
    }

    // Maybe __heap.first_free still points to this block?
    // That's okay. Only freeing and coalescing blocks moves it back.
    // Only splitting the first free block moves it forward

    return payload_ptr_of(best_addr);
}

void free(void *p)
{
    if (!p)
        return;

    uint16_t addr = addr_of(p);

    if (__heap.first_header + 3 > addr)
        debug_break();

    // Mark as free
    set_id_of(addr - 3, 0);

    // Move free space rover back to this block
    // if this block is before the old first free
    // block
    if (__heap.first_free > addr) {
        __heap.first_free = addr - 3;
        debug_ff(addr);
    }
}

void *calloc(uint16_t num, uint16_t size)
{
    uint16_t bytes = num * size;
    void *block = malloc(bytes);
    memset(block, 0, bytes);
    return block;
}

#ifndef NDEBUG

static uint16_t *alloc_random_block()
{
    uint16_t size = rand_range(8, 512) >> 1;
    uint16_t *block = (uint16_t*)malloc(size * sizeof(uint16_t));

    if (!block) {
        print_line("Warning, malloc failed, size=%d", size);
        return 0;
    }

    uint16_t addr = addr_of(block);

    block[0] = size;
    block[1] = ~size ^ addr;
    for (uint16_t i = 2; i < size; ++i)
        block[i] = (0x8000 + i) ^ addr;
    return block;
}

static void check_random_block(uint16_t *block)
{
    uint16_t size = block[0];

    if (size < 4)
    {
        print_line("check_random_block failed, size=%d", size);
        debug_break();
    }

    uint16_t addr = addr_of(block);
    uint16_t expect = ~size ^ addr;
    uint16_t got = block[1];

    if (got != expect) {
        print_line("check_random_block failed,"
                   " expected=%d, value=%d", expect, got);
        debug_break();
    }

    for (uint16_t i = 2; i < size; ++i)
    {
        expect = (0x8000 + i) ^ addr;
        got = block[i];
        if (got != expect) {
            print_line("check_random_block failed,"
                       " expected=%d, value=%d", expect, got);
            debug_break();
        }
    }
}

#define TEST_SIZE 48
void test_malloc(void)
{
    uint16_t *ptrs[TEST_SIZE];
    const uint32_t iters = 0xFFFFFF;

    // Populate with random blocks
    for (uint16_t i = 0; i < TEST_SIZE; ++i) {
        ptrs[i] = alloc_random_block();

        if (!ptrs[i])
            debug_break();
    }

    for (uint32_t i = 0; i < iters; ++i)
    {
        // Pick something to free
        uint16_t f = rand_range(0, TEST_SIZE);
        if (ptrs[f])
            check_random_block(ptrs[f]);
        free(ptrs[f]);

        ptrs[f] = alloc_random_block();
        if (!ptrs[f])
            debug_break();

        if (!(i & 0xFFFF))
            print_line("Iter %d", i);
    }

    // Free everything
    for (uint16_t i = 0; i < TEST_SIZE; ++i)
        free(ptrs[i]);

    // Try huge allocation and make sure it fails
    // Also coalesces all free blocks
    ptrs[0] = (uint16_t *)malloc(0xFFFF);
    // Should fail
    if (ptrs[0])
        debug_break();

    //uint16_t chk = malloc_next_header(addr_of(&__heap));
    //print_line("Max block remaining is %x", chk);
}

#endif

void *operator new(size_t size)
{
    return malloc(size);
}

void operator delete(void *block, unsigned long size)
{
    (void)size;
    free(block);
}

void operator delete(void *block, unsigned size)
{
    (void)size;
    free(block);
}

void operator delete(void *block)
{
    free(block);
}

void operator delete[](void *block)
{
    free(block);
}

void operator delete[](void *block, unsigned int)
{
    free(block);
}
