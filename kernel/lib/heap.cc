#include "heap.h"
#include "mutex.h"
#include "assert.h"
#include "string.h"
#include "bitsearch.h"
#include "asan.h"
#include "stdlib.h"
#include "atomic.h"
#include "debug.h"
#include "printk.h"

#ifdef __DGOS_KERNEL__
#include "mm.h"
#endif

__BEGIN_ANONYMOUS

// Enable an extremely large number of validations to occur around things
#define HEAP_EXCESSIVE_VALIDATION  0

// Don't free virtual address ranges, just free physical pages
#define HEAP_NOVFREE 0

struct heap_hdr_t {
    uintptr_t size_next;
    uint32_t sig1;
    uint32_t heap_id;
};

C_ASSERT(sizeof(heap_hdr_t) == 16);

static constexpr uint32_t HEAP_BLK_TYPE_USED = 0xeda10ca1;  // "a10ca1ed"
static constexpr uint32_t HEAP_BLK_TYPE_FREE = 0x0cb1eefe;  // "feeeb10c"

/// bucket  slot sz item sz items efficiency
/// [ 0] ->      32      16  2048     50.00%
/// [ 1] ->      64      48  1024     75.00%
/// [ 2] ->     128     112   512     87.50%
/// [ 3] ->     256     240   256     93.75%
/// [ 4] ->     512     496   128     96.88%
/// [ 5] ->    1024    1008    64     98.44%
/// [ 6] ->    2048    2032    32     99.22%
/// [ 7] ->    4096    4080    16     99.61%
/// [ 8] ->    8192    8176     8     99.80%
/// [ 9] ->   16384   16368     4     99.90%
/// [10] ->   32768   32752     2     99.95%
/// [11] ->   65536   65520     1     99.98%
/// .... -> use mmap

static constexpr size_t HEAP_1ST_BUCKET = 5;

static constexpr size_t HEAP_BUCKET_COUNT = 12;

static constexpr size_t HEAP_MMAP_THRESHOLD =
        (size_t(1)<<(HEAP_1ST_BUCKET + HEAP_BUCKET_COUNT - 1));

static constexpr size_t HEAP_BUCKET_SIZE =
        (size_t(1)<<(HEAP_BUCKET_COUNT + HEAP_1ST_BUCKET - 1));

struct heap_page_t {
    void *mem;
    size_t slot_size;

    operator void *() const
    {
        return mem;
    }

    operator size_t() const
    {
        return slot_size;
    }
};

// When the main heap_t::arenas array overflows, an additional
// page is allocated to hold additional arena pointers.
struct heap_ext_arena_t {
    heap_ext_arena_t *prev;
    size_t arena_count;
    heap_page_t arenas[(PAGESIZE - sizeof(heap_ext_arena_t*) -
                        sizeof(size_t)) / sizeof(heap_page_t)];
};

// The maximum number of arenas without adding extended arenas
static constexpr const size_t HEAP_MAX_ARENAS =
    ((PAGESIZE -
      sizeof(heap_hdr_t*) * HEAP_BUCKET_COUNT - // free_chains
      sizeof(size_t) -                          // arena_count
      sizeof(heap_ext_arena_t*) -               // last_ext_arena
      sizeof(mutex_t) -                         // heap_lock
      sizeof(uint32_t)) /                       // id
      sizeof(heap_page_t)) - 1;                 // arenas

C_ASSERT(sizeof(heap_ext_arena_t) == PAGESIZE);

struct heapimpl_t : public heap_t {
    using lock_type = ext::mutex;
    using scoped_lock = ext::unique_lock<lock_type>;
public:
    static heap_t *create();
    static heap_t *create(size_t initial_reserve);
    static void destroy(heap_t *heap);
    static uint32_t get_heap_id(heap_t *heap);

    heapimpl_t();
    ~heapimpl_t();
    void *operator new(size_t) noexcept;
    void operator delete(void *) noexcept;

    _malloc _assume_aligned(16) _alloc_size(2)
    void *alloc(size_t size);
    _malloc _assume_aligned(16) _alloc_size(2, 3)
    void *calloc(size_t num, size_t size);
    _assume_aligned(16) _alloc_size(3)
    void *realloc(void *block, size_t size);
    void free(void *block);
    bool maybe_blk(void *block);
    bool validate(bool dump = false) const;
    bool validate_locked(bool dump, scoped_lock const& lock) const;
    _malloc _assume_aligned(16) _alloc_size(2)
    void *large_alloc(size_t size, uint32_t heap_id);
    void large_free(heap_hdr_t *hdr, size_t size);
    _noinline
    bool validate_failed() const;

