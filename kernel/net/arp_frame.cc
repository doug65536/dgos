#include "arp_frame.h"
#include "arp.h"
#include "bswap.h"
#include "string.h"
#include "printk.h"

#define ARP_DEBUG   1
#if ARP_DEBUG
#define ARP_TRACE(...) printdbg("ARP: " __VA_ARGS__)
#else
#define ARP_TRACE(...) ((void)0)
#endif

void arp_frame_received(ethq_pkt_t *pkt)
{
    arp_packet_t *ap = (arp_packet_t*)&pkt->pkt;

    if (ap->hlen == 6 && ap->plen == 4) {
        ARP_TRACE("Sender=%02x:%02x:%02x:%02x:%02x:%02x"
                  " IP=%d.%d.%d.%d"
                  " Target=%02x:%02x:%02x:%02x:%02x:%02x"
                  " IP=%d.%d.%d.%d hlen=%d plen=%d htype=%d oper=%d\n",
                  ap->source_mac[0],
                ap->source_mac[1],
                ap->source_mac[2],
                ap->source_mac[3],
                ap->source_mac[4],
                ap->source_mac[5],
                ap->sender_ip[0],
                ap->sender_ip[1],
                ap->sender_ip[2],
                ap->sender_ip[3],
                ap->target_mac[0],
                ap->target_mac[1],
                ap->target_mac[2],
                ap->target_mac[3],
                ap->target_mac[4],
                ap->target_mac[5],
                ap->target_ip[0],
                ap->target_ip[1],
                ap->target_ip[2],
                ap->target_ip[3],
                ap->hlen,
                ap->plen,
                ntohs(ap->htype),
                ntohs(ap->oper));

        if (ap->target_ip[0] == 192 &&
                ap->target_ip[1] == 168 &&
                ap->target_ip[2] == 122 &&
                ap->target_ip[3] == 42) {
            ethq_pkt_t *reply_pkt = ethq_pkt_acquire();
            arp_packet_t *reply = (arp_packet_t*)&reply_pkt->pkt;
            // Ethernet
            reply->htype = htons(1);
            // IPv4
            reply->ptype = htons(ETHERTYPE_IPv4);
            // 6 byte MAC
            reply->hlen = 6;
            // 4 byte IP
            reply->plen = 4;
            // Reply
            reply->oper = htons(2);

            // Reply to sender's IP
            memcpy(reply->sender_ip, ap->target_ip, 4);
            // From this IP
            memcpy(reply->target_ip, ap->sender_ip, 4);

            //
            // Ethernet headers

            reply->eth_hdr.len_ethertype = htons(ETHERTYPE_ARP);

            // Get MAC from NIC (from me)
            pkt->nic->get_mac(reply->eth_hdr.s_mac);
            memcpy(reply->source_mac, reply->eth_hdr.s_mac, 6);

            // Broadcast destination
            memset(reply->eth_hdr.d_mac, 0xFF, 6);

            // To them
            memcpy(reply->target_mac, ap->source_mac, 6);

            reply_pkt->size = sizeof(*reply);

            ARP_TRACE("Reply: Sender=%02x:%02x:%02x:%02x:%02x:%02x"
                      " IP=%d.%d.%d.%d"
                      " Target=%02x:%02x:%02x:%02x:%02x:%02x"
                      " IP=%d.%d.%d.%d\n",
                    reply->source_mac[0],
                    reply->source_mac[1],
                    reply->source_mac[2],
                    reply->source_mac[3],
                    reply->source_mac[4],
                    reply->source_mac[5],
                    reply->sender_ip[0],
                    reply->sender_ip[1],
                    reply->sender_ip[2],
                    reply->sender_ip[3],
                    reply->target_mac[0],
                    reply->target_mac[1],
                    reply->target_mac[2],
                    reply->target_mac[3],
                    reply->target_mac[4],
                    reply->target_mac[5],
                    reply->target_ip[0],
                    reply->target_ip[1],
                    reply->target_ip[2],
                    reply->target_ip[3]);

            // Transmit
            ARP_TRACE("Sending ARP reply\n");
            pkt->nic->send(reply_pkt);
        }
    } else {
        ARP_TRACE("Unrecognized packet, hlen=%d, plen=%d\n",
                  ap->hlen, ap->plen);
    }

    (void)ap;
}

