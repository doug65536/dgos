#include "contig_alloc.h"
#include "printk.h"
#include "engunit.h"

#include "inttypes.h"
#include "cpu/atomic.h"
#include "mmu.h"

#define DEBUG_LINEAR_SANITY     0
#define DEBUG_ADDR_ALLOC        0
#define DEBUG_ADDR_EARLY        0

//
// Linear address allocator

static void dump_addr_tree(contiguous_allocator_t::tree_t const *tree,
                           char const *key_name, char const *val_name)
{
    for (auto const& item : *tree) {
        dbgout << key_name << '=' << hex << item.first <<
                  ", " << val_name << '=' << hex << item.second << '\n';
    }
}

void contiguous_allocator_t::set_early_base(linaddr_t *base_ptr)
{
    linear_base_ptr = base_ptr;
}

uintptr_t contiguous_allocator_t::early_init(size_t size, char const *name)
{
    scoped_lock lock(free_addr_lock);

    this->name = name;

//    linaddr_t initial_addr = *linear_base_ptr;

//    // Account for space taken creating trees

//    size_t size_adj = *linear_base_ptr - initial_addr;
//    size -= size_adj;

    uintptr_t aligned_base;

    bool shared = false;

    for (;;) {
        linaddr_t prev_base = *linear_base_ptr;

        // and align to 1GB boundary to make pointers more readable
        aligned_base = prev_base;

        // Page align
        aligned_base = (aligned_base + PAGE_SIZE - 1) & -PAGE_SIZE;

        std::pair<tree_t::iterator, bool>
                ins_by_size = free_addr_by_size.insert({size, aligned_base});

        if (!shared) {
            shared = true;
            free_addr_by_addr.share_allocator(free_addr_by_size);
        }

        std::pair<tree_t::iterator, bool>
                ins_by_addr = free_addr_by_addr.insert({aligned_base, size});

        if (likely(*linear_base_ptr == prev_base))
            break;

        // Whoa, memory allocation disturbed that pair, undo
        free_addr_by_size.erase(ins_by_size.first);
        free_addr_by_addr.erase(ins_by_addr.first);

        // retry...
    }

    ready = true;

    validate_locked(lock);

    return aligned_base;
}

EXPORT void contiguous_allocator_t::init(
        linaddr_t addr, size_t size, char const *name)
{
    scoped_lock lock(free_addr_lock);

    this->name = name;

    if (size) {
        free_addr_by_size.insert({size, addr});
        free_addr_by_addr.share_allocator(free_addr_by_size);
        free_addr_by_addr.insert({addr, size});
    }

    validate_locked(lock);

    ready = true;
}

EXPORT uintptr_t contiguous_allocator_t::alloc_linear(size_t size)
{
    linaddr_t addr;

    if (likely(ready)) {
        scoped_lock lock(free_addr_lock);

#if DEBUG_ADDR_ALLOC
        //dump("Before Alloc %#" PRIx64 "\n", size);
        validate_locked(lock);
#endif

//        free_addr_by_size.dump("by size before alloc_linear");
//        free_addr_by_size.validate();
//        free_addr_by_addr.dump("addr before alloc_linear");
//        free_addr_by_addr.validate();

        // Find the lowest address item that is big enough
        tree_t::iterator place = free_addr_by_size.lower_bound({size, 0});

        if (unlikely(place == free_addr_by_size.end()))
            return 0;

        // Copy the sufficiently-sized block description
        tree_t::value_type by_size = *place;

        assert(by_size.first >= size);

        // Extract the pair of entries
        tree_t::node_type node_by_size = free_addr_by_size.extract(place);

        tree_t::node_type node_by_addr =
                free_addr_by_addr.extract({by_size.second,
                                           by_size.first});

        assert(bool(node_by_size));
        assert(bool(node_by_addr));

        std::pair<tree_t::iterator, bool> chk;

        if (by_size.first > size) {
            // Insert remainder by size
            node_by_size.value() = { by_size.first - size,
                    by_size.second + size };
            chk = free_addr_by_size.insert(std::move(node_by_size));
            assert(chk.second);

            // Insert remainder by address
            node_by_addr.value() = {by_size.second + size,
                    by_size.first - size};
            chk = free_addr_by_addr.insert(std::move(node_by_addr));
            assert(chk.second);
        }

        addr = by_size.second;

#if DEBUG_ADDR_ALLOC
        //dump("after alloc_linear sz=%#zx addr=%#zx\n", size, addr);
        validate_locked(lock);
#endif

#if DEBUG_LINEAR_SANITY
        sanity_check_by_size(free_addr_by_size);
        sanity_check_by_addr(free_addr_by_addr);
#endif

#if DEBUG_ADDR_ALLOC
        printdbg("Allocated address space @ %#" PRIx64
                 ", size=%#" PRIx64 "\n", addr, size);
#endif
    } else {
        addr = atomic_xadd(linear_base_ptr, size);

#if DEBUG_ADDR_EARLY
        printdbg("Took early address space @ %#" PRIx64
                 ", size=%#" PRIx64 ""
                 ", new linear_base=%#" PRIx64 "\n",
                 addr, size, *linear_base_ptr);
#endif
    }

    return addr;
}

