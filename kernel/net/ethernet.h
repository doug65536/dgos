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
