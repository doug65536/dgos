#ifdef _ASAN_ENABLED
#include "types.h"
#include "likely.h"
#include "printk.h"
#include "string.h"
#include "assert.h"
#include "radix_tree.h"
#include "main.h"

using vaddr_t = uintptr_t;

//#define _asan_optimize _always_optimize
#define _asan_optimize

bool asan_ready;

static radix_tree_t asan_shadow;

_no_asan
static constexpr vaddr_t asan_canonical(vaddr_t addr)
{
    return vaddr_t(intptr_t(addr << 16) >> 16);
}

_no_asan
static void asan_error(vaddr_t addr, size_t size)
{
    if (!asan_ready)
        return;

    // Ignore physical mapping errors
    if (addr >= (kernel_params->phys_mapping & 0xFFFFFFFFFFFF) &&
            addr < (kernel_params->phys_mapping & 0xFFFFFFFFFFFF) +
            kernel_params->phys_mapping_sz)
        return;

    printdbg("Accessed uninitialized %zd-byte value at %#zx\n",
             size, asan_canonical(addr));
    assert(!"ASAN error");
}


_no_asan
static void asan_oom()
{
    printdbg("asan out of memory, maybe increase ASAN_POOL_PAGE_COUNT\n");
    assert(!"ASAN OOM");
}

extern "C" _no_asan _asan_optimize
void __asan_load1_noabort(vaddr_t addr)
{
    addr &= 0xFFFFFFFFFFFF;
    uint8_t *word = (uint8_t*)asan_shadow.lookup(addr >> 3, false);
    uint8_t mask = 1 << (addr & 7);
    if (unlikely(!word || !(*word & mask)))
        asan_error(addr, 1);
}

extern "C" _no_asan _asan_optimize
void __asan_load2_noabort(vaddr_t addr)
{
    if (likely((addr & 1) == 0)) {
        addr &= 0xFFFFFFFFFFFF;
        uint8_t *word = (uint8_t*)asan_shadow.lookup(addr >> 3, false);
        uint8_t mask = 0x3U << (addr & 7);
        if (unlikely(!word || (*word & mask) != mask))
            asan_error(addr, 2);
    } else {
        __asan_load1_noabort(addr);
        __asan_load1_noabort(addr+1);
    }
}

extern "C" _no_asan _asan_optimize
void __asan_load4_noabort(vaddr_t addr)
{
    if (likely((addr & 3) == 0)) {
        addr &= 0xFFFFFFFFFFFF;
        uint8_t *word = (uint8_t*)asan_shadow.lookup(addr >> 3, false);
        uint8_t mask = 0xFU << (addr & 4);
        if (unlikely(!word || (*word & mask) != mask))
            asan_error(addr, 4);
    } else {
        __asan_load2_noabort(addr);
        __asan_load2_noabort(addr+2);
    }
}

extern "C" _no_asan _asan_optimize
void __asan_load8_noabort(vaddr_t addr)
{
    if (likely((addr & 7) == 0)) {
        addr &= 0xFFFFFFFFFFFF;
        uint8_t *word = (uint8_t*)asan_shadow.lookup(addr >> 3, false);
        if (unlikely(!word || *word != 0xFF))
            asan_error(addr, 8);
    } else {
        __asan_load4_noabort(addr);
        __asan_load4_noabort(addr+4);
    }
}

extern "C" _no_asan _asan_optimize
void __asan_load16_noabort(vaddr_t addr)
{
    if (likely((addr & 15) == 0)) {
        addr &= 0xFFFFFFFFFFFF;
        uint16_t *word = (uint16_t*)asan_shadow.lookup(addr >> 3, false);
        if (unlikely(!word || *word != 0xFFFF))
            asan_error(addr, 16);
    } else {
        __asan_load8_noabort(addr);
        __asan_load8_noabort(addr+8);
    }
}

extern "C" _no_asan _asan_optimize
void __asan_loadN_noabort(vaddr_t addr, size_t size)
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

    if (unlikely(!asan_shadow.is_filled_with(start_fill, 0xFF, len_fill)))
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

