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
    typedef std::unique_lock<std::spinlock> scoped_lock_t;
    std::spinlock alloc_lock;

    // Bit is set if corresponding level1 entry is full
    int64_t level0;

    // Bit is set if descriptor value is used
    int64_t level1[64];
};
