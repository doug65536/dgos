#pragma once
#include "ipv4.h"

typedef struct udp_hdr_t {
    ipv4_hdr_t ipv4_hdr;
    uint16_t s_port;
    uint16_t d_port;
    uint16_t len;
    uint16_t checksum;
} __attribute__((packed)) udp_hdr_t;

uint16_t udp_checksum(const udp_hdr_t *hdr);
