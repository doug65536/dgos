#include "dev_usb_ctl.h"

usb_class_drv_t *usb_class_drv_t::first_driver;

EXPORT usb_class_drv_t *usb_class_drv_t::find_driver(
        usb_config_helper *cfg, usb_bus_t *bus)
{
    usb_class_drv_t *drv = first_driver;
    while (drv && !drv->probe(cfg, bus))
        drv = drv->next_driver;
    return drv;
}

EXPORT usb_class_drv_t::usb_class_drv_t()
{
    next_driver = first_driver;
    first_driver = this;
}

EXPORT usb_class_drv_t::match_result
usb_class_drv_t::match_config(usb_config_helper *cfg_hlp, size_t index,
                              int dev_class, int dev_subclass, int dev_proto,
                              int iface_proto,
                              int vendor_id, int product_id)
{
    usb_desc_device const &dev_desc = cfg_hlp->device();

    match_result result{ &dev_desc, nullptr, nullptr, -1, -1 };

    do {
        if (dev_proto >= 0 && dev_desc.dev_protocol != 0 &&
                dev_desc.dev_protocol != dev_proto)
            break;

        int match_index = -1;

        if (vendor_id == dev_desc.vendor_id &&
            product_id == dev_desc.vendor_id) {
            if (size_t(++match_index) == index)
                return result;
        }

        if (dev_class >= 0 || dev_subclass >= 0) {
            // For each configuration
            for (result.cfg_idx = 0;
                 (result.cfg = cfg_hlp->find_config(result.cfg_idx)) != nullptr;
                 ++result.cfg_idx) {
                // For each interface
                for (result.iface_idx = 0;
                     (result.iface = cfg_hlp->find_iface(
                          result.cfg, result.iface_idx)) != nullptr;
                     ++result.iface_idx) {
                    // Match filter parameters
                    if ((dev_class < 0 ||
                         dev_class == result.iface->iface_class) &&
                            (dev_subclass < 0 ||
                             dev_subclass == result.iface->iface_subclass) &&
                            ((iface_proto < 0 ||
                              iface_proto == result.iface->iface_proto))) {
                        if (size_t(++match_index) == index)
                            return result;
                    }
                }
            }
        }
    } while (false);

    result.dev = nullptr;
    result.cfg = nullptr;
    result.iface = nullptr;
    result.cfg_idx = -1;
    result.iface_idx = -1;

    return result;
}

EXPORT int usb_pipe_t::send_default_control(
        uint8_t request_type, uint8_t request,
        uint16_t value, uint16_t index,
        uint16_t length, void *data) const
{
//    printdbg("Sending USB control: reqt=%#.2x req=%#.2x, val=%#.4x,"
//             " idx=%#.2x, len=%#.4x, data=%p\n",
//             request_type, request, value, index, length, data);

    return bus->send_control(
                slotid, request_type, request, value, index, length, data);
}

EXPORT int usb_pipe_t::send_default_control_async(uint8_t request_type,
                                           uint8_t request, uint16_t value,
                                           uint16_t index, uint16_t length,
                                           void *data, usb_iocp_t *iocp) const
{
    return bus->send_control_async(slotid, request_type, request, value, index,
                                   length, data, iocp);
}

EXPORT int usb_pipe_t::recv(void *data, uint32_t length) const
{
    return bus->xfer(slotid, epid, 0, length, data, 1);
}

EXPORT int usb_pipe_t::recv_async(void *data, uint32_t length,
                                  usb_iocp_t *iocp) const
{
    return bus->xfer_async(slotid, epid, 0, length, data, 1, iocp);
}

EXPORT int usb_pipe_t::send(void const *data, uint32_t length) const
{
    return bus->xfer(slotid, epid, 0, length, const_cast<void*>(data), 0);
}

EXPORT int usb_pipe_t::send_async(void const *data, uint32_t length,
                           usb_iocp_t *iocp) const
{
    return bus->xfer_async(slotid, epid, 0, length, const_cast<void*>(data),
                           0, iocp);
}

EXPORT int usb_pipe_t::clear_ep_halt(usb_pipe_t const& target)
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

EXPORT bool usb_pipe_t::add_hub_port(int port)
{
    return bus->configure_hub_port(slotid, port);
}

EXPORT bool usb_pipe_t::set_hub_port_config(usb_hub_desc const& hub_desc,
                                     usb_config_helper const* cfg_hlp)
{
    return bus->set_hub_port_count(slotid, hub_desc);
}

EXPORT bool usb_bus_t::alloc_pipe(int slotid, usb_desc_iface const *iface,
                           usb_desc_ep const *ep, usb_pipe_t &pipe)
{
    return alloc_pipe(slotid, pipe, ep->ep_addr, iface->iface_index,
                      iface->iface_num, iface->alt_setting,
                      ep->max_packet_sz, ep->interval, ep->ep_attr);
}

EXPORT usb_pipe_t::usb_pipe_t()
    : bus(nullptr)
    , slotid(-1)
    , epid(-1)
{
}
