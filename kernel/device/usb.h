#pragma once
#include "types.h"
#include "vector.h"
#include "assert.h"

//
// USB Descriptor types (USB 3.1 spec, Table 9.5)

enum struct usb_rqcode_t : uint8_t {
    GET_STATUS           =  0,
    CLEAR_FEATURE        =  1,
    SET_FEATURE          =  3,
    SET_ADDRESS          =  5,
    GET_DESCRIPTOR       =  6,
    SET_DESCRIPTOR       =  7,
    GET_CONFIGURATION    =  8,
    SET_CONFIGURATION    =  9,
    GET_INTERFACE        = 10,
    SET_INTERFACE        = 11,
    SYNCH_FRAME          = 12,
    SET_ENCRYPTION       = 13,
    GET_ENCRYPTION       = 14,
    SET_HANDSHAKE        = 15,
    GET_HANDSHAKE        = 16,
    SET_CONNECTION       = 17,
    SET_SECURITY_DATA    = 18,
    GET_SECURITY_DATA    = 19,
    SET_WUSB_DATA        = 20,
    LOOPBACK_DATA_WRITE  = 21,
    LOOPBACK_DATA_READ   = 22,
    SET_INTERFACE_DS     = 23,
    SET_SEL              = 48,
    SET_ISOCH_DELAY      = 49,
};

enum struct usb_featcode_t : uint8_t {
    ENDPOINT_HALT = 0,
    FUNCTION_SUSPEND = 0,
    DEVICE_REMOTE_WAKEUP = 1,
    TEST_MODE = 2,
    b_hnp_enable = 3,
    a_hnp_support = 4,
    a_alt_hnp_support = 5,
    WUSB_DEVICE = 6,
    U1_ENABLE = 48,
    U2_ENABLE = 49,
    LTM_ENABLE = 50,
    B3_NTF_HOST_REL = 51,
    B3_RSP_ENABLE = 52,
    LDM_ENABLE = 53
};

enum struct usb_req_type : uint8_t {
    STD     = 0,
    CLASS   = 1,
    VENDOR  = 2
};

enum struct usb_req_recip_t : uint8_t {
    DEVICE      = 0,
    INTERFACE   = 1,
    CLASS       = 2,
    ENDPOINT    = 3,
    OTHER       = 3,
    VENDOR      = 31
};

//
// Descriptor Types (USB 3.1 spec, Table 9-6)

enum struct usb_desctype_t : uint8_t {
    DEVICE                     =  1,
    CONFIGURATION              =  2,
    STRING                     =  3,
    INTERFACE                  =  4,
    ENDPOINT                   =  5,
    INTERFACE_POWER            =  8,
    OTG                        =  9,
    DEBUG                      = 10,
    INTERFACE_ASSOCIATION      = 11,
    BOS                        = 15,
    DEVICE_CAPABILITY          = 16,
    SS_EP_COMPANION            = 48,
    SSPLUS_ISOCH_EP_COMPANION  = 49,

    // Class types

    HUB_SS = 0x2A
};

enum struct usb_dir_t : uint8_t {
    OUT = 0,
    IN = 0x80
};

enum struct usb_class_t : uint8_t {
    audio = 0x01,
    comm = 0x02,
    hid = 0x03,
    physical = 0x05,
    image = 0x06,
    printer = 0x07,
    mass_storage = 0x08,
    hub = 0x09,
    cdc_data = 0x0A,
    smart_card = 0x0B,
    content_sec = 0x0D,
    video = 0x0E,
    healthcare = 0x0F,
    av = 0x10,
    billboard = 0x11,
    typec_bridge = 0x12,
    diag = 0xDC,
    wireless_ctrl = 0xE0,
    misc = 0xEF,
    app_specific = 0xFE,
    vendor_specific = 0xFF
};

//
// USB Device Descriptor

struct usb_desc_hdr_t {
    // Length of descriptor
    uint8_t len;

    // USB_DESCTYPE_*
    usb_desctype_t desc_type;
} _packed;

