#include "dhcp.h"
#include "string.h"
#include "bswap.h"

void dhcp_builder_begin(void *buf)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t*)buf;

    memset(pkt, 0, sizeof(*pkt));

    //
    // Ethernet

    // Destination MAC FF:FF:FF:FF:FF:FF
    memset(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac, 0xFF,
           sizeof(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac));

    // EtherType field
    pkt->udp_hdr.ipv4_hdr.eth_hdr.len_ethertype = htons(ETHERTYPE_IPv4);

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
    dhcp_pkt_t *pkt = (dhcp_pkt_t*)buf;

    memcpy(pkt->udp_hdr.ipv4_hdr.eth_hdr.s_mac, mac_addr,
           sizeof(pkt->udp_hdr.ipv4_hdr.eth_hdr.s_mac));
}

void dhcp_builder_d_mac(void *buf, const uint8_t *mac_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    memcpy(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac, mac_addr,
           sizeof(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac));
}

void dhcp_builder_set_op(void *buf, uint8_t op)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    pkt->op = op;
}

void dhcp_builder_set_client_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    memcpy(pkt->ci_addr, &ip_addr, sizeof(pkt->gi_addr));
}

void dhcp_builder_set_your_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    memcpy(pkt->yi_addr, &ip_addr, sizeof(pkt->gi_addr));
}

void dhcp_builder_set_server_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    memcpy(pkt->si_addr, &ip_addr, sizeof(pkt->gi_addr));
}

void dhcp_builder_set_gateway_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    memcpy(pkt->gi_addr, &ip_addr, sizeof(pkt->gi_addr));
}

int dhcp_builder_add_option(void *buf, uint8_t option)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

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
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

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
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

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

    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    char const *end = (char const *)&pkt->options[option_len];

    udp_finalize(&pkt->udp_hdr, end);

    return end - (char*)pkt;
}

uint16_t dhcp_build_discover(void *buf, uint8_t const *mac_addr)
{
    dhcp_pkt_t *discover = (dhcp_pkt_t *)buf;

    dhcp_builder_begin(discover);
    dhcp_builder_s_mac(discover, mac_addr);

    // Type
    dhcp_builder_add_option(discover, DHCP_OPT_DHCP_TYPE);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_DHCP_TYPE, DHCP_TYPE_DISCOVER);

    // Parameter request
    dhcp_builder_add_option(discover, DHCP_OPT_PARM_REQ);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_ROUTER);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_SUPPLY_MASK);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_DOMAIN);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_DNS_SRV);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_IP_TTL);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_HOSTNAME);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_IP_LEASE_TIME);

    return dhcp_builder_finalize(discover);
}
