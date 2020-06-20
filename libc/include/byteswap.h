#pragma once
#include <stdint.h>

#define bswap_16(n) \
    ((((n) >> 8) & 0xFFU) | \
    (((n) << 8) & 0xFF00U))

#define bswap_32(n) \
    (((n) >> 24) | \
    (((n) >> 8) & UINT32_C(0xFF00)) | \
    (((n) << 8) & UINT32_C(0xFF0000)) | \
    (((n) << 24) & UINT32_C(0xFF000000)))

#define bswap_64(n) \
    (((n) >> 56) | \
    (((n) >> 40) & UINT64_C(0xFF00)) | \
    (((n) >> 24) & UINT64_C(0xFF0000)) | \
    (((n) >>  8) & UINT64_C(0xFF000000)) | \
    (((n) <<  8) & UINT64_C(0xFF00000000)) | \
    (((n) << 24) & UINT64_C(0xFF0000000000)) | \
    (((n) << 40) & UINT64_C(0xFF000000000000)) | \
    (((n) << 56) & UINT64_C(0xFF00000000000000)))
