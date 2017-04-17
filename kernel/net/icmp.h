#pragma once
#include "ipv4.h"

struct icmp_hdr_t {
    ipv4_hdr_t ipv4_hdr;

    uint8_t type;
    uint8_t code;
    uint16_t checksum;
} __packed;

// Echo reply
#define ICMP_TYPE_ECHO_REPLY    0

// Destination unreachable
#define ICMP_TYPE_DEST_UNREACH  3

// Deprecated
#define ICMP_TYPE_SRC_QUENCH    4

// Redirect
#define ICMP_TYPE_REDIRECT      5

// Echo request
#define ICMP_TYPE_ECHO          8

// Time exceeded
#define ICMP_TYPE_TIME_EXCEEDED 11

// Timestamp request
#define ICMP_TYPE_TS            13

// Timestamp reply
#define ICMP_TYPE_TS_REPLY      14

// Address mask request
#define ICMP_TYPE_MASK          17

// Address mask reply
#define ICMP_TYPE_MASK_REPLY    18

struct icmp_echo_hdr_t {
    icmp_hdr_t icmp_hdr;

    uint16_t identifier;
    uint16_t seq;
};

struct icmp_redirect_pkt_t {
    icmp_hdr_t icmp_hdr;

    uint8_t ip_addr[4];
    ipv4_hdr_t orig_ipv4_hdr;
    uint8_t orig_ipv4_payload[8];
};

#define ICMP_REDIRECT_CODE_NET          0
#define ICMP_REDIRECT_CODE_HOST         1
#define ICMP_REDIRECT_CODE_TOS_NET      2
#define ICMP_REDIRECT_CODE_TOS_HOST     3

struct icmp_time_exceeded_pkt_t {
    icmp_hdr_t icmp_hdr;

    uint32_t unused;
    ipv4_hdr_t orig_ipv4_hdr;
    uint8_t orig_ipv4_payload[8];
};

#define ICMP_TIME_EXCEEDED_CODE_TTL     0
#define ICMP_TIME_EXCEEDED_CODE_FRAG    1

struct icmp_timestamp_pkt_t {
    icmp_hdr_t icmp_hdr;

    uint16_t identifier;
    uint16_t seq;
    uint32_t orig_timestamp;
    uint32_t rx_timestamp;
    uint32_t tx_timestamp;
};

struct icmp_addr_mask_pkt_t {
    icmp_hdr_t icmp_hdr;

    uint8_t addr_mask[4];
};

struct icmp_unreachable_pkt_t {
    icmp_hdr_t icmp_hdr;

    uint16_t unused;
    uint16_t next_hop_mtu;
    ipv4_hdr_t orig_ipv4_hdr;
    char orig_ipv4_payload[8];
};

uint16_t icmp_checksum(icmp_hdr_t const *hdr, void const *end);
uint16_t icmp_finalize(icmp_hdr_t *hdr, void const *end);
