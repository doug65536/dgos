#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#if 0
#define _packed __attribute__((__packed__))

__BEGIN_DECLS

// IEEE802.3 Ethernet Frame

// Minimum ethernet packet is 6+6+2+64+4=82 bytes
// Maximum is 6+6+2+1500+4=1518 bytes
// 14 bytes
struct ethernet_hdr_t {
    uint8_t d_mac[6];
    uint8_t s_mac[6];
    uint16_t len_ethertype;
    // ... followed by 64 to 1500 byte payload
    // ... followed by 32 bit CRC
} _packed;

// 1518 bytes
struct ethernet_pkt_t {
    // Header
    ethernet_hdr_t hdr;

    // 1500 byte packet plus room for CRC
    uint8_t packet[sizeof(ethernet_hdr_t) + 1500 + sizeof(uint32_t)];
} _packed;

#define ETHERTYPE_IPv4      0x0800
#define ETHERTYPE_ARP       0x0806
#define ETHERTYPE_WOL       0x0842
#define ETHERTYPE_RARP      0x8035
#define ETHERTYPE_IPv6      0x86DD
#define ETHERTYPE_FLOWCTL   0x8808

struct ipv4_hdr_t {
    ethernet_hdr_t eth_hdr;
    uint8_t ver_ihl;
    uint8_t dscp_ecn;
    uint16_t len;
    uint16_t id;
    uint16_t flags_fragofs;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t hdr_checksum;
    uint8_t s_ip[4];
    uint8_t d_ip[4];

    size_t version() const
    {
        return IPv4_VERLEN_VER_GET(ver_ihl);
    }

    size_t header_sz() const
    {
        return IPv4_VERLEN_LEN_GET(ver_ihl) * sizeof(uint32_t);
    }
} _packed;

#define IPV4_PROTO_ICMP     0x01
#define IPV4_PROTO_TCP      0x06
#define IPV4_PROTO_UDP      0x11

struct udp_hdr_t {
    ipv4_hdr_t ipv4_hdr;
    uint16_t s_port;
    uint16_t d_port;
    uint16_t len;
    uint16_t checksum;
} __attribute__((__packed__));

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

void dhcp_builder_begin(void *buf)
{
    dhcp_pkt_t *pkt = reinterpret_cast<dhcp_pkt_t*>(
                memset(buf, 0, sizeof(*pkt)));

    //
    // Ethernet

    // Destination MAC FF:FF:FF:FF:FF:FF
    memset(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac, 0xFF,
           sizeof(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac));

    // EtherType field
    pkt->udp_hdr.ipv4_hdr.eth_hdr.len_ethertype = htons(ETHERTYPE_IPv4);

    //
    // IPv4

    // IPv4 destination IP 255.255.255.255
    memset(pkt->udp_hdr.ipv4_hdr.d_ip, 0xFF,
           sizeof(pkt->udp_hdr.ipv4_hdr.d_ip));

    pkt->udp_hdr.ipv4_hdr.ver_ihl = 0x45;
    pkt->udp_hdr.ipv4_hdr.dscp_ecn = 0;
    pkt->udp_hdr.ipv4_hdr.id = 0;
    pkt->udp_hdr.ipv4_hdr.flags_fragofs = 0;
    pkt->udp_hdr.ipv4_hdr.ttl = 64;
    pkt->udp_hdr.ipv4_hdr.protocol = IPV4_PROTO_UDP;

    // 0.0.0.0
    memset(pkt->udp_hdr.ipv4_hdr.s_ip, 0,
           sizeof(pkt->udp_hdr.ipv4_hdr.s_ip));

    // 255.255.255.255
    memset(pkt->udp_hdr.ipv4_hdr.d_ip, 255,
           sizeof(pkt->udp_hdr.ipv4_hdr.d_ip));

    //
    // UDP

    pkt->udp_hdr.d_port = htons(67);
    pkt->udp_hdr.s_port = htons(68);

    //
    // DHCP

    pkt->op = 1;
    pkt->htype = 1;
    pkt->hlen = 6;
    pkt->hops = 0;
    pkt->xid = htonl(0x3903f326);
    pkt->secs = 0;
    pkt->flags = htons(0x8000);

    pkt->magic_cookie = htonl(0x63825363);
}

