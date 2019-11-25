#include "ipv4_frame.h"
#include "tcp_frame.h"
#include "udp_frame.h"
#include "icmp_frame.h"
#include "printk.h"
#include "bswap.h"

#define IPV4_FRAME_DEBUG 1
#if IPV4_FRAME_DEBUG
#define IPV4_FRAME_TRACE(...) printdbg("ipv4_trace: " __VA_ARGS__)
#else
#define IPV4_FRAME_TRACE(...)
#endif

void ipv4_frame_received(ethq_pkt_t *pkt)
{
    ipv4_hdr_t *ipv4_pkt = (ipv4_hdr_t*)&pkt->pkt;

    switch (ipv4_pkt->protocol) {
    case IPV4_PROTO_TCP:
        tcp_frame_received(pkt);
        break;

    case IPV4_PROTO_UDP:
        udp_frame_received(pkt);
        break;

    case IPV4_PROTO_ICMP:
        icmp_frame_received(pkt);
        break;

    default:
        IPV4_FRAME_TRACE("Dropped unrecognized protocol=%#x\n",
                         ntohs(ipv4_pkt->protocol));
        break;
    }
}
