#pragma once
#include "types.h"

static inline uint16_t bswap_16(uint16_t n)
{
    return (n >> 8) | (n << 8);
}

static inline uint32_t bswap_32(uint32_t n)
{
    return (n >> 24) |
            ((n >> 8) & 0xFF00U) |
            ((n << 8) & 0xFF0000U) |
            ((n << 24) & 0xFF000000U);
}

static inline uint64_t bswap_64(uint64_t n)
{
    return (n >> 56) |
            ((n >> 40) & 0xFF00UL) |
            ((n >> 24) & 0xFF0000UL) |
            ((n >>  8) & 0xFF000000UL) |
            ((n <<  8) & 0xFF00000000UL) |
            ((n << 24) & 0xFF0000000000UL) |
            ((n << 40) & 0xFF000000000000UL) |
            ((n << 56) & 0xFF00000000000000UL);
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

static inline uint16_t htons(uint16_t n)
{
    return bswap_16(n);
}

static inline uint32_t htonl(uint32_t n)
{
    return bswap_32(n);
}

static inline uint64_t htobe64(uint64_t n)
{
    return bswap_64(n);
}

static inline uint16_t ntohs(uint16_t n)
{
    return bswap_16(n);
}

static inline uint32_t ntohl(uint32_t n)
{
    return bswap_32(n);
}

static inline uint64_t be64toh(uint64_t n)
{
    return bswap_64(n);
}

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

static inline uint16_t htons(uint16_t n)
{
    return n;
}

static inline uint32_t htonl(uint32_t n)
{
    return n;
}

static inline uint64_t htobe64(uint64_t n)
{
    return n;
}

static inline uint16_t ntohs(uint16_t n)
{
    return n;
}

static inline uint32_t ntohl(uint32_t n)
{
    return n;
}

static inline uint64_t be64toh(uint64_t n)
{
    return n;
}

#else
#error Unsupported endianness
#endif
