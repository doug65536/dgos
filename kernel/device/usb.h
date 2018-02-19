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

    // 4=max 16 bytes, 12=max 4kb, 64=unlimited, etc
    uint8_t log2_maxpktsz;

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
};

//
// Endpoint descriptor

struct usb_desc_ep {
    // Length of descriptor
    uint8_t len;

    // USB_DESCTYPE_*
    usb_desctype_t desc_type;

    uint8_t ep_addr;
    uint8_t ep_attr;
    uint16_t max_packet_sz;
    uint8_t interval;
};

class usb_config_helper {
public:
    usb_config_helper(void *data, size_t len);

    usb_desc_config *find_config(int cfg_index);
    static usb_desc_iface *find_iface(usb_desc_config *cfg, int iface_index);
    static usb_desc_ep *find_ep(usb_desc_iface *iface, int ep_index);

    static char const *class_code_text(uint8_t cls);

private:
    void *data;
    int len;
};