extern "C" _no_asan _asan_optimize
void __asan_store1_noabort(vaddr_t addr)
{
    addr &= 0xFFFFFFFFFFFF;
    uint8_t *word = (uint8_t*)asan_shadow.lookup(addr >> 3, true);
    uint8_t mask = 1 << (addr & 7);
    atomic_or(word, mask);
}

extern "C" _no_asan _asan_optimize
void __asan_store2_noabort(vaddr_t addr)
{
    if (likely((addr & 1) == 0)) {
        addr &= 0xFFFFFFFFFFFF;
        uint8_t *word = (uint8_t*)asan_shadow.lookup(addr >> 3, true);
        uint8_t mask = 0x3U << (addr & 6);
        atomic_or(word, mask);
    } else {
        __asan_store1_noabort(addr);
        __asan_store1_noabort(addr+1);
    }
}

extern "C" _no_asan _asan_optimize
void __asan_store4_noabort(vaddr_t addr)
{
    if (likely((addr & 3) == 0)) {
        addr &= 0xFFFFFFFFFFFF;
        uint8_t *word = (uint8_t*)asan_shadow.lookup(addr >> 3, true);
        uint8_t mask = 0xF << (addr & 4);
        atomic_or(word, mask);
    } else {
        __asan_store1_noabort(addr);
        __asan_store1_noabort(addr+1);
        __asan_store1_noabort(addr+2);
        __asan_store1_noabort(addr+3);
    }
}

extern "C" _no_asan _asan_optimize
void __asan_store8_noabort(vaddr_t addr)
{
    if (likely((addr & 15) == 0)) {
        addr &= 0xFFFFFFFFFFFF;
        uint8_t *word = (uint8_t*)asan_shadow.lookup(addr >> 3, true);
        *word = 0xFF;
    } else {
        __asan_store4_noabort(addr);
        __asan_store4_noabort(addr+4);
    }
}

extern "C" _no_asan _asan_optimize
void __asan_store16_noabort(vaddr_t addr)
{
    if (likely((addr & 15) == 0)) {
        addr &= 0xFFFFFFFFFFFF;
        uint16_t *word = (uint16_t*)asan_shadow.lookup(addr >> 3, true);
        *word = 0xFFFF;
    } else {
        __asan_store8_noabort(addr);
        __asan_store8_noabort(addr+8);
    }
}

extern "C" _no_asan _asan_optimize
void __asan_storeN_noabort(vaddr_t addr, size_t size)
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

    if (unlikely(!asan_shadow.fill(start_fill, 0xFF, len_fill)))
        asan_oom();

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
extern "C" _no_asan
void __asan_before_dynamic_init(char const *module_name)
{
}

// Called after dynamic initializers of a single module run
extern "C" _no_asan
void __asan_after_dynamic_init()
{
}

// Performs cleanup before a NoReturn function. Must be called before things
// like _exit and execl to avoid false positives on stack.
extern "C" _no_asan
void __asan_handle_no_return()
{
}

extern "C" _no_asan _asan_optimize
void __asan_free1_noabort(vaddr_t addr)
{
    addr &= 0xFFFFFFFFFFFF;
    uint8_t *byte = (uint8_t*)asan_shadow.lookup(addr >> 3, true);
    uint8_t mask = 1 << (addr & 7);
    atomic_and(byte, ~mask);
}

extern "C"
void __asan_freeN_noabort(void const *m, size_t size)
{
    vaddr_t addr = vaddr_t(m);

    addr &= 0xFFFFFFFFFFFF;

    // Get start aligned on an 8 byte boundary
    while ((addr & 7) && size) {
        __asan_free1_noabort(addr);
        ++addr;
        --size;
    }

    if (!size)
        return;

    uint64_t start_fill = addr >> 3;
    uint64_t end_fill = (addr + size) >> 3;
    uint64_t len_fill = end_fill - start_fill;

    if (unlikely(!asan_shadow.fill(start_fill, 0, len_fill)))
        asan_oom();

    start_fill += len_fill;
    addr += len_fill << 3;
    size -= len_fill << 3;

    while (size) {
        __asan_free1_noabort(addr);
        ++addr;
        --size;
    }
}
#endif
