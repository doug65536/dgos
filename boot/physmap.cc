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

static uint64_t physmap_top;

static void physmap_realloc(uint32_t capacity_hint);
static uint32_t physmap_replace(uint32_t index, physmem_range_t const& entry);
static void physmap_delete(uint32_t index);

static uint64_t physmap_check_freespace()
{
    uint64_t total = 0;
    for (size_t i = 0; i < physalloc_count; ++i)
        total += (physalloc_ranges[i].size &
                  -(physalloc_ranges[i].type == PHYSMEM_TYPE_NORMAL));
    return total;
}

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

ATTRIBUTE_FORMAT(1, 0)
void vphysmap_dump(tchar const *format, va_list ap)
{
    vprint_line(0x7, format, ap);
    for (size_t i = 0; i < physalloc_count; ++i) {
        physmem_range_t &ent = physalloc_ranges[i];
        PRINT("[%zu] base=%" PRIx64
              ", end=%" PRIx64
              ", size=%" PRIx64
              ", type=%s",
              i, ent.base, ent.base + ent.size, ent.size,
              ent.type < countof(physmap_types)
              ? physmap_types[ent.type] : "<invalid>");
    }
    PRINT("State"
          ": 1st_20=%" PRIu32
          ", p64=%" PRIu32
          "?",
          physalloc_20bit_1st, physalloc_64bit_st);

    PRINT("---");
}

ATTRIBUTE_FORMAT(1, 2)
void physmap_dump(tchar const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vphysmap_dump(format, ap);
    va_end(ap);
}