struct usb_desc_device {
    usb_desc_hdr_t hdr;

    // USB spec, USB2.1=0x210, etc
    uint16_t usb_spec;

    // Device class, subclass, and protocol, else 0 if interface specific
    uint8_t dev_class;
    uint8_t dev_subclass;
    uint8_t dev_protocol;

    // USB3, log2(size), USB < 3, size in bytes
    uint8_t maxpktsz;

    // Vendor, product, revision
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t product_ver;

    // Indices into string descriptor
    uint8_t manufact_stridx;
    uint8_t vendor_stridx;
    uint8_t serial_stridx;

    // Number of possible configurations
    uint8_t num_config;
} _packed;

C_ASSERT(sizeof(usb_desc_device) == 18);

//
//  Configuration descriptor

struct usb_desc_config {
    usb_desc_hdr_t hdr;

    uint16_t total_len;
    uint8_t num_iface;
    uint8_t cfg_value;
    uint8_t cfg;
    uint8_t attr;
    uint8_t max_power;
} _packed;

//
// Interface descriptor

struct usb_desc_iface {
    usb_desc_hdr_t hdr;

    uint8_t iface_num;
    uint8_t alt_setting;
    uint8_t num_ep;
    uint8_t iface_class;
    uint8_t iface_subclass;
    uint8_t iface_proto;
    uint8_t iface_index;
} _packed;

//
// Endpoint descriptor


enum struct usb_cc_t : uint8_t {
    invalid,
    success,
    data_buf_err,   // can't keep up with tx or rx
    babble,
    usb_txn_err,
    trb_err,
    stall_err,
    resource_err,
    bw_err,
    no_slots_avail,
    invalid_stream_type,
    slot_not_enabled,
    ep_not_enabled,
    short_pkt,
    ring_underrun,
    ring_overrun,
    vf_evt_ring_full,
    parameter_err,
    bw_overrun,
    ctx_state_err,
    no_ping_response,
    evt_ring_full,
    incompatible_device,
    missed_service,
    cmd_ring_stopped,
    cmd_aborted,
    stopped,
    stopped_len_invalid,
    stopped_short_pkt,
    max_exit_lat_too_large,
    reserved,
    isoch_buf_overrun,
    event_lost,
    undefined_err,
    invalid_stream_id,
    secondary_bw_err,
    split_txn_err
    // 192-223 vendor defined error
    // 224-255 vendor defined info
};

enum struct usb_ep_attr : uint8_t {
    control,
    isoch,
    bulk,
    interrupt
};

enum struct usb_ep_state_t : uint8_t {
    disabled,
    running,
    stopped,
    halted,
    error
};

struct usb_desc_ep {
    usb_desc_hdr_t hdr;

    uint8_t ep_addr;
    usb_ep_attr ep_attr;
    uint16_t max_packet_sz;
    uint8_t interval;
} _packed;

C_ASSERT(sizeof(usb_desc_ep) == 7);

struct usb_desc_ep_companion {
    usb_desc_hdr_t hdr;

    uint8_t max_burst;
} _packed;

// Binary device object store (BOS)
struct usb_desc_bos {
    usb_desc_hdr_t hdr;

    uint16_t total_len;
    uint8_t num_device_caps;
} _packed;

C_ASSERT(sizeof(usb_desc_bos) == 5);

enum struct usb_dev_cap_type : uint8_t {
    wireless_usb = 0x1,
    usb2ext = 0x2,
    superspeed_usb = 0x3,
    container_id = 0x4,
    platform = 0x5,
    power_deliv_cap = 0x6,
    bat_info_cap = 0x7,
    pd_consumer_port_cap = 0x8,
    pd_provider_port_cap = 0x9,
    superspeed_plus = 0xA,
    precision_time_meas = 0xB,
    wireless_usb_ext = 0xC
};

struct usb_dev_cap_hdr_t {
    usb_desc_hdr_t hdr;
    usb_dev_cap_type cap_type;
} _packed;

template<usb_dev_cap_type cap_type>
struct usb_dev_cap
{
    struct pod;
};

