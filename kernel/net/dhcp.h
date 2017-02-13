#pragma once
#include "net/udp.h"

typedef struct dhcp_pkt_t {
    udp_hdr_t udp_hdr;

    // discover=1, offer=2
    uint8_t op;

    // discover=1, offer=1
    uint8_t htype;

    // 6
    uint8_t hlen;

    // 0
    uint8_t hops;

    // 0x3903F326
    uint32_t xid;

    // 0
    uint16_t secs;

    // discover=0x8000, offer=0
    uint16_t flags;

    // Client IP address
    uint8_t ci_addr[4];

    // Your IP address, offer=assigned address
    uint8_t yi_addr[4];

    // Server IP address, offer=server address
    uint8_t si_addr[4];

    // Gateway IP address, offer=0
    uint8_t gi_addr[4];

    // Client MAC address
    uint8_t ch_addr[16];

    uint8_t overflow[192];

    // 0x63825363
    uint32_t magic_cookie;

    char options[32];

    // ... followed by DHCP options
    //  53: 1 (DHCP Discover)
    //  50: 192.168.1.100 (requested)
    //  55: Parameter request list
    //  1: (Request subnet mask), offer=subnet mask
    //  3: (Router), offer=router ip address
    //  15: (Domain name)
    //  6: (DNS server), offer=dns server list
} __attribute__((packed)) dhcp_pkt_t;

void dhcp_builder_begin(void *buf);
void dhcp_builder_s_mac(void *buf, uint8_t const *mac_addr);
void dhcp_builder_d_mac(void *buf, uint8_t const *mac_addr);
void dhcp_builder_set_op(void *buf, uint8_t op);
void dhcp_builder_set_client_ip(void *buf, uint32_t ip_addr);
void dhcp_builder_set_your_ip(void *buf, uint32_t ip_addr);
void dhcp_builder_set_server_ip(void *buf, uint32_t ip_addr);
void dhcp_builder_set_gateway_ip(void *buf, uint32_t ip_addr);
int dhcp_builder_add_option(void *buf, uint8_t option);
int dhcp_builder_add_option_param(void *buf,uint8_t option, uint8_t param);
int dhcp_builder_add_option_params(void *buf, uint8_t option,
                                    const void *data, size_t bytes);
uint16_t dhcp_builder_finalize(void *buf);

//ssize_t dhcp_build_discover(void *buf, size_t buf_size, uint8_t *src_mac_addr);
