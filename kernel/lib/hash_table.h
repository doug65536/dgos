#pragma once
#include "types.h"
#include "hash.h"
#include "vector.h"
#include "refcount.h"

template<typename T, typename K,
         K const T::*key_member,
         size_t key_sz = sizeof(K)>
struct hashtbl_t {
public:
    hashtbl_t()
        : count(0)
        , log2_capacity(0)
    {
    }

    ~hashtbl_t();

    bool rehash();

    T *lookup(void *key);

    bool del(void *key);

    bool insert(T *item);

    void clear();

private:
    vector<refptr<T>> items;
    uint32_t count;
    uint8_t log2_capacity;
};

template<typename T, typename K, K const T::*key_member, size_t key_sz>
hashtbl_t<T, K, key_member, key_sz>::~hashtbl_t()
{
    clear();
}

template<typename T, typename K, K const T::*key_member, size_t key_sz>
bool hashtbl_t<T, K, key_member, key_sz>::rehash()
{
    uint8_t new_log2 = log2_capacity
            ? log2_capacity + 1
            : 4;

    size_t new_capacity = 1 << new_log2;
    vector<refptr<T>> new_tbl;
    if (!new_tbl.resize(new_capacity, nullptr))
        return false;

    uint32_t new_mask = ~(uint32_t(-1) << new_log2);
    uint32_t new_count = 0;

    if (count) {
        for (uint32_t i = 0, e = 1 << log2_capacity; i < e; ++i) {
            // Skip nulls and holes
            if (items[i].get() <= (void*)1)
                continue;

            uint32_t hash = hash_32(&(items[i].get()->*key_member), key_sz);

            hash &= new_mask;

            // Probe and insert
            for (uint32_t k = 0; k < new_capacity;
                 ++k, hash = (hash + 1) & new_mask) {
                if (!new_tbl[k]) {
                    new_tbl[hash] = move(items[i]);
                    ++new_count;
                    break;
                }
            }
        }
    }

    items.swap(new_tbl);
    log2_capacity = new_log2;
    count = new_count;

    return true;
}

template<typename T, typename K, K const T::*key_member, size_t key_sz>
T *hashtbl_t<T, K, key_member, key_sz>::lookup(void *key)
{
    T *item = nullptr;

    if (count) {
        uint32_t hash = hash_32(key, key_sz);
        uint32_t mask = ~((uint32_t)-1 << log2_capacity);

        hash &= mask;

        for (uint32_t k = 0, e = 1 << log2_capacity;
             k < e; ++k, hash = (hash + 1) & mask) {
            if (items[hash].get() > (void*)1) {
                void const *check = &(items[hash].get()->*key_member);
                if (!memcmp(check, key, key_sz)) {
                    item = items[hash].get();
                    break;
                }
            } else if (!items[hash]) {
                break;
            }
        }
    }

    return item;
}

template<typename T, typename K, K const T::*key_member, size_t key_sz>
bool hashtbl_t<T, K, key_member, key_sz>::del(void *key)
{
    if (count) {
        uint32_t hash = hash_32(key, key_sz);
        uint32_t mask = ~(uint32_t(-1) << log2_capacity);

        hash &= mask;

        for (uint32_t k = 0, e = 1U << log2_capacity;
             k < e; ++k, hash = (hash + 1) & mask) {
            if (items[hash].get() > (void*)1) {
                void const *check = &(items[hash].get()->*key_member);

                if (!memcmp(check, key, key_sz)) {
                    if (items[(hash + 1) & mask].get()) {
                        // Next item is not null, punch hole
                        items[hash] = (T*)1;
                    } else {
                        // Next item is null, really delete
                        items[hash] = nullptr;
                        --count;
                    }

                    return true;
                }
            } else if (!items[hash]) {
                break;
            }
        }
    }

    return false;
}

template<typename T, typename K, K const T::*key_member, size_t key_sz>
bool hashtbl_t<T, K, key_member, key_sz>::insert(T *item)
{
    if (items.empty() || count >= ((1U << log2_capacity) >> 1)) {
        if (!rehash())
            return false;
    }

    uint32_t hash = hash_32(&(item->*key_member), key_sz);
    uint32_t mask = ~(uint32_t(-1) << log2_capacity);

    hash &= mask;

    for (uint32_t k = 0, e = 1U << log2_capacity;
         k < e; ++k, hash = (hash + 1) & mask) {
        if (items[hash].get() <= (void*)1) {
            // Increase count if it wasn't deletion hole
            count += (items[hash] == 0);
            items[hash] = item;
            break;
        }
    }

    return true;
}

template<typename T, typename K, K const T::*key_member, size_t key_sz>
void hashtbl_t<T, K, key_member, key_sz>::clear()
{
    items.clear();
    count = 0;
    log2_capacity = 0;
}
