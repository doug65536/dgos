#include "physmap.h"
#include "physmem.h"
#include "assert.h"
#include "likely.h"
#include "halt.h"
#include "malloc.h"
#include "string.h"
#include "ctors.h"

static physmem_range_t *physalloc_ranges;
static int physalloc_count;
static int physalloc_capacity;

// Keep track of the first index in the map for 20 and 64 bit entries
static int physalloc_20bit_st;
static int physalloc_64bit_st;
static int physalloc_20bit_1st;

// Special block for taking pages from the top of 32 bit range
static void physmap_realloc(int capacity_hint);
static int physmap_replace(int index, physmem_range_t const& entry);
static void physmap_delete(int index);

static bool physmap_init()
{
    if (!get_ram_regions())
        return false;

    // Perform fixups
    bool did_something;
    physmem_range_t entry1{}, entry2{};

    do {
        int count = 0;
        physmem_range_t const *ranges = physmap_get(&count);

        did_something = false;

        for (int i = 1; i < count && !did_something; ++i) {
            physmem_range_t const& prev = ranges[i - 1];
            physmem_range_t const& curr = ranges[i];

            uint64_t prev_end = prev.base + prev.size;
            uint64_t curr_end = curr.base + curr.size;

            bool prev_normal = prev.type == PHYSMEM_TYPE_NORMAL;
            bool curr_normal = curr.type == PHYSMEM_TYPE_NORMAL;

            if (curr.base >= prev.base && curr_end <= prev_end) {
                // Current entry lies entirely within previous! Seriously?
                if (curr_normal) {
                    // Just delete current
                    physmap_delete(i);
                } else {
                    // Clip current out of previous
                    entry1.base = prev.base;
                    entry1.size = curr.base - prev.base;
                    entry1.type = prev.type;
                    entry1.valid = 1;

                    entry2.base = curr_end;
                    entry2.size = prev_end - curr_end;
                    entry2.type = prev.type;
                    entry2.valid = 1;

                    physmap_delete(i);
                    physmap_delete(i - 1);

                    if (entry1.size)
                        physmap_insert(entry1);

                    if (entry2.size)
                        physmap_insert(entry2);
                }

                did_something = true;
                break;
            }

            // Resolve end of previous entry overlapping start of current entry
            if (prev_end > curr.base) {
                if (!prev_normal && curr_normal) {
                    // Previous entry wins, replace curr entry
                    entry1.base = prev_end;
                    entry1.size = curr_end - prev_end;
                    entry1.type = curr.type;
                    entry1.valid = 1;

                    physmap_replace(i, entry1);
                    did_something = true;
                    break;
                } else if (prev_normal && !curr_normal) {
                    // Current entry wins, replace prev entry
                    entry1.base = prev.base;
                    entry1.size = curr.base - prev.base;
                    entry1.type = prev.type;
                    entry1.valid = 1;

                    physmap_replace(i - 1, entry1);
                    did_something = true;
                    break;
                }

                // Don't know which one wins, don't care, neither is normal...
            }

            // Combine adjacent free normal ranges
            if (prev_normal && curr_normal && prev_end == curr.base) {
                entry1.base = prev.base;
                entry1.size = curr_end - prev.base;
                entry1.type = prev.type;
                entry1.valid = 1;

                physmap_delete(i);
                physmap_replace(i - 1, entry1);
            }
        }
    } while (did_something);

    return true;
}

__constructor((ctor_physmem)) void physmap_startup()
{
    if (!physmap_init())
        PANIC("Could not initialize physical memory map");
}

physmem_range_t *physmap_get(int *ret_count)
{
    *ret_count = physalloc_count;
    return physalloc_ranges;
}

static void physmap_realloc(int capacity_hint)
{
    if (capacity_hint < 16)
        capacity_hint = 16;
    physalloc_capacity = capacity_hint;
    physalloc_ranges = (physmem_range_t*)realloc(
                physalloc_ranges, sizeof(*physalloc_ranges) * capacity_hint);

    if (!physalloc_ranges)
        PANIC("Could not allocate memory for physical allocation map");
}

// Double the capacity of the physical memory map
// Panic on insufficient memory
static void physmap_grow()
{
    physmap_realloc(physalloc_capacity * 2);
}

static void physmap_delete(int index)
{
    assert(index >= 0);
    assert(index < physalloc_count);

    physmem_range_t const& entry = physalloc_ranges[index];

    physalloc_64bit_st -= (entry.base < 0x100000000);
    physalloc_20bit_st -= (entry.base < 0x100000);

    memmove(physalloc_ranges + index, physalloc_ranges + index + 1,
            sizeof(*physalloc_ranges) * --physalloc_count - index);
}

static int physmap_replace(int index, physmem_range_t const& entry)
{
    physmap_delete(index);
    return physmap_insert(entry);
}

static int physmap_insert_at(int index, physmem_range_t const& entry)
{
    // Grow the map if necessary
    if (unlikely(physalloc_count + 1 > physalloc_capacity))
        physmap_grow();

    // Insert the item
    memmove(physalloc_ranges + index + 1, physalloc_ranges + index,
            sizeof(*physalloc_ranges) * (physalloc_count - index));
    physalloc_ranges[index] = entry;
    ++physalloc_count;

    physalloc_20bit_st += (entry.base < 0x100000);
    physalloc_64bit_st += (entry.base < 0x100000000);

    return index;
}

// Sorted insertion into physical map, sort by base
int physmap_insert(physmem_range_t const& entry)
{
    int st = 0;
    int en = physalloc_count;

    if (physalloc_count && physalloc_ranges[physalloc_count-1].base +
            physalloc_ranges[physalloc_count-1].size <= entry.base) {
        // Fastpath insertion at the end
        st = physalloc_count;
    } else {
        // Binary search for insertion point
        while (st < en) {
            int mid = ((en - st) >> 1) + st;
            physmem_range_t const& candidate = physalloc_ranges[mid];

            if (candidate.base <= entry.base)
                st = mid + 1;
            else
                en = mid;
        }
    }

    return physmap_insert_at(st, entry);
}

phys_alloc_t alloc_phys(uint64_t size)
{
    phys_alloc_t result;

    if (unlikely(physalloc_20bit_1st == 0))
        physalloc_20bit_1st = physalloc_20bit_st;

    size = (size + 4095) & -4096;

    for (int i = physalloc_20bit_1st; i < physalloc_64bit_st; ++i) {
        physmem_range_t &entry1 = physalloc_ranges[i];

        if (entry1.type == PHYSMEM_TYPE_NORMAL) {
            result.base = entry1.base;

            if (entry1.size > size) {
                // Take some, create new free range after it
                result.size = size;

                physmem_range_t entry2{};
                entry2.base = entry1.base + size;
                entry2.size = entry1.size - size;
                entry2.type = PHYSMEM_TYPE_NORMAL;
                entry2.valid = 1;

                physmap_insert_at(i + 1, entry2);
            } else if (entry1.size <= size) {
                result.size = entry1.size;
                entry1.type = PHYSMEM_TYPE_ALLOCATED;
            }

            physalloc_20bit_1st = i + 1;
            break;
        }
    }

    assert((result.base & 0xFFF) == 0);
    assert(result.base >= 0x100000);
    assert(result.base < 0x100000000);
    assert(result.size == size);

    take_pages(result.base, result.size);

    return result;
}