/*
 * 9 scenarios                                                         *
 *                                                                     *
 *   A-----B  X: The range we are taking                               *
 *   C-----D  Y: The existing range                                    *
 *
 * Query finds [ ranges.lower_bound(A), ranges.upper_bound(B) )
 *
 * For each one use this table to determine outcome against the first
 *
 *  +-------+-------+-------------+-------+--------------------------------+
 *  | A<=>C | B<=>D |             | Count |                                |
 *  +-------+-------+-------------+-------+--------------------------------+
 *  |       |       |             |       |                                |
 *  |  -1   |  -1   | <--->       |  +1   | No overlap, do nothing, done   |
 *  |       |       |       <---> |       |                                |
 *  |       |       |             |       |                                |
 *  |  -1   |   0   | <---------> |   0   | Replace obstacle, done         |
 *  |       |       |       <xxx> |       |                                |
 *  |       |       |             |       |                                |
 *  |  -1   |   1   | <---------> |   0   | Replace obstacle, continue     |
 *  |       |       |    <xxx>    |       |                                |
 *  |       |       |             |       |                                |
 *  |   0   |  -1   | <--->       |   1   | Clip obstacle start, done      |
 *  |       |       | <xxx------> |       |                                |
 *  |       |       |             |       |                                |
 *  |   0   |   0   | <--->       |   0   | Replace obstacle, done         |
 *  |       |       | <xxx>       |       |                                |
 *  |       |       |             |       |                                |
 *  |   0   |   1   | <---------> |   0   | Replace obstacle, continue     |
 *  |       |       | <xxx>       |       |                                |
 *  |       |       |             |       |                                |
 *  |   1   |  -1   |   <--->     |   2   | Duplicate obstacle, clip end   |
 *  |       |       | <-->x<-->   |       | of original, clip start of     |
 *  |       |       |             |       | duplicate, done                |
 *  |       |       |             |       |                                |
 *  |   1   |   0   |   <--->     |   1   | Clip obstacle end, done        |
 *  |       |       | <--xxx>     |       |                                |
 *  |       |       |             |       |                                |
 *  |   1   |   1   |     <-----> |   1   | Clip obstacle end, continue    |
 *  |       |       |   <--xx>    |       |                                |
 *  |       |       |             |       |                                |
 *  +-------+-------+-------------+-------+--------------------------------+
 *
 * "done" means, there is no point in continuing to iterate forward in the
 * range query results, there is no way it could overlap any more items.
 * "continue" means, there may be more blocks that overlap the range,
 * continue with the next block, which may be another relevant obstacle.
 *
 */

