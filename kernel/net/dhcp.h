#pragma once
#include "net/udp.h"

__BEGIN_DECLS

// TODO: Move this to user mode

struct dhcp_pkt_t {
    udp_hdr_t udp_hdr;

    // discover=1, offer=2
    uint8_t op;

    // discover=1, offer=1
    uint8_t htype;

    // 6
    uint8_t hlen;

    // 0
    uint8_t hops;

    // 0x3903F326
    uint32_t xid;

    // 0
    uint16_t secs;

    // discover=0x8000, offer=0
    uint16_t flags;

    // Client IP address
    uint8_t ci_addr[4];

    // Your IP address, offer=assigned address
    uint8_t yi_addr[4];

    // Server IP address, offer=server address
    uint8_t si_addr[4];

    // Gateway IP address, offer=0
    uint8_t gi_addr[4];

    // Client MAC address
    uint8_t ch_addr[16];

    uint8_t overflow[192];

    // 0x63825363
    uint32_t magic_cookie;

    char options[32];

    // ... followed by DHCP options
    //  53: 1 (DHCP Discover)
    //  50: 192.168.1.100 (requested)
    //  55: Parameter request list
    //  1: (Request subnet mask), offer=subnet mask
    //  3: (Router), offer=router ip address
    //  15: (Domain name)
    //  6: (DNS server), offer=dns server list
} _packed;

void dhcp_builder_begin(void *buf);
void dhcp_builder_s_mac(void *buf, uint8_t const *mac_addr);
void dhcp_builder_d_mac(void *buf, uint8_t const *mac_addr);
void dhcp_builder_set_op(void *buf, uint8_t op);
void dhcp_builder_set_client_ip(void *buf, uint32_t ip_addr);
void dhcp_builder_set_your_ip(void *buf, uint32_t ip_addr);
void dhcp_builder_set_server_ip(void *buf, uint32_t ip_addr);
void dhcp_builder_set_gateway_ip(void *buf, uint32_t ip_addr);
int dhcp_builder_add_option(void *buf, uint8_t option);
int dhcp_builder_add_option_param(void *buf,uint8_t option, uint8_t param);
int dhcp_builder_add_option_params(void *buf, uint8_t option,
                                   void const *data, size_t bytes);
uint16_t dhcp_builder_finalize(void *buf);

uint16_t dhcp_build_discover(void *buf, uint8_t const *mac_addr);

// RFC 1533
#define DHCP_OPT_END            255
#define DHCP_OPT_SUBNET_MASK    1
#define DHCP_OPT_TIME_OFS       2
#define DHCP_OPT_ROUTER         3
#define DHCP_OPT_TIME_SRV       4
#define DHCP_OPT_NAME_SRV       5
#define DHCP_OPT_DNS_SRV        6
#define DHCP_OPT_LOG_SRV        7
#define DHCP_OPT_COOKIE_SRV     8
#define DHCP_OPT_LPR_SRV        9
#define DHCP_OPT_IMPRESS_SRV    10
#define DHCP_OPT_RSRCLOC_SRV    11
#define DHCP_OPT_HOSTNAME       12
#define DHCP_OPT_BOOTFILE       13
#define DHCP_OPT_DUMP_PATH      14
#define DHCP_OPT_DOMAIN         15
#define DHCP_OPT_SWAP_SRV       16
#define DHCP_OPT_ROOT_PATH      17
#define DHCP_OPT_EXT_PATH       18
#define DHCP_OPT_IP_FORW        19
#define DHCP_OPT_SRC_ROUTE      20
#define DHCP_OPT_POL_FILTER     21
#define DHCP_OPT_MAX_REASSY     22
#define DHCP_OPT_IP_TTL         23
#define DHCP_OPT_MTU_AGING      24
#define DHCP_OPT_MTU_PLATEAU    25
#define DHCP_OPT_MTU            26
#define DHCP_OPT_ALL_SUB_LOCAL  27
#define DHCP_OPT_BCAST_ADDR     28
#define DHCP_OPT_DISCOVER_MASK  29
#define DHCP_OPT_SUPPLY_MASK    30
#define DHCP_OPT_DISC_ROUTER    31
#define DHCP_OPT_ROUTER_SOLIC   32
#define DHCP_OPT_STATIC_ROUTE   33
#define DHCP_OPT_ARP_TRAILER    34
#define DHCP_OPT_ARP_TIMEOUT    35
#define DHCP_OPT_IEEE_802_3     36
#define DHCP_OPT_TCP_TTL        37
#define DHCP_OPT_TCP_KA         38
#define DHCP_OPT_TCP_KA_GARB    39
#define DHCP_OPT_NIS_DOMAIN     40
#define DHCP_OPT_NIS_SERVERS    41
#define DHCP_OPT_NTP_SERVERS    42
#define DHCP_OPT_VENDOR_SPEC    43
#define DHCP_OPT_NBT_NAMESRV    44
#define DHCP_OPT_NBT_NBDD_SRV   45
#define DHCP_OPT_NBT_NODE_TYPE  46
#define DHCP_OPT_NBT_SCOPE      47
#define DHCP_OPT_XW_FONT_SRV    48
#define DHCP_OPT_XW_DISPLAY     49
#define DHCP_OPT_DHCP_RQ_ADDR   50
#define DHCP_OPT_IP_LEASE_TIME  51
#define DHCP_OPT_OPT_OVERLOAD   52
#define DHCP_OPT_DHCP_TYPE      53
#define DHCP_OPT_DHCP_SRV       54
#define DHCP_OPT_PARM_REQ       55
#define DHCP_OPT_MESSAGE        56
#define DHCP_OPT_MAX_DHCP_MSG   57
#define DHCP_OPT_DHCP_RENEWAL   58
#define DHCP_OPT_DHCP_REBIND    59
#define DHCP_OPT_CLSID          60
#define DHCP_OPT_CLIID          61

#define DHCP_TYPE_DISCOVER      1
#define DHCP_TYPE_OFFER         2
#define DHCP_TYPE_REQUEST       3
#define DHCP_TYPE_DECLINE       4
#define DHCP_TYPE_ACK           5
#define DHCP_TYPE_NACK          6
#define DHCP_TYPE_RELEASE       7

__END_DECLS
