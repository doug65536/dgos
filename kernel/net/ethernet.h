#pragma once
#include "types.h"

// IEEE802.3 Ethernet Frame

// 14 bytes
typedef struct ethernet_hdr_t {
    uint8_t d_mac[6];
    uint8_t s_mac[6];
    uint16_t len_ethertype;
    // ... followed by 64 to 1500 byte payload
    // ... followed by 32 bit CRC
} __attribute__((packed)) ethernet_hdr_t;

// 1518 bytes
typedef struct ethernet_pkt_t {
    // Header
    ethernet_hdr_t hdr;

    // 1500 byte packet plus room for CRC
    uint8_t packet[1500 + sizeof(uint32_t)];
} __attribute__((packed)) ethernet_pkt_t;

#define ETHERTYPE_IPv4      0x0800
#define ETHERTYPE_ARP       0x0806
#define ETHERTYPE_WOL       0x0842
#define ETHERTYPE_RARP      0x8035
#define ETHERTYPE_IPv6      0x86DD
#define ETHERTYPE_FLOWCTL   0x8808