    // Set the reserve count and fill it with arena size pages
    bool reserve_reset(size_t bucket_count, scoped_lock &lock);

    bool reserve_grow_capacity(size_t bucket_count, scoped_lock &lock);

    // Allocate or free buckets for/from reserve pool
    // until bucket count equals `bucket_count`
    bool reserve_adj_pool(size_t bucket_count, const scoped_lock &lock);

    _assume_aligned(16)
    heap_hdr_t *create_arena(uint8_t log2size, scoped_lock &lock);

    heap_hdr_t *free_chains[HEAP_BUCKET_COUNT];

    // The first HEAP_MAX_ARENAS arena pointers are here
    heap_page_t arenas[HEAP_MAX_ARENAS];
    size_t arena_count;

    // Singly linked list of overflow arena pointer pages
    heap_ext_arena_t *last_ext_arena;
    lock_type mutable heap_lock;

    // Reserve of bucket-sized blocks, to sidestep frequent mmap/munmap
    void **reserve;
    size_t reserve_count;
    size_t reserve_capacity;

    uint32_t id;
private:
    char *alloc_arena(scoped_lock &lock);
    void free_arena(void *arena);
};

C_ASSERT(sizeof(heap_t) <= PAGESIZE);

static uint32_t next_heap_id;

uint32_t heapimpl_t::get_heap_id(heap_t *heap)
{
    return static_cast<heapimpl_t*>(heap)->id;
}

heap_t *heapimpl_t::create(void)
{
    heap_t *heap = new heapimpl_t();
    return heap;
}

heap_t *heapimpl_t::create(size_t initial_reserve)
{
    heapimpl_t *heap = new heapimpl_t();

    if (likely(heap)) {
        heapimpl_t::scoped_lock lock(heap->heap_lock);

        bool ok = heap->reserve_reset(initial_reserve, lock);

        if (unlikely(!ok)) {
            delete heap;
            heap = nullptr;
        }
    }

    return heap;
}

heapimpl_t::~heapimpl_t()
{
    scoped_lock lock(heap_lock);

#if HEAP_DEBUG
    validate_locked(false, lock);
#endif

    // Free buckets in extended arena lists
    // and the extended arena lists themselves
    heap_ext_arena_t *ext_arena = last_ext_arena;
    while (ext_arena) {
        for (size_t i = 0; i < ext_arena->arena_count; ++i)
            munmap(ext_arena->arenas[i], HEAP_BUCKET_SIZE);
        ext_arena = ext_arena->prev;
        munmap(ext_arena, PAGESIZE);
    }

    // Free the buckets in the main arena list
    for (size_t i = 0; i < arena_count; ++i)
        munmap(arenas[i], HEAP_BUCKET_SIZE);

    lock.unlock();
}

void *heapimpl_t::operator new(size_t) noexcept
{
    void *mem = mmap(nullptr, PAGE_SIZE,
                     PROT_READ | PROT_WRITE,
                     MAP_UNINITIALIZED | MAP_POPULATE);
    if (unlikely(mem == MAP_FAILED))
        return nullptr;

    return mem;
}

void heapimpl_t::operator delete(void *heap) noexcept
{
    munmap(heap, PAGE_SIZE);
}

void heapimpl_t::destroy(heap_t *heap)
{
    delete static_cast<heapimpl_t*>(heap);
}

char *heapimpl_t::alloc_arena(scoped_lock &lock)
{
    void *bucket;

    if (unlikely(!reserve_count)) {
        // Double the size of the pool
        size_t new_cap = 512;

        if (reserve_capacity)
            new_cap = reserve_capacity * 2;

        if (unlikely(!reserve_reset(new_cap, lock)))
            return nullptr;
    }

    bucket = reserve[--reserve_count];

    return (char*)bucket;
}

void heapimpl_t::free_arena(void *arena)
{
    // Put the arena into the reserve pool if it is not full
    if (reserve_count < reserve_capacity) {
        reserve[reserve_count++] = arena;
    } else {
        // Really free it
        munmap(arena, HEAP_BUCKET_SIZE);
    }
}

