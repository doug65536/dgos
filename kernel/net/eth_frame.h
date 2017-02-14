#pragma once
#include "eth_q.h"

void eth_frame_received(ethq_pkt_t *pkt);
void eth_frame_transmit(ethq_pkt_t *pkt);
