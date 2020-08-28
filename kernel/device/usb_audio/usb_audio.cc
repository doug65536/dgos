#include "usb_audio.h"
#include "dev_usb_ctl.h"

#include "usb_audio.bits.h"

// Audio interface class and subclass defines
#define USB_AUDIO_IF_CLASS 1
#define USB_AUDIO_IF_SCLS_CONTROL   1
#define USB_AUDIO_IF_SCLS_AUDIOSTREAMING 2
#define USB_AUDIO_IF_SCLS_MIDISTREAMING   3

#define USB_AUDIO_DATA_TYPE_1           0
#define USB_AUDIO_DATA_TYPE_1_PCM       1
#define USB_AUDIO_DATA_TYPE_1_PCM8      2
#define USB_AUDIO_DATA_TYPE_1_IEEEFLT   3
#define USB_AUDIO_DATA_TYPE_1_ALAW      4
#define USB_AUDIO_DATA_TYPE_1_MULAW     5

#define USB_AUDIO_DATA_TYPE_2           0x1000
#define USB_AUDIO_DATA_TYPE_2_MPEG      0x1001
#define USB_AUDIO_DATA_TYPE_2_AC3       0x1002

#define USB_AUDIO_DATA_TYPE_3           0x2000
#define USB_AUDIO_DATA_TYPE_3_AC3       0x2001
#define USB_AUDIO_DATA_TYPE_3_MPEGL1    0x2002
#define USB_AUDIO_DATA_TYPE_3_MPEGL23N  0x2003
#define USB_AUDIO_DATA_TYPE_3_MPEGL2E   0x2004
#define USB_AUDIO_DATA_TYPE_3_MPEGL1LS  0x2005
#define USB_AUDIO_DATA_TYPE_3_MPEGL23LS 0x2006

#define USB_AUDIO_EP_DESCTYPE_ENDPOINT  (uint8_t(usb_desctype_t::ENDPOINT)|0x20)

class usb_audio_dev_t {

};

class usb_audio_class_drv_t final : public usb_class_drv_t {
protected:
    usb_audio_class_drv_t() = default;
    ~usb_audio_class_drv_t() = default;

    static usb_audio_class_drv_t usb_audio;

    // usb_class_drv_t interface
    virtual bool probe(usb_config_helper *cfg, usb_bus_t *bus) override final;

    virtual char const *name() const override final;
};

usb_audio_class_drv_t usb_audio_class_drv_t::usb_audio;

static ext::vector<usb_audio_dev_t*> audio_devs;

struct usb_audio_cs_iface {
    usb_desc_hdr_t hdr;
    uint8_t sub_type;
    uint8_t format_type;
    uint8_t nr_channels;
    uint8_t subframe_sz;
    uint8_t bit_res;
    uint8_t samp_freq_type;
    union samp_freqs {
        // Array of 3-byte (24-bit) values, alternating min/max, N entries
        uint8_t ranges[1][2][3];

        uint8_t discrete[1][3];
    } samp_freq_tbl;
};

bool usb_audio_class_drv_t::probe(usb_config_helper *cfg, usb_bus_t *bus)
{
    match_result match;

    // Try to find usb audio interface
    for (size_t index = 0; ; ++index) {
        match = match_config(cfg, index, int(usb_class_t::audio),
                             // subclass
                             -1,// USB_AUDIO_IF_SCLS_AUDIOSTREAMING,
                             // proto, ifaceproto, vendor, device
                             -1, -1, -1, -1);

        if (!match.dev)
            return false;

        if (match.iface)
            break;
    }

    usb_pipe_t audio_strm;

    usb_desc_ep const *ep_desc;
    for (size_t index = 0; ; ++index) {
        usb_desc_config const *cfg_desc = cfg->find_config(0);
        usb_audio_cs_iface const *cfg_iface = (usb_audio_cs_iface const *)
                cfg->find_iface(cfg_desc, 1);
        usb_desc_ep const *ep = cfg->find_ep((usb_desc_iface*)cfg_iface, 0);

        //bus->alloc_pipe(cfg->slot(), match.iface, ep_desc, audio_strm);

        break;
    }

    printdbg("Found usb_audio interface: C=%#x S=%#x P=%#x\n",
             match.iface->iface_class,
             match.iface->iface_subclass,
             match.iface->iface_proto);

    usb_pipe_t control, in, out;

    bus->get_pipe(cfg->slot(), 0, control);

    assert(control);

    // Try to find interrupt pipes
    usb_desc_ep const *in_ep_desc = cfg->match_ep(
                match.iface, 1, usb_ep_attr::interrupt);

    usb_desc_ep const *out_ep_desc = cfg->match_ep(
                match.iface, 0, usb_ep_attr::interrupt);

    if (in_ep_desc) {
        bus->alloc_pipe(cfg->slot(), match.iface, in_ep_desc, in);
        assert(in);
    }

    if (out_ep_desc) {
        bus->alloc_pipe(cfg->slot(), match.iface, out_ep_desc, out);
        assert(out);
    }

    usb_audio_dev_t *dev = nullptr;

    // Protocol
    int iface = match.iface->iface_proto;

//    if (match.iface->iface_proto == 1) {
//        dev = new (ext::nothrow)
//                usb_hid_keybd_t(control, in, out, match.iface_idx);
//    } else if (match.iface->iface_proto == 2) {
//        dev = new (ext::nothrow)
//                usb_hid_mouse_t(control, in, out, match.iface_idx);
//    } else {
//        USBHID_TRACE("Unhandled interface protocol: 0x%02x\n",
//                     match.iface->iface_proto);
//    }

    if (dev) {
        if (unlikely(!audio_devs.push_back(dev)))
            panic_oom();
    }

    return dev != nullptr;
}

char const *usb_audio_class_drv_t::name() const
{
    return "usb_audio";
}