// 9.6.2.1
template<>
struct usb_dev_cap<usb_dev_cap_type::superspeed_usb>
{
    struct pod {
        usb_dev_cap_hdr_t hdr;
        uint8_t attr;               // bit 1 = LTM capable
        uint16_t spd_support;
        uint8_t func_support;       // minimum speed supported
        uint8_t u1_dev_exit_lat;
        uint16_t u2_dev_exit_lat;
    } _packed;
};

// 9.6.2.2
template<>
struct usb_dev_cap<usb_dev_cap_type::usb2ext>
{
    struct pod {
        usb_dev_cap_hdr_t hdr;
        uint8_t attr;
    } _packed;
};

// 9.6.2.3
template<>
struct usb_dev_cap<usb_dev_cap_type::container_id>
{
    struct pod {
        usb_dev_cap_hdr_t hdr;
    } _packed;
};

// 9.6.2.4
template<>
struct usb_dev_cap<usb_dev_cap_type::platform>
{
    struct pod {
        usb_dev_cap_hdr_t hdr;
        uint8_t reserved;
        uint8_t uuid[16];
        // Followed by variable length platform specific data
    } _packed;
};

// 9.6.2.5
template<>
struct usb_dev_cap<usb_dev_cap_type::superspeed_plus>
{
    struct pod {
        usb_dev_cap_hdr_t hdr;
        uint8_t reserved;
        uint8_t attr;
        uint16_t func_support;
        uint16_t reserved2;
        uint32_t sublink_speed_attr;
        // Followed by variable number of additional sublink speed attributes
    } _packed;
};

// 9.5.2.6
template<>
struct usb_dev_cap<usb_dev_cap_type::precision_time_meas>
{
    struct pod {
        usb_dev_cap_hdr_t hdr;
    } _packed;
};

class usb_config_helper {
public:
    usb_config_helper(int slotid, usb_desc_device const& dev_desc,
                      usb_desc_bos *bos, void *data, size_t len);

    int slot() const;
    usb_desc_device const& device() const;
    usb_desc_config const *find_config(int cfg_index) const;
    static usb_desc_iface const *find_iface(
            usb_desc_config const *cfg, int iface_index);
    static usb_desc_ep const *find_ep(
            usb_desc_iface const *iface, int ep_index);

    static usb_desc_iface const *match_iface(
            usb_desc_config const *cfg, int protocol);

    static usb_desc_ep const *match_ep(usb_desc_iface const *iface,
                                       int dir, usb_ep_attr attr);

    usb_desc_ep_companion const *get_ep_companion(usb_desc_ep const *ep);

    static char const *class_code_text(uint8_t cls);
    static char const *ep_attr_text(usb_ep_attr attr);

    template<usb_dev_cap_type cap_type>
    typename usb_dev_cap<cap_type>::pod const *get_bos(int index) const
    {
        void *search = get_bos_raw(cap_type, index);
        return (typename usb_dev_cap<cap_type>::pod *)search;
    }

    void const* get_bos_raw(usb_dev_cap_type cap_type, int index = 0) const;

private:
    usb_desc_device const dev_desc;
    void const * const data;
    usb_desc_bos *bos;
    int len;
    int slotid;
};

struct usb_hub_desc {
    usb_desc_hdr_t hdr;

    uint8_t num_ports;
    uint16_t characteristics;
    uint8_t pwr2pwr_good;   // in 2ms increments
    uint8_t current;
    uint8_t hdr_decode_lat;
    uint16_t hub_delay;
    uint16_t removable;
} _packed;

C_ASSERT(sizeof(usb_hub_desc) == 12);

// Interface association

struct usb_iface_assoc {
    usb_desc_hdr_t hdr;

    uint8_t first_iface;
    uint8_t num_iface;
    uint8_t func_class;
    uint8_t func_subclass;
    uint8_t func_proto;
    uint8_t func_index;
} _packed;

C_ASSERT(sizeof(usb_iface_assoc) == 8);
