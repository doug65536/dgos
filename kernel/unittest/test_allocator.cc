#include "unittest.h"
#include "rbtree.h"

class block_data_t : public refcounted<block_data_t> {
public:
    virtual ~block_data_t() = 0;

    size_t size() const;

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

    uintptr_t alloc_free(size_t sz);
    void free(uintptr_t base, size_t sz);
    void take(uintptr_t base, block_data_t *block);

private:
    std::set<block_entry_t> blks;
    std::set<free_entry_t> freeblks;
};
