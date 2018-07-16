#include "types.h"
#include "likely.h"
#include "printk.h"
#include "string.h"
#include "assert.h"

#define PAGE_SIZE_BIT   12
#define PAGE_SIZE       (size_t(1) << PAGE_SIZE_BIT)
#define PAGE_MASK       (PAGE_SIZE - 1)

using vaddr_t = uintptr_t;

// Page pool for very early accesses
static uint8_t asan_pool[4096*1024];
static size_t asan_alloc_ptr;

bool asan_ready;

static void *asan_alloc_page()
{
    if (likely(asan_alloc_ptr < countof(asan_pool))) {
        void *result = asan_pool + asan_alloc_ptr;
        asan_alloc_ptr += 4096;
        return result;
    }

    return nullptr;
}

// Compactly represent up to 128TB sparse array
class radix_tree_t {
public:
    _no_asan __attribute__((optimize("-O2")))
    void *lookup(uint64_t addr, bool commit_pages)
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

    _no_asan //__attribute__((optimize("-O2")))
    void fill(uint64_t start, uint8_t value, uint64_t len)
    {
        while (len) {
            uint64_t page_end = (start + PAGE_SIZE) & -PAGE_SIZE;
            uint64_t fill = page_end - start;
            if (fill > len)
                fill = len;
            uint8_t *p = (uint8_t*)lookup(start, true);
            for (size_t i = 0; i < fill; ++i)
                p[i] = value;

            start += fill;
            len -= fill;
        }
    }

    _no_asan __attribute__((optimize("-O2")))
    bool is_filled_with(uint64_t start, uint8_t value, uint64_t len)
    {
        while (len) {
            uint64_t page_end = (start + PAGE_SIZE) & -PAGE_SIZE;
            uint64_t fill = page_end - start;
            if (fill > len)
                fill = len;
            uint8_t *p = (uint8_t*)lookup(start, true);

            for (size_t i = 0; i < fill; ++i) {
                if (p[i] != value)
                    return false;
            }

            start += fill;
            len -= fill;
        }

        return true;
    }

private:
    template<typename T>
    _no_asan void *commit(T &p) {
        p = (T)asan_alloc_page();
        return p;
    }

    static constexpr int log2_radix_slots = 9;
    static constexpr size_t radix_slots = size_t(1) << log2_radix_slots;

    // Points to a single page. 4-level radix tree.
    // Works the same as 4-level x86_64 paging structure without the flag bits
    // At each level of indirection, the pointer points to a page of 512
    // entries for progressively less significant groups of 9 bits
    // of the key
    // Handles key range from 0 to 256TB-1
    void *****radix_tree;
};

static radix_tree_t asan_shadow;

_no_asan
static void asan_error(vaddr_t addr, size_t size)
{
    if (!asan_ready)
        return;

    printdbg("Accessed uninitialized %zd-byte value at %#zx\n", size, addr);
    assert(!"ASAN error");
}

extern "C" _no_asan void __asan_load1_noabort(vaddr_t addr)
{
    addr &= 0xFFFFFFFFFFFF;
    uint8_t *byte = (uint8_t*)asan_shadow.lookup(addr >> 3, false);
    uint8_t mask = 1 << (addr & 7);
    if (!byte || !(*byte & mask))
        asan_error(addr, 1);
}

extern "C" _no_asan void __asan_load2_noabort(vaddr_t addr)
{
    if (likely(((addr >> 3) & 0xFFF) < 0xFFF)) {
        addr &= 0xFFFFFFFFFFFF;
        uint16_t *word = (uint16_t*)asan_shadow.lookup(addr >> 3, false);
        uint16_t mask = 3 << (addr & 7);
        if (!word || (*word & mask) != mask)
            asan_error(addr, 2);
    } else {
        __asan_load1_noabort(addr);
        __asan_load1_noabort(addr+1);
    }
}

extern "C" _no_asan void __asan_load4_noabort(vaddr_t addr)
{
    if (likely(((addr >> 3) & 0xFFF) < 0xFFF)) {
        addr &= 0xFFFFFFFFFFFF;
        uint16_t *word = (uint16_t*)asan_shadow.lookup(addr >> 3, false);
        uint16_t mask = 0xF << (addr & 7);
        if (!word || (*word & mask) != mask)
            asan_error(addr, 4);
    } else {
        __asan_load1_noabort(addr);
        __asan_load1_noabort(addr+1);
        __asan_load1_noabort(addr+2);
        __asan_load1_noabort(addr+3);
    }
}

extern "C" _no_asan void __asan_load8_noabort(vaddr_t addr)
{
    if (likely(((addr >> 3) & 0xFFF) < 0xFFF)) {
        addr &= 0xFFFFFFFFFFFF;
        uint16_t *word = (uint16_t*)asan_shadow.lookup(addr >> 3, false);
        uint16_t mask = 0xFF << (addr & 7);
        if (!word || (*word & mask) != mask)
            asan_error(addr, 8);
    } else {
        for (size_t i = 0; i < 8; ++i)
            __asan_load1_noabort(addr + i);
    }
}

