#include "usb_hub.h"
#include "dev_usb_ctl.h"

#define USBHUB_DEBUG 1
#if USBHUB_DEBUG
#define USBHUB_TRACE(...) printdbg("usbhub: " __VA_ARGS__)
#else
#define USBHUB_TRACE(...) ((void)0)
#endif

class usb_hub_t {
public:
    usb_hub_t(usb_pipe_t const& control, usb_pipe_t const& status);

    bool init(usb_config_helper const *cfg_hlp);

private:
    enum struct hub_request_t : uint8_t {

    };

    enum struct hub_class_feature_t : uint8_t {
        // hub
        C_HUB_LOCAL_POWER = 0,
        C_HUB_OVER_CURRENT = 1,

        // port
        PORT_CONNECTION = 0,
        PORT_ENABLE = 1,
        PORT_SUSPEND = 2,
        PORT_OVER_CURRENT = 3,
        PORT_RESET = 4,
        PORT_POWER = 8,
        PORT_LOW_SPEED = 9,
        C_PORT_CONNECTION = 16,
        C_PORT_ENABLE = 17,
        C_PORT_SUSPEND = 18,
        C_PORT_OVER_CURRENT = 19,
        C_PORT_RESET = 20,
        PORT_TEST = 21,
        PORT_INDICATOR = 22
    };

    int get_port_status(int port);

    void post_status_recv();
    static void status_completion(usb_iocp_result_t const& result,
                                  uintptr_t arg);
    void status_completion(usb_iocp_result_t const& result);

    usb_pipe_t control;
    usb_pipe_t status;
    usb_iocp_t status_iocp;
    usb_hub_desc hub_desc;
    uint16_t status_data;
};

class usb_hub_class_t : public usb_class_drv_t {
public:

protected:
    // usb_class_drv_t interface
    bool probe(usb_config_helper *cfg_hlp, usb_bus_t *bus) override;

    char const *name() const override final;

private:
};

static std::vector<usb_hub_t*> hubs;

usb_hub_t::usb_hub_t(usb_pipe_t const& control, usb_pipe_t const& status)
    : control(control)
    , status(status)
    , hub_desc{}
    , status_data(0)
{
}

bool usb_hub_t::init(usb_config_helper const* cfg_hlp)
{
    int reply_sz = control.send_default_control(
                uint8_t(usb_dir_t::IN) |
                (uint8_t(usb_req_type::CLASS) << 5) |
                uint8_t(usb_req_recip_t::DEVICE),
                uint8_t(usb_rqcode_t::GET_DESCRIPTOR),
                0, 0, sizeof(hub_desc), &hub_desc);

    bool config_success = control.set_hub_port_config(hub_desc, cfg_hlp);

    for (size_t port = 1; port <= hub_desc.num_ports; ++port) {
        uint32_t status = get_port_status(port);
        USBHUB_TRACE("Port %zu status == %#x\n", port, status);

        if (status & 1) {
            if (control.add_hub_port(port)) {
                USBHUB_TRACE("-- Configured port %zu hub device\n", port);
            } else {
                USBHUB_TRACE("-- FAILED to configure hub port %zu\n", port);
            }
        }
    }

    post_status_recv();

    return true;
}

int usb_hub_t::get_port_status(int port)
{
    uint32_t port_status = 0;

    int err = control.send_default_control(
            uint8_t(usb_dir_t::IN) |
            (uint8_t(usb_req_type::CLASS) << 5) |
            uint8_t(usb_req_recip_t::ENDPOINT),
            uint8_t(usb_rqcode_t::GET_STATUS),
            0, port, sizeof(port_status), &port_status);

    if (unlikely(err))
        return err;

    return port_status;
}

void usb_hub_t::post_status_recv()
{
    status_data = 0;
    status_iocp.reset(&usb_hub_t::status_completion, uintptr_t(this));
    status.recv_async(&status_data, sizeof(status_data), &status_iocp);
    status_iocp.set_expect(1);
}

void usb_hub_t::status_completion(usb_iocp_result_t const& result,
                                  uintptr_t arg)
{
    reinterpret_cast<usb_hub_t*>(arg)->status_completion(result);
}

void usb_hub_t::status_completion(const usb_iocp_result_t &result)
{
    post_status_recv();
}

bool usb_hub_class_t::probe(usb_config_helper *cfg_hlp, usb_bus_t *bus)
{
    //bool multi_tt = true;

    // Try to find multi-TT interface
    match_result match = match_config(cfg_hlp, 0, int(usb_class_t::hub),
                                      0, 2, 1, -1, -1);

    // If not found, try to find single-TT interface
    if (match.dev)
        USBHUB_TRACE("Found Multi-TT hub interface\n");
    else {
        //multi_tt = false;

        match = match_config(cfg_hlp, 0, int(usb_class_t::hub), 0, 1,
                             -1, -1, -1);

        if (match.dev)
            USBHUB_TRACE("Found Single-TT hub interface\n");
        else
            return false;
    }

    usb_pipe_t control, status;

    if (!bus->get_pipe(cfg_hlp->slot(), 0, control))
        return false;

    assert(control);

    usb_desc_ep const *ep = cfg_hlp->find_ep(match.iface, 0);

    //usb_desc_ep_companion const* epc = cfg_hlp->get_ep_companion(ep);
    //int max_burst = epc ? epc->max_burst : 1;

    if (!bus->alloc_pipe(cfg_hlp->slot(), match.iface, ep, status))
        return false;

    assert(status);

    usb_hub_t *hub = new (ext::nothrow) usb_hub_t(control, status);
    if (unlikely(!hubs.push_back(hub)))
        return false;

    return hub->init(cfg_hlp);
}

char const *usb_hub_class_t::name() const
{
    return "USB hub";
}

static usb_hub_class_t usb_hub_class;
