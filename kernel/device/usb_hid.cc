


#include "usb_hid.h"
#include "dev_usb_ctl.h"
#include "keyboard.h"
#include "mouse.h"
#include "usb_hid_keybd_lookup.h"

#define USBHID_DEBUG 1
#if USBHID_DEBUG
#define USBHID_TRACE(...) printdbg("usbhid: " __VA_ARGS__)
#else
#define USBHID_TRACE(...) ((void)0)
#endif

class usb_hid_report_parser_t {
public:
    usb_hid_report_parser_t() {

    }

    usb_hid_report_parser_t(usb_hid_report_parser_t const&) = delete;
    usb_hid_report_parser_t& operator=(usb_hid_report_parser_t) = delete;

    /// type
    /// tag
    /// size
};

class usb_hid_dev_t {
protected:
    enum struct hid_request_t : uint8_t {
        GET_REPORT = 0x1,
        GET_IDLE = 0x2,
        GET_PROTOCOL = 0x3,

        SET_REPORT = 0x9,
        SET_IDLE = 0xA,
        SET_PROTOCOL = 0xB
    };

    usb_hid_dev_t(usb_pipe_t const& control,
                  usb_pipe_t const& in,
                  usb_pipe_t const& out,
                  uint8_t iface_idx, usb_iocp_t::callback_t in_callback);

    bool set_protocol(uint16_t proto) const;
    bool set_idle(uint8_t report_id, uint8_t idle) const;
    bool get_descriptor(void *data, uint16_t len,
                        uint8_t type, uint8_t index, uint8_t lang_id);

    static constexpr unsigned phase_count = 4;

    using lock_type = ext::mcslock;
    using scoped_lock = std::unique_lock<ext::mcslock>;
    lock_type change_lock;
    usb_iocp_t in_iocp[phase_count];
    unsigned phase;

    usb_pipe_t control;
    usb_pipe_t in;
    usb_pipe_t out;
    uint8_t iface_idx;
};

class usb_hid_keybd_t final : public usb_hid_dev_t {
public:
    usb_hid_keybd_t(usb_pipe_t const& control,
                    usb_pipe_t const& in,
                    usb_pipe_t const& out,
                    uint8_t iface_idx);

private:
    void post_keybd_in(unsigned phase);
    static void keybd_completion(usb_iocp_result_t const& result,
                                 uintptr_t arg);
    void keybd_completion(usb_iocp_result_t const& result);

    void detect_keybd_changes();

    keybd_fsa_t fsa;

    uint8_t last_keybd_state[8];
    uint8_t this_keybd_state[phase_count][8];
};

class usb_hid_mouse_t final : public usb_hid_dev_t {
public:
    usb_hid_mouse_t(usb_pipe_t const& control,
                    usb_pipe_t const& in,
                    usb_pipe_t const& out,
                    uint8_t iface_idx);

private:
    void post_mouse_in(unsigned phase);
    static void mouse_completion(usb_iocp_result_t const& result,
                                 uintptr_t arg);
    void mouse_completion(usb_iocp_result_t const& result);

    uint8_t this_mouse_state[phase_count][8];
};

static std::vector<usb_hid_dev_t*> hid_devs;

class usb_hid_class_drv_t final : public usb_class_drv_t {
protected:
    usb_hid_class_drv_t() = default;
    ~usb_hid_class_drv_t() = default;

    static usb_hid_class_drv_t usb_hid;

    // usb_class_drv_t interface
    virtual bool probe(usb_config_helper *cfg, usb_bus_t *bus) override final;

    virtual char const *name() const override final;
};

