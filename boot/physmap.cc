#include "physmap.h"
#include "physmem.h"
#include "assert.h"
#include "likely.h"
#include "halt.h"
#include "malloc.h"
#include "string.h"
#include "ctors.h"
#include "screen.h"

#define DEBUG_PHYSMAP 0

static physmem_range_t *physalloc_ranges;
static uint32_t physalloc_count;
static uint32_t physalloc_capacity;

// Keep track of the first index in the map for 20 and 64 bit entries
static uint32_t physalloc_20bit_st;
static uint32_t physalloc_64bit_st;

// Keep track of the first free block in 20 bit range
static uint32_t physalloc_20bit_1st;

// Special block for taking pages from the top of 32 bit range
static uint32_t physalloc_20bit_pt;

static uint64_t physmap_top;

static void physmap_realloc(uint32_t capacity_hint);
static uint32_t physmap_replace(uint32_t index, physmem_range_t const& entry);
static void physmap_delete(uint32_t index);

static bool physmap_init()
{
    physalloc_20bit_pt = -1;

    if (!get_ram_regions())
        return false;

    // Perform fixups
    bool did_something;
    physmem_range_t entry1{}, entry2{};

    do {
        size_t count = 0;
        physmem_range_t *ranges = physmap_get(&count);

        for (size_t i = 1; i < count && !did_something; ++i) {
            if (!ranges[i].valid)
                continue;

            uint64_t top = ranges[i].base + ranges[i].size;
            if (physmap_top < top)
                physmap_top = top;
        }

        did_something = false;

        for (size_t i = 1; i < count && !did_something; ++i) {
            physmem_range_t & prev = ranges[i - 1];
            physmem_range_t & curr = ranges[i];

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
                    assert(curr.base >= prev.base);
                    entry1.size = curr.base - prev.base;
                    entry1.type = prev.type;
                    entry1.valid = 1;

                    entry2.base = curr_end;
                    assert(prev_end >= curr_end);
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

            if (prev_normal) {
                if (prev.base & PAGE_MASK) {
                    uint64_t new_base = (prev.base + PAGE_SIZE) & -PAGE_SIZE;
                    int64_t adj = new_base - prev.base;
                    prev.base += adj;
                    assert(int64_t(prev.size) >= adj);
                    prev.size -= adj;
                }
                prev.size &= -PAGE_SIZE;
            }

            if (curr_normal) {
                if (curr.base & PAGE_MASK) {
                    uint64_t new_base = (curr.base + PAGE_SIZE) & -PAGE_SIZE;
                    int64_t adj = new_base - curr.base;
                    curr.base += adj;
                    assert(int64_t(curr.size) >= adj);
                    curr.size -= adj;
                }
                curr.size &= -PAGE_SIZE;
            }

            if (prev.size == 0) {
                physmap_delete(i - 1);
                did_something = true;
                break;
            }

            if (curr.size == 0) {
                physmap_delete(i);
                did_something = true;
                break;
            }

            // Combine adjacent free normal ranges
            if (prev_normal && curr_normal && prev_end == curr.base) {
                entry1.base = prev.base;
                entry1.size = curr_end - prev.base;
                entry1.type = prev.type;
                entry1.valid = 1;

                physmap_delete(i);
                physmap_replace(i - 1, entry1);

                did_something = true;
                break;
            }
        }
    } while (did_something);

    return true;
}

_constructor((ctor_physmem)) void physmap_startup()
{
    if (unlikely(!physmap_init()))
        PANIC("Could not initialize physical memory map");
}

physmem_range_t *physmap_get(size_t *ret_count)
{
    *ret_count = physalloc_count;
    return physalloc_ranges;
}

static void physmap_realloc(uint32_t capacity_hint)
{
    if (capacity_hint < 16)
        capacity_hint = 16;

    physalloc_ranges = (physmem_range_t*)realloc(
                physalloc_ranges, sizeof(*physalloc_ranges) * capacity_hint);

    if (unlikely(!physalloc_ranges))
        PANIC("Could not allocate memory for physical allocation map");

    physalloc_capacity = capacity_hint;
}

// Double the capacity of the physical memory map
// Panic on insufficient memory
static void physmap_grow()
{
    physmap_realloc(physalloc_capacity * 2);
}

static void physmap_delete(uint32_t index)
{
    assert(index < physalloc_count);

    physalloc_64bit_st -= (physalloc_64bit_st < index);
    physalloc_20bit_st -= (physalloc_20bit_st < index);
    physalloc_20bit_pt -= (physalloc_20bit_pt < index);

    memmove(physalloc_ranges + index, physalloc_ranges + index + 1,
            sizeof(*physalloc_ranges) * --physalloc_count - index);
}

static uint32_t physmap_replace(uint32_t index, physmem_range_t const& entry)
{
    physmap_delete(index);
    return physmap_insert(entry);
}

