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

    char const *in = (char const *)&hdr->s_port;
    uint16_t native_udplen = ntohs(hdr->len);
    for (size_t i = 0, e = native_udplen >> 1; i < e; ++i) {
        uint16_t tmp;
        memcpy(&tmp, in + (i*2), sizeof(tmp));
        uint32_t native = ntohs(tmp);
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

    // 14 is a hack, wireshark says it is correct with that adjustment factor
    uint16_t udp_size = (char*)end -
            ((char*)&hdr->ipv4_hdr + (hdr->ipv4_hdr.ver_ihl & 0xF) * 4) - 14;
    hdr->len = htons(udp_size);
    hdr->checksum = udp_checksum(hdr);

    return (char*)end - (char*)&hdr->ipv4_hdr.eth_hdr;
}

void udp_port_get(ipv4_addr_pair_t *addr, udp_hdr_t const *hdr)
{
    memcpy(&addr->s.port, &hdr->s_port, sizeof(addr->s.port));
    memcpy(&addr->d.port, &hdr->d_port, sizeof(addr->d.port));
    addr->s.port = ntohs(addr->s.port);
    addr->d.port = ntohs(addr->d.port);
}

void udp_port_set(udp_hdr_t *hdr, ipv4_addr_pair_t const *addr)
{
    memcpy(&hdr->d_port, &addr->d.port, sizeof(hdr->d_port));
    memcpy(&hdr->s_port, &addr->s.port, sizeof(hdr->s_port));
    hdr->d_port = htons(hdr->d_port);
    hdr->s_port = htons(hdr->s_port);
}
