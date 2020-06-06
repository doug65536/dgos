#include "desc_alloc.h"
#include "likely.h"

int desc_alloc_t::alloc()
{
    scoped_lock_t lock(alloc_lock);

    // See if every one is used
    if (unlikely(level0 == ~int64_t(0)))
        return -1;

    // Look for first one that is not full
    uint8_t bitlvl0 = bit_lsb_set(~level0);

    int64_t& mask = level1[bitlvl0];

    // Should be impossible due to level0 value
    assert(mask != ~int64_t(0));

    // Find first available fd
    uint8_t bitlvl1 = bit_lsb_set(~mask);

    // Mark it taken
    mask |= uint64_t(1) << bitlvl1;

    // If this one became full clear bit in level0
    if (mask == ~int64_t(0))
        level0 &= ~(uint64_t(1) << bitlvl0);

    return (bitlvl0 << 6) + bitlvl1;
}

void desc_alloc_t::free(int fd)
{
    assert(fd >= 0 && fd < 4096);
    if (unlikely(fd < 0 || fd >= 4096))
        return;

    int bitlvl0 = fd >> 6;
    int bitlvl1 = fd & 63;

    scoped_lock_t lock(alloc_lock);

    int64_t& mask = level1[bitlvl0];

    uint64_t clr = uint64_t(1) << bitlvl1;

    assert(mask & clr);

    // Clear level 1 bit
    mask &= ~clr;

    // Level 0 entry is not full
    level0 &= ~(uint64_t(1) << bitlvl0);
}

bool desc_alloc_t::take(int fd)
{
    scoped_lock_t lock(alloc_lock);
    return take_locked(fd, lock);
}

bool desc_alloc_t::take_locked(int fd, scoped_lock_t& lock)
{
    assert(fd >= 0 && fd < 4096);
    if (unlikely(fd < 0 || fd >= 4096))
        return false;

    uint8_t bitlvl0 = fd >> 6;
    uint8_t bitlvl1 = fd & 63;

    int64_t& mask = level1[bitlvl0];

    uint64_t set = uint64_t(1) << bitlvl1;

    if (unlikely(mask & set))
        return false;

    // Mark it taken
    mask |= set;

    // If this one became full clear bit in level0
    if (unlikely(mask == ~int64_t(0)))
        level0 &= ~(uint64_t(1) << bitlvl0);

    return true;
}

bool desc_alloc_t::take(std::initializer_list<int> fds)
{
    scoped_lock_t lock(alloc_lock);
    bool result = true;
    for (auto it = fds.begin(), en = fds.end(); it != en; ++it)
        result &= take_locked(*it, lock);
    return result;
}
