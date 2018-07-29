#include "contig_alloc.h"
#include "printk.h"

#include "inttypes.h"
#include "cpu/atomic.h"

#define DEBUG_LINEAR_SANITY     0
#define DEBUG_ADDR_ALLOC        0

//
// Linear address allocator

static int contiguous_allocator_cmp_key(
        typename rbtree_t<>::kvp_t const *lhs,
        typename rbtree_t<>::kvp_t const *rhs,
        void *p)
{
    (void)p;
    return lhs->key < rhs->key ? -1 :
            rhs->key < lhs->key ? 1 :
            0;
}

static int contiguous_allocator_cmp_both(
        typename rbtree_t<>::kvp_t const *lhs,
        typename rbtree_t<>::kvp_t const *rhs,
        void *p)
{
    (void)p;
    return lhs->key < rhs->key ? -1 :
            rhs->key < lhs->key ? 1 :
            lhs->val < rhs->val ? -1 :
            rhs->val < lhs->val ? 1 :
            0;
}

#if DEBUG_LINEAR_SANITY
static void sanity_check_by_size(rbtree_t *tree)
{
    static int call = 0;
    ++call;

    rbtree_t::kvp_t prev = { 0, 0 };
    rbtree_t::kvp_t curr;

    for (tree->iter_t it = rbtree_first(0);
         it;
         it = tree->next(it)) {
        curr = tree->item(it);
        assert(prev.val + prev.key != curr.val);
        prev = curr;
    }
}

static void sanity_check_by_addr(rbtree_t *tree)
{
    rbtree_t::kvp_t prev = { 0, 0 };
    rbtree_t::kvp_t curr;

    for (rbtree_t::iter_t it = tree->first(0);
         it;
         it = tree->next(it)) {
        curr = tree->item(it);
        assert(prev.key + prev.val != curr.key);
        prev = curr;
    }
}
#endif

template<typename K, typename V>
static int dump_addr_node(typename rbtree_t<K,V>::kvp_t *kvp, void *p)
{
    char const **names = (char const **)p;
    printdbg("%s=%#16" PRIx64 " %s=%#16" PRIx64 "\n",
             names[0], kvp->key, names[1], kvp->val);
    return 0;
}

template<typename K, typename V>
static void dump_addr_tree(rbtree_t<K,V> *tree,
                           char const *key_name, char const *val_name)
{
    char const *names[] = {
        key_name,
        val_name
    };

    tree->walk(dump_addr_node<K,V>, names);
}

void contiguous_allocator_t::set_early_base(linaddr_t *base_ptr)
{
    linear_base_ptr = base_ptr;
}

void contiguous_allocator_t::early_init(size_t size, char const *name)
{
    this->name = name;

    linaddr_t initial_addr = *linear_base_ptr;

    free_addr_by_addr.init(contiguous_allocator_cmp_key, nullptr);
    free_addr_by_size.init(contiguous_allocator_cmp_both, nullptr);

    // Account for space taken creating trees

    size_t size_adj = *linear_base_ptr - initial_addr;
    size -= size_adj;

    free_addr_by_size.insert(size, *linear_base_ptr);
    free_addr_by_addr.insert(*linear_base_ptr, size);

    dump("After early_init\n");
}

void contiguous_allocator_t::init(
        linaddr_t addr, size_t size, char const *name)
{
    this->name = name;

    free_addr_by_addr.init(contiguous_allocator_cmp_key, nullptr);
    free_addr_by_size.init(contiguous_allocator_cmp_both, nullptr);

    if (size) {
        free_addr_by_size.insert(size, addr);
        free_addr_by_addr.insert(addr, size);
    }

    dump("After init\n");
}

uintptr_t contiguous_allocator_t::alloc_linear(size_t size)
{
    linaddr_t addr;

    if (likely(free_addr_by_addr && free_addr_by_size)) {
        scoped_lock lock(free_addr_lock);

#if DEBUG_ADDR_ALLOC
        dump("Before Alloc %#" PRIx64 "\n", size);
#endif

        // Find the lowest address item that is big enough
        tree_t::iter_t place = free_addr_by_size.lower_bound(size, 0);

        if (unlikely(!place))
            return 0;

        tree_t::kvp_t by_size = free_addr_by_size.item(place);

        if (by_size.key < size) {
            place = free_addr_by_size.next(place);
            by_size = free_addr_by_size.item(place);
        }

        assert(by_size.key >= size);

        free_addr_by_size.delete_at(place);

        // Delete corresponding entry by address
        bool did_del = free_addr_by_addr.delete_item(by_size.val, by_size.key);
        assert(did_del);

        if (by_size.key > size) {
            // Insert remainder by size
            free_addr_by_size.insert(by_size.key - size, by_size.val + size);

            // Insert remainder by address
            free_addr_by_addr.insert(by_size.val + size, by_size.key - size);
        }

        addr = by_size.val;

#if DEBUG_ADDR_ALLOC
        dump("after alloc_linear sz=%zx addr=%zx\n", size, addr);
#endif

#if DEBUG_LINEAR_SANITY
        sanity_check_by_size(free_addr_by_size);
        sanity_check_by_addr(free_addr_by_addr);
#endif

#if DEBUG_ADDR_ALLOC
        printdbg("Took address space @ %#" PRIx64
                 ", size=%#" PRIx64 "\n", addr, size);
#endif
    } else {
        addr = atomic_xadd(linear_base_ptr, size);

        printdbg("Took early address space @ %#" PRIx64
                 ", size=%#" PRIx64 ""
                 ", new linear_base=%#" PRIx64 "\n",
                 addr, size, *linear_base_ptr);
    }

    return addr;
}

