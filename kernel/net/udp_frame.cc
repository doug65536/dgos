#include "udp_frame.h"
#include "udp.h"
#include "stdlib.h"
#include "rbtree.h"
#include "hash_table.h"
#include "cpu/atomic.h"
#include "refcount.h"
#include "export.h"

struct udp_bind_t : public refcounted<udp_bind_t> {
    ipv4_addr_pair_t pair;
    int handle;
};

static ref_hashtbl_t<udp_bind_t, ipv4_addr_pair_t,
    &udp_bind_t::pair> udp_handle_lookup;
static ref_hashtbl_t<udp_bind_t, int, &udp_bind_t::handle> udp_addr_lookup;
static int next_handle;

__attribute__((constructor))
static void udp_handle_tbl_init(void)
{
    next_handle = 1;
}

EXPORT void udp_frame_received(ethq_pkt_t *pkt)
{
    udp_hdr_t const *p = (udp_hdr_t*)pkt;

    ipv4_addr_pair_t pair;

    udp_port_get(&pair, p);
    ipv4_ip_get(&pair, &p->ipv4_hdr);

    udp_bind_t *bind = udp_addr_lookup.lookup(&pair);

    (void)bind;
    (void)p;
}

EXPORT bool udp_bind(ipv4_addr_pair_t const *pair)
{
    int handle = atomic_xadd(&next_handle, 1);
    udp_bind_t *bind = (udp_bind_t*)malloc(sizeof(*bind));
    if (unlikely(!bind))
        return false;
    bind->handle = handle;
    bind->pair = *pair;
    return udp_handle_lookup.insert(bind) && udp_addr_lookup.insert(bind);
}