heap_hdr_t *heapimpl_t::create_arena(uint8_t log2size, scoped_lock &lock)
{
    assert(log2size >= HEAP_1ST_BUCKET);
    assert(lock.is_locked());

    size_t *arena_count_ptr;
    heap_page_t *arena_list_ptr;

    if (arena_count < countof(arenas)) {
        // Main arena list
        arena_count_ptr = &arena_count;
        arena_list_ptr = arenas;
    } else if (last_ext_arena &&
               last_ext_arena->arena_count <
               countof(last_ext_arena->arenas)) {
        // Last overflow arena has room
        arena_count_ptr = &last_ext_arena->arena_count;
        arena_list_ptr = last_ext_arena->arenas;
    } else {
        // Create a new arena overflow page
        heap_ext_arena_t *new_list;

        new_list = (heap_ext_arena_t*)mmap(
                    nullptr, PAGESIZE, PROT_READ | PROT_WRITE,
                    MAP_UNINITIALIZED | MAP_POPULATE);
        if (!new_list || new_list == MAP_FAILED)
            return nullptr;

        new_list->prev = last_ext_arena;
        new_list->arena_count = 0;
        last_ext_arena = new_list;

        arena_count_ptr = &new_list->arena_count;
        arena_list_ptr = new_list->arenas;
    }

    size_t bucket = log2size - HEAP_1ST_BUCKET;

    char *arena = alloc_arena(lock);

    if (unlikely(arena == MAP_FAILED))
        panic_oom();

    char *arena_end = arena + HEAP_BUCKET_SIZE;

    size_t slot = (*arena_count_ptr)++;
    arena_list_ptr[slot].mem = arena;
    arena_list_ptr[slot].slot_size = log2size;

    size_t size = size_t(1) << log2size;

    heap_hdr_t *hdr = nullptr;
    heap_hdr_t *first_free = free_chains[bucket];
    for (char *fill = arena_end - size; fill >= arena; fill -= size) {
        hdr = (heap_hdr_t*)fill;
        hdr->size_next = uintptr_t(first_free);
        first_free = hdr;
        hdr->sig1 = HEAP_BLK_TYPE_FREE;
#if HEAP_DEBUG
        memset(hdr + 1, 0xFE, size - sizeof(heap_hdr_t));
#endif
    }
    free_chains[bucket] = first_free;

    return hdr;
}

void *heapimpl_t::calloc(size_t num, size_t size)
{
    size *= num;
    void *block = alloc(size);

    return memset(block, 0, size);
}

void *heapimpl_t::large_alloc(size_t size, uint32_t heap_id)
{
    heap_hdr_t *hdr = (heap_hdr_t*)mmap(nullptr, size,
                           PROT_READ | PROT_WRITE,
                           MAP_POPULATE | MAP_UNINITIALIZED);

    assert(hdr != MAP_FAILED);

    if (hdr == MAP_FAILED)
        return nullptr;

    hdr->size_next = size;
    hdr->sig1 = HEAP_BLK_TYPE_USED;
    hdr->heap_id = heap_id;

    return hdr + 1;
}

void heapimpl_t::large_free(heap_hdr_t *hdr, size_t size)
{
    munmap(hdr, size);
}

void *heapimpl_t::alloc(size_t size)
{
    // Assert on utterly ridiculous size
    // or negative values
    assert(size < UINT64_C(1) << 36);

    if (unlikely(size == 0))
        return nullptr;

    // Add room for bucket header
    size += sizeof(heap_hdr_t);

    // Calculate ceil(log(size) / log(2))
    uint8_t log2size = bit_log2(size);

    // Round up to bucket item size
    size = size_t(1) << log2size;

    // Bucket 0 is 32 bytes
    assert(log2size >= HEAP_1ST_BUCKET);
    size_t bucket = log2size - HEAP_1ST_BUCKET;

    heap_hdr_t *first_free;

    if (unlikely(bucket >= HEAP_BUCKET_COUNT))
        return large_alloc(size, id);

    {
        scoped_lock lock(heap_lock);

#if HEAP_EXCESSIVE_VALIDATION
        assert(validate_locked(false, lock));
#endif

        // Try to take a free item
        first_free = free_chains[bucket];

        if (!first_free) {
            // Create a new bucket
            first_free = create_arena(log2size, lock);

#if HEAP_EXCESSIVE_VALIDATION
            assert(validate_locked(false, lock));
#endif
        }

        // Remove block from chain
        free_chains[bucket] = (heap_hdr_t*)first_free->size_next;
    }

    if (likely(first_free)) {
        // Store size (including size of header) in header
        first_free->size_next = size;

        assert(first_free->sig1 == HEAP_BLK_TYPE_FREE);

        first_free->sig1 = HEAP_BLK_TYPE_USED;
        first_free->heap_id = id;

#if HEAP_DEBUG
        memset(first_free + 1, 0xa0, size - sizeof(*first_free));
#endif

#if HEAP_EXCESSIVE_VALIDATION
        assert(malloc_validate(false));
#endif

        return first_free + 1;
    }

    return nullptr;
}