bool usb_hid_class_drv_t::probe(usb_config_helper *cfg_hlp, usb_bus_t *bus)
{
    match_result match;

    // Try to find keyboard interface
    match = match_config(cfg_hlp, 0, int(usb_class_t::hid),
                         1, -1, 1, -1, -1);

    if (!match.dev) {
        match = match_config(cfg_hlp, 0, int(usb_class_t::hid),
                             1, -1, 2, -1, -1);
    }

    if (!match.dev)
        return false;

    usb_pipe_t control, in, out;

    bus->get_pipe(cfg_hlp->slot(), 0, control);

    assert(control);

    // Try to find interrupt pipes
    usb_desc_ep const *in_ep_desc = cfg_hlp->match_ep(
                match.iface, 1, usb_ep_attr::interrupt);

    usb_desc_ep const *out_ep_desc = cfg_hlp->match_ep(
                match.iface, 0, usb_ep_attr::interrupt);

    if (in_ep_desc) {
        bus->alloc_pipe(cfg_hlp->slot(), match.iface, in_ep_desc, in);
        assert(in);
    }

    if (out_ep_desc) {
        bus->alloc_pipe(cfg_hlp->slot(), match.iface, out_ep_desc, out);
        assert(out);
    }

    usb_hid_dev_t *dev = nullptr;

    // Keyboard
    if (match.iface->iface_proto == 1) {
        dev = new (std::nothrow)
                usb_hid_keybd_t(control, in, out, match.iface_idx);
    } else if (match.iface->iface_proto == 2) {
        dev = new (std::nothrow)
                usb_hid_mouse_t(control, in, out, match.iface_idx);
    } else {
        USBHID_TRACE("Unhandled interface protocol: 0x%02x\n",
                     match.iface->iface_proto);
    }

    if (dev) {
        if (unlikely(!hid_devs.push_back(dev)))
            panic_oom();
    }

    return dev != nullptr;
}

char const *usb_hid_class_drv_t::name() const
{
    return "USB HID";
}

usb_hid_dev_t::usb_hid_dev_t(usb_pipe_t const& control,
                             usb_pipe_t const& in,
                             usb_pipe_t const& out,
                             uint8_t iface_idx,
                             usb_iocp_t::callback_t in_callback)
    : phase(0)
    , control(control)
    , in(in)
    , out(out)
    , iface_idx(iface_idx)
{
}

bool usb_hid_dev_t::set_protocol(uint16_t proto) const
{
    return control.send_default_control(
                uint8_t(usb_dir_t::OUT) |
                (uint8_t(usb_req_type::CLASS) << 5) |
                uint8_t(usb_req_recip_t::INTERFACE),
                uint8_t(hid_request_t::SET_PROTOCOL),
                proto, iface_idx, 0, nullptr) >= 0;
}

bool usb_hid_dev_t::set_idle(uint8_t report_id, uint8_t idle) const
{
    return control.send_default_control(
                uint8_t(usb_dir_t::OUT) |
                (uint8_t(usb_req_type::CLASS) << 5) |
                uint8_t(usb_req_recip_t::INTERFACE),
                uint8_t(hid_request_t::SET_IDLE),
                report_id | (idle << 8), iface_idx, 0, nullptr) >= 0;
}

bool usb_hid_dev_t::get_descriptor(void *data, uint16_t len,
                                   uint8_t type, uint8_t index,
                                   uint8_t lang_id)
{
    return control.send_default_control(
            uint8_t(usb_dir_t::IN) |
            (uint8_t(usb_req_type::STD) << 5) |
            uint8_t(usb_req_recip_t::INTERFACE),
            uint8_t(usb_rqcode_t::GET_DESCRIPTOR),
            (type << 8) | index, lang_id, sizeof(data), data) >= 0;
}

void usb_hid_keybd_t::post_keybd_in(unsigned phase)
{
    in_iocp[phase].reset(&usb_hid_keybd_t::keybd_completion, uintptr_t(this));
    in.recv_async(this_keybd_state[phase], sizeof(this_keybd_state[phase]),
                  &in_iocp[phase]);
    in_iocp[phase].set_expect(1);
}

void usb_hid_keybd_t::keybd_completion(
        usb_iocp_result_t const& result, uintptr_t arg)
{
    reinterpret_cast<usb_hid_keybd_t*>(arg)->keybd_completion(result);
}

void usb_hid_keybd_t::keybd_completion(usb_iocp_result_t const& result)
{
    detect_keybd_changes();
}

