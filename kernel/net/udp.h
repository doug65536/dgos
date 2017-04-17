#pragma once
#include "ipv4.h"

struct udp_hdr_t {
    ipv4_hdr_t ipv4_hdr;
    uint16_t s_port;
    uint16_t d_port;
    uint16_t len;
    uint16_t checksum;
} __packed;

uint16_t udp_checksum(const udp_hdr_t *hdr);

uint16_t udp_finalize(udp_hdr_t *hdr, void const *end);

void udp_port_get(ipv4_addr_pair_t *addr, udp_hdr_t const *hdr);
void udp_port_set(udp_hdr_t *hdr, ipv4_addr_pair_t const *addr);