EXPORT bool contiguous_allocator_t::take_linear(
        linaddr_t addr, size_t size, bool require_free)
{
    scoped_lock lock(free_addr_lock);

#if DEBUG_ADDR_ALLOC
    //dump("---- Take %#" PRIx64 " @ %#" PRIx64 "\n", size, addr);
#endif

    assert(size > 0);

    assert(free_addr_by_addr.size() == free_addr_by_size.size());

    assert(!free_addr_by_addr.empty());
    assert(!free_addr_by_size.empty());

    uintptr_t end = addr + size;

    tree_t::iterator lo_it = free_addr_by_addr.lower_bound({addr, 0});
    tree_t::iterator hi_it = free_addr_by_addr.lower_bound({end, 0});
    tree_t::value_type item;
    tree_t::value_type pred;
    tree_t::iterator pred_it;

    std::pair<tree_t::iterator, bool> chk;

    if (lo_it != free_addr_by_addr.begin()) {
        pred_it = lo_it;
        --pred_it;
        pred = *pred_it;
    }

    if (lo_it != free_addr_by_addr.end()) {
        item = *lo_it;
    }

    uintptr_t item_end = item.first + item.second;
    uintptr_t pred_end = pred.first + pred.second;

    if (require_free) {
        if (addr >= item.first && addr < item_end &&
                end - item.first < size)
            return false;

        if (addr >= pred.first && addr < pred_end &&
                end - pred.first < size)
            return false;
    }

    tree_t::value_type before;
    tree_t::value_type after;

    tree_t::node_type item_by_addr;
    tree_t::node_type item_by_size;

    if (pred.second) {

        if (pred_end > addr) {
            // Carve some out of predecessor
            before = {pred.first,
                      addr > pred.first
                      ? addr - pred.first
                      : 0};

            after = {end,
                     pred_end > end
                     ? pred_end - end
                     : 0};

            item_by_addr = free_addr_by_addr.extract(pred_it);
            item_by_size = free_addr_by_size.extract({pred.second,
                                                      pred.first});
            assert((bool)item_by_size);

            if (before.second) {
                item_by_addr.value() = before;

                item_by_size.value() = {before.second, before.first};

                chk = free_addr_by_addr.insert(std::move(item_by_addr));
                assert(chk.second);

                chk = free_addr_by_size.insert(std::move(item_by_size));
                assert(chk.second);
            }

            if (after.second) {
                if (item_by_addr) {
                    item_by_addr.value() = after;

                    item_by_size.value() = {after.second, after.first};

                    chk = free_addr_by_addr.insert(std::move(item_by_addr));
                    assert(chk.second);

                    chk = free_addr_by_size.insert(std::move(item_by_size));
                    assert(chk.second);
                } else {
                    chk = free_addr_by_addr.insert(after);
                    assert(chk.second);

                    chk = free_addr_by_size.insert({after.second,
                                                    after.first});
                    assert(chk.second);
                }
            }
        }
    }

    while (lo_it != free_addr_by_addr.end() && lo_it != hi_it) {
        tree_t::value_type item = *lo_it;

        if (item.first >= end)
            break;

        uintptr_t item_end = item.first + item.second;

        before = {item.first,
                  addr > item.first
                  ? addr - item.first
                  : 0};

        after = {end,
                 item_end > end
                 ? item_end - end
                 : 0};

        item_by_addr = free_addr_by_addr.extract(lo_it);
        item_by_size = free_addr_by_size.extract({item.second,
                                                 item.first});
        assert(bool(item_by_addr));
        assert(bool(item_by_size));

        if (before.second) {
            item_by_addr.value() = before;

            item_by_size.value() = {before.second, before.first};

            chk = free_addr_by_addr.insert(std::move(item_by_addr));
            assert(chk.second);

            chk = free_addr_by_size.insert(std::move(item_by_size));
            assert(chk.second);
        }

        if (after.second) {
            if (item_by_addr) {
                item_by_addr.value() = after;

                item_by_size.value() = {after.second, after.first};

                chk = free_addr_by_addr.insert(std::move(item_by_addr));
                assert(chk.second);

                chk = free_addr_by_size.insert(std::move(item_by_size));
                assert(chk.second);
            } else {
                chk = free_addr_by_addr.insert(after);
                assert(chk.second);

                chk = free_addr_by_size.insert({after.second, after.first});
                assert(chk.second);
            }
        }
    }

    assert(free_addr_by_addr.size() == free_addr_by_size.size());

    return true;
}

