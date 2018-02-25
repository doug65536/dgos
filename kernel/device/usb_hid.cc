#include "usb_hid.h"
#include "dev_usb_ctl.h"
#include "mouse.h"

class usb_hid_t : public usb_class_drv_t {
protected:
    static usb_hid_t usb_hid;

    enum struct hid_request_t : uint8_t {
        SET_PROTOCOL = 0xB
    };

    // usb_class_drv_t interface
    virtual bool probe(usb_config_helper *cfg, usb_bus_t *bus) override final;

private:
    static int keyboard_poll_thread(void *p);
    int keyboard_poll_thread();

    static int keyboard_in_thread(void *p);
    int keyboard_in_thread();

    static int mouse_poll_thread(void *p);
    int mouse_poll_thread();

    ticketlock print_lock;
    usb_pipe_t keybd_control;
    usb_pipe_t keybd_in;
    usb_pipe_t mouse_control;
    usb_pipe_t mouse_in;
    uint8_t keybd_iface_idx;
    uint8_t mouse_iface_idx;
};

bool usb_hid_t::probe(usb_config_helper *cfg_hlp, usb_bus_t *bus)
{
    match_result match = match_config(
                cfg_hlp, 0, int(usb_class_t::hid), 1, -1, -1);

    if (!match.dev)
        return false;

    // Keyboard
    if (match.iface->iface_proto == 1) {
        keybd_iface_idx = match.iface_idx;

        bus->get_pipe(cfg_hlp->slot(), 0, keybd_control);

        keybd_control.send_default_control(
                    uint8_t(usb_dir_t::OUT) |
                    (uint8_t(usb_req_type::CLASS) << 5) |
                    uint8_t(usb_req_recip_t::INTERFACE),
                    uint8_t(hid_request_t::SET_PROTOCOL),
                    0, 0, 0, nullptr);

        // Try to find interrupt pipe
        usb_desc_ep const *ep_desc = cfg_hlp->find_ep(match.iface, 0);

        int tid = -1;

        if (ep_desc) {
            // Allocate interrupt IN endpoint and use it
            if (bus->alloc_pipe(cfg_hlp->slot(), ep_desc->ep_addr, keybd_in,
                                ep_desc->max_packet_sz, ep_desc->interval,
                                ep_desc->ep_attr)) {
                tid = thread_create(keyboard_in_thread, this, 0, false);
            }
        }

        // Fallback to polling if we didn't create the interrupt IN endpoint
        if (tid == -1)
            tid = thread_create(keyboard_poll_thread, this, 0, false);

        thread_set_priority(tid, 16);

        return true;
    } else if (match.iface->iface_proto == 2) {
        // FIXME Mouse disabled
        return false;

        mouse_iface_idx = match.iface_idx;

        bus->get_pipe(cfg_hlp->slot(), 0, mouse_control);

        mouse_control.send_default_control(
                    uint8_t(usb_dir_t::OUT) |
                    (uint8_t(usb_req_type::CLASS) << 5) |
                    uint8_t(usb_req_recip_t::INTERFACE),
                    uint8_t(hid_request_t::SET_PROTOCOL),
                    0, 0, 0, nullptr);

        int tid = thread_create(mouse_poll_thread, this, 0, false);
        thread_set_priority(tid, 16);

        return true;
    }

    return false;
}

int usb_hid_t::keyboard_poll_thread(void *p)
{
    return ((usb_hid_t*)p)->keyboard_poll_thread();
}

int usb_hid_t::keyboard_poll_thread()
{
    uint8_t zeros[8] = {};
    while (true) {
        uint8_t data[8] = {};

        keybd_control.send_default_control(
                    uint8_t(usb_dir_t::IN) |
                    (uint8_t(usb_req_type::CLASS) << 5) |
                    uint8_t(usb_req_recip_t::INTERFACE),
                    1,
                    0, 0, 8, data);

        if (memcmp(zeros, data, sizeof(data))) {
            unique_lock<ticketlock> lock(print_lock);
            printdbg("  Key: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                    data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7]);
        }

        thread_sleep_for(20);
    }

    return true;
}

int usb_hid_t::keyboard_in_thread(void *p)
{
    return ((usb_hid_t*)p)->keyboard_in_thread();
}

int usb_hid_t::keyboard_in_thread()
{
    thread_sleep_for(1000);
    //uint8_t zeros[8] = {};
    while (true) {
        uint8_t data[12] = {};

        int sz = keybd_in.recv(12, data);

        unique_lock<ticketlock> lock(print_lock);
        printdbg("  Key: %02x %02x %02x %02x %02x %02x %02x %02x sz=%d\n",
                data[0], data[1], data[2], data[3],
                data[4], data[5], data[6], data[7], sz);

        //thread_sleep_for(20);
    }

    return true;
}

int usb_hid_t::mouse_poll_thread(void *p)
{
    return ((usb_hid_t*)p)->mouse_poll_thread();
}

int usb_hid_t::mouse_poll_thread()
{
    uint8_t zeros[8] = {};
    while (true) {
        uint8_t data[8] = {};

        mouse_control.send_default_control(
                    uint8_t(usb_dir_t::IN) |
                    (uint8_t(usb_req_type::CLASS) << 5) |
                    uint8_t(usb_req_recip_t::INTERFACE),
                    1,
                    0x100, mouse_iface_idx, 8, data);

        if (memcmp(zeros, data, sizeof(data))) {
            unique_lock<ticketlock> lock(print_lock);
            printdbg("Mouse: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                    data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7]);

            mouse_raw_event_t evt;
            evt.hdist = (char)data[1];
            evt.vdist = -(char)data[2];
            evt.wdist = (char)data[3];
            evt.buttons = data[0];
            mouse_event(evt);
        }

        thread_sleep_for(20);
    }

    return true;
}

usb_hid_t usb_hid_t::usb_hid;
