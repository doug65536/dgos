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

    bool init();

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

private:
};

static vector<usb_hub_t*> hubs;

usb_hub_t::usb_hub_t(usb_pipe_t const& control, usb_pipe_t const& status)
    : control(control)
    , status(status)
{
}

bool usb_hub_t::init()
{
    post_status_recv();

    control.send_default_control(
                uint8_t(usb_dir_t::IN) |
                (uint8_t(usb_req_type::CLASS) << 5) |
                uint8_t(usb_req_recip_t::DEVICE),
                uint8_t(usb_rqcode_t::GET_DESCRIPTOR),
                0, 0, sizeof(hub_desc), &hub_desc);

    control.set_hub_port_count(hub_desc);

    for (int port = 1; port <= hub_desc.num_ports; ++port) {
        uint32_t status = get_port_status(port);
        USBHUB_TRACE("Port %d status == %x\n", port, status);

        if (status & 1) {
            if (control.add_hub_port(port)) {
                USBHUB_TRACE("-- Configured port %d hub device\n", port);
            } else {
                USBHUB_TRACE("-- FAILED to configure hub port %d\n", port);
            }
        }
    }

    return true;
}

int usb_hub_t::get_port_status(int port)
{
    uint32_t port_status = 0;

    control.send_default_control(
            uint8_t(usb_dir_t::IN) |
            (uint8_t(usb_req_type::CLASS) << 5) |
            uint8_t(usb_req_recip_t::ENDPOINT),
            uint8_t(usb_rqcode_t::GET_STATUS),
            0, port, sizeof(port_status), &port_status);

    return port_status;
}

void usb_hub_t::post_status_recv()
{
    status_iocp.reset(&usb_hub_t::status_completion);
    status_iocp.set_expect(1);
    status.recv_async(&status_data, sizeof(status_data), &status_iocp);
}

void usb_hub_t::status_completion(usb_iocp_result_t const& result,
                                        uintptr_t arg)
{
    reinterpret_cast<usb_hub_t*>(arg)->status_completion(result);
}

void usb_hub_t::status_completion(const usb_iocp_result_t &result)
{
    //post_status_recv();
}

bool usb_hub_class_t::probe(usb_config_helper *cfg_hlp, usb_bus_t *bus)
{
    match_result match = match_config(cfg_hlp, 0, int(usb_class_t::hub),
                                      -1, -1, -1);

    if (!match.dev)
        return false;

    usb_pipe_t control, status;

    bus->get_pipe(cfg_hlp->slot(), 0, control);

    usb_desc_ep const *ep = cfg_hlp->find_ep(match.iface, 0);
    bus->alloc_pipe(cfg_hlp->slot(), ep, status);

    usb_hub_t *hub = new usb_hub_t(control, status);
    hubs.push_back(hub);

    return hub->init();
}

static usb_hub_class_t usb_hub_class;
