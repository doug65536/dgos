#pragma once
#include "types.h"
#include "mm.h"

#define _asan_optimize

// Compactly represent up to 128TB sparse array
class radix_tree_t {
public:
    _no_asan _asan_optimize
    void *lookup(uint64_t addr, bool commit_pages);

    _no_asan _asan_optimize
    void fill(uint64_t start, uint8_t value, uint64_t len);

    _no_asan _asan_optimize
    bool is_filled_with(uint64_t start, uint8_t value, uint64_t len);

private:
    _no_asan _asan_optimize
    static void *alloc_page();

    static uint8_t asan_pool[4096*1024];
    static size_t asan_alloc_ptr;

    template<typename T>
    _no_asan _asan_optimize
    void *commit(T &p) {
        p = (T)alloc_page();
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
