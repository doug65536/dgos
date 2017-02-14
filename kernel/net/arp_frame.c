#include "arp_frame.h"
#include "arp.h"
#include "printk.h"

#define ARP_DEBUG   1
#if ARP_DEBUG
#define ARP_TRACE(...) printdbg("ARP: " __VA_ARGS__)
#else
#define ARP_TRACE(...) ((void)0)
#endif

void arp_frame_received(ethq_pkt_t *pkt)
{
    arp_packet_t *ap = (void*)&pkt->pkt;

    if (ap->hlen == 6 && ap->plen == 4) {
        ARP_TRACE("Sender=%02x:%02x:%02x:%02x:%02x:%02x"
                  " IP=%d.%d.%d.%d"
                  " Target=%02x:%02x:%02x:%02x:%02x:%02x"
                  " IP=%d.%d.%d.%d\n",
                  ap->s_addr[0],
                ap->s_addr[1],
                ap->s_addr[2],
                ap->s_addr[3],
                ap->s_addr[4],
                ap->s_addr[5],
                ap->sender_ip[0],
                ap->sender_ip[1],
                ap->sender_ip[2],
                ap->sender_ip[3],
                ap->d_addr[0],
                ap->d_addr[1],
                ap->d_addr[2],
                ap->d_addr[3],
                ap->d_addr[4],
                ap->d_addr[5],
                ap->target_ip[0],
                ap->target_ip[1],
                ap->target_ip[2],
                ap->target_ip[3]);
    } else {
        ARP_TRACE("Unrecognized packet, hlen=%d, plen=%d\n",
                  ap->hlen, ap->plen);
    }

    (void)ap;
}

