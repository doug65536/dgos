#pragma once
#include "types.h"
#include "bitsearch.h"
#include "mutex.h"

class desc_alloc_t {
public:
    int alloc();
    void free(int fd);
    bool take(int fd);

private:
    using scoped_lock_t = std::unique_lock<ext::spinlock>;
    ext::spinlock alloc_lock;

    // Bit is set if corresponding level1 entry is full
    int64_t level0;

    // Bit is set if descriptor value is used
    int64_t level1[64];
};