void dhcp_builder_s_mac(void *buf, uint8_t const *mac_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t*)buf;

    memcpy(pkt->udp_hdr.ipv4_hdr.eth_hdr.s_mac, mac_addr,
           sizeof(pkt->udp_hdr.ipv4_hdr.eth_hdr.s_mac));
}

void dhcp_builder_d_mac(void *buf, uint8_t const *mac_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    memcpy(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac, mac_addr,
           sizeof(pkt->udp_hdr.ipv4_hdr.eth_hdr.d_mac));
}

void dhcp_builder_set_op(void *buf, uint8_t op)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    pkt->op = op;
}

void dhcp_builder_set_client_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    memcpy(pkt->ci_addr, &ip_addr, sizeof(pkt->gi_addr));
}

void dhcp_builder_set_your_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    memcpy(pkt->yi_addr, &ip_addr, sizeof(pkt->gi_addr));
}

void dhcp_builder_set_server_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    memcpy(pkt->si_addr, &ip_addr, sizeof(pkt->gi_addr));
}

void dhcp_builder_set_gateway_ip(void *buf, uint32_t ip_addr)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    memcpy(pkt->gi_addr, &ip_addr, sizeof(pkt->gi_addr));
}

int dhcp_builder_add_option(void *buf, uint8_t option)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    for (size_t i = 0; i < countof(pkt->options); ) {
        if (pkt->options[i] == 0) {
            pkt->options[i] = option;
            pkt->options[i + 1] = 0;
            return i + 2;
        }

        i = (i + pkt->options[i + 1] + 3) & -2;
    }
    return 0;
}

int dhcp_builder_add_option_param(void *buf, uint8_t option, uint8_t param)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    for (size_t i = 0; i < countof(pkt->options); ) {
        if (pkt->options[i] == option) {
            size_t params = pkt->options[i + 1]++;
            pkt->options[i + 2 + params] = param;
            return 2 + params + 1;
        }

        i = (i + pkt->options[i + 1] + 3) & -2;
    }
    return 0;
}

int dhcp_builder_add_option_params(void *buf, uint8_t option,
                                    void const *data, size_t bytes)
{
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    for (size_t i = 0; i < countof(pkt->options); ) {
        if (pkt->options[i] == option) {
            size_t params = pkt->options[i + 1];
            pkt->options[i + 1] += bytes;
            memcpy(&pkt->options[i + 2 + params], data, bytes);
            return 2 + params + bytes;
        }

        i = (i + pkt->options[i + 1] + 3) & -2;
    }
    return 0;
}

uint16_t dhcp_builder_finalize(void *buf)
{
    int option_len = (dhcp_builder_add_option(buf, 255) + 1) & -2;

    dhcp_pkt_t *pkt = (dhcp_pkt_t *)buf;

    char const *end = (char const *)&pkt->options[option_len];

    udp_finalize(&pkt->udp_hdr, end);

    return end - (char*)pkt;
}

uint16_t dhcp_build_discover(void *buf, uint8_t const *mac_addr)
{
    dhcp_pkt_t *discover = (dhcp_pkt_t *)buf;

    dhcp_builder_begin(discover);
    dhcp_builder_s_mac(discover, mac_addr);

    // Type
    dhcp_builder_add_option(discover, DHCP_OPT_DHCP_TYPE);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_DHCP_TYPE, DHCP_TYPE_DISCOVER);

    // Parameter request
    dhcp_builder_add_option(discover, DHCP_OPT_PARM_REQ);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_ROUTER);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_SUPPLY_MASK);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_DOMAIN);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_DNS_SRV);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_IP_TTL);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_HOSTNAME);

    dhcp_builder_add_option_param(
                discover, DHCP_OPT_PARM_REQ, DHCP_OPT_IP_LEASE_TIME);

    return dhcp_builder_finalize(discover);
}
#endif

int main()
{
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);


}
