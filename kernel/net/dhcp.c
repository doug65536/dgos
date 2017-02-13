#include "dhcp.h"
#include "string.h"
#include "bswap.h"

#if 0
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
#endif

void dhcp_builder_begin(void *buf)
{
    dhcp_pkt_t *pkt = buf;

    memset(pkt, 0, sizeof(*pkt));

    //
    // Ethernet

    // Destination MAC FF:FF:FF:FF:FF:FF
    memset(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac, 0xFF,
           sizeof(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac));

    // EtherType field
    pkt->udp_hdr.ipv4_hdr.eth_hdr.len_ethertype = htons(0x800);

    //
    // IPv4

    // IPv4 destination IP 255.255.255.255
    memset(pkt->udp_hdr.ipv4_hdr.d_ip, 0xFF,
           sizeof(pkt->udp_hdr.ipv4_hdr.d_ip));

    pkt->udp_hdr.ipv4_hdr.ver_ihl = 0x45;
    pkt->udp_hdr.ipv4_hdr.dscp_ecn = 0;
    pkt->udp_hdr.ipv4_hdr.id = 0;
    pkt->udp_hdr.ipv4_hdr.flags_fragofs = 0;
    pkt->udp_hdr.ipv4_hdr.ttl = 64;
    pkt->udp_hdr.ipv4_hdr.protocol = IPV4_PROTO_UDP;

    // 0.0.0.0
    memset(pkt->udp_hdr.ipv4_hdr.s_ip, 0,
           sizeof(pkt->udp_hdr.ipv4_hdr.s_ip));

    // 255.255.255.255
    memset(pkt->udp_hdr.ipv4_hdr.d_ip, 255,
           sizeof(pkt->udp_hdr.ipv4_hdr.d_ip));

    //
    // UDP

    pkt->udp_hdr.d_port = htons(67);
    pkt->udp_hdr.s_port = htons(68);

    //
    // DHCP

    pkt->op = 1;
    pkt->htype = 1;
    pkt->hlen = 6;
    pkt->hops = 0;
    pkt->xid = htonl(0x3903f326);
    pkt->secs = 0;
    pkt->flags = htons(0x8000);

    pkt->magic_cookie = htonl(0x63825363);
}

void dhcp_builder_s_mac(void *buf, const uint8_t *mac_addr)
{
    dhcp_pkt_t *pkt = buf;

    memcpy(pkt->udp_hdr.ipv4_hdr.eth_hdr.s_mac, mac_addr,
           sizeof(pkt->udp_hdr.ipv4_hdr.eth_hdr.s_mac));
}

void dhcp_builder_d_mac(void *buf, const uint8_t *mac_addr)
{
    dhcp_pkt_t *pkt = buf;

    memcpy(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac, mac_addr,
           sizeof(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac));
}

void dhcp_builder_set_op(void *buf, uint8_t op)
{
    dhcp_pkt_t *pkt = buf;

    pkt->op = op;
}

void dhcp_builder_set_client_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = buf;

    memcpy(pkt->ci_addr, &ip_addr, sizeof(pkt->gi_addr));
}

void dhcp_builder_set_your_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = buf;

    memcpy(pkt->yi_addr, &ip_addr, sizeof(pkt->gi_addr));
}

void dhcp_builder_set_server_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = buf;

    memcpy(pkt->si_addr, &ip_addr, sizeof(pkt->gi_addr));
}

void dhcp_builder_set_gateway_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = buf;

    memcpy(pkt->gi_addr, &ip_addr, sizeof(pkt->gi_addr));
}

int dhcp_builder_add_option(void *buf, uint8_t option)
{
    dhcp_pkt_t *pkt = buf;

    for (size_t i = 0; i < countof(pkt->options); ) {
        if (pkt->options[i] == 0) {
            pkt->options[i] = option;
            pkt->options[i + 1] = 0;
            return i + 2;
        }

        i = (i + pkt->options[i + 1] + 3) & -2;
    }
    return 0;
}

int dhcp_builder_add_option_param(void *buf, uint8_t option, uint8_t param)
{
    dhcp_pkt_t *pkt = buf;

    for (size_t i = 0; i < countof(pkt->options); ) {
        if (pkt->options[i] == option) {
            size_t params = pkt->options[i + 1]++;
            pkt->options[i + 2 + params] = param;
            return 2 + params + 1;
        }

        i = (i + pkt->options[i + 1] + 3) & -2;
    }
    return 0;
}

int dhcp_builder_add_option_params(void *buf, uint8_t option,
                                    void const *data, size_t bytes)
{
    dhcp_pkt_t *pkt = buf;

    for (size_t i = 0; i < countof(pkt->options); ) {
        if (pkt->options[i] == option) {
            size_t params = pkt->options[i + 1];
            pkt->options[i + 1] += bytes;
            memcpy(&pkt->options[i + 2 + params], data, bytes);
            return 2 + params + bytes;
        }

        i = (i + pkt->options[i + 1] + 3) & -2;
    }
    return 0;
}

uint16_t dhcp_builder_finalize(void *buf)
{
    int option_len = (dhcp_builder_add_option(buf, 255) + 1) & -2;

    dhcp_pkt_t *pkt = buf;

    char const *end = (void*)&pkt->options[option_len];

    uint16_t udp_size = end - (char*)(&pkt->udp_hdr.ipv4_hdr + 1);
    uint16_t ipv4_size = end - (char*)(&pkt->udp_hdr.ipv4_hdr.ver_ihl);

    pkt->udp_hdr.len = htons(udp_size);

    pkt->udp_hdr.ipv4_hdr.len = htons(ipv4_size);

    pkt->udp_hdr.ipv4_hdr.hdr_checksum =
            ipv4_checksum(&pkt->udp_hdr.ipv4_hdr, 0);

    pkt->udp_hdr.checksum =
            udp_checksum(&pkt->udp_hdr);

    return end - (char*)pkt;
}

