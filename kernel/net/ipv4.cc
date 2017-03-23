#include "ipv4.h"
#include "bswap.h"
#include "memory.h"

uint16_t ipv4_checksum(ipv4_hdr_t const *hdr)
{
    uint16_t *in = (void*)&hdr->ver_ihl;

    uint32_t total = 0;
    for (size_t i = 0, e = (hdr->ver_ihl & 0xF) << 1;
         i < e; ++i)
        total += htons(in[i]);

    total += total >> 16;
    total &= 0xFFFF;

    return htons(~total);
}

void ipv4_finalize(ipv4_hdr_t *hdr, void const *end)
{
    uint16_t ipv4_size = (char*)end - (char*)(&hdr->ver_ihl);
    hdr->len = htons(ipv4_size);
    hdr->hdr_checksum = ipv4_checksum(hdr);
}

void ipv4_ip_get(ipv4_addr_pair_t *addr, ipv4_hdr_t const *hdr)
{
    memcpy(&addr->s.ip, hdr->s_ip, sizeof(addr->s.ip));
    memcpy(&addr->d.ip, hdr->d_ip, sizeof(addr->d.ip));
    addr->s.ip = ntohl(addr->s.ip);
    addr->s.ip = ntohl(addr->d.ip);
}

void const *ipv4_end_get(ipv4_hdr_t const *hdr)
{
    return (char*)&hdr->ver_ihl + ntohs(hdr->len);
}
