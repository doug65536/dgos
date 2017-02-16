#pragma once

#include "eth_q.h"

typedef struct arp_packet_t {
    ethernet_hdr_t eth_hdr;

    // Ethernet = 1
    uint16_t htype;

    // ETHERTYPE_IPv4
    uint16_t ptype;

    // Ethernet = 6
    uint8_t hlen;

    // IPv4 = 4
    uint8_t plen;

    // 1 = request, 2 = reply
    uint16_t oper;

    // Source MAC
    uint8_t source_mac[6];

    // Source IPv4 address
    uint8_t sender_ip[4];

    // Target MAC
    uint8_t target_mac[6];

    // Target IPv4 address
    uint8_t target_ip[4];
} __attribute__((packed)) arp_packet_t;
