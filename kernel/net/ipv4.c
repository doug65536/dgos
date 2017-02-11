#include "ipv4.h"
#include "bswap.h"

uint16_t ipv4_checksum(ipv4_hdr_t *hdr, size_t size)
{
    uint16_t *in = (void*)&hdr->ver_ihl;

    if (size == 0)
        size = sizeof(*hdr);

    uint32_t total = 0;
    for (size_t i = 0, e = (hdr->ver_ihl & 0xF) << 1;
         i < e; ++i)
        total += htons(in[i]);

    total += total >> 16;

    return htons(~total);
}
