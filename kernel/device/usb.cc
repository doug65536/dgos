#include "usb.h"
#include "export.h"

EXPORT usb_config_helper::usb_config_helper(int slotid,
                                     usb_desc_device const& dev_desc,
                                     usb_desc_bos *bos,
                                     void *data, size_t len)
    : dev_desc(dev_desc)
    , data(data)
    , bos(bos)
    , len(len)
    , slotid(slotid)
{
}

EXPORT int usb_config_helper::slot() const
{
    return slotid;
}

EXPORT usb_desc_device const &usb_config_helper::device() const
{
    return dev_desc;
}

EXPORT usb_desc_config const *usb_config_helper::find_config(
        size_t cfg_index) const
{
    if (unlikely(!data))
        return nullptr;

    usb_desc_config const *cfg = (usb_desc_config*)data;

    size_t index = 0;
    size_t ofs = 0;
    bool is_cfg = false;
    while (ofs < len && cfg->hdr.len && cfg->hdr.len != 0xFF &&
           (!(is_cfg = mask_desctype_cs(cfg->hdr.desc_type) ==
           usb_desctype_t::CONFIGURATION) || index < cfg_index)) {
        ofs += cfg->total_len;
        cfg = (usb_desc_config*)((char*)cfg + cfg->total_len);
        index += is_cfg;
        is_cfg = false;
    }

    assert(!is_cfg ||
           (cfg && is_cfg));

    return is_cfg ? cfg : nullptr;
}

EXPORT usb_desc_iface const *usb_config_helper::find_iface(
        usb_desc_config const *cfg, int iface_index)
{
    if (unlikely(!cfg || iface_index >= cfg->num_iface || iface_index < 0))
        return nullptr;

    usb_desc_iface const *iface_st = (usb_desc_iface const*)
            ((char*)cfg + cfg->hdr.len);
    usb_desc_iface const *iface_en = iface_st + cfg->num_iface;
    usb_desc_iface const *iface = iface_st;
    int index = 0;
    bool is_iface = false;
    while (iface < iface_en && iface->hdr.len && iface->hdr.len != 0xFF &&
           (!(is_iface = mask_desctype_cs(iface->hdr.desc_type) ==
            usb_desctype_t::INTERFACE) || index < iface_index)) {
        iface = (usb_desc_iface*)((char*)iface + iface->hdr.len);
        index += is_iface;
        is_iface = false;
    }

    assert(!is_iface || iface);

    return is_iface ? iface : nullptr;
}

EXPORT usb_desc_ep const *usb_config_helper::find_ep(
        usb_desc_iface const *iface, size_t ep_index)
{
    if (unlikely(!iface))
        return nullptr;

    usb_desc_ep const *ep_st = (usb_desc_ep*)
            ((char const *)iface + iface->hdr.len);

    usb_desc_ep const *ep = ep_st;

    size_t index = 0;

    bool is_ep = false;
    while (ep->hdr.len && ep->hdr.len != 0xFF &&
           (!(is_ep = mask_desctype_cs(ep->hdr.desc_type) ==
              usb_desctype_t::ENDPOINT) || index < ep_index)) {
        ep = (usb_desc_ep*)((char*)ep + ep->hdr.len);
        index += is_ep;
        is_ep = false;
    }

    assert(!is_ep || ep);

    return is_ep ? ep : nullptr;
}

EXPORT usb_desc_iface const *usb_config_helper::match_iface(
        usb_desc_config const* cfg, int protocol)
{
    for (int index = 0; index < cfg->num_iface; ++index) {
        usb_desc_iface const* iface = find_iface(cfg, index);
        if (iface->iface_proto == protocol)
            return iface;
    }
    return nullptr;
}

EXPORT usb_desc_ep const *usb_config_helper::match_ep(
        usb_desc_iface const *iface, int dir, usb_ep_attr attr)
{
    usb_desc_ep const *ep;
    for (int i = 0; (ep = find_ep(iface, i)) != nullptr; ++i) {
        if (ep->ep_addr >= 0x80 && dir && ep->ep_attr == attr)
            break;
        if (ep->ep_addr < 0x80 && !dir && ep->ep_attr == attr)
            break;
    }
    return ep;
}

EXPORT usb_desc_ep_companion const*
usb_config_helper::get_ep_companion(usb_desc_ep const *ep)
{
    usb_desc_ep_companion const* end = (usb_desc_ep_companion const*)
            ((char*)data + len);

    usb_desc_ep_companion const* epc =(usb_desc_ep_companion const*)(ep + 1);

    if (epc < end && epc->hdr.desc_type == usb_desctype_t::SS_EP_COMPANION)
        return epc;

    return nullptr;
}

EXPORT char const *usb_config_helper::class_code_text(uint8_t cls)
{
    switch (usb_class_t(cls)) {
    case usb_class_t::audio: return "Audio";
    case usb_class_t::comm: return "Communications";
    case usb_class_t::hid: return "HID";
    case usb_class_t::physical: return "Physical";
    case usb_class_t::image: return "Image";
    case usb_class_t::printer: return "Printer";
    case usb_class_t::mass_storage: return "Mass storage";
    case usb_class_t::hub: return "Hub";
    case usb_class_t::cdc_data: return "CDC-data";
    case usb_class_t::smart_card: return "Smart card";
    case usb_class_t::content_sec: return "Content security";
    case usb_class_t::video: return "Video";
    case usb_class_t::healthcare: return "Personal healthcare";
    case usb_class_t::av: return "A/V";
    case usb_class_t::billboard: return "Billboard";
    case usb_class_t::typec_bridge: return "Type-C bridge";
    case usb_class_t::diag: return "Diagnostic";
    case usb_class_t::wireless_ctrl: return "Wireless controller";
    case usb_class_t::misc: return "Miscellaneous";
    case usb_class_t::app_specific: return "Application specific";
    case usb_class_t::vendor_specific: return "Vendor specific";
    default: return "Unknown";
    }
}

EXPORT char const *usb_config_helper::ep_attr_text(usb_ep_attr attr)
{
    switch (attr) {
    case usb_ep_attr::control: return "control";
    case usb_ep_attr::isoch: return "isoch";
    case usb_ep_attr::bulk: return "bulk";
    case usb_ep_attr::interrupt: return "interrupt";
    default: return "unknown";
    }
}

EXPORT void const *usb_config_helper::get_bos_raw(
        usb_dev_cap_type cap_type, int index) const
{
    uint8_t const* bos_raw = (uint8_t const*)bos;
    uint8_t const* bos_end = bos_raw + bos->total_len;

    int n = 0;
    for (usb_dev_cap_hdr_t const* dch = (usb_dev_cap_hdr_t const*)bos_raw + 1;
         (uint8_t const*)dch < bos_end;
         dch = (usb_dev_cap_hdr_t const*)((uint8_t const*)dch + dch->hdr.len)) {
        if (dch->cap_type == cap_type && n++ == index)
            return dch;
    }

    return nullptr;
}
