#pragma once
#include "types.h"

#define bswap_16_m(n) \
    (((n) >> 8) | \
    ((n) << 8))

#define bswap_32_m(n) \
    (((n) >> 24) | \
    (((n) >> 8) & 0xFF00U) | \
    (((n) << 8) & 0xFF0000U) | \
    (((n) << 24) & 0xFF000000U))

#define bswap_64_m(n) \
    (((n) >> 56) | \
    (((n) >> 40) & 0xFF00UL) | \
    (((n) >> 24) & 0xFF0000UL) | \
    (((n) >>  8) & 0xFF000000UL) | \
    (((n) <<  8) & 0xFF00000000UL) | \
    (((n) << 24) & 0xFF0000000000UL) | \
    (((n) << 40) & 0xFF000000000000UL) | \
    (((n) << 56) & 0xFF00000000000000UL))

#define bswap_4c_m(a,b,c,d) \
    (((a) << 24) | \
    ((a) << 16) | \
    ((a) << 8) | \
    (a))

static _always_inline _const uint16_t bswap_16(uint16_t n)
{
    return bswap_16_m(n);
}

static _always_inline _const uint32_t bswap_32(uint32_t n)
{
    return bswap_32_m(n);
}

static _always_inline _const uint64_t bswap_64(uint64_t n)
{
    return bswap_64_m(n);
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

static _always_inline _const uint16_t htons(uint16_t n)
{
    return bswap_16(n);
}

static _always_inline _const uint32_t htonl(uint32_t n)
{
    return bswap_32(n);
}

static _always_inline _const uint64_t htobe64(uint64_t n)
{
    return bswap_64(n);
}

static _always_inline _const uint16_t ntohs(uint16_t n)
{
    return bswap_16(n);
}

static _always_inline _const uint32_t ntohl(uint32_t n)
{
    return bswap_32(n);
}

static _always_inline _const uint64_t be64toh(uint64_t n)
{
    return bswap_64(n);
}


static _always_inline void htons_buf(void *buf, size_t bytes)
{
    uint16_t *w = (uint16_t*)buf;
    size_t count = bytes / sizeof(uint16_t);
    for (size_t i = 0; i < count; ++i)
        w[i] = bswap_16(w[i]);
}

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

static _always_inline _const uint16_t htons(uint16_t n)
{
    return n;
}

static _always_inline _const uint32_t htonl(uint32_t n)
{
    return n;
}

static _always_inline _const uint64_t htobe64(uint64_t n)
{
    return n;
}

static _always_inline _const uint16_t ntohs(uint16_t n)
{
    return n;
}

static _always_inline _const uint32_t ntohl(uint32_t n)
{
    return n;
}

static _always_inline _const uint64_t be64toh(uint64_t n)
{
    return n;
}

static _always_inline void htons_buf(void *, size_t)
{
}

#else
#error Unsupported endianness
#endif