static int physmap_insert_at(uint32_t index, physmem_range_t const& entry)
{
    assert(index <= physalloc_count);

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

_pure
static int physmap_find_insertion_point(uint64_t base)
{
    int st = 0;
    int en = physalloc_count;

    if (!physalloc_count || physalloc_ranges[physalloc_count-1].base +
            physalloc_ranges[physalloc_count-1].size <= base) {
        // Fastpath insertion at the end
        st = physalloc_count;
    } else {
        // Binary search for insertion point
        while (st < en) {
            int mid = ((en - st) >> 1) + st;
            physmem_range_t const& candidate = physalloc_ranges[mid];

            if (candidate.base <= base)
                st = mid + 1;
            else
                en = mid;
        }
    }

    return st;
}

// Sorted insertion into physical map, sort by base
int physmap_insert(physmem_range_t const& entry)
{
    int index = physmap_find_insertion_point(entry.base);
    return physmap_insert_at(index, entry);
}

#if DEBUG_PHYSMAP
static char const *physmap_types[] = {
    "<zero?>",
    "NORMAL",
    "UNUSABLE",
    "RECLAIMABLE",
    "NVS",
    "BAD",

    // Custom types
    "ALLOCATED",
    "BOOTLOADER"
};

void physmap_dump()
{
    PRINT("--- physmap dump:");
    for (int i = 0; i < physalloc_count; ++i) {
        physmem_range_t &ent = physalloc_ranges[i];
        PRINT("base=%" PRIx64", size=%" PRIx64 ", type=%s",
              ent.base, ent.size, ent.type < countof(physmap_types)
              ? physmap_types[ent.type] : "<invalid>");
    }
    PRINT("---");
}
#endif

phys_alloc_t alloc_phys(uint64_t size)
{
#if DEBUG_PHYSMAP
    physmap_dump();
#endif

    phys_alloc_t result;

    if (unlikely(physalloc_20bit_1st == 0))
        physalloc_20bit_1st = physalloc_20bit_st;

    size = (size + (PAGE_SIZE - 1)) & -PAGE_SIZE;

    for (uint32_t i = physalloc_20bit_1st; i < physalloc_64bit_st; ++i) {
        physmem_range_t &entry1 = physalloc_ranges[i];

        if (entry1.type == PHYSMEM_TYPE_NORMAL) {
            result.base = entry1.base;

            if (i > 0) {
                // Peek at previous entry
                physmem_range_t &prev = physalloc_ranges[i - 1];

                // If previous entry is allocated and reaches all the way
                // up to the base of this entry...
                if (prev.type == PHYSMEM_TYPE_ALLOCATED &&
                        prev.base + prev.size == entry1.base) {
                    // ...then we can just adjust the ranges

                    if (size < entry1.size) {
                        // Expand previous range to cover allocated range
                        prev.size += size;
                        // Move the base of the free range forward
                        entry1.base += size;
                        // Reduce the size of the free range
                        entry1.size -= size;

                        result.size = size;
                    } else {
                        // We have consumed this entire free range, delete it
                        result.size = entry1.size;
                        prev.size += entry1.size;
                        physmap_delete(i);
                    }

                    break;
                }
            }

            if (entry1.size > size) {
                // Take some, create new free range after it
                result.size = size;

                physmem_range_t entry2{};
                entry2.base = entry1.base + size;
                entry2.size = entry1.size - size;
                entry2.type = PHYSMEM_TYPE_NORMAL;
                entry2.valid = 1;

                entry1.size = size;
                entry1.type = PHYSMEM_TYPE_ALLOCATED;

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
    assert(result.base < INT64_C(0x100000000));
    //assert(result.size == size);

    take_pages(result.base, result.size);

    return result;
}

int physmap_take_range(uint64_t base, uint64_t size, uint32_t type)
{
    uint64_t end = base + size;

    uint32_t index = physmap_find_insertion_point(base);

    // Bulldoze all entries that end at or before the end of taken range
    while (index < physalloc_count &&
           physalloc_ranges[index].base + physalloc_ranges[index].size <= end)
        physmap_delete(index);

    // If the range overlaps following range,
    // clip space off the beginning of it
    if (physalloc_ranges[index].base < end) {
        physalloc_ranges[index].size = physalloc_ranges[index].base +
                physalloc_ranges[index].size - end;
        physalloc_ranges[index].base = end;
    }

    physmem_range_t entry{};

    if (index > 0) {
        // Adjust previous entry to avoid taken range
        physmem_range_t &prev = physalloc_ranges[index - 1];

        uint64_t prev_end = prev.base + prev.size;


        if (prev_end < end) {
            // Clip space off the end of previous range and insert new range

            prev.size = base - prev.base;

            // If new range begins at the same place as the previous range
            // replace previous range
            if (prev.size == 0) {
                physmap_delete(index - 1);
                --index;
            }

            entry.base = base;
            entry.size = size;
            entry.type = type;
            entry.valid = 1;

            return physmap_insert_at(index, entry);
        } else {
            // Insert range in the middle of existing range

            prev.size = base - prev.base;

            if (prev.size == 0) {
                physmap_delete(index - 1);
                --index;
            }

            entry.base = end;
            entry.size = end - prev_end;
            entry.type = prev.type;
            entry.valid = 1;

            if (entry.size)
                physmap_insert_at(index, entry);

            entry.base = base;
            entry.size = size;
            entry.type = type;
            entry.valid = 1;
            return physmap_insert_at(index, entry);
        }
    }

    // All overlaps resolved, just insert it
    entry.base = base;
    entry.size = size;
    entry.type = type;
    entry.valid = 1;
    return physmap_insert_at(index, entry);
}

uint64_t physmap_top_addr()
{
    return physmap_top;
}
