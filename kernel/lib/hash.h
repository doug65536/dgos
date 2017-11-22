#pragma once
#include "types.h"

// jenkins hash
constexpr static __always_inline uint32_t hash_32(
        void const* k, size_t length)
{
    uint8_t const* key = (uint8_t const *)k;

    size_t i = 0;
    uint32_t hash = 0;

    while (i != length) {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }

    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;

    return hash;
}

