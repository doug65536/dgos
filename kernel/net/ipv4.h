#pragma once
#include "ethernet.h"

typedef struct ipv4_hdr_t {
    ethernet_hdr_t eth_hdr;
    uint8_t ver_ihl;
    uint8_t dscp_ecn;
    uint16_t len;
    uint16_t id;
    uint16_t flags_fragofs;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t hdr_checksum;
    uint8_t s_ip[4];
    uint8_t d_ip[4];
} __attribute__((packed)) ipv4_hdr_t;

#define IPV4_PROTO_UDP    0x11

uint16_t ipv4_checksum(ipv4_hdr_t *hdr, size_t size);