extern "C" _no_asan void __asan_load16_noabort(vaddr_t addr)
{
    if (likely(((addr >> 3) & 0xFFF) < 0xFFD)) {
        addr &= 0xFFFFFFFFFFFF;
        uint32_t *word = (uint32_t*)asan_shadow.lookup(addr >> 3, false);
        uint32_t mask = 0xFFFF << (addr & 7);
        if (!word || (*word & mask) != mask)
            asan_error(addr, 16);
    } else {
        for (size_t i = 0; i < 16; ++i)
            __asan_load1_noabort(addr + i);
    }
}

extern "C" _no_asan void __asan_loadN_noabort(vaddr_t addr, size_t size)
{
    addr &= 0xFFFFFFFFFFFF;

    // Get start aligned on an 8 byte boundary
    while ((addr & 7) && size) {
        __asan_load1_noabort(addr);
        ++addr;
        --size;
    }

    if (!size)
        return;

    uint64_t start_fill = addr >> 3;
    uint64_t end_fill = (addr + size) >> 3;
    uint64_t len_fill = end_fill - start_fill;

    if (!asan_shadow.is_filled_with(start_fill, 0xFF, len_fill))
        asan_error(addr, size);

    start_fill += len_fill;
    addr += len_fill << 3;
    size -= len_fill << 3;

    while (size) {
        __asan_load1_noabort(addr);
        ++addr;
        --size;
    }
}

extern "C" _no_asan void __asan_store1_noabort(vaddr_t addr)
{
    addr &= 0xFFFFFFFFFFFF;
    uint8_t *byte = (uint8_t*)asan_shadow.lookup(addr >> 3, true);
    uint8_t mask = 1 << (addr & 7);
    *byte |= mask;
}

extern "C" _no_asan void __asan_store2_noabort(vaddr_t addr)
{
    if (likely(((addr >> 3) & 0xFFF) < 0xFFF)) {
        addr &= 0xFFFFFFFFFFFF;
        uint16_t *word = (uint16_t*)asan_shadow.lookup(addr >> 3, true);
        uint16_t mask = 3 << (addr & 7);
        *word |= mask;
    } else {
        __asan_store1_noabort(addr);
        __asan_store1_noabort(addr+1);
    }
}

extern "C" _no_asan void __asan_store4_noabort(vaddr_t addr)
{
    if (likely(((addr >> 3) & 0xFFF) < 0xFFF)) {
        addr &= 0xFFFFFFFFFFFF;
        uint16_t *word = (uint16_t*)asan_shadow.lookup(addr >> 3, true);
        uint16_t mask = 0xF << (addr & 7);
        *word |= mask;
    } else {
        __asan_store1_noabort(addr);
        __asan_store1_noabort(addr+1);
        __asan_store1_noabort(addr+2);
        __asan_store1_noabort(addr+3);
    }
}

extern "C" _no_asan void __asan_store8_noabort(vaddr_t addr)
{
    if (likely(((addr >> 3) & 0xFFF) < 0xFFF)) {
        addr &= 0xFFFFFFFFFFFF;
        uint16_t *word = (uint16_t*)asan_shadow.lookup(addr >> 3, true);
        uint16_t mask = 0xFF << (addr & 7);
        *word |= mask;
    } else {
        for (size_t i = 0; i < 8; ++i)
            __asan_store1_noabort(addr+i);
    }
}

extern "C" _no_asan void __asan_store16_noabort(vaddr_t addr)
{
    if (likely(((addr >> 3) & 0xFFF) < 0xFFD)) {
        addr &= 0xFFFFFFFFFFFF;
        uint32_t *dword = (uint32_t*)asan_shadow.lookup(addr >> 3, true);
        uint32_t mask = 0xFFFF << (addr & 7);
        *dword |= mask;
    } else {
        for (size_t i = 0; i < 16; ++i)
            __asan_store1_noabort(addr+i);
    }
}

extern "C" _no_asan void __asan_storeN_noabort(vaddr_t addr, size_t size)
{
    addr &= 0xFFFFFFFFFFFF;

    // Get start aligned on an 8 byte boundary
    while ((addr & 7) && size) {
        __asan_store1_noabort(addr);
        ++addr;
        --size;
    }

    if (!size)
        return;

    uint64_t start_fill = addr >> 3;
    uint64_t end_fill = (addr + size) >> 3;
    uint64_t len_fill = end_fill - start_fill;

    asan_shadow.fill(start_fill, 0xFF, len_fill);

    start_fill += len_fill;
    addr += len_fill << 3;
    size -= len_fill << 3;

    while (size) {
        __asan_store1_noabort(addr);
        ++addr;
        --size;
    }
}

// Called before dynamic initializers of a single module run
extern "C" _no_asan void __asan_before_dynamic_init(char const *module_name)
{
}

// Called after dynamic initializers of a single module run
extern "C" _no_asan void __asan_after_dynamic_init()
{
}

// Performs cleanup before a NoReturn function. Must be called before things
// like _exit and execl to avoid false positives on stack.
extern "C" _no_asan void __asan_handle_no_return()
{
}