void usb_hid_keybd_t::detect_keybd_changes()
{
    static int constexpr modifier_vk[] = {
        KEYB_VK_LCTRL,
        KEYB_VK_LSHIFT,
        KEYB_VK_LALT,
        KEYB_VK_LGUI,
        KEYB_VK_RCTRL,
        KEYB_VK_RSHIFT,
        KEYB_VK_RALT,
        KEYB_VK_RGUI
    };

    scoped_lock hold_change_lock(change_lock);

    uint8_t const *state = this_keybd_state[phase];

    // Detect modifier changes
    uint8_t modifier_changes = last_keybd_state[0] ^ state[0];

    // Generate modifier key up/down events
    for (int i = 0; i < 8; ++i) {
        int mask = 1 << i;
        int sign = (((state[0] & mask) != 0) * 2) - 1;

        if (modifier_changes & mask)
            fsa.deliver_vk(modifier_vk[i] * sign);
    }

    // Scan for keys released since last event
    for (int i = 2; i < 8; ++i) {
        uint8_t scancode = last_keybd_state[i];

        if (scancode < 4)
            continue;

        bool pressed = memchr(state + 2, scancode, 6);

        if (!pressed) {
            // Generate keyup event
            USBHID_TRACE("keydown, scancode=%#x\n", scancode);

            int vk = (scancode < usb_hid_keybd_lookup_count)
                    ? usb_hid_keybd_lookup[scancode] : 0;

            fsa.deliver_vk(-vk);
        }
    }

    // Scan for keys pressed down in this event
    for (int i = 2; i < 8; ++i) {
        uint8_t scancode = state[i];

        if (scancode < 4)
            continue;

        // Detect edges
        if (memchr(last_keybd_state + 2, scancode, 6))
            continue;

        USBHID_TRACE("keydown, scancode=%#x\n", scancode);

        // Generate keydown event
        int vk = (scancode < usb_hid_keybd_lookup_count)
                ? usb_hid_keybd_lookup[scancode] : 0;

        fsa.deliver_vk(vk);
    }

    memcpy(last_keybd_state, state, sizeof(last_keybd_state));

    post_keybd_in(phase);

    phase = (phase + 1) & (phase_count - 1);
}

void usb_hid_mouse_t::post_mouse_in(unsigned phase)
{
    in_iocp[phase].reset(&usb_hid_mouse_t::mouse_completion, uintptr_t(this));
    in.recv_async(this_mouse_state[phase], sizeof(this_mouse_state[phase]),
                  &in_iocp[phase]);
    in_iocp[phase].set_expect(1);
}

void usb_hid_mouse_t::mouse_completion(
        usb_iocp_result_t const& result, uintptr_t arg)
{
    reinterpret_cast<usb_hid_mouse_t*>(arg)->mouse_completion(result);
}

void usb_hid_mouse_t::mouse_completion(const usb_iocp_result_t &result)
{
    scoped_lock hold_change_lock(change_lock);

    mouse_raw_event_t evt;
    uint8_t *state = this_mouse_state[phase];
    evt.buttons = state[0];
    evt.hdist = int8_t(state[1]);
    evt.vdist = -int8_t(state[2]);
    evt.wdist = int8_t(state[3]);
    mouse_event(evt);

    post_mouse_in(phase);

    phase = (phase + 1) & (phase_count - 1);
}

usb_hid_keybd_t::usb_hid_keybd_t(usb_pipe_t const& control,
                                 usb_pipe_t const& in,
                                 usb_pipe_t const& out,
                                 uint8_t iface_idx)
    : usb_hid_dev_t(control, in, out, iface_idx,
                    &usb_hid_keybd_t::keybd_completion)
{
//    uint8_t hid_desc[256] = {};
//    uint8_t report_desc[256] = {};
//    uint8_t phys_desc[256] = {};

    // Report protocol
    //int proto_cc = set_protocol(1);

    // Try to read report descriptor
    //get_descriptor(hid_desc, sizeof(hid_desc), 0x21, 0, 0);
    //get_descriptor(report_desc, sizeof(report_desc), 0x22, 0, 0);
    //get_descriptor(phys_desc, sizeof(phys_desc), 0x23, 0, 0);

    set_protocol(0);
    set_idle(0, 0);

    memset(last_keybd_state, 0, sizeof(last_keybd_state));

    for (unsigned phase = 0; phase < phase_count; ++phase)
        post_keybd_in(phase);
}

usb_hid_mouse_t::usb_hid_mouse_t(usb_pipe_t const& control,
                                 usb_pipe_t const& in,
                                 usb_pipe_t const& out,
                                 uint8_t iface_idx)
    : usb_hid_dev_t(control, in, out, iface_idx,
                    usb_hid_mouse_t::mouse_completion)
{
    set_protocol(0);
    set_idle(0, 0);

    for (unsigned phase = 0; phase < phase_count; ++phase)
        post_mouse_in(phase);
}

usb_hid_class_drv_t usb_hid_class_drv_t::usb_hid;