void heapimpl_t::free(void *block)
{
    if (unlikely(!block))
        return;

    heap_hdr_t *hdr = (heap_hdr_t*)block - 1;

    assert(hdr->sig1 == HEAP_BLK_TYPE_USED);

#if HEAP_DEBUG
    memset(block, 0xfe, hdr->size_next - sizeof(*hdr));
#endif

    uint8_t log2size = bit_log2(hdr->size_next);
    assert(log2size >= HEAP_1ST_BUCKET && log2size < 32);
    size_t bucket = log2size - HEAP_1ST_BUCKET;

    if (bucket < HEAP_BUCKET_COUNT) {
        hdr->sig1 = HEAP_BLK_TYPE_FREE;

        scoped_lock lock(heap_lock);

#if HEAP_EXCESSIVE_VALIDATION
        validate_locked(false, lock);
#endif

        hdr->size_next = uintptr_t(free_chains[bucket]);
        free_chains[bucket] = hdr;
    } else {
        large_free(hdr, hdr->size_next);
    }
    __asan_freeN_noabort(hdr, hdr->size_next);
}

bool heapimpl_t::maybe_blk(void *block)
{
    uintptr_t addr = uintptr_t(block);
    for (size_t i = 0; i < arena_count; ++i) {
        uintptr_t st = uintptr_t(arenas[i]);
        uintptr_t en = st + HEAP_BUCKET_SIZE;
        if (st <= addr && en > addr)
            return true;
    }

    for (heap_ext_arena_t *prev, *ext = last_ext_arena; ext; ext = prev) {
        prev = ext->prev;
        for (size_t i = 0; i < arena_count; ++i) {
            for (size_t i = 0; i < ext->arena_count; ++i) {
                uintptr_t st = uintptr_t(ext->arenas[i]);
                uintptr_t en = st + HEAP_BUCKET_SIZE;
                if (st <= addr && en > addr)
                    return true;
            }
        }
    }

    return false;
}

bool heapimpl_t::validate_failed() const
{
    cpu_debug_break();
    return false;
}

bool heapimpl_t::reserve_reset(size_t bucket_count, scoped_lock& lock)
{
    assert(lock.is_locked());

    if (unlikely(!reserve_adj_pool(0, lock)))
        return false;

    if (reserve) {
        munmap(reserve, reserve_capacity * sizeof(*reserve));
        reserve = nullptr;
        reserve_capacity = 0;
    }

    // Round up to full pages
    size_t reserve_bytes = (bucket_count * sizeof(*reserve) +
                            (PAGE_SIZE - 1)) & -PAGE_SIZE;

    // Get rounded up count
    reserve_count = reserve_bytes / sizeof(*reserve);

    // Calculate rounded size
    reserve_bytes = reserve_count * sizeof(*reserve);

    // Calculate size of arena pages
    size_t block_size = reserve_count * HEAP_BUCKET_SIZE;

    char *block = (char*)mmap(nullptr, reserve_bytes + block_size,
                       PROT_READ | PROT_WRITE,
                       MAP_UNINITIALIZED | MAP_POPULATE);

    if (unlikely(block == MAP_FAILED))
        return false;

    reserve_capacity = reserve_count;
    reserve = (void**)block;

    char *arenas = block + reserve_bytes;

    for (size_t i = 0; i < reserve_count; ++i)
        reserve[i] = arenas + HEAP_BUCKET_SIZE * i;


    return true;
}