static bool physmap_init()
{
    if (unlikely(!get_ram_regions()))
        return false;

    size_t count = 0;
    physmem_range_t *ranges = physmap_get(&count);

    physmap_top = 0;
    for (size_t i = 0; i < count; ++i) {
        if (ranges[i].type != PHYSMEM_TYPE_NORMAL)
            continue;

        auto end = ranges[i].base + ranges[i].size;

        if (physmap_top < end)
            physmap_top = end;
    }

//    return true;

    // Perform fixups
    bool did_something;
    physmem_range_t entry1{}, entry2{};

    do {
        size_t count = 0;
        physmem_range_t *ranges = physmap_get(&count);

        uint64_t top = 0;

        if (count && ranges[0].valid)
            top = ranges[0].base + ranges[0].size;

        // Find the top of all valid ranges
        for (size_t i = 1; i < count; ++i) {
            if (unlikely(!ranges[i].valid))
                continue;

            top = ranges[i].base + ranges[i].size;
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

#if DEBUG_PHYSMAP
    physmap_dump(TSTR "After fixup");
#endif

    return true;
}

_constructor(ctor_physmem) static void physmap_startup()
{
    if (unlikely(!physmap_init()))
        PANIC("Could not initialize physical memory map");
}

physmem_range_t *physmap_get(size_t *ret_count)
{
    *ret_count = physalloc_count;
    return physalloc_ranges;
}

_noinline
static char const *physmap_validate_failed(char const *reason)
{
    cpu_debug_break();

    return reason;
}

char const *physmap_validate(bool fix)
{
    for (size_t i = 0; i < physalloc_count; ++i) {
        physmem_range_t const &range = physalloc_ranges[i];

        if (i > 0) {
            physmem_range_t &prev = physalloc_ranges[i - 1];

            if (unlikely(prev.base > range.base))
                return physmap_validate_failed("Blocks are not sorted");

            if (unlikely(prev.base == range.base))
                return physmap_validate_failed(
                            "Multiple blocks start at the same address");

            if (unlikely(prev.base + prev.size > range.base))
                return physmap_validate_failed("Block overlap");

            if (unlikely(prev.type == range.type &&
                         prev.base + prev.size == range.base)) {
                if (likely(!fix))
                    return physmap_validate_failed(
                                "Uncoalesced adjacent blocks of same type");

                uint64_t adjusted_size = range.base + range.size - prev.base;

                // Fix it
                PRINT("Coalescing adjacent memory ranges of same type"
                      " {%#" PRIx64 ",%#" PRIx64 "}"
                      " and"
                      " {%#" PRIx64 ",%#" PRIx64 "}"
                      " now"
                      " {%#" PRIx64 ",%#" PRIx64 "}",
                      prev.base, prev.size,
                      range.base, range.size,
                      prev.base, adjusted_size);

                prev.size = adjusted_size;

                physmap_delete(i);
                --i;

                continue;
            }
        }

        if (unlikely(physalloc_20bit_st > i && range.base >= 0x100000))
            return physmap_validate_failed("physalloc_20bit_st is wrong");

        if (unlikely(physalloc_64bit_st < i && range.base >= 0x100000000))
            return physmap_validate_failed("physalloc_64bit_st is wrong");

        if (unlikely((range.base & -PAGE_SIZE) != range.base))
            return physmap_validate_failed("Misaligned block base");

        if (unlikely((range.size & -PAGE_SIZE) != range.size))
            return physmap_validate_failed("Misaligning block size");
    }

    return nullptr;
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

    physalloc_64bit_st -= (index < physalloc_64bit_st);
    physalloc_20bit_1st -= (index < physalloc_20bit_st);
    physalloc_20bit_st -= (index < physalloc_20bit_st);

    --physalloc_count;

    memmove(physalloc_ranges + index, physalloc_ranges + (index + 1),
            sizeof(*physalloc_ranges) * (physalloc_count - index));
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
    if (unlikely(physalloc_count + 1 >= physalloc_capacity))
        physmap_grow();

    // Shift items forward, starting at the insertion point
    memmove(physalloc_ranges + index + 1, physalloc_ranges + index,
            sizeof(*physalloc_ranges) * (physalloc_count - index));

    physalloc_ranges[index] = entry;

    ++physalloc_count;

    physalloc_20bit_st += (entry.base < 0x100000);
    physalloc_20bit_1st += (entry.base < 0x100000);
    physalloc_64bit_st += (entry.base < 0x100000000);

    return index;
}

_pure
static uint32_t physmap_find_insertion_point(uint64_t base)
{
    uint32_t st = 0;
    uint32_t en = physalloc_count;

    if (!physalloc_count || physalloc_ranges[physalloc_count-1].base +
            physalloc_ranges[physalloc_count-1].size <= base) {
        // Fastpath insertion at the end
        st = physalloc_count;
    } else {
        // Binary search for insertion point
        while (st < en) {
            int mid = ((en - st) >> 1) + st;
            physmem_range_t const& candidate = physalloc_ranges[mid];

            st = (candidate.base < base) ? mid + 1 : st;
            en = (candidate.base < base) ? en : mid;
        }
    }

    return st;
}

// Sorted insertion into physical map, sort by base
int physmap_insert(physmem_range_t const& entry)
{
    uint32_t index = physmap_find_insertion_point(entry.base);
    return physmap_insert_at(index, entry);
}

__BEGIN_ANONYMOUS

class physmap_precedence
{
public:
    // Higher number has more precedence
    static unsigned precedence_from_type(uint8_t type)
    {
        return type < sizeof(instance.precedence)
                ? instance.precedence[type]
                  : instance.precedence[PHYSMEM_TYPE_BAD];
    }
private:
    uint8_t precedence[5];

    constexpr physmap_precedence()
    {
        lookup[PHYSMEM_TYPE_BAD] = 5;
        lookup[PHYSMEM_TYPE_UNUSABLE] = 4;
        lookup[PHYSMEM_TYPE_NVS] = 3;
        lookup[PHYSMEM_TYPE_RECLAIMABLE] = 2;
        lookup[PHYSMEM_TYPE_NORMAL] = 1;
    }

    uint8_t lookup[5];

    static physmap_precedence instance;
};

physmap_precedence physmap_precedence::instance;

__END_ANONYMOUS

static int physmap_precedence_from_type(unsigned type)
{
    return physmap_precedence::precedence_from_type(type);
}

// size of block,
// virtual address it will be placed at,
// insist true to only accept block whose size is the full amount,
// keep looking if insisting and current block is not large enough
// insist false if you can trivially handle getting less than you wanted
phys_alloc_t alloc_phys(uint64_t size, uint64_t for_addr, bool insist)
{
    uint64_t space_before = physmap_check_freespace();

    size = (size + (PAGE_SIZE - 1)) & -PAGE_SIZE;

#if DEBUG_PHYSMAP
    PRINT("-");
    physmap_dump(TSTR "alloc_phys allocating 0x%" PRIx64 " bytes", size);
#endif

    // AMD zen TLBs can hold a contiguous 16KB aligned 16KB region with the
    // same permission in a single TLB entry (4x efficiency)
    // Try to make the low 14 bits of the linear address and
    // physical address the same to leverage that capability

    phys_alloc_t result{};

    // A portion may be returned to the pool to provide
    // allocations that are aligned with the virtual address
    phys_alloc_t unwanted{};

    if (unlikely(physalloc_20bit_1st == 0))
        physalloc_20bit_1st = physalloc_20bit_st;

    uint32_t i;
    for (i = physalloc_20bit_1st; likely(i < physalloc_64bit_st); ++i) {
        physmem_range_t &entry1 = physalloc_ranges[i];

        if (likely(entry1.type == PHYSMEM_TYPE_NORMAL)) {
            // See how much you would have to advance the block base
            // physical address to make it 16KB aligned with
            // the linear address
            // Will be 0, 4096, 8192, or 12288
            // virt - phys
            // 0xxx - 3xxx = 01... : phys needs to be advanced 1000
            // 0xxx - 2xxx = 10... : phys needs to be advanced 2000
            // 0xxx - 1xxx = 11... : phys needs to be advanced 3000
            // 0xxx - 0xxx = 00... : already aligned
            unsigned realign = for_addr
                    ? (for_addr - entry1.base) & 0x3000
                    : 0;

            // Don't bother realigning if the block is smaller than 16KB
            realign &= -(size >= 0x4000);

            // Move on if this block is so small it can't even provide one
            // page after realignment
            if (unlikely(entry1.size <= realign))
                continue;

            // Increase size by amount needed to realign
            size += realign;

            result.base = entry1.base + realign;

            unwanted.base = entry1.base;
            unwanted.size = realign;

            if (likely(i > 0)) {
                // Peek at previous entry
                physmem_range_t &prev = physalloc_ranges[i - 1];

                // If previous entry is allocated and reaches all the way
                // up to the base of this entry...
                if (likely(prev.type == PHYSMEM_TYPE_ALLOCATED &&
                        prev.base + prev.size == entry1.base)) {
                    // ...then we can just adjust the ranges

                    if (likely(size < entry1.size)) {
                        // Falls through straight to here in most common case

                        // Expand previous range to cover allocated range
                        prev.size += size;
                        // Move the base of the free range forward
                        entry1.base += size;
                        // Reduce the size of the free range
                        entry1.size -= size;

                        result.size = size - unwanted.size;

                        break;
                    } else if (insist) {
                        // Block is not large enough, keep looking
                        continue;
                    } else {
                        // Consumed this entire free range, delete it
                        result.size = entry1.size - realign;
                        prev.size += entry1.size;
                        physmap_delete(i);

                        while (i < physalloc_count &&
                               prev.type ==
                               physalloc_ranges[i].type &&
                               prev.base + prev.size ==
                               physalloc_ranges[i].base) {
                            prev.size += physalloc_ranges[i].size;
                            physmap_delete(i);
                        }

                        break;
                    }
                }
            }

            if (entry1.size > size) {
                // Take some, create new free range after it
                result.size = size - unwanted.size;

                physmem_range_t entry2{};
                entry2.base = entry1.base + size;
                entry2.size = entry1.size - size;
                entry2.type = PHYSMEM_TYPE_NORMAL;
                entry2.valid = 1;

                entry1.size = size;
                entry1.type = PHYSMEM_TYPE_ALLOCATED;

                physmap_insert_at(i + 1, entry2);
            } else if (entry1.size <= size) {
                result.size = entry1.size - unwanted.size;
                entry1.type = PHYSMEM_TYPE_ALLOCATED;
            }

            physalloc_20bit_1st = i + 1;
            break;
        }
    }

    assert((result.base & 0xFFF) == 0);
    assert(result.base >= 0x100000);
    assert(result.base < INT64_C(0x100000000));

    physmap_validate();

    if (unwanted.size)
        free_phys(unwanted, i);

    if (likely(result.size))
        take_pages(result.base, result.size);

#if DEBUG_PHYSMAP
    physmap_dump(TSTR "alloc_phys returning"
                      " 0x%" PRIx64 " bytes at 0x%" PRIx64,
                 result.size, result.base);
#endif

    uint64_t space_taken = space_before - physmap_check_freespace();

    // Verify that the amount of free space changed the correct amount
    assert(space_taken == result.size);

    return result;
}

void free_phys(phys_alloc_t freed, size_t hint)
{
    uint64_t space_before = physmap_check_freespace();

    assert((freed.base & -PAGE_SIZE) == freed.base);
    freed.size = (freed.size + (PAGE_SIZE - 1)) & -PAGE_SIZE;

#if DEBUG_PHYSMAP
    PRINT("-");
    PRINT("Freeing 0x%" PRIx64 " from 0x%" PRIx64 " to 0x%" PRIx64,
          freed.size, freed.base, freed.base + freed.size);
#endif

    uint64_t freed_end = freed.base + freed.size;

    assert(freed.base >= 0x100000 && freed.base + freed.size <= 0x100000000);

    // Binary search to find insertion point above 1MB and below 4GB
    size_t st;
    size_t en;

    if (1 || hint == -1U) {
        st = physalloc_20bit_1st;
        en = physalloc_64bit_st;
        while (st < en) {
            size_t md = st + ((en - st) >> 1);
            physmem_range_t const& block = physalloc_ranges[md];

            st = (freed.base > block.base) ? md + 1 : st;
            en = (freed.base > block.base) ? en : md;
        }
    } else {
        st = hint;
    }

    // If we went one past it, bump back one
    if (st > 0 && physalloc_ranges[st].base > freed.base)
        --st;

    // See if we found the exact allocation
    if (st < physalloc_count &&
            physalloc_ranges[st].type == PHYSMEM_TYPE_ALLOCATED &&
            physalloc_ranges[st].base == freed.base &&
            physalloc_ranges[st].size == freed.size) {
        PRINT("Found exact allocated match in free_phys");

        // Delete the whole thing
        physmap_delete(st);
    } else if (st < physalloc_count &&
               physalloc_ranges[st].type == PHYSMEM_TYPE_ALLOCATED &&
               physalloc_ranges[st].base + physalloc_ranges[st].size ==
               freed_end) {
        PRINT("Taking the end off allocated range\n");

        // Reduce the size of the allocated block
        physalloc_ranges[st].size = freed.base - physalloc_ranges[st].base;
    } else if (st < physalloc_count &&
               physalloc_ranges[st].type == PHYSMEM_TYPE_ALLOCATED &&
               physalloc_ranges[st].base == freed.base) {
        PRINT("Taking the beginning off allocated range\n");

        // Move the start of the block forward and reduce its size
        physalloc_ranges[st].size -= freed.size;
        physalloc_ranges[st].base += freed.size;
    } else if (st < physalloc_count &&
               physalloc_ranges[st].type == PHYSMEM_TYPE_ALLOCATED &&
               physalloc_ranges[st].base < freed.base &&
               physalloc_ranges[st].base +
               physalloc_ranges[st].size > freed_end) {
        PRINT("Punch a hole in the middle of allocated block");

        physmem_range_t after;
        physmem_range_t &block = physalloc_ranges[st];

        // Compute the remaining piece after the freed region
        after.base = freed_end;
        after.size = block.base + block.size - freed_end;
        after.type = PHYSMEM_TYPE_ALLOCATED;
        after.valid = 1;

        // Reduce the size of the block before the freed region
        block.size = freed.base - block.base;

        // Insert the second half of the split block and advance current index
        physmap_insert_at(++st, after);
    } else {
        // It shouldn't be this hard, something is wrong
        PRINT("Uh oh, what is happening");
        assert(false);
    }

    // If sane, previous entry is before freed block
    assert(st <= 0 || physalloc_ranges[st-1].base < freed.base);

    uint64_t prev_en = st > 0 && st < physalloc_count
            ? physalloc_ranges[st-1].base + physalloc_ranges[st-1].size
            : 0;

    uint64_t next_st = st < physalloc_count
            ? physalloc_ranges[st].base
            : UINT64_MAX;

    // Only simple operations are allowed, no complex multiple block overlaps
    assert(prev_en <= freed.base);
    assert(next_st >= freed_end);

    bool adjacent_to_prev = st > 0 &&
            st < physalloc_count &&
            physalloc_ranges[st-1].type == PHYSMEM_TYPE_NORMAL &&
            prev_en == freed.base;

    bool adjacent_to_next = st < physalloc_count &&
            freed_end == next_st &&
            physalloc_ranges[st-1].type == PHYSMEM_TYPE_NORMAL;

    if (adjacent_to_prev && !adjacent_to_next) {
        // It coalesces only with previous block
        physalloc_ranges[st-1].size += freed.size;
    } else if (!adjacent_to_prev && adjacent_to_next) {
        // It coalesces only with next block
        physalloc_ranges[st].base -= freed.size;
        physalloc_ranges[st].size += freed.size;
    } else if (adjacent_to_prev && adjacent_to_next) {
        // Close a hole
        physalloc_ranges[st-1].size += freed.size;
        physmap_delete(st);
    } else {
        // Free range in non-adjacent empty space
        physmem_range_t entry;
        entry.base = freed.base;
        entry.size = freed.size;
        entry.type = PHYSMEM_TYPE_NORMAL;
        entry.valid = 1;
        physmap_insert_at(st, entry);
    }

#if DEBUG_PHYSMAP
    physmap_dump(TSTR "After freeing 0x%" PRIx64 " at 0x%" PRIx64,
                 freed.size, freed.base);
#endif

    //physmap_validate();

    uint64_t space_freed = physmap_check_freespace() - space_before;

    assert(space_freed == freed.size);
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
