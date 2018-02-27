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

struct usb_desc_device {
    // Length of descriptor
    uint8_t len;

    // USB_DESCTYPE_*
    uint8_t desc_type;

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
} __packed;

C_ASSERT(sizeof(usb_desc_device) == 18);

//
//  Configuration descriptor

struct usb_desc_config {
    // Length of descriptor
    uint8_t len;

    // USB_DESCTYPE_*
    usb_desctype_t desc_type;

    uint16_t total_len;
    uint8_t num_iface;
    uint8_t cfg_value;
    uint8_t cfg;
    uint8_t attr;
    uint8_t max_power;
} __packed;

//
// Interface descriptor

struct usb_desc_iface {
    // Length of descriptor
    uint8_t len;

    // USB_DESCTYPE_*
    usb_desctype_t desc_type;

    uint8_t iface_num;
    uint8_t alt_setting;
    uint8_t num_ep;
    uint8_t iface_class;
    uint8_t iface_subclass;
    uint8_t iface_proto;
    uint8_t iface_index;
} __packed;

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
    // Length of descriptor
    uint8_t len;

    // USB_DESCTYPE_*
    usb_desctype_t desc_type;

    uint8_t ep_addr;
    usb_ep_attr ep_attr;
    uint16_t max_packet_sz;
    uint8_t interval;
} __packed;

C_ASSERT(sizeof(usb_desc_ep) == 7);

class usb_config_helper {
public:
    usb_config_helper(int slotid, usb_desc_device const& dev_desc,
                      void *data, size_t len);

    int slot() const;
    usb_desc_device const& device() const;
    usb_desc_config const *find_config(int cfg_index) const;
    static usb_desc_iface const *find_iface(
            usb_desc_config const *cfg, int iface_index);
    static usb_desc_ep const *find_ep(
            usb_desc_iface const *iface, int ep_index);

    static char const *class_code_text(uint8_t cls);
    static char const *ep_attr_text(usb_ep_attr attr);

private:
    usb_desc_device const dev_desc;
    void const * const data;
    int len;
    int slotid;
};

struct usb_hub_desc {
    uint8_t len;

    uint8_t desc_type;  // 0x2A for enhanced superspeed hub
    uint8_t num_ports;
    uint16_t characteristics;
    uint8_t pwr2pwr_good;   // in 2ms increments
    uint8_t current;
    uint8_t hdr_decode_lat;
    uint16_t hub_delay;
    uint16_t removable;
} __packed;

C_ASSERT(sizeof(usb_hub_desc) == 12);

// Interface association

struct usb_iface_assoc {
    uint8_t len;

    // usb_desctype_t::INTERFACE_ASSOCIATION
    uint8_t desc_type;

    uint8_t first_iface;
    uint8_t num_iface;
    uint8_t func_class;
    uint8_t func_subclass;
    uint8_t func_proto;
    uint8_t func_index;
} __packed;

C_ASSERT(sizeof(usb_iface_assoc) == 8);
