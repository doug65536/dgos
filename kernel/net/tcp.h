#pragma once
#include "eth_q.h"
#include "ipv4.h"

struct tcp_hdr_t {
    ipv4_hdr_t ipv4_hdr;
    uint16_t s_port;
    uint16_t d_port;
    uint32_t seq;
    uint32_t ack;
    uint16_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} _packed;

#define TCP_FLAGS_FIN_BIT       0
#define TCP_FLAGS_SYN_BIT       1
#define TCP_FLAGS_RST_BIT       2
#define TCP_FLAGS_PSH_BIT       3
#define TCP_FLAGS_ACK_BIT       4
#define TCP_FLAGS_URG_BIT       5
#define TCP_FLAGS_ECE_BIT       6
#define TCP_FLAGS_CWR_BIT       7
#define TCP_FLAGS_NS_BIT        8
#define TCP_FLAGS_DATAOFS_BIT   12
#define TCP_FLAGS_DATAOFS_BITS  4

void tcp_received_frame(ethq_pkt_t *pkt);
