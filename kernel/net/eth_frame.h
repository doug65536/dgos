#pragma once
#include "eth_q.h"

__BEGIN_DECLS

void eth_frame_received(ethq_pkt_t *pkt);
void eth_frame_transmit(ethq_pkt_t *pkt);

__END_DECLS