EXPORT void contiguous_allocator_t::release_linear(uintptr_t addr, size_t size)
{
    scoped_lock lock(free_addr_lock);

#if DEBUG_ADDR_ALLOC
    //dump("---- Free %#" PRIx64 " @ %#" PRIx64 "\n", size, addr);
    validate_locked(lock);
#endif

    assert(free_addr_by_addr.size() == free_addr_by_size.size());

    tree_t::value_type range{addr, size};
    uintptr_t end = addr + size;

    std::pair<tree_t::iterator, bool>
            ins = free_addr_by_addr.insert(range);

    // Did we luckily free a range that already exactly exists?
    if (unlikely(!ins.second))
        return;

    std::pair<tree_t::iterator, bool>
            ins_size = free_addr_by_size.insert({range.second, range.first});

    // If by addr went in, by size surely will
    assert(ins_size.second);

    tree_t::node_type ins_node;
    tree_t::node_type ins_size_node;

    // Do this before we extract, because it makes traversal work
    tree_t::iterator succ_it = ins.first;
    ++succ_it;

    std::pair<tree_t::iterator, bool> chk;

    // If there is a predecessor
    if (ins.first != free_addr_by_addr.begin()) {
        // See if we need to coalesce with predecessor
        tree_t::iterator pred_it = ins.first;
        --pred_it;

        tree_t::value_type pred = *pred_it;

        size_t pred_end = pred.first + pred.second;

        // If overlapping or adjacent
        if (pred.first <= addr && pred_end >= addr) {
            // If freeing inside an already freed region
            if (addr > pred.first) {
                // Move start of freed region to line up with
                // overlapping one being replaced
                addr = pred.first;
                size = end - addr;
            }

            // If freeing inside an already freed region
            if (end < pred_end) {
                end = pred_end;
                // Move end of freed region to line up with
                // overlapping one being replaced
                size = end - addr;
            }

            // Remove interfering/adjacent/overlapping free range
            free_addr_by_addr.erase(pred_it);
            size_t n = free_addr_by_size.erase({pred.second,
                                                pred.first});
            assert(n == 1);

            //free_addr_by_addr.dump("after erasing interfering node");

            // Extract the new node
            ins_node = free_addr_by_addr.extract(ins.first);
            assert(bool(ins_node));

            //free_addr_by_addr.dump("after extracting inserted node");

            ins_size_node = free_addr_by_size.extract({ins.first->second,
                                                       ins.first->first});
            assert(bool(ins_node));

            // Adjust the inserted range to cover entire area of predecessor
            addr = pred.first;
            size = end - pred.first;

            ins_node.value() = {addr, size};
            ins_size_node.value() = {size, addr};
        }
    }

    while (succ_it != free_addr_by_addr.end()) {
        tree_t::value_type succ = *succ_it;
        uintptr_t succ_end = succ.first + succ.second;

        // Stop coalescing when hitting node that begins after
        // end of inserted range
        if (succ.first > end)
            break;

        // Coalesce with successor
        succ_it = free_addr_by_addr.erase(succ_it);
        size_t n = free_addr_by_size.erase({succ.second, succ.first});
        assert(n == 1);

        if (!ins_node) {
            ins_node = free_addr_by_addr.extract(ins.first);
            assert(bool(ins_node));

            ins_size_node = free_addr_by_size.extract({ins.first->second,
                                                       ins.first->first});
            assert(bool(ins_size_node));
        }

        size = succ_end - addr;

        ins_node.value() = {addr, size};
        ins_size_node.value() = {size, addr};
    }

    if (succ_it == free_addr_by_addr.end() && addr + size < end) {
        // Freed region extends past end of existing nodes
        size = end - addr;

        if (!ins_node) {
            ins_node = free_addr_by_addr.extract(ins.first);
            assert(bool(ins_node));

            ins_size_node = free_addr_by_size.extract({ins.first->second,
                                                       ins.first->first});
            assert(bool(ins_size_node));
        }

        ins_node.value() = {addr, size};
        ins_size_node.value() = {size, addr};
    }

    assert(addr == ins.first->first);
    assert(size == ins.first->second);

    if (ins_node) {
        assert(bool(ins_size_node));

        chk = free_addr_by_addr.insert(std::move(ins_node));
        assert(chk.second);

        //dump("after inserting extracted node");

        chk = free_addr_by_size.insert(std::move(ins_size_node));
        assert(chk.second);
    }

    assert(free_addr_by_addr.size() == free_addr_by_size.size());

#if DEBUG_ADDR_ALLOC
    if (addr == 0xfffffd0052257000 && size==0x40000)
        dump_locked(lock, "after bug");

    //dump("Addr map by addr (after free)");
    validate_locked(lock);
#endif
}



