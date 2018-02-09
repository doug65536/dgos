#include "udp_frame.h"
#include "udp.h"
#include "stdlib.h"
#include "rbtree.h"
#include "hash_table.h"
#include "cpu/atomic.h"

struct udp_bind_t {
    ipv4_addr_pair_t pair;
    int handle;
};

static hashtbl_t udp_handle_lookup;
static hashtbl_t udp_addr_lookup;
static int next_handle;

__attribute__((constructor))
static void udp_handle_tbl_init(void)
{
    htbl_create(&udp_addr_lookup,
                offsetof(udp_bind_t, pair),
                sizeof(ipv4_addr_pair_t));
    htbl_create(&udp_handle_lookup,
                offsetof(udp_bind_t, handle),
                sizeof(int));
    next_handle = 1;
}

void udp_frame_received(ethq_pkt_t *pkt)
{
    udp_hdr_t const *p = (udp_hdr_t*)pkt;

    ipv4_addr_pair_t pair;

    udp_port_get(&pair, p);
    ipv4_ip_get(&pair, &p->ipv4_hdr);

    udp_bind_t *bind = (udp_bind_t*)htbl_lookup(&udp_addr_lookup, &pair);

    (void)bind;
    (void)p;
}

int udp_bind(ipv4_addr_pair_t const *pair)
{
    int handle = atomic_xadd(&next_handle, 1);
    udp_bind_t *bind = (udp_bind_t*)malloc(sizeof(*bind));
    bind->handle = handle;
    bind->pair = *pair;
    return htbl_insert(&udp_handle_lookup, bind) &&
            htbl_insert(&udp_addr_lookup, bind);
}
