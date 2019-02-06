#pragma once
#include "eth_q.h"
#include "udp.h"

__BEGIN_DECLS

void udp_frame_received(ethq_pkt_t *pkt);

int udp_bind(ipv4_addr_pair_t const *pair);

__END_DECLS
