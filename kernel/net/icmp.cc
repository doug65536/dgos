#include "icmp.h"
#include "ipv4.h"
#include "bswap.h"

uint16_t icmp_checksum(icmp_hdr_t const *hdr, void const *end)
{
    uint32_t checksum = 0;
    for (uint16_t const *in = (void*)&hdr->type; (void*)in < end; ++in)
        checksum += ntohs(*in);
    checksum += (checksum >> 16);
    checksum &= 0xFFFF;
    return htons(~checksum);
}

uint16_t icmp_finalize(icmp_hdr_t *hdr, void const *end)
{
    hdr->checksum = icmp_checksum(hdr, end);

    ipv4_finalize(&hdr->ipv4_hdr, end);

    return (char*)end - (char*)hdr;
}
