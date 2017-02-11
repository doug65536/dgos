#include "dhcp.h"
#include "string.h"
#include "bswap.h"

ssize_t dhcp_build_discover(void *buf, size_t buf_size,
                            uint8_t *src_mac_addr)
{
    dhcp_pkt_t *discover = buf;

    memset(discover, 0, buf_size);

    //
    // Ethernet

    // Destination MAC FF:FF:FF:FF:FF:FF
    memset(discover->udp_hdr.ipv4_hdr.eth_hdr.d_mac, 0xFF,
           sizeof(discover->udp_hdr.ipv4_hdr.eth_hdr.d_mac));

    // Source MAC from NIC
    memcpy(discover->udp_hdr.ipv4_hdr.eth_hdr.s_mac, src_mac_addr,
           sizeof(discover->udp_hdr.ipv4_hdr.eth_hdr.s_mac));

    // EtherType field
    discover->udp_hdr.ipv4_hdr.eth_hdr.len_ethertype = htons(0x800);

    //
    // IPv4

    // IPv4 destination IP 255.255.255.255
    memset(discover->udp_hdr.ipv4_hdr.d_ip, 0xFF,
           sizeof(discover->udp_hdr.ipv4_hdr.d_ip));

    discover->udp_hdr.ipv4_hdr.ver_ihl = 0x45;
    discover->udp_hdr.ipv4_hdr.dscp_ecn = 0;
    discover->udp_hdr.ipv4_hdr.id = 0;
    discover->udp_hdr.ipv4_hdr.flags_fragofs = 0;
    discover->udp_hdr.ipv4_hdr.ttl = 64;
    discover->udp_hdr.ipv4_hdr.protocol = IPV4_PROTO_UDP;

    // 0.0.0.0
    memset(discover->udp_hdr.ipv4_hdr.s_ip, 0,
           sizeof(discover->udp_hdr.ipv4_hdr.s_ip));

    // 255.255.255.255
    memset(discover->udp_hdr.ipv4_hdr.d_ip, 255,
           sizeof(discover->udp_hdr.ipv4_hdr.d_ip));

    //
    // UDP

    discover->udp_hdr.d_port = htons(67);
    discover->udp_hdr.s_port = htons(68);

    //
    // DHCP

    discover->op = 1;
    discover->htype = 1;
    discover->hlen = 6;
    discover->hops = 0;
    discover->xid = htonl(0x3903f326);
    discover->secs = 0;
    discover->flags = htons(0x8000);
    memcpy(discover->ch_addr, src_mac_addr, discover->hlen);
    discover->magic_cookie = htonl(0x63825363);

    // DHCP message type = DHCPDISCOVER
    int opt = 0;
    discover->options[opt++] = 53;
    discover->options[opt++] = 1;
    discover->options[opt++] = 1;
    discover->options[opt++] = 0;

    // Parameter request list
    discover->options[opt++] = 55;
    discover->options[opt++] = 7;
    discover->options[opt++] = 3;    // router
    discover->options[opt++] = 1;    // subnet mask
    discover->options[opt++] = 15;   // domain name
    discover->options[opt++] = 6;    // dns servers
    discover->options[opt++] = 23;   // default TTL
    discover->options[opt++] = 12;   // host name
    discover->options[opt++] = 51;   // address lease time
    discover->options[opt++] = 0;
    discover->options[opt++] = 255;

    if (opt & 1)
        discover->options[opt++] = 0;

    char const *end = (void*)&discover->options[opt];

    uint16_t udp_size = end - (char*)(&discover->udp_hdr.ipv4_hdr + 1);
    uint16_t ipv4_size = end - (char*)(&discover->udp_hdr.ipv4_hdr.ver_ihl);

    discover->udp_hdr.len = htons(udp_size);

    discover->udp_hdr.ipv4_hdr.len = htons(ipv4_size);

    discover->udp_hdr.ipv4_hdr.hdr_checksum =
            ipv4_checksum(&discover->udp_hdr.ipv4_hdr, 0);

    discover->udp_hdr.checksum =
            udp_checksum(&discover->udp_hdr);

    return end - (char*)discover;
}