bool heapimpl_t::reserve_adj_pool(size_t bucket_count, scoped_lock const& lock)
{
    assert(lock.is_locked());

    if (unlikely(bucket_count > reserve_capacity))
        bucket_count = reserve_capacity;

    if (bucket_count > reserve_count) {
        size_t added = bucket_count - reserve_count;

        size_t added_size = added * HEAP_BUCKET_SIZE;

        // Allocate all added arena blocks in one allocation
        char *block = (char*)mmap(nullptr, added_size,
                                  PROT_READ | PROT_WRITE,
                                  MAP_UNINITIALIZED | MAP_POPULATE);

        if (unlikely(block == MAP_FAILED))
            return false;

        char *block_iter = block;

        for (size_t i = 0; i < added; ++i, block_iter += HEAP_BUCKET_SIZE)
            reserve[reserve_count++] = block_iter;

        return true;
    }

    // Shrink while shrinking
    while (reserve_count > bucket_count) {
        munmap(reserve[--reserve_count], HEAP_BUCKET_SIZE);
        reserve[reserve_count] = nullptr;
    }

    return true;
}

bool heapimpl_t::reserve_grow_capacity(size_t bucket_count, scoped_lock &lock)
{
    assert(lock.is_locked());

    size_t old_nr = reserve_capacity * sizeof(*reserve);
    size_t new_nr = bucket_count * sizeof(*reserve);

    size_t old_sz = old_nr * sizeof(*reserve);
    size_t new_sz = new_nr * sizeof(*reserve);

    new_sz = (new_sz + PAGE_SIZE - 1) & -PAGE_SIZE;

    void **new_reserve = (void**)mremap(
                reserve, old_sz, new_sz, MREMAP_MAYMOVE);

    if (unlikely(new_reserve == MAP_FAILED))
        return false;

    reserve = new_reserve;

    for (size_t i = old_sz; i < new_sz; ++i)
        reserve[i] = nullptr;

    reserve_capacity = new_nr;

    return true;
}

bool heapimpl_t::validate(bool dump) const
{
    scoped_lock lock(heap_lock);
    return validate_locked(dump, lock);
}

bool heapimpl_t::validate_locked(bool dump, scoped_lock const& lock) const
{
    for (size_t i = 0; i < arena_count; ++i) {
        size_t sz = size_t(1) << arenas[i].slot_size;

        heap_hdr_t *st = (heap_hdr_t*)arenas[i].mem;

        if (unlikely(uintptr_t(st) >= -HEAP_BUCKET_SIZE))
            return validate_failed();

        unsigned long long en_addr;
        if (unlikely(__builtin_add_overflow((unsigned long long)st,
                                            HEAP_BUCKET_SIZE, &en_addr)))
            return validate_failed();

        heap_hdr_t *en = (heap_hdr_t*)en_addr;

        if (unlikely(dump))
            dbgout << "Processing  bucket, " << sz << " bytes per slot\n";

        for (heap_hdr_t *next, *blk = st; blk < en; blk = next) {
            next = (heap_hdr_t*)(uintptr_t(blk) + sz);
            switch (blk->sig1) {
            case HEAP_BLK_TYPE_USED:
                if (unlikely(dump))
                    hex_dump(blk, sz);
                break;

            case HEAP_BLK_TYPE_FREE:
#if HEAP_DEBUG
                // Must be full of 0xFE
                for (uint8_t *p = (uint8_t*)(blk + 1),
                     *e = p + sz - sizeof(heap_hdr_t);
                     p < e; ++p) {
                    if (likely(*p == 0xFE))
                        continue;

                    dbgout << "Free memory corrupted\n";
                }
#else
                if (unlikely(dump))
                    hex_dump(blk, sz);
#endif
                break;

            default:
                dbgout << "Invalid heap block header\n";
                return validate_failed();
            }
        }
    }

    for (heap_ext_arena_t *prev, *ext = last_ext_arena; ext; ext = prev) {
        prev = ext->prev;
        for (size_t i = 0; i < ext->arena_count; ++i) {
            size_t sz = size_t(1) << ext->arenas[i].slot_size;
            heap_hdr_t *st = (heap_hdr_t*)ext->arenas[i].mem;
            heap_hdr_t *en = (heap_hdr_t*)((char*)st + HEAP_BUCKET_SIZE);

            for (heap_hdr_t *next, *blk = st; blk < en; blk = next) {
                next = (heap_hdr_t*)(uintptr_t(blk) + sz);
                switch (blk->sig1) {
                case HEAP_BLK_TYPE_FREE:
#if HEAP_DEBUG
                    // Must be full of 0xFE
                    for (uint8_t *p = (uint8_t*)(blk + 1),
                         *e = p + sz - sizeof(heap_hdr_t);
                         p < e; ++p) {
                        if (likely(*p == 0xFE))
                            continue;

                        dbgout << "Free memory corrupted\n";
                    }
#endif
                    break;

                case HEAP_BLK_TYPE_USED:
                    if (unlikely(dump))
                        hex_dump(blk, sz);
                    break;

                default:
                    dbgout << "Invalid heap block signature\n";
                    return validate_failed();
                }
            }
        }
    }

    return true;
}

