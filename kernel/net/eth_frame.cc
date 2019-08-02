#include "eth_frame.h"
#include "ipv4_frame.h"
#include "arp_frame.h"
#include "bswap.h"
#include "export.h"

EXPORT void eth_frame_received(ethq_pkt_t *pkt)
{
    switch (ntohs(pkt->pkt.hdr.len_ethertype)) {
    case ETHERTYPE_IPv4:
        ipv4_frame_received(pkt);
        break;

    case ETHERTYPE_ARP:
        arp_frame_received(pkt);
        break;

    }
}

void eth_frame_transmit(ethq_pkt_t *pkt)
{
    (void)pkt;
}
