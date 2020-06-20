#pragma once
#include <byteswap.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

static inline uint32_t htonl(uint32_t __hostlong)
{
    return bswap_32(__hostlong);
}

static inline uint16_t htons(uint16_t __hostshort)
{
    return bswap_16(__hostshort);
}

static inline uint32_t ntohl(uint32_t __netlong)
{
    return bswap_32(__netlong);
}

static inline uint16_t ntohs(uint16_t __netshort)
{
    return bswap_16(__netshort);
}

#else

static inline uint32_t htonl(uint32_t hostlong)
{
    return hostlong;
}

static inline uint16_t htons(uint16_t hostshort)
{
    return hostshort;
}

static inline uint32_t ntohl(uint32_t netlong)
{
    return netlong;
}

static inline uint16_t ntohs(uint16_t netshort)
{
    return netshort;
}

#endif