EXPORT void contiguous_allocator_t::dump(char const *format, ...) const
{
    va_list ap;
    va_start(ap, format);
    dumpv(format, ap);
    va_end(ap);
}

EXPORT void contiguous_allocator_t::dumpv(char const *format, va_list ap) const
{
    scoped_lock lock(free_addr_lock);
    return dump_lockedv(lock, format, ap);
}

EXPORT void contiguous_allocator_t::dump_lockedv(
        scoped_lock &lock, char const *format, va_list ap) const
{
    vprintdbg(format, ap);

    printdbg("\nBy addr\n");
    for (tree_t::const_iterator st = free_addr_by_addr.begin(),
         en = free_addr_by_addr.end(), it = st, prev; it != en; ++it) {
        engineering_t eng(it->second);

        if (it != st && prev->first + prev->second < it->first) {
            printdbg("---  addr=%#zx, size=%#zx (%sB)\n",
                     prev->first + prev->second, it->first -
                     (prev->first + prev->second),
                     engineering_t(it->first -
                                   (prev->first + prev->second)).ptr());
        }


        printdbg("ent, addr=%#zx, size=%#zx (%s)\n",
                 it->first, it->second,
                 eng.ptr());

        prev = it;
    }
    free_addr_by_addr.dump("tree");

    printdbg("\nBy size\n");
    dump_addr_tree(&free_addr_by_size, "size", "addr");
    free_addr_by_size.dump("tree");
}

bool contiguous_allocator_t::validate() const
{
    scoped_lock lock(free_addr_lock);
    return validate_locked(lock);
}


EXPORT void contiguous_allocator_t::dump_locked(
        scoped_lock &lock, char const *format, ...) const
{
    va_list ap;
    va_start(ap, format);
    dump_lockedv(lock, format, ap);
    va_end(ap);
}

EXPORT bool contiguous_allocator_t::validate_locked(scoped_lock& lock) const
{
    free_addr_by_addr.validate();
    free_addr_by_size.validate();

    // Every by-addr entry matches a corresponding by-size entry
    for (tree_t::const_iterator
         it = free_addr_by_addr.begin(),
         en = free_addr_by_addr.end();
         it != en; ++it) {
        tree_t::const_iterator
                other = free_addr_by_size.find({it->second, it->first});
        if (other == free_addr_by_size.end())
            return validation_failed(lock);
    }

    // Every by-size entry matches a corresponding by-address entry
    for (tree_t::const_iterator it = free_addr_by_size.begin(),
         en = free_addr_by_size.end(); it != en; ++it) {
        tree_t::const_iterator
                other = free_addr_by_addr.find({it->second, it->first});
        if (unlikely(other == free_addr_by_addr.end())) {
            return validation_failed(lock);
        }
    }

    // No overlap
    tree_t::const_iterator prev;
    for (tree_t::const_iterator st = free_addr_by_addr.begin(),
         en = free_addr_by_addr.end(), it = st; it != en; ++it) {
        if (it != st) {
            uintptr_t end = prev->first + prev->second;

            if (unlikely(end > it->first))
                return validation_failed(lock);
        }

        prev = it;
    }

    return true;
}

_noinline
bool contiguous_allocator_t::validation_failed(scoped_lock &lock) const
{
    dump_locked(lock, "contiguous allocator validation failed\n");
    cpu_debug_break();
    return false;
}
