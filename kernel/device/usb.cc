#include "usb.h"


usb_config_helper::usb_config_helper(void *data, size_t len)
    : data(data)
    , len(len)
{
}

usb_desc_config *usb_config_helper::find_config(int cfg_index)
{
    if (unlikely(!data))
        return nullptr;

    usb_desc_config *cfg = (usb_desc_config*)data;

    int index = 0;
    int ofs = 0;
    while (index < cfg_index && ofs < len && cfg->len) {
        cfg = (usb_desc_config*)((char*)cfg + cfg->total_len);
        ofs += cfg->total_len;
        ++index;
    }

    return ofs < len &&
            cfg->desc_type == usb_desctype_t::CONFIGURATION
            ? cfg : nullptr;
}

usb_desc_iface *usb_config_helper::find_iface(
        usb_desc_config *cfg, int iface_index)
{
    if (unlikely(!cfg || iface_index >= cfg->num_iface || iface_index < 0))
        return nullptr;

    usb_desc_iface *iface_st = (usb_desc_iface*)((char*)cfg + cfg->len);
    usb_desc_iface *iface_en = iface_st + cfg->num_iface;
    usb_desc_iface *iface = iface_st;
    int index = 0;
    while (index < iface_index && iface < iface_en && iface->len) {
        iface = (usb_desc_iface*)((char*)iface + iface->len);
        ++index;
    }
    return iface < iface_en ? iface : nullptr;
}

usb_desc_ep *usb_config_helper::find_ep(
        usb_desc_iface *iface, int ep_index)
{
    if (unlikely(!iface || ep_index >= iface->num_ep || ep_index < 0))
        return nullptr;

    usb_desc_ep *ep_st = (usb_desc_ep*)((char*)iface + iface->len);
    usb_desc_ep *ep_en = ep_st + iface->num_ep;
    usb_desc_ep *ep = ep_st;
    int index = 0;
    while (index < ep_index && ep->len) {
        ep = (usb_desc_ep*)((char*)ep + ep->len);
        ++index;
    }
    return ep < ep_en ? ep : nullptr;
}

const char *usb_config_helper::class_code_text(uint8_t cls)
{
    switch (cls) {
    case 0x01: return "Audio";
    case 0x02: return "Communications";
    case 0x03: return "HID";
    case 0x05: return "Physical";
    case 0x06: return "Image";
    case 0x07: return "Printer";
    case 0x08: return "Mass storage";
    case 0x09: return "Hub";
    case 0x0A: return "CDC-data";
    case 0x0B: return "Smart card";
    case 0x0D: return "Content security";
    case 0x0E: return "Video";
    case 0x0F: return "Personal healthcare";
    case 0x10: return "A/V";
    case 0x11: return "Billboard";
    case 0x12: return "Type-C bridge";
    case 0xDC: return "Diagnostic";
    case 0xE0: return "Wireless controller";
    case 0xEF: return "Miscellaneous";
    case 0xFE: return "Application specific";
    case 0xFF: return "Vendor specific";
    default: return "Unknown";
    }
}