void *heapimpl_t::realloc(void *block, size_t size)
{
    if (likely(block && size)) {
        heap_hdr_t *hdr = (heap_hdr_t*)block - 1;

        assert(hdr->sig1 == HEAP_BLK_TYPE_USED);

        uint8_t log2size = bit_log2(hdr->size_next);
        uint8_t newlog2size = bit_log2(size + sizeof(heap_hdr_t));

        // Reallocating to a size that is in the same bucket is a no-op
        // If it ends up in a different bucket...
        if (newlog2size != log2size) {
            // Allocate a new block
            void *new_block = alloc(size);

            // If allocation fails, leave original block unaffected
            if (unlikely(!new_block))
                return nullptr;

            // Copy the original to the new block
            memcpy(new_block, block, hdr->size_next - sizeof(heap_hdr_t));

            // Free the original
            free(block);

            // Return new block
            block = new_block;
        }
    } else if (block && !size) {
        // Reallocating to zero size is equivalent to heap_free
        free(block);
        block = nullptr;
    } else {
        // Reallocating null block is equivalent to heap_alloc
        return alloc(size);
    }

    return block;
}

heapimpl_t::heapimpl_t()
    : free_chains{}
    , arenas{}
    , arena_count{}
    , last_ext_arena{}
    , id{atomic_xadd(&next_heap_id, 1)}
{
}

__END_ANONYMOUS

uint32_t heap_get_block_heap_id(void *block)
{

    if (likely(block)) {
        heap_hdr_t *hdr = (heap_hdr_t*)block - 1;

        return hdr->heap_id;
    }

    return thread_cpu_number();
}

heap_t *pageheap_create()
{
    heapimpl_t *heap = (heapimpl_t*)mmap(nullptr, sizeof(heap_t),
                                 PROT_READ | PROT_WRITE,
                                 MAP_UNINITIALIZED | MAP_POPULATE);
    if (unlikely(heap == MAP_FAILED))
        return nullptr;
    heap->id = atomic_xadd(&next_heap_id, 1);
    return heap;
}

void pageheap_destroy(heap_t *heap)
{
    munmap(heap, sizeof(heap_t));
}

_assume_aligned(16)
void *pageheap_calloc(heap_t *heap, size_t num, size_t size)
{
    num *= size;
    void *blk = pageheap_alloc(heap, num);
    return memset(blk, 0, num);
}

// |      ...     |
// +--------------+ <-- mmap return value
// |  Guard Page  |
// +--------------+ <-- 4KB aligned
// |//// 0xFB ////| <-- 0 to 4080 bytes ("fill before")
// +--------------+ <-- 16 byte aligned
// |  heap_hdr_t  |
// +--------------+ <-- 16 byte aligned, return value
// |     data     | <-- 0xA0 filled
// +--------------+
// |//// 0xFA ////| <-- 0 to 15 bytes ("fill after")
// +--------------+ <-- 4KB aligned
// |  Guard Page  |
// +--------------+ <-- mmap size
// |      ...     |

