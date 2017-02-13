#include "ipv4.h"
#include "bswap.h"

uint16_t ipv4_checksum(ipv4_hdr_t const *hdr, size_t size)
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

void ipv4_finalize(ipv4_hdr_t *hdr, void const *end)
{
    uint16_t ipv4_size = (char*)end - (char*)(&hdr->ver_ihl);
    hdr->len = htons(ipv4_size);
    hdr->hdr_checksum = ipv4_checksum(hdr, 0);
}
