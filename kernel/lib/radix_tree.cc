#include "radix_tree.h"

#define PAGE_SIZE_BIT   12
#define PAGE_SIZE       (size_t(1) << PAGE_SIZE_BIT)
#define PAGE_MASK       (PAGE_SIZE - 1)

#define PAGE_SIZE_BIT 12
#define PAGE_SIZE (size_t(1) << PAGE_SIZE_BIT)

void *radix_tree_t::lookup(uint64_t addr, bool commit_pages)
{
    unsigned misalignment = addr & PAGE_MASK;
    addr -= misalignment;

    size_t ai = addr >> (PAGE_SIZE_BIT + (log2_radix_slots * 0));
    size_t bi = addr >> (PAGE_SIZE_BIT + (log2_radix_slots * 1));
    size_t ci = addr >> (PAGE_SIZE_BIT + (log2_radix_slots * 2));
    size_t di = addr >> (PAGE_SIZE_BIT + (log2_radix_slots * 3));
    ai &= (radix_slots - 1);
    bi &= (radix_slots - 1);
    ci &= (radix_slots - 1);
    di &= (radix_slots - 1);

    if (!commit_pages) {
        if (unlikely(!radix_tree ||
                     !radix_tree[di] ||
                     !radix_tree[di][ci] ||
                     !radix_tree[di][ci][bi] ||
                     !radix_tree[di][ci][bi][ai]))
            return nullptr;

        return (char*)radix_tree[di][ci][bi][ai] + misalignment;
    }

    if (unlikely(!radix_tree)) {
        if (unlikely(!commit(radix_tree)))
            return nullptr;
    }

    void ****&level0 = radix_tree[di];

    if (unlikely(!level0)) {
        if (unlikely(!commit(level0)))
            return nullptr;
    }

    void ***&level1 = level0[ci];

    if (unlikely(!level1)) {
        if (unlikely(!commit(level1)))
            return nullptr;
    }

    void **&level2 = level1[bi];

    if (unlikely(!level2)) {
        if (unlikely(!commit(level2)))
            return nullptr;
    }

    void *&level3 = level2[ai];

    if (unlikely(!level3)) {
        if (unlikely(!commit(level3)))
            return nullptr;
    }

    return (char*)level3 + misalignment;
}

bool radix_tree_t::fill(uint64_t start, uint8_t value, uint64_t len)
{
    while (len) {
        uint64_t page_end = (start + PAGE_SIZE) & -PAGE_SIZE;
        uint64_t fill = page_end - start;
        if (fill > len)
            fill = len;

        uint8_t *p = (uint8_t*)lookup(start, true);

        if (unlikely(!p))
            return false;

        // Note, can't use memset here because that would cause
        // recursive ASAN store calls, so just fill manually in a loop
        for (size_t i = 0; i < fill; ++i)
            p[i] = value;

        start += fill;
        len -= fill;
    }

    return true;
}

bool radix_tree_t::is_filled_with(uint64_t start, uint8_t value, uint64_t len)
{
    while (len) {
        uint64_t page_end = (start + PAGE_SIZE) & -PAGE_SIZE;
        uint64_t fill = page_end - start;
        if (fill > len)
            fill = len;
        uint8_t *p = (uint8_t*)lookup(start, false);

        if (unlikely(!p))
            return false;

        for (size_t i = 0; i < fill; ++i) {
            if (p[i] != value)
                return false;
        }

        start += fill;
        len -= fill;
    }

    return true;
}

// One page can track 32KB
#define ASAN_POOL_BYTE_LIMIT    (UINT64_C(1)<<28)
#define ASAN_POOL_PAGE_COUNT    (ASAN_POOL_BYTE_LIMIT >> 14)

_section(".asanbss")
uint8_t radix_tree_t::asan_pool[PAGE_SIZE*ASAN_POOL_PAGE_COUNT];
_section(".asanbss")
size_t radix_tree_t::asan_alloc_ptr;

void *radix_tree_t::alloc_page()
{
    if (likely(asan_alloc_ptr < countof(asan_pool))) {
        size_t offset = atomic_xadd(&asan_alloc_ptr, PAGE_SIZE);
        if (likely(offset < countof(asan_pool)))
            return asan_pool + offset;
    }
    return nullptr;
}