_assume_aligned(16)
void *pageheap_alloc(heap_t *base_heap, size_t size)
{
    heapimpl_t *heap = static_cast<heapimpl_t*>(base_heap);

    size_t exact_size = size;

    // Round size up to a multiple of 16 bytes, include header in size
    size = ((size + 15) & -16) + sizeof(heap_hdr_t);

    // Accessible data size in multiples of pages
    size_t reserve = (size + PAGESIZE - 1) & -PAGESIZE;

    // Allocate accessible range plus guard pages at both ends
    size_t alloc = reserve + PAGESIZE * 2;

    // Offset to guard page at end of range
    size_t end_guard = alloc - PAGESIZE;

    // Reserve virtual address space
    char *blk = (char*)mmap(nullptr, alloc,
                            PROT_READ | PROT_WRITE, MAP_NOCOMMIT);

    mprotect(blk, PAGESIZE, PROT_NONE);
    mprotect(blk + end_guard, PAGESIZE, PROT_NONE);

    // Calculate pointer to header as near end of accessible area as posssible
    heap_hdr_t *hdr = (heap_hdr_t*)(blk + (end_guard - size));


    // Fill region before the block

    madvise(blk + PAGESIZE,
            (char*)hdr - ((char*)blk + PAGESIZE), MADV_WILLNEED);

    memset(blk + PAGESIZE, 0xFB, intptr_t(hdr) - intptr_t(blk + PAGESIZE));

    // Fill region after exact block
    memset((char*)(hdr + 1) + exact_size, 0xFA,
           (char*)hdr + size - ((char*)(hdr + 1) + exact_size));

    // Ideally fill heap allocation with garbage that will crash
    // if uninitialized contents are used as a pointer
    memset(hdr + 1, 0xF0, exact_size);

    // We promised the compiler it is aligned like this, make sure
    assert((uintptr_t(hdr + 1) & ~-16) == 0);

    hdr->size_next = size;
    hdr->sig1 = HEAP_BLK_TYPE_USED;
    hdr->heap_id = heap->id;

    return hdr + 1;
}

void pageheap_free(heap_t *base_heap, void *block)
{
    heapimpl_t *heap = static_cast<heapimpl_t*>(base_heap);

    heap_hdr_t *hdr = (heap_hdr_t*)block - 1;

    assert(hdr->sig1 == HEAP_BLK_TYPE_USED);
    assert(hdr->heap_id == heap->id);

    uintptr_t end = uintptr_t(hdr) + hdr->size_next;
    uintptr_t st = uintptr_t(hdr) & -PAGESIZE;

    mprotect((void*)st, end - st, PROT_NONE);
    __asan_freeN_noabort(hdr, hdr->size_next);
}

_assume_aligned(16)
void *pageheap_realloc(heap_t *base_heap, void *block, size_t size)
{
    heapimpl_t *heap = static_cast<heapimpl_t*>(base_heap);

    if (unlikely(!block))
        return pageheap_alloc(heap, size);

    if (unlikely(size == 0)) {
        pageheap_free(heap, block);
        return nullptr;
    }

    heap_hdr_t *hdr = (heap_hdr_t*)block - 1;
    assert(hdr->heap_id == heap->id);

    char *other = (char*)pageheap_alloc(heap, size);

    memcpy(other, block, hdr->size_next - sizeof(heap_hdr_t));
    __asan_freeN_noabort(hdr, hdr->size_next);

    return block;
}

heap_t *heap_create(size_t initial_reserve)
{
    // Create with 16MB reserve pool of buckets
    return heapimpl_t::create(initial_reserve);
}

void *heap_alloc(heap_t *heap, size_t size)
{
    return static_cast<heapimpl_t*>(heap)->alloc(size);
}

uint32_t heap_get_heap_id(heap_t *heap)
{
    return static_cast<heapimpl_t*>(heap)->id;
}

void *heap_calloc(heap_t *heap, size_t num, size_t size)
{
    return static_cast<heapimpl_t*>(heap)->calloc(num, size);
}

void heap_free(heap_t *heap, void *block)
{
    static_cast<heapimpl_t*>(heap)->free(block);
}

void *heap_realloc(heap_t *heap, void *block, size_t size)
{
    return static_cast<heapimpl_t*>(heap)->realloc(block, size);
}

bool heap_maybe_blk(heap_t *heap, void *block)
{
    return static_cast<heapimpl_t*>(heap)->maybe_blk(block);
}

bool heap_validate(heap_t *heap, bool dump)
{
    if (likely(heap))
        return static_cast<heapimpl_t*>(heap)->validate(dump);
    return true;
}