bool contiguous_allocator_t::take_linear(linaddr_t addr, size_t size,
                                         bool require_free)
{
    assert(free_addr_by_addr);
    assert(free_addr_by_size);

    scoped_lock lock(free_addr_lock);

    linaddr_t end = addr + size;

    // Find the last free range before or at the address
    tree_t::iter_t by_addr_place = free_addr_by_addr.lower_bound(addr, 0);

    tree_t::iter_t next_place;

    for (; by_addr_place; by_addr_place = next_place) {
        tree_t::kvp_t by_addr = free_addr_by_addr.item(by_addr_place);

        if (by_addr.key <= addr && (by_addr.key + by_addr.val) >= end) {
            //
            // Need to punch a hole in the middle of the free block

            // Delete the size entry
            free_addr_by_size.delete_item(by_addr.val, by_addr.key);

            // Delete the address entry
            free_addr_by_addr.delete_at(by_addr_place);

            // Free space up to beginning of hole
            tree_t::kvp_t new_before = {
                by_addr.key,
                addr - by_addr.key
            };

            // Free space after end of hole
            tree_t::kvp_t new_after = {
                end,
                (by_addr.key + by_addr.val) - end
            };

            if (new_before.val > 0)
                free_addr_by_addr.insert_pair(&new_before);

            if (new_after.val > 0)
                free_addr_by_addr.insert_pair(&new_after);

            if (new_before.val > 0)
                free_addr_by_size.insert(new_before.val, new_before.key);

            if (new_after.val > 0)
                free_addr_by_size.insert(new_after.val, new_after.key);

            return true;
        } else if (require_free) {
            return false;
        } else if (by_addr.key < addr && by_addr.key + by_addr.val > addr) {
            //
            // The found free block is before the range and overlaps it

            // Save next block
            next_place = free_addr_by_addr.next(by_addr_place);

            // Delete the size entry
            free_addr_by_size.delete_item(by_addr.val, by_addr.key);

            // Delete the address entry
            free_addr_by_addr.delete_at(by_addr_place);

            // Create a smaller block that does not overlap taken range
            by_addr.val = addr - by_addr.key;

            // Insert smaller range by address
            free_addr_by_addr.insert_pair(&by_addr);

            // Insert smaller range by size
            free_addr_by_size.insert(by_addr.val, by_addr.key);
        } else if (by_addr.key >= addr && by_addr.key + by_addr.val <= end) {
            //
            // Range completely covers block, delete block

            next_place = free_addr_by_addr.next(by_addr_place);

            free_addr_by_size.delete_item(
                        by_addr.val, by_addr.key);

            free_addr_by_addr.delete_at(by_addr_place);
        } else if (by_addr.key > addr) {
            //
            // Range cut off some of beginning of block

            free_addr_by_size.delete_item(by_addr.val, by_addr.key);

            free_addr_by_addr.delete_at(by_addr_place);

            return true;
        } else {
            assert(by_addr.key >= end);

            // Block is past end of allocated range, done
            return true;
        }
    }

    return false;
}

void contiguous_allocator_t::release_linear(uintptr_t addr, size_t size)
{
    linaddr_t end = addr + size;

    scoped_lock lock(free_addr_lock);

#if DEBUG_ADDR_ALLOC
    dump("---- Free %#" PRIx64 " @ %#" PRIx64 "\n", size, addr);
#endif

    // Find the nearest free block before the freed range
    tree_t::iter_t pred_it = free_addr_by_addr.lower_bound(addr, 0);

    // Find the nearest free block after the freed range
    tree_t::iter_t succ_it = free_addr_by_addr.lower_bound(end, ~0UL);

    tree_t::kvp_t pred = free_addr_by_addr.item(pred_it);
    tree_t::kvp_t succ = free_addr_by_addr.item(succ_it);

    int coalesce_pred = ((pred.key + pred.val) == addr);
    int coalesce_succ = (succ.key == end);

    if (coalesce_pred) {
        addr -= pred.val;
        size += pred.val;
        free_addr_by_addr.delete_at(pred_it);

        free_addr_by_size.delete_item(pred.val, pred.key);
    }

    if (coalesce_succ) {
        size += succ.val;
        free_addr_by_addr.delete_at(succ_it);

        free_addr_by_size.delete_item(succ.val, succ.key);
    }

    free_addr_by_size.insert(size, addr);
    free_addr_by_addr.insert(addr, size);

#if DEBUG_ADDR_ALLOC
    dump("Addr map by addr (after free)");
#endif
}

void contiguous_allocator_t::dump(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vprintdbg(format, ap);
    va_end(ap);

    printdbg("By addr\n");
    dump_addr_tree(&free_addr_by_addr, "addr", "size");
    printdbg("By size\n");
    dump_addr_tree(&free_addr_by_size, "size", "addr");
}
