#pragma once
#include "ethernet.h"

struct ipv4_hdr_t {
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
} __packed;

#define IPV4_PROTO_ICMP     0x01
#define IPV4_PROTO_TCP      0x06
#define IPV4_PROTO_UDP      0x11

#define IPV4_ADDR32(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))

struct ipv4_addr_t {
    uint32_t ip;
    uint16_t port;
    uint16_t align;
};

struct ipv4_addr_pair_t {
    ipv4_addr_t s;
    ipv4_addr_t d;
};

uint16_t ipv4_checksum(ipv4_hdr_t const *hdr);
void ipv4_ip_get(ipv4_addr_pair_t *addr, ipv4_hdr_t const *hdr);

void const *ipv4_end_get(ipv4_hdr_t const *hdr);

void ipv4_finalize(ipv4_hdr_t *hdr, void const *end);
