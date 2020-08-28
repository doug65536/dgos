#include "unittest.h"
#include "basic_set.h"

#include "utility.h"

// Top level address space


class block_data_t : public refcounted<block_data_t> {
public:
    virtual ~block_data_t() = 0;

    size_t size() const;

    // Cut this off at md and return a clone of this with remainder at md
    block_data_t *split(uintptr_t md);

    // Clip space off the start of the range
    virtual bool clip_st(uintptr_t new_st) = 0;

    // Clip space off the end of the range
    virtual bool clip_en(uintptr_t new_en) = 0;

    uintptr_t st;
    uintptr_t en;
};

class block_entry_t {
public:
    block_entry_t(block_data_t *ptr)
        : ptr(ptr)
    {
    }

    bool operator<(block_entry_t const& rhs) const
    {
        return ptr->st < rhs.ptr->st;
    }

    refptr<block_data_t> ptr;
};

class free_entry_t {
public:
    free_entry_t() = default;
    free_entry_t(uintptr_t sz, uintptr_t st)
        : sz(sz)
        , st(st)
    {
    }

    bool operator<(free_entry_t const& rhs) const
    {
        // First sort by size, then by base
        return sz < rhs.sz ? true :
               sz > rhs.sz ? false :
               st < rhs.st;
    }

    uintptr_t sz = 0;
    uintptr_t st = 0;
};

class range_alloc_t
{
public:
    range_alloc_t() = default;

    // Find the lowest free region that is large enough and take that range
    // Returns reserved virtual address space pointer
    uintptr_t alloc_free(size_t sz);

    // Remove
    void free(uintptr_t base, size_t sz);

    // Force a specific range
    void take(uintptr_t base, uintptr_t sz, block_data_t *block);

private:
    ext::set<block_entry_t> blks;
    ext::set<free_entry_t> freeblks;
};

size_t block_data_t::size() const
{
    return en - st;
}

block_data_t *block_data_t::split(uintptr_t md)
{
    assert(st < md && en > md);
    return nullptr;
}

