#include "icmp_frame.h"
#include "icmp.h"
#include "printk.h"
#include "eth_q.h"
#include "string.h"
#include "bswap.h"
#include "assert.h"

#define ICMP_DEBUG  1
#if ICMP_DEBUG
#define ICMP_TRACE(...) printdbg("icmp: " __VA_ARGS__)
#else
#define ICMP_TRACE(...) ((void)0)
#endif

static void icmp_echo(ethq_pkt_t *pkt)
{
    icmp_echo_hdr_t *hdr = (icmp_echo_hdr_t*)&pkt->pkt;

    uint16_t check_checksum = ipv4_checksum(&hdr->icmp_hdr.ipv4_hdr);

    if (check_checksum != 0) {
        ICMP_TRACE("Invalid IPv4 checksum! Dropped echo request\n");
        return;
    }

    check_checksum = icmp_checksum(
                &hdr->icmp_hdr, ipv4_end_get(&hdr->icmp_hdr.ipv4_hdr));

    if (check_checksum != 0) {
        ICMP_TRACE("Invalid ICMP checksum! Dropped echo request\n");
        return;
    }

    ethq_pkt_t *reply_pkt = ethq_pkt_acquire();
    icmp_echo_hdr_t *reply = (icmp_echo_hdr_t*)&reply_pkt->pkt;

    memset(reply, 0, sizeof(*reply));

    // Reply same length as request
    reply->icmp_hdr.ipv4_hdr.len = hdr->icmp_hdr.ipv4_hdr.len;

    // Reply MAC addresses
    memcpy(reply->icmp_hdr.ipv4_hdr.eth_hdr.d_mac,
           hdr->icmp_hdr.ipv4_hdr.eth_hdr.s_mac,
           sizeof(reply->icmp_hdr.ipv4_hdr.eth_hdr.d_mac));

    // IPv4
    reply->icmp_hdr.ipv4_hdr.eth_hdr.len_ethertype = htons(ETHERTYPE_IPv4);

    // Reply IP addresses
    memcpy(reply->icmp_hdr.ipv4_hdr.d_ip, hdr->icmp_hdr.ipv4_hdr.s_ip,
           sizeof(reply->icmp_hdr.ipv4_hdr.d_ip));

    reply->icmp_hdr.ipv4_hdr.s_ip[0] = 192;
    reply->icmp_hdr.ipv4_hdr.s_ip[1] = 168;
    reply->icmp_hdr.ipv4_hdr.s_ip[2] = 122;
    reply->icmp_hdr.ipv4_hdr.s_ip[3] = 42;

    reply->icmp_hdr.ipv4_hdr.protocol = IPV4_PROTO_ICMP;

    reply->icmp_hdr.ipv4_hdr.ttl = 64;

    reply->icmp_hdr.ipv4_hdr.ver_ihl = 0x45;

    char const *end = (char const *)ipv4_end_get(&hdr->icmp_hdr.ipv4_hdr);

    char const *payload = (char*)(hdr + 1);

    char *reply_payload = (char*)(reply + 1);
    char *reply_end = reply_payload + (end - payload);

    memcpy(reply_payload, payload, end - payload);

    reply->icmp_hdr.type = ICMP_TYPE_ECHO_REPLY;
    reply->seq = hdr->seq;
    reply->identifier = hdr->identifier;

    reply_pkt->size = icmp_finalize(&reply->icmp_hdr, reply_end);

    ICMP_TRACE("Transmitting ICMP reply seq=%d pkt=%p\n",
               ntohs(reply->seq), (void*)reply_pkt);

    pkt->nic->send(reply_pkt);
}

void icmp_frame_received(ethq_pkt_t *pkt)
{
    icmp_hdr_t *hdr = (icmp_hdr_t*)&pkt->pkt;

    switch (hdr->type) {
    case ICMP_TYPE_ECHO:
        icmp_echo(pkt);
        break;

    }
}
