#pragma once
#include "types.h"
#include "hash.h"
#include "vector.h"
#include "refcount.h"

// Open addressing hash table
// When deleting an item with a non-null next slot,
// we "punch a hole" by setting the item pointer to
// (void*)1. We continue probe when encountering a hole
// we really delete items if the next slot is null.
// A count of used items and holes are maintained.
// If more than half of the entries are hole at deletion
// then rehash without reducing the table size.
// Rehashing resets eliminates all holes.

template<typename T, typename K, typename P,
         K const T::*key_member, size_t key_sz>
struct basic_hashtbl_t {
public:
    basic_hashtbl_t()
        : used(0)
        , holes(0)
        , log2_capacity(0)
    {
    }

    ~basic_hashtbl_t();

    bool grow();
    bool rehash(uint8_t new_log2);
    T *lookup(void *key);
    bool del(void *key);
    bool insert(T *item);
    void clear();

private:
    uint32_t hash_of_item(const P &item) const;

    std::vector<P> items;
    uint32_t used;
    uint32_t holes;
    uint8_t log2_capacity;
};

template<typename T, typename K, typename P,
         K const T::*key_member, size_t key_sz>
basic_hashtbl_t<T, K, P, key_member, key_sz>::~basic_hashtbl_t()
{
    clear();
}

template<typename T, typename K, typename P,
         K const T::*key_member, size_t key_sz>
bool basic_hashtbl_t<T, K, P, key_member, key_sz>::grow()
{
    return rehash(log2_capacity ? log2_capacity + 1 : 4);
}

template<typename T, typename K, typename P,
         K const T::*key_member, size_t key_sz>
uint32_t
basic_hashtbl_t<T, K, P, key_member, key_sz>::hash_of_item(P const& item) const
{
    return hash_32(&(((T const*)item)->*key_member), key_sz);
}

template<typename T, typename K, typename P,
         K const T::*key_member, size_t key_sz>
bool basic_hashtbl_t<T, K, P, key_member, key_sz>::rehash(uint8_t new_log2)
{
    size_t new_capacity = 1 << new_log2;
    std::vector<P> new_tbl;
    if (!new_tbl.resize(new_capacity, nullptr))
        return false;

    uint32_t new_used = 0;

    if (used) {
        uint32_t new_mask = ~(uint32_t(-1) << new_log2);

        for (uint32_t src = 0, e = 1 << log2_capacity; src < e; ++src) {
            P item(std::move(items[src]));

            // Skip nulls and holes
            if ((T*)item <= (void*)1)
                continue;

            uint32_t hash = hash_of_item(item);

            hash &= new_mask;

            // Probe and insert
            for (uint32_t k = 0, e = 1U << log2_capacity;
                 k < e; ++k, hash = (hash + 1) & new_mask) {
                T *candidate = (T*)new_tbl[hash];
                if (candidate == nullptr) {
                    new_tbl[hash] = std::move(item);
                    ++new_used;
                    break;
                }
            }
        }
    }

    items.swap(new_tbl);
    used = new_used;
    holes = 0;
    log2_capacity = new_log2;

    return true;
}

template<typename T, typename K, typename P,
         K const T::*key_member, size_t key_sz>
bool basic_hashtbl_t<T, K, P, key_member, key_sz>::insert(T *item)
{
    if (unlikely(items.empty() || used >= ((1U << log2_capacity) >> 1))) {
        if (!grow())
            return false;
    }

    uint32_t hash = hash_of_item(item);
    uint32_t mask = ~(uint32_t(-1) << log2_capacity);

    hash &= mask;

    for (uint32_t k = 0, e = 1U << log2_capacity;
         k < e; ++k, hash = (hash + 1) & mask) {
        T *candidate = (T *)items[hash];
        if (candidate <= (void*)1) {
            holes -= (candidate == (void*)1);
            items[hash] = item;
            ++used;
            return true;
        }
    }

    assert_msg(false, "Should not reach here, insert failed!");
    return false;
}

template<typename T, typename K, typename P,
         K const T::*key_member, size_t key_sz>
T *basic_hashtbl_t<T, K, P, key_member, key_sz>::lookup(void *key)
{
    T *item;

    if (used) {
        uint32_t hash = hash_32(key, key_sz);
        uint32_t mask = ~((uint32_t)-1 << log2_capacity);

        hash &= mask;

        for (uint32_t k = 0, e = 1 << log2_capacity;
             k < e; ++k, hash = (hash + 1) & mask) {
            item = (T*)items[hash];
            if (item > (void*)1) {
                void const *check = &(item->*key_member);
                if (!memcmp(check, key, key_sz))
                    return item;
            } else if (item == nullptr) {
                break;
            }
        }
    }

    return nullptr;
}

template<typename T, typename K, typename P,
         K const T::*key_member, size_t key_sz>
bool basic_hashtbl_t<T, K, P, key_member, key_sz>::del(void *key)
{
    if (used) {
        uint32_t hash = hash_32(key, key_sz);
        uint32_t mask = ~(uint32_t(-1) << log2_capacity);

        hash &= mask;

        for (uint32_t k = 0, e = 1U << log2_capacity;
             k < e; ++k, hash = (hash + 1) & mask) {
            T *candidate = (T*)items[hash];

            if (candidate > (void*)1) {
                void const *check = &(candidate->*key_member);

                if (!memcmp(check, key, key_sz)) {
                    if (items[(hash + 1) & mask]) {
                        // Next item is not null, punch hole
                        items[hash] = (T*)1;
                        ++holes;
                    } else {
                        // Next item is null, really delete
                        items[hash] = nullptr;
                    }

                    --used;

                    // If too many holes have accumulated, rehash
                    if (holes > 64 && holes > (items.size() >> 1))
                        rehash(log2_capacity);

                    return true;
                }
            } else if (candidate == nullptr) {
                break;
            }
        }
    }

    return false;
}

template<typename T, typename K, typename P,
         K const T::*key_member, size_t key_sz>
void basic_hashtbl_t<T, K, P, key_member, key_sz>::clear()
{
    items.clear();
    used = 0;
    holes = 0;
    log2_capacity = 0;
}

template<typename T, typename K,
         K const T::*key_member, size_t key_sz = sizeof(K)>
using ref_hashtbl_t = basic_hashtbl_t<T, K, refptr<T>, key_member, key_sz>;


template<typename T, typename K,
         K const T::*key_member, size_t key_sz = sizeof(K)>
using hashtbl_t = basic_hashtbl_t<T, K, T*, key_member, key_sz>;
