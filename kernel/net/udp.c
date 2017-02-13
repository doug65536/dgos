#include "udp.h"
#include "bswap.h"
#include "string.h"

uint16_t udp_checksum(udp_hdr_t const *hdr)
{
    // Some IPv4 header fields are included in the UDP checksum
    uint16_t ipv4_fields[6];

    // Source and destination IP
    memcpy(ipv4_fields, hdr->ipv4_hdr.s_ip, 4);
    memcpy(ipv4_fields+2, hdr->ipv4_hdr.d_ip, 4);
    ipv4_fields[4] = ntohs(hdr->ipv4_hdr.protocol);
    memcpy(ipv4_fields + 5, &hdr->len, 2);

    uint32_t total = 0;
    for (size_t i = 0; i < 6; ++i) {
        uint32_t native = ntohs(ipv4_fields[i]);
        total += native;
    }

    uint16_t const *in = &hdr->s_port;
    uint16_t native_udplen = ntohs(hdr->len);
    for (size_t i = 0, e = native_udplen >> 1; i < e; ++i) {
        uint32_t native = ntohs(in[i]);
        total += native;
    }

    total += total >> 16;
    total = ~total;
    total &= 0xFFFF;

    return total ? htons(total) : 0xFFFF;
}

uint16_t udp_finalize(udp_hdr_t *hdr, void const *end)
{
    ipv4_finalize(&hdr->ipv4_hdr, end);

    uint16_t udp_size = (char*)end - (char*)(&hdr->ipv4_hdr + 1);
    hdr->len = htons(udp_size);
    hdr->checksum = udp_checksum(hdr);

    return (char*)end - (char*)&hdr->ipv4_hdr.eth_hdr;
}
