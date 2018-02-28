#include "dev_usb_ctl.h"

usb_class_drv_t *usb_class_drv_t::first_driver;

void usb_class_drv_t::find_driver(usb_config_helper *cfg, usb_bus_t *bus)
{
    for (usb_class_drv_t *drv = first_driver;
         drv && !drv->probe(cfg, bus); drv = drv->next_driver);
}

usb_class_drv_t::usb_class_drv_t()
{
    next_driver = first_driver;
    first_driver = this;
}


//        USBXHCI_TRACE("cfg #0x%x max_power=%umA {\n",
//                      cfg_idx, cfg->max_power * 2);
//            USBXHCI_TRACE("  iface #0x%x: class=0x%x (%s)"
//                          " subclass=0x%x, proto=0x%x {\n",
//                          iface->iface_num, iface->iface_class,
//                          cfg_hlp.class_code_text(iface->iface_class),
//                          iface->iface_subclass, iface->iface_proto);
//                USBXHCI_TRACE("    ep #0x%x: dir=%s addr=0x%x attr=%s,"
//                              " maxpktsz=0x%x, interval=%u\n",
//                              ep_idx, ep->ep_addr >= 0x80 ? "IN" : "OUT",
//                              ep->ep_addr,
//                              usb_config_helper::ep_attr_text(ep->ep_attr),
//                              ep->max_packet_sz, ep->interval);
//            USBXHCI_TRACE("  }\n");
//        USBXHCI_TRACE("}\n");
//    USBXHCI_TRACE("Done configuration descriptors\n");

usb_class_drv_t::match_result
usb_class_drv_t::match_config(usb_config_helper *cfg_hlp, int index,
                              int dev_class, int dev_subclass,
                              int vendor_id, int product_id)
{
    usb_desc_device const &dev_desc = cfg_hlp->device();

    match_result result{ &dev_desc, nullptr, nullptr, -1, -1 };

    int match_index = -1;

    if (vendor_id == dev_desc.vendor_id &&
        product_id == dev_desc.vendor_id) {
        if (++match_index == index)
            return result;
    }

    if (dev_class >= 0 || dev_subclass >= 0) {
        for (result.cfg_idx = 0;
             (result.cfg = cfg_hlp->find_config(result.cfg_idx)) != nullptr;
             ++result.cfg_idx) {
            for (result.iface_idx = 0;
                 (result.iface = cfg_hlp->find_iface(
                      result.cfg, result.iface_idx)) != nullptr;
                 ++result.iface_idx) {
                if ((dev_class < 0 ||
                     dev_class == result.iface->iface_class) &&
                        (dev_subclass < 0 ||
                         dev_subclass == result.iface->iface_subclass)) {
                    if (++match_index == index)
                        return result;
                }
            }
        }
    }

    result.dev = nullptr;
    result.cfg = nullptr;
    result.iface = nullptr;
    result.cfg_idx = -1;
    result.iface_idx = -1;

    return result;
}

int usb_pipe_t::send_default_control(uint8_t request_type, uint8_t request,
                                     uint16_t value, uint16_t index,
                                     uint16_t length, void *data) const
{
//    printdbg("Sending USB control: reqt=0x%02x req=0x%02x, val=0x%04x,"
//             " idx=0x%02x, len=0x%04x, data=%p\n",
//             request_type, request, value, index, length, data);

    return bus->send_control(
                slotid, request_type, request, value, index, length, data);
}

int usb_pipe_t::send_default_control_async(uint8_t request_type,
                                           uint8_t request, uint16_t value,
                                           uint16_t index, uint16_t length,
                                           void *data, usb_iocp_t *iocp) const
{
    return bus->send_control_async(slotid, request_type, request, value, index,
                                   length, data, iocp);
}

int usb_pipe_t::recv(void *data, uint32_t length) const
{
    return bus->xfer(slotid, epid, 0, length, data, 1);
}

int usb_pipe_t::recv_async(void *data, uint32_t length, usb_iocp_t *iocp) const
{
    return bus->xfer_async(slotid, epid, 0, length, data, 1, iocp);
}

int usb_pipe_t::send(void const *data, uint32_t length) const
{
    return bus->xfer(slotid, epid, 0, length, const_cast<void*>(data), 0);
}

int usb_pipe_t::send_async(void const *data, uint32_t length,
                           usb_iocp_t *iocp) const
{
    return bus->xfer_async(slotid, epid, 0, length, const_cast<void*>(data),
                           0, iocp);
}

int usb_pipe_t::clear_ep_halt(usb_pipe_t const& target)
{
    // Must be sent to control pipe
    assert(epid == 0);

    usb_ep_state_t state = bus->get_ep_state(slotid, epid);

    if (state != usb_ep_state_t::halted)
        return 0;

    bus->reset_ep(slotid, epid);

    int ncc = send_default_control(
               uint8_t(usb_dir_t::OUT) |
               (uint8_t(usb_req_type::STD) << 5) |
               uint8_t(usb_req_recip_t::ENDPOINT),
               uint8_t(usb_rqcode_t::CLEAR_FEATURE),
               uint16_t(usb_featcode_t::ENDPOINT_HALT),
               target.epid, 0, nullptr);

    return ncc;
}

bool usb_pipe_t::add_hub_port(int port)
{
    return bus->configure_hub_port(slotid, port);
}

bool usb_pipe_t::set_hub_port_count(usb_hub_desc const& hub_desc)
{
    return bus->set_hub_port_count(slotid, hub_desc);
}

bool usb_bus_t::alloc_pipe(int slotid, usb_desc_ep const *ep, usb_pipe_t &pipe)
{
    return alloc_pipe(slotid, ep->ep_addr, pipe,
                      ep->max_packet_sz, ep->interval, ep->ep_attr);
}
