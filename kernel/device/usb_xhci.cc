#include "callout.h"
#include "pci.h"
#include "printk.h"
#include "stdlib.h"
#include "mm.h"
#include "cpu/atomic.h"
#include "string.h"
#include "hash_table.h"
#include "mutex.h"
#include "usb.h"
#include "vector.h"
#include "dev_usb_ctl.h"
#include "iocp.h"
#include "refcount.h"

#include "usb_xhcibits.h"

#define USBXHCI_DEBUG   0
#if USBXHCI_DEBUG
#define USBXHCI_TRACE(...) printdbg("xhci: " __VA_ARGS__)
#else
#define USBXHCI_TRACE(...) (void)0
#endif

//
// 5.3 Host Controller Capability Registers

struct usbxhci_capreg_t {
    // 5.3.1 00h Capability register length
    uint8_t caplength;

    uint8_t rsvd1;

    // 5.3.2 02h Interface version number
    uint16_t hciversion;

    // 5.3.3 04h Structural Parameters 1
    uint32_t hcsparams1;

    // 5.3.4 08h Structural Parameters 2
    uint32_t hcsparams2;

    // 5.3.5 0Ch Structural Parameters 3
    uint32_t hcsparams3;

    // 5.3.6 10h Capability Parameters 1
    uint32_t hccparams1;

    // 5.3.7 14h Doorbell offset
    uint32_t dboff;

    // 5.3.8 18h Runtime Register Space offset
    uint32_t rtsoff;

    // 5.3.9 1Ch Capability Parameters 2
    uint32_t hccparams2;
} __packed;

C_ASSERT(sizeof(usbxhci_capreg_t) == 0x20);

// 5.5.2 Interrupt Register Set

struct usbxhci_intr_t {
    // 5.5.2.1 Interrupt Management Register
    uint32_t iman;

    // 5.5.2.2 Interrupt Moderation Register
    uint32_t imod;

    // 5.5.2.3 Event Ring Segment Table Size Register
    uint32_t erstsz;

    uint32_t rsvdP;

    // 5.5.2.3.2 Event Ring Segment Table Base Address Register
    // Must be 64 byte aligned
    uint64_t erstba;

    // 5.5.2.3.3 Event Ring Dequeue Pointer Register
    uint64_t erdp;
} __packed;

C_ASSERT(sizeof(usbxhci_intr_t) == 0x20);

// 5.5 Host Controller Runtime Registers

struct usbxhci_rtreg_t  {
    // 5.5.1 Microframe Index Register
    uint32_t mfindex;

    uint8_t rsvdz[0x20-0x04];

    // 5.5.2 Interrupter Register Set
    usbxhci_intr_t ir[1023];
} __packed;

C_ASSERT(sizeof(usbxhci_rtreg_t) == 0x8000);

typedef uint32_t usbxhci_dbreg_t;

struct usbxhci_portreg_t {
    // 5.4.8 0h Port Status and Control
    uint32_t portsc;

    // 5.4.9 4h Port Power Management Status and Control
    uint32_t portpmsc;

    // 5.4.10 8h Port Link Info
    uint32_t portli;

    // 5.4.11 Ch Port Hardware LPM Control
    uint32_t porthlpmc;
} __packed;

C_ASSERT(sizeof(usbxhci_portreg_t) == 0x10);

struct usbxhci_opreg_t {
    // 5.4.1 0x00 USB Command
    uint32_t usbcmd;

    // 5.4.2 0x04 USB Status
    uint32_t usbsts;

    // 5.4.3 0x08 Page Size
    uint32_t pagesize;

    // 0x0C 0x13 RsvdZ
    uint8_t rsvd1[0x14-0x0C];

    // 5.4.4 0x14 Device Notification Control
    uint32_t dnctrl;

    // 5.4.5 0x18 Command Ring Control
    uint64_t crcr;

    // 0x20 0x2F RsvdZ
    uint8_t rsvd2[0x30-0x20];

    // 5.4.6 0x30 Device Context Base Address Array Pointer
    uint64_t dcbaap;

    // 5.4.7 0x38 Configure
    uint32_t config;

    // 0x3C 0x3FF RsvdZ
    uint8_t rsvd3[0x400-0x3C];

    // 5.4.8 0x400 0x13FF Port Register Set 1-MaxPorts
    // The actual number of ports is in HCSPARAMS1
    usbxhci_portreg_t ports[256];
} __packed;

C_ASSERT(sizeof(usbxhci_opreg_t) == 0x1400);

// 6.2.2 Slot Context

struct usbxhci_slotctx_t {
    // Route string, speed, Multi-TT, hub, last endpoint index
    uint32_t rsmhc;

    // Worst case wakeup latency in microseconds
    uint16_t max_exit_lat;

    // Root hub port number
    uint8_t root_hub_port_num;

    // Number of hubs (0 if not a hub)
    uint8_t num_ports;

    // if low/full speed and connected to high spd hub,
    // then this is the slot id of the parent high speed hub,
    // else 0
    uint8_t tthub_slotid;

    // if low/full speed and connected to high spd hub,
    // then this is the port number of the parent high speed hub
    // else 0
    uint8_t ttportnum;

    // if this is a high speed hub tt think time needed to proceed to
    // the next full/low speed transaction,
    // and interrupter target
    uint16_t ttt_intrtarget;

    // USB device address
    uint8_t usbdevaddr;

    uint8_t rsvd[2];

    // Slot state (disabled/enabled, default, addressed, configured)
    uint8_t slotstate;

    uint32_t rsvd2[4];

    // If the HCCPARAMS1 CSZ field is 1, then this structure is 32 bytes larger
} __packed;

C_ASSERT(sizeof(usbxhci_slotctx_t) == 0x20);

#define USBXHCI_SLOTCTX_2_TTT_BIT           0
#define USBXHCI_SLOTCTX_2_TTT_BITS          2
#define USBXHCI_SLOTCTX_2_INTRTARGET_BIT    6
#define USBXHCI_SLOTCTX_2_INTRTARGET_BITS   10

#define USBXHCI_SLOTCTX_2_TTT_MASK    ((1U<<USBXHCI_SLOTCTX_2_TTT_BITS)-1)
#define USBXHCI_SLOTCTX_2_TTT_n(n)    ((n)<<USBXHCI_SLOTCTX_2_TTT_BIT)
#define USBXHCI_SLOTCTX_2_TTT \
    (USBXHCI_SLOTCTX_2_TTT_MASK<<USBXHCI_SLOTCTX_2_TTT_BIT)

#define USBXHCI_SLOTCTX_2_INTRTARGET_MASK \
    ((1U<<USBXHCI_SLOTCTX_2_INTRTARGET_BITS)-1)
#define USBXHCI_SLOTCTX_2_INTRTARGET_n(n) \
    ((n)<<USBXHCI_SLOTCTX_2_INTRTARGET_BIT)
#define USBXHCI_SLOTCTX_2_INTRTARGET \
    (USBXHCI_SLOTCTX_2_INTRTARGET_MASK<<USBXHCI_SLOTCTX_2_INTRTARGET_BIT)

#define USBXHCI_SLOTCTX_3_SLOTSTATE_BIT     4
#define USBXHCI_SLOTCTX_3_SLOTSTATE_BITS    4

#define USBXHCI_SLOTCTX_3_SLOTSTATE_MASK \
    ((1U<<USBXHCI_SLOTCTX_3_SLOTSTATE_BITS)-1)
#define USBXHCI_SLOTCTX_3_SLOTSTATE_n(n) \
    ((n)<<USBXHCI_SLOTCTX_3_SLOTSTATE_BIT)
#define USBXHCI_SLOTCTX_3_SLOTSTATE \
    (USBXHCI_SLOTCTX_3_SLOTSTATE_MASK<<USBXHCI_SLOTCTX_3_SLOTSTATE_BIT)

// 6.2.3 Endpoint Context

struct usbxhci_ep_ctx_t {
    // Endpoint state (3 bits)
    uint8_t ep_state;

    // LSA:MaxPStreams:Mult
    // Mult: Maximum supported number of bursts in an interval
    //   If LEC == 1, this must be 0
    // MaxPStreams: Maximum number of primary stream IDs
    //   Must be zero for non-SS, control, isoch, and interrupt endpoints
    // LSA: 1=enable secondary stream arrays
    //   if MaxPStreams is 0, this must be zero
    uint8_t mml;

    // delay between consecutive requests. (2**interval) * 125us units
    uint8_t interval;

    // Max Endpoint Service Time Interval Payload Hi
    //   If LEC == 0, this is 0
    uint8_t max_eist_pl_hi;

    // HID:EPType:CErr
    //  Error count: 0=allow infinite retries, n=allow n errors then error trb
    //  EPType: 0=not valid, 1=isoch out, 2=bulk out, 3=interrupt out
    //    4=control bidir, 5=isoch in, 6=bulk in, 7=interrupt in
    //  HostInitiateDisable: 0=normal
    uint8_t ceh;

    // Max consecutive transactions per opportunity, minus one
    uint8_t max_burst;

    // Max packet size
    uint16_t max_packet;

    // Transfer Ring dequeue pointer
    //  if MaxPStreams = 0, this points to a transfer ring
    //  if MaxPStreams > 0, this points to a stream context array
    //  Also contains dequeue cycle state
    uint64_t tr_dq_ptr;

    // Average TRB length (used to approximate bandwidth requirements)
    uint16_t avg_trb_len;

    // Max Endpoint Service Time Interval Payload Lo
    uint16_t max_eist_pl_lo;

    uint32_t rsvd[3];
} __packed;

C_ASSERT(sizeof(usbxhci_ep_ctx_t) == 0x20);

#define USBXHCI_EPTYPE_INVALID    0
#define USBXHCI_EPTYPE_ISOCHOUT   1
#define USBXHCI_EPTYPE_BULKOUT    2
#define USBXHCI_EPTYPE_INTROUT    3
#define USBXHCI_EPTYPE_CTLBIDIR   4
#define USBXHCI_EPTYPE_ISOCHIN    5
#define USBXHCI_EPTYPE_BULKIN     6
#define USBXHCI_EPTYPE_INTRIN     7

// 6.2.1 Device Context

struct usbxhci_devctx_small_t {
    usbxhci_slotctx_t slotctx;
    usbxhci_ep_ctx_t epctx[16];
} __packed;

struct usbxhci_devctx_large_t {
    usbxhci_slotctx_t slotctx;
    uint8_t rsvd[32];

    usbxhci_ep_ctx_t epctx[16];
} __packed;

union usbxhci_devctx_t {
    void *any;
    usbxhci_devctx_small_t *small;
    usbxhci_devctx_large_t *large;
};

// 6.2.5.1 Input Control Context

struct usbxhci_inpctlctx_t {
    uint32_t drop_bits;
    uint32_t add_bits;
    uint32_t rsvd[5];
    uint8_t cfg;
    uint8_t iface_num;
    uint8_t alternate;
    uint8_t rsvd2;
} __packed;

// 6.2.5 Input Context

struct usbxhci_inpctx_small_t {
    usbxhci_inpctlctx_t inpctl;

    usbxhci_slotctx_t slotctx;

    usbxhci_ep_ctx_t epctx[32];
} __packed;

struct usbxhci_inpctx_large_t {
    usbxhci_inpctlctx_t inpctl;

    usbxhci_slotctx_t slotctx;
    uint8_t rsvd[32];

    usbxhci_ep_ctx_t epctx[32];
} __packed;

union usbxhci_inpctx_t {
    void *any;
    usbxhci_inpctx_small_t *small;
    usbxhci_inpctx_large_t *large;
};

// 6.4.3 Command TRB

struct usbxhci_cmd_trb_t {
    uint32_t data[4];
} __packed;

struct usbxhci_cmd_trb_noop_t {
    uint32_t rsvd1[3];
    uint8_t cycle;
    uint8_t trb_type;
    uint16_t rsvd2;
} __packed;

struct usbxhci_cmd_trb_reset_ep_t {
    uint32_t rsvd1[3];
    uint8_t cycle;
    uint8_t trb_type_tsp;
    uint8_t epid;
    uint8_t slotid;
} __packed;

struct usbxhci_cmd_trb_setaddr_t {
    uint64_t input_ctx_physaddr;
    uint32_t rsvd;
    uint8_t cycle;
    uint8_t trb_type;
    uint8_t rsvd2;
    uint8_t slotid;
} __packed;

C_ASSERT(sizeof(usbxhci_cmd_trb_setaddr_t) == 16);

// 6.4.4.1 Link TRB
struct usbxhci_cmd_trb_link_t {
    uint64_t ring_physaddr;
    uint16_t rsvd;
    uint16_t intrtarget;
    uint8_t c_tc_ch_ioc;
    uint8_t trb_type;
    uint16_t rsvd2;
} __packed;

C_ASSERT(sizeof(usbxhci_cmd_trb_link_t) == 16);

// 6.5 Event Ring Segment Table

struct usbxhci_evtring_seg_t {
    // Base address, must be 64-byte aligned
    uint64_t base;

    // Minimum count=16, maximum count=4096
    uint16_t trb_count;

    uint16_t resvd;
    uint32_t resvd2;
} __packed;

C_ASSERT(sizeof(usbxhci_evtring_seg_t) == 0x10);

//
//

// 6.4.2 Event TRBs

struct usbxhci_evt_t {
    uint32_t data[3];
    uint16_t flags;
    uint8_t id;
    uint8_t slotid;
} __packed;

C_ASSERT(sizeof(usbxhci_evt_t) == 0x10);

#define USBXHCI_EVT_FLAGS_C_BIT     0
#define USBXHCI_EVT_FLAGS_C         (1U<<USBXHCI_EVT_FLAGS_C_BIT)

#define USBXHCI_EVT_FLAGS_TYPE_BIT  10
#define USBXHCI_EVT_FLAGS_TYPE_BITS 6
#define USBXHCI_EVT_FLAGS_TYPE_MASK ((1U<<USBXHCI_EVT_FLAGS_TYPE_BITS)-1)
#define USBXHCI_EVT_FLAGS_TYPE_n(n) ((n)<<USBXHCI_EVT_FLAGS_TYPE_BIT)
#define USBXHCI_EVT_FLAGS_TYPE \
    (USBXHCI_EVT_FLAGS_TYPE_MASK<<USBXHCI_EVT_FLAGS_TYPE_BIT)

// 6.4.2.2 Command Completion Event TRB

struct usbxhci_evt_cmdcomp_t {
    uint64_t command_trb_ptr;

    // USBXHCI_EVT_CMDCOMP_INFO
    uint32_t info;

    uint16_t flags;
    uint8_t id;
    uint8_t slotid;
} __packed;

C_ASSERT(sizeof(usbxhci_evt_cmdcomp_t) == 0x10);

//
// 6.4.6 TRB Types

// Transfer ring
#define USBXHCI_TRB_TYPE_NORMAL             1
#define USBXHCI_TRB_TYPE_SETUP              2
#define USBXHCI_TRB_TYPE_DATA               3
#define USBXHCI_TRB_TYPE_STATUS             4
#define USBXHCI_TRB_TYPE_ISOCH              5
// Command ring or Transfer ring
#define USBXHCI_TRB_TYPE_LINK               6
// Transfer ring
#define USBXHCI_TRB_TYPE_EVTDATA            7
#define USBXHCI_TRB_TYPE_NOOP               8
// Command ring
#define USBXHCI_TRB_TYPE_ENABLESLOTCMD      9
#define USBXHCI_TRB_TYPE_DISABLESLOTCMD     10
#define USBXHCI_TRB_TYPE_ADDRDEVCMD         11
#define USBXHCI_TRB_TYPE_CONFIGUREEPCMD     12
#define USBXHCI_TRB_TYPE_EVALCTXCMD         13
#define USBXHCI_TRB_TYPE_RESETEPCMD         14
#define USBXHCI_TRB_TYPE_STOPEPCMD          15
#define USBXHCI_TRB_TYPE_SETTRDEQPTRCMD     16
#define USBXHCI_TRB_TYPE_RESETDEVCMD        17
#define USBXHCI_TRB_TYPE_FORCEEVTCMD        18
#define USBXHCI_TRB_TYPE_NEGOBWCMD          19
#define USBXHCI_TRB_TYPE_SETLATTOLVALCMD    20
#define USBXHCI_TRB_TYPE_GETPORTBWCMD       21
#define USBXHCI_TRB_TYPE_FORCEHDRCMD        22
#define USBXHCI_TRB_TYPE_NOOPCMD            23
// Event ring
#define USBXHCI_TRB_TYPE_XFEREVT            32
#define USBXHCI_TRB_TYPE_CMDCOMPEVT         33
#define USBXHCI_TRB_TYPE_PORTSTSCHGEVT      34
#define USBXHCI_TRB_TYPE_BWREQEVT           35
#define USBXHCI_TRB_TYPE_DBEVT              36
#define USBXHCI_TRB_TYPE_HOSTCTLEVT         37
#define USBXHCI_TRB_TYPE_DEVNOTIFEVT        38
#define USBXHCI_TRB_TYPE_MFINDEXWRAPEVT     39

//
// 6.4.2.1 Transfer Event TRB

struct usbxhci_evt_xfer_t {
    uint64_t trb_ptr;
    uint16_t len_lo;
    uint8_t len_hi;
    uint8_t cc;
    uint8_t flags;
    uint8_t trb_type;
    uint8_t ep_id;
    uint8_t slotid;
} __packed;

//
// 6.4.2.2 Command Completion Event TRB

struct usbxhci_evt_completion_t {
    uint64_t trb_ptr;
    uint16_t len_lo;
    uint8_t len_hi;
    uint8_t cc;
    uint8_t flags;
    uint8_t trb_type;
    uint8_t ep_id;
    uint8_t slotid;
} __packed;

//
// 6.4.2.3 Port Status Change Event TRB

struct usbxhci_evt_portstchg_t {
    uint16_t rsvd1;
    uint8_t rsvd2;
    uint8_t portid;
    uint32_t rsvd3;
    uint16_t rsvd4;
    uint8_t cc;
    uint16_t flags;
    uint16_t rsvd5;
} __packed;

//
// 6.4.2.4 Bandwidth Request Event TRB

struct usbxhci_evt_bwreq_t {
    uint32_t rsvd1;
    uint32_t rsvd2;
    uint16_t rsvd3;
    uint8_t rsvd4;
    uint8_t cc;
    uint16_t flags;
    uint8_t rsvd5;
    uint8_t slotid;
} __packed;

//
// 6.4.2.5 Doorbell Event TRB

struct usbxhci_evt_db_t {
    uint8_t reason;
    uint8_t rsvd[10];
    uint8_t cc;
    uint16_t flags;
    uint8_t vf_id;
    uint8_t slotid;
} __packed;

//
// 6.4.1.2 Control TRBs

struct usbxhci_ctl_trb_generic_t {
    uint32_t data[3];
    uint16_t flags;
    uint16_t trt;
} __packed;

C_ASSERT(sizeof(usbxhci_ctl_trb_generic_t) == 0x10);
C_ASSERT(offsetof(usbxhci_ctl_trb_generic_t, flags) == 0x0c);

// 6.4.1.2.1 Setup stage TRB

// USB 3.1 spec 9.3
struct usbxhci_ctl_trb_setup_t {
    //
    // USB fields

    // bit 7 0=host-to-device, 1=device-to-host, 6:5=type, 4:0=recipient
    uint8_t bm_req_type;

    usb_rqcode_t request;

    uint16_t value;
    uint16_t index;
    uint16_t length;

    //
    // xHCI fields

    uint32_t xferlen_intr;
    uint16_t flags;
    uint16_t trt;
} __packed;

C_ASSERT(sizeof(usbxhci_ctl_trb_setup_t) == 0x10);
C_ASSERT(offsetof(usbxhci_ctl_trb_setup_t, flags) == 0x0c);

struct usbxhci_ctl_trb_data_t {
    uint64_t data_physaddr;
    uint32_t xfer_td_intr;
    uint16_t flags;
    uint16_t dir;
} __packed;

C_ASSERT(sizeof(usbxhci_ctl_trb_data_t) == 0x10);
C_ASSERT(offsetof(usbxhci_ctl_trb_data_t, flags) == 0x0c);

struct usbxhci_ctl_trb_status_t {
    uint64_t rsvd;
    uint16_t rsvd2;
    uint16_t intr;
    uint16_t flags;
    uint16_t dir;
} __packed;

C_ASSERT(sizeof(usbxhci_ctl_trb_status_t) == 0x10);
C_ASSERT(offsetof(usbxhci_ctl_trb_status_t, flags) == 0x0c);

union usbxhci_ctl_trb_t {
    usbxhci_ctl_trb_generic_t generic;
    usbxhci_ctl_trb_setup_t setup;
    usbxhci_ctl_trb_data_t data;
    usbxhci_ctl_trb_status_t status;
} __packed;

C_ASSERT(sizeof(usbxhci_ctl_trb_t) == 0x10);

//
// 6.4.3.6 Evaluate Context command

struct usbxhci_ctl_trb_evalctx_t {
    uint64_t input_ctx_ptr;
    uint32_t rsvd[1];
    uint16_t flags;
    uint16_t trt;
} __packed;

#define USBXHCI_CTL_TRB_BMREQT_TYPE_STD         0
#define USBXHCI_CTL_TRB_BMREQT_TYPE_CLASS       1
#define USBXHCI_CTL_TRB_BMREQT_TYPE_VENDOR      2

#define USBXHCI_CTL_TRB_BMREQT_RECIP_DEVICE     0
#define USBXHCI_CTL_TRB_BMREQT_RECIP_INTERFACE  1
#define USBXHCI_CTL_TRB_BMREQT_RECIP_ENDPOINT   2
#define USBXHCI_CTL_TRB_BMREQT_RECIP_OTHER      3
#define USBXHCI_CTL_TRB_BMREQT_RECIP_VENDOR     31

#define USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_BIT    0
#define USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_BITS   17
#define USBXHCI_CTL_TRB_XFERLEN_INTR_INTR_BIT       22
#define USBXHCI_CTL_TRB_XFERLEN_INTR_INTR_BITS      10

#define USBXHCI_CTL_TRB_TRT_NODATA  0
#define USBXHCI_CTL_TRB_TRT_OUT     2
#define USBXHCI_CTL_TRB_TRT_IN      3

//
// Doorbells

#define USBXHCI_DB_VAL_CTL_EP_UPD       1
#define USBXHCI_DB_VAL_OUT_EP_UPD_n(n)  ((n)*2+0)
#define USBXHCI_DB_VAL_IN_EP_UPD_n(n)   ((n)*2+1)

//
// Interface Interface

struct usbxhci_device_vtbl_t {
    void (*init)(void);
};

struct usbxhci_interrupter_info_t {
    usbxhci_evt_t volatile *evt_ring;
    uint64_t evt_ring_physaddr;
    uint32_t next;
    uint32_t count;
    uint8_t ccs;
};

struct usbxhci_portinfo_t {
    usbxhci_evtring_seg_t volatile *dev_evt_segs;
    usbxhci_evt_t volatile *dev_evt_ring;
};

struct usbxhci_endpoint_target_t {
    uint8_t slotid;
    uint8_t epid;
};

struct usbxhci_endpoint_data_t : public refcounted<usbxhci_endpoint_data_t> {
    usbxhci_endpoint_target_t target;

    usbxhci_cmd_trb_t *xfer_ring;
    uint64_t xfer_ring_physaddr;
    uint32_t xfer_next;
    uint32_t xfer_count;
    uint8_t ccs;
};

struct usbxhci_pending_cmd_t;

class usbxhci;

struct usbxhci_pending_cmd_t final
        : public refcounted<usbxhci_pending_cmd_t> {
    uint64_t cmd_physaddr;
    usb_iocp_t *iocp;

    size_t hash() const
    {
        return cmd_physaddr;
    }
};

class usbxhci : public usb_bus_t {
public:
    usbxhci();

    static void detect();

    void dump_config_desc(const usb_config_helper &cfg_hlp);

    bool add_device(int port, int route);

protected:
    //
    // usb_bus_t interface

    int get_max_ports() override final
    {
        return maxports;
    }

    bool port_device_present(int port) override final
    {
        if (likely(port >= 0 && port < maxports))
            return mmio_op->ports[port].portsc & USBXHCI_PORTSC_CCS;
        return false;
    }

    int enable_slot(int port) override final;

    int set_address(int slotid, int port, uint32_t route) override final;

    bool get_pipe(int slotid, int epid, usb_pipe_t &pipe) override final;

    bool alloc_pipe(int slotid, int epid, usb_pipe_t &pipe,
                    int max_packet_sz, int interval,
                    usb_ep_attr ep_type) override final;

    int send_control(int slotid, uint8_t request_type, uint8_t request,
                     uint16_t value, uint16_t index, uint16_t length,
                     void *data) override final;

    int send_control_async(int slotid, uint8_t request_type, uint8_t request,
            uint16_t value, uint16_t index, uint16_t length, void *data,
            usb_iocp_t *iocp) override final;

    int xfer(int slotid, uint8_t epid, uint16_t stream_id,
             uint32_t length, void *data, int dir) override final;

    int xfer_async(int slotid, uint8_t epid, uint16_t stream_id,
                   uint32_t length, void *data, int dir,
                   usb_iocp_t *iocp) override final;

    usb_ep_state_t get_ep_state(int slotid, uint8_t epid) override final;

    int reset_ep(int slotid, uint8_t epid) override final;

    int reset_ep_async(int slotid, uint8_t epid,
                       usb_iocp_t *iocp) override final;

    bool configure_hub_port(int slotid, int port) override final;

    bool set_hub_port_count(int slotid,
                            usb_hub_desc const &hub_desc) override final;

private:
    using lock_type = mcslock;
    using scoped_lock = unique_lock<lock_type>;

    errno_t cc_to_errno(usb_cc_t cc);

    usbxhci_slotctx_t *dev_ctx_ent_slot(size_t slotid);

    usbxhci_ep_ctx_t *dev_ctx_ent_ep(size_t slot, size_t i);

    void set_cmd_trb_cycle(void *cmd) {
        char *c = (char*)cmd + 0xc;
        *c |= (pcs != 0);
    }

    void ring_doorbell(uint32_t doorbell, uint8_t value,
                       uint16_t stream_id);

    void issue_cmd(void *cmd, usb_iocp_t *iocp);

    void add_xfer_trbs(uint8_t slotid, uint8_t epid,
                       uint16_t stream_id, size_t count, int dir,
                       void *trbs, usb_iocp_t *iocp);

    void insert_pending_command(uint64_t cmd_physaddr, usb_iocp_t *iocp,
                                scoped_lock const&);

    int make_data_trbs(usbxhci_ctl_trb_data_t *trbs, size_t trb_capacity,
                       void *data, uint32_t length, int dir, bool intr);

    int make_setup_trbs(usbxhci_ctl_trb_t *trbs, int trb_capacity,
                        void *data, uint16_t length, int dir,
                        uint8_t request_type, uint8_t request,
                        uint16_t value, uint16_t index);

    int make_setup_trbs(usbxhci_ctl_trb_t *trbs, size_t trb_capacity,
            void *data, uint16_t length, int dir,
            uint8_t bmreq_type, uint8_t bmreq_recip,
            bool to_host, usb_rqcode_t request,
            uint16_t value, uint16_t index);

    int set_config(uint8_t slotid, uint8_t config);

    int get_descriptor(uint8_t slotid, uint8_t epid,
                       void *desc, uint16_t desc_size,
                       usb_req_type req_type,
                       usb_req_recip_t recip,
                       usb_desctype_t desc_type,
                       uint8_t desc_index);

    usb_cc_t fetch_inp_ctx(int slotid, int epid, usbxhci_inpctx_t &inp,
                      usbxhci_inpctlctx_t **p_ctlctx,
                      usbxhci_slotctx_t **p_inpslotctx,
                      usbxhci_ep_ctx_t **p_inpepctx);

    usb_cc_t commit_inp_ctx(int slotid, int epid,
                            usbxhci_inpctx_t &inp, uint32_t trb_type);

    int update_slot_ctx(uint8_t slotid, usb_desc_device *dev_desc);

    void get_config(uint8_t slotid, usb_desc_config *desc, uint8_t desc_size,
                    usb_iocp_t *iocp);

    void cmd_comp(usbxhci_evt_t *evt, usb_iocp_t *iocp);

    usbxhci_endpoint_data_t *add_endpoint(uint8_t slotid, uint8_t epid);

    usbxhci_endpoint_data_t *lookup_endpoint(uint8_t slotid, uint8_t epid);

    void evt_handler(usbxhci_interrupter_info_t *ir_info,
                     usbxhci_intr_t *ir, usbxhci_evt_t *evt, size_t ii);

    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    void irq_handler(int irq);

    void init(pci_dev_iterator_t& pci_iter);

    //

    uint64_t mmio_addr;

    usbxhci_capreg_t volatile *mmio_cap;
    usbxhci_opreg_t volatile *mmio_op;
    usbxhci_rtreg_t volatile *mmio_rt;
    usbxhci_dbreg_t volatile *mmio_db;

    uint64_t volatile *dev_ctx_ptrs;
    usbxhci_devctx_t dev_ctx;
    usbxhci_cmd_trb_t volatile *dev_cmd_ring;
    uint64_t cmd_ring_physaddr;

    usbxhci_evtring_seg_t volatile *dev_evt_segs;

    usbxhci_interrupter_info_t *interrupters;

    lock_type endpoints_lock;
    vector<refptr<usbxhci_endpoint_data_t>> endpoints;

    // Endpoint data keyed on usbxhci_endpoint_target_t
    hashtbl_t<usbxhci_endpoint_data_t, usbxhci_endpoint_target_t,
    &usbxhci_endpoint_data_t::target> endpoint_lookup;

    // Maximums
    uint32_t maxslots;
    uint32_t maxintr;
    int maxports;

    // Next command slot
    uint32_t cr_next;

    // Command ring size
    uint32_t cr_size;

    usbxhci_portinfo_t *ports;
    unsigned port_count;

    bool use_msi;

    // Producer Cycle State
    uint8_t pcs;

    // 0 for 32 byte usbxhci_devctx_t, 1 for 64 byte usbxhci_devctx_large_t
    uint32_t dev_ctx_large;

    pci_irq_range_t irq_range;

    hashtbl_t<usbxhci_pending_cmd_t,
    uint64_t, &usbxhci_pending_cmd_t::cmd_physaddr> usbxhci_pending_ht;

    vector<usb_iocp_t*> completed_iocp;

    // Command issue lock
    lock_type lock_cmd;
};

static vector<usbxhci*> usbxhci_devices;

// Handle 32 or 64 byte device context size
usbxhci_slotctx_t *usbxhci::dev_ctx_ent_slot(size_t slotid)
{
    if (dev_ctx_large)
        return &dev_ctx.large[slotid].slotctx;
    return &dev_ctx.small[slotid].slotctx;
}

// Handle 32 or 64 byte device context size
__used
usbxhci_ep_ctx_t *usbxhci::dev_ctx_ent_ep(size_t slot, size_t i)
{
    if (dev_ctx_large)
        return &dev_ctx.large[slot].epctx[i];
    return &dev_ctx.small[slot].epctx[i];
}

void usbxhci::ring_doorbell(uint32_t doorbell, uint8_t value,
                            uint16_t stream_id)
{
    atomic_barrier();
    mmio_db[doorbell] = value | (stream_id << 16);
    atomic_barrier();
}

void usbxhci::insert_pending_command(uint64_t cmd_physaddr,
                                     usb_iocp_t *iocp,
                                     const scoped_lock &)
{
    usbxhci_pending_cmd_t *pc = new usbxhci_pending_cmd_t;
    pc->cmd_physaddr = cmd_physaddr;
    pc->iocp = iocp;
    usbxhci_pending_ht.insert(pc);
}

void usbxhci::issue_cmd(void *cmd, usb_iocp_t *iocp)
{
    scoped_lock hold_cmd_lock(lock_cmd);

    USBXHCI_TRACE("Writing command to command ring at %u\n", cr_next);

    usbxhci_cmd_trb_t *s = (usbxhci_cmd_trb_t *)&dev_cmd_ring[cr_next++];

    set_cmd_trb_cycle(cmd);

    // This memcpy write at least 32 bits at a time!
    // Cycle byte is at offset 12
    memcpy(s, cmd, sizeof(*s));

    size_t offset = uintptr_t(s) - uintptr_t(dev_cmd_ring);

    uint64_t cmd_physaddr = cmd_ring_physaddr + offset;

    insert_pending_command(cmd_physaddr, iocp, hold_cmd_lock);

    if (cr_next == cr_size) {
        usbxhci_cmd_trb_link_t *link = (usbxhci_cmd_trb_link_t *)
                &dev_cmd_ring[cr_next];

        USBXHCI_CMD_TRB_C_SET(link->c_tc_ch_ioc, pcs != 0);

        pcs = !pcs;
        cr_next = 0;
    }

    // Ring controller command doorbell
    ring_doorbell(0, 0, 0);

    hold_cmd_lock.unlock();
}

void usbxhci::add_xfer_trbs(uint8_t slotid, uint8_t epid, uint16_t stream_id,
                            size_t count, int dir, void *trbs,
                            usb_iocp_t *iocp)
{
    scoped_lock hold_cmd_lock(lock_cmd);

    usbxhci_endpoint_data_t *epd = lookup_endpoint(slotid, epid);

    for (size_t i = 0; i < count; ++i) {
        USBXHCI_TRACE("Writing TRB s%d:ep%d to %zx\n", slotid, epid,
                      mphysaddr(&epd->xfer_ring[epd->xfer_next]));

        // Get pointer to source TRB
        auto src = (usbxhci_cmd_trb_t *)trbs + i;

        USBXHCI_CTL_TRB_FLAGS_C_SET(src->data[3], !epd->ccs);
        auto dst = (usbxhci_cmd_trb_t *)&epd->xfer_ring[epd->xfer_next];

        // Verify that the cycle bit of the existing TRB is as expected
        assert(USBXHCI_CTL_TRB_FLAGS_C_GET(dst->data[3]) != (epd->ccs != 0));

        // Copy the TRB carefully, ensuring cycle bit is set last
        dst->data[0] = src->data[0];
        dst->data[1] = src->data[1];
        dst->data[2] = src->data[2];
        // Initially set the cycle bit to the value that prevents TRB execution
        atomic_st_rel(&dst->data[3],
                (src->data[3] & ~USBXHCI_CTL_TRB_FLAGS_C) |
                USBXHCI_CTL_TRB_FLAGS_C_n(!epd->ccs));
        // Guarantee ordering and set cycle bit last
        atomic_st_rel(&dst->data[3],
                (src->data[3] & ~USBXHCI_CTL_TRB_FLAGS_C) |
                USBXHCI_CTL_TRB_FLAGS_C_n(epd->ccs));

        if (iocp && ((i + 1) == count)) {
            insert_pending_command(
                        epd->xfer_ring_physaddr + epd->xfer_next *
                        sizeof(*epd->xfer_ring), iocp, hold_cmd_lock);
        }

        if (++epd->xfer_next >= epd->xfer_count) {
            // Update link TRB cycle bit
            auto link = (usbxhci_cmd_trb_t*)(epd->xfer_ring + epd->xfer_next);

            // Copy the chain bit from the last TRB to propagate possible
            // chain across the link TRB
            bool chain = USBXHCI_CTL_TRB_FLAGS_CH_GET(src->data[3]);
            USBXHCI_CTL_TRB_FLAGS_CH_SET(link->data[3], chain);

            atomic_st_rel(&dst->data[3],
                    (src->data[3] & ~USBXHCI_CTL_TRB_FLAGS_C) |
                    USBXHCI_CTL_TRB_FLAGS_C_n(epd->ccs));

            epd->ccs = !epd->ccs;
            epd->xfer_next = 0;
        }
    }

    ring_doorbell(slotid, (dir || !epid)
                  ? USBXHCI_DB_VAL_IN_EP_UPD_n(epid & 0xF)
                  : USBXHCI_DB_VAL_OUT_EP_UPD_n(epid & 0xF),
                  stream_id);
}

int usbxhci::set_config(uint8_t slotid, uint8_t config)
{
    usbxhci_ctl_trb_t trbs[2] = {};

    int trb_count = make_setup_trbs(
                trbs, countof(trbs),
                nullptr, 0, 1, USBXHCI_CTL_TRB_BMREQT_TYPE_STD,
                USBXHCI_CTL_TRB_BMREQT_RECIP_DEVICE, false,
                usb_rqcode_t::SET_CONFIGURATION, config, 0);

    usb_blocking_iocp_t block;

    add_xfer_trbs(slotid, 0, 0, trb_count, 1, trbs, &block);
    block.set_expect(1);

    block.wait();

    return block.get_result().len_or_error();
}

int usbxhci::get_descriptor(uint8_t slotid, uint8_t epid,
                            void *desc, uint16_t desc_size,
                            usb_req_type req_type, usb_req_recip_t recip,
                            usb_desctype_t desc_type, uint8_t desc_index)
{
    usbxhci_ctl_trb_t trbs[4] = {};

    int trb_count = make_setup_trbs(
                trbs, countof(trbs),
                desc, desc_size, 1, uint8_t(req_type),
                uint8_t(recip), true,
                usb_rqcode_t::GET_DESCRIPTOR,
                (uint8_t(desc_type) << 8) | desc_index, 0);

    assert(trb_count >= 0 && trb_count <= (int)countof(trbs));

    usb_blocking_iocp_t block;

    add_xfer_trbs(slotid, epid, 0, trb_count, 1, trbs, &block);
    block.set_expect(1);

    block.wait();

    return block.get_result().len_or_error();
}

void usbxhci::cmd_comp(usbxhci_evt_t *evt, usb_iocp_t *iocp)
{
    if (iocp) {
        usb_iocp_result_t& result = iocp->get_result();
        result.cc = usb_cc_t(USBXHCI_EVT_CMDCOMP_INFO_CC_GET(evt->data[2]));
        result.ccp = USBXHCI_EVT_CMDCOMP_INFO_CCP_GET(evt->data[2]);
        result.slotid = evt->slotid;
        completed_iocp.push_back(iocp);
    } else {
        USBXHCI_TRACE("Got cmd_comp with null iocp\n");
    }
}

usbxhci_endpoint_data_t *usbxhci::add_endpoint(uint8_t slotid, uint8_t epid)
{
    usbxhci_endpoint_data_t *newepd = new usbxhci_endpoint_data_t;
    if (!newepd)
        return nullptr;

    newepd->target.slotid = slotid;
    newepd->target.epid = epid;

    scoped_lock hold_endpoints_lock(endpoints_lock);

    if (!endpoints.emplace_back(newepd))
        return nullptr;

    newepd->xfer_next = 0;
    newepd->xfer_count = PAGESIZE / sizeof(*newepd->xfer_ring);
    newepd->xfer_ring = (usbxhci_cmd_trb_t *)
            mmap(0, sizeof(*newepd->xfer_ring) * newepd->xfer_count,
                 PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
    if (unlikely(newepd->xfer_ring == MAP_FAILED || !newepd->xfer_ring)) {
        endpoints.pop_back();
        return nullptr;
    }
    newepd->ccs = 1;

    newepd->xfer_ring_physaddr = mphysaddr(newepd->xfer_ring);

    // Create link TRB to wrap transfer ring
    usbxhci_cmd_trb_link_t *link = (usbxhci_cmd_trb_link_t *)
            newepd->xfer_ring + (newepd->xfer_count - 1);

    *link = {};
    link->trb_type = USBXHCI_CMD_TRB_TYPE_n(USBXHCI_TRB_TYPE_LINK);
    link->ring_physaddr = newepd->xfer_ring_physaddr;
    link->c_tc_ch_ioc = USBXHCI_CMD_TRB_TC | USBXHCI_CMD_TRB_C_n(!newepd->ccs);

    // Avoid overwriting link TRB
    --newepd->xfer_count;

    USBXHCI_TRACE("Transfer ring physical address for slot=%d, ep=%d: %lx\n",
                  slotid, epid, newepd->xfer_ring_physaddr);

    endpoint_lookup.insert(newepd);

    return newepd;
}

usbxhci_endpoint_data_t *usbxhci::lookup_endpoint(uint8_t slotid, uint8_t epid)
{
    scoped_lock hold_endpoints_lock(endpoints_lock);

    usbxhci_endpoint_target_t key{ slotid, epid };
    usbxhci_endpoint_data_t *data = (usbxhci_endpoint_data_t *)
            endpoint_lookup.lookup(&key);

    return data;
}

void usbxhci::dump_config_desc(usb_config_helper const& cfg_hlp)
{
    USBXHCI_TRACE("Dumping configuration descriptors\n");

    for (uint8_t const *base = (uint8_t const *)(cfg_hlp.find_config(0)),
         *raw = base;
         *raw; raw += *raw)
        hex_dump(raw, *raw, raw - base);

    usb_desc_config const *cfg;
    usb_desc_iface const *iface;
    usb_desc_ep const *ep;

    for (int cfg_idx = 0;
         (cfg = cfg_hlp.find_config(cfg_idx)) != nullptr;
         ++cfg_idx) {

        USBXHCI_TRACE("cfg #0x%x max_power=%umA {\n",
                      cfg_idx, cfg->max_power * 2);

        for (int iface_idx = 0;
             (iface = cfg_hlp.find_iface(cfg, iface_idx)) != nullptr;
             ++iface_idx) {
            USBXHCI_TRACE("  iface #0x%x: class=0x%x (%s)"
                          " subclass=0x%x, proto=0x%x {\n",
                          iface->iface_num, iface->iface_class,
                          cfg_hlp.class_code_text(iface->iface_class),
                          iface->iface_subclass, iface->iface_proto);

            for (int ep_idx = 0;
                 (ep = cfg_hlp.find_ep(iface, ep_idx)) != nullptr;
                 ++ep_idx) {
                USBXHCI_TRACE("    ep 0x%x: dir=%s attr=%s,"
                              " maxpktsz=0x%x, interval=%u\n",
                              ep->ep_addr,
                              ep->ep_addr >= 0x80 ? "IN" : "OUT",
                              usb_config_helper::ep_attr_text(ep->ep_attr),
                              ep->max_packet_sz, ep->interval);
            }

            USBXHCI_TRACE("  }\n");
        }

        USBXHCI_TRACE("}\n");
    }

    USBXHCI_TRACE("Done configuration descriptors\n");
}

bool usbxhci::add_device(int port, int route)
{
    USBXHCI_TRACE("Adding device, port=%d, route=0x%x\n", port, route);

    int slotid = enable_slot(port);

    int err = set_address(slotid, port, route);
    if (err < 0)
        return false;

    usb_desc_device dev_desc{};

    err = get_descriptor(slotid, 0, &dev_desc, 8, usb_req_type::STD,
                         usb_req_recip_t::DEVICE,
                         usb_desctype_t::DEVICE, 0);
    if (err < 0)
        return false;

    if (dev_desc.maxpktsz != 8) {
        err = update_slot_ctx(slotid, &dev_desc);
        if (err < 0)
            return false;
    }

    // Get first 8 bytes of device descriptor to get max packet size
    unique_ptr<usb_desc_config> cfg_buf((usb_desc_config*)calloc(1, 128));

    err = get_descriptor(slotid, 0, cfg_buf, 128, usb_req_type::STD,
                         usb_req_recip_t::DEVICE,
                         usb_desctype_t::CONFIGURATION, 0);
    if (err < 0)
        return false;

    usb_config_helper cfg_hlp(slotid, dev_desc, cfg_buf, 128);

    dump_config_desc(cfg_hlp);

    usb_class_drv_t::find_driver(&cfg_hlp, this);

    return true;
}

void usbxhci::init(pci_dev_iterator_t& pci_iter)
{
    mmio_addr = (pci_iter.config.base_addr[0] & -16) |
            (uint64_t(pci_iter.config.base_addr[1]) << 32);

    mmio_cap = (usbxhci_capreg_t*)
            mmap((void*)uintptr_t(mmio_addr),
                 64<<10, PROT_READ | PROT_WRITE,
                 MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU, -1, 0);

    mmio_op = (usbxhci_opreg_t*)((char*)mmio_cap + mmio_cap->caplength);

    mmio_rt = (usbxhci_rtreg_t*)((char*)mmio_cap + (mmio_cap->rtsoff & -32));

    mmio_db = (usbxhci_dbreg_t*)((char*)mmio_cap + (mmio_cap->dboff & -4));

    // 4.2 Host Controller Initialization

    USBXHCI_TRACE("Stopping controller\n");

    // Stop the controller
    mmio_op->usbcmd &= ~USBXHCI_USBCMD_RUNSTOP;

    // Wait for controller to stop
    while (!(mmio_op->usbsts & USBXHCI_USBSTS_HCH))
        pause();

    USBXHCI_TRACE("Resetting controller\n");

    // Reset the controller
    mmio_op->usbcmd |= USBXHCI_USBCMD_HCRST;

    // Wait for reset to complete
    while (mmio_op->usbcmd & USBXHCI_USBCMD_HCRST)
        pause();

    uint32_t hcsparams1 = mmio_cap->hcsparams1;
    maxslots = USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_GET(hcsparams1);
    maxintr = USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_GET(hcsparams1);
    maxports = USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_GET(hcsparams1);

    dev_ctx_large = USBXHCI_CAPREG_HCCPARAMS1_CSZ_GET(mmio_cap->hccparams1);

    USBXHCI_CONFIG_MAXSLOTSEN_SET(mmio_op->config, maxslots);

    USBXHCI_TRACE("devslots=%d, maxintr=%d, maxports=%d\n",
                  maxslots, maxintr, maxports);

    use_msi = pci_try_msi_irq(pci_iter, &irq_range,
                              0, false, min(16U, maxintr),
                              &usbxhci::irq_handler,
                              "usb_xhci");

    USBXHCI_TRACE("Using IRQs msi=%d, base=%u, count=%u\n",
               use_msi, irq_range.base, irq_range.count);

    size_t dev_ctx_size = dev_ctx_large
            ? sizeof(usbxhci_devctx_large_t)
            : sizeof(usbxhci_devctx_small_t);

    // Program device context address array pointer
    dev_ctx_ptrs = (uint64_t*)
            mmap(0, sizeof(*dev_ctx_ptrs) * maxslots,
                 PROT_READ | PROT_WRITE,
                 MAP_POPULATE, -1, 0);

    dev_ctx.any = mmap(0, dev_ctx_size * maxslots,
                        PROT_READ | PROT_WRITE,
                        MAP_POPULATE, -1, 0);

    // Device Context Base Address Array
    for (size_t i = 0; i < maxslots; ++i)
        dev_ctx_ptrs[i] = mphysaddr(dev_ctx_ent_slot(i));

    // Device Context Base Address Array Pointer
    mmio_op->dcbaap = mphysaddr(dev_ctx_ptrs);

    // Command ring size in entries
    cr_size = PAGESIZE / sizeof(usbxhci_cmd_trb_t);

    uint64_t cmd_ring_sz = cr_size * sizeof(usbxhci_cmd_trb_t);

    // Reserve entry for link TRB
    --cr_size;

    // Command Ring
    dev_cmd_ring = (usbxhci_cmd_trb_t*)mmap(
                nullptr, cmd_ring_sz, PROT_READ | PROT_WRITE,
                MAP_POPULATE, -1, 0);

    usbxhci_cmd_trb_link_t *link = (usbxhci_cmd_trb_link_t *)
            &dev_cmd_ring[cr_size];

    *link = {};
    link->ring_physaddr = cmd_ring_physaddr;
    link->c_tc_ch_ioc = USBXHCI_CMD_TRB_TC |
            USBXHCI_CMD_TRB_C_n(pcs != 0);
    link->trb_type = USBXHCI_CMD_TRB_TYPE_n(USBXHCI_TRB_TYPE_LINK);

    cmd_ring_physaddr = mphysaddr(dev_cmd_ring);

    USBXHCI_TRACE("Command ring at %zx-%zx\n",
                  cmd_ring_physaddr, cmd_ring_physaddr + cmd_ring_sz);

    // Command Ring Control Register
    mmio_op->crcr = cmd_ring_physaddr;

    // Event segments
    dev_evt_segs = (usbxhci_evtring_seg_t*)mmap(
                nullptr, sizeof(*dev_evt_segs) * maxintr * 4,
                PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

    interrupters = (usbxhci_interrupter_info_t*)
            malloc(sizeof(*interrupters) * maxintr);

    for (size_t i = 0; i < maxintr; ++i) {
        // Initialize Consumer Cycle State
        interrupters[i].ccs = 1;

        // Event ring
        interrupters[i].evt_ring = (usbxhci_evt_t*)
                mmap(0, PAGESIZE, PROT_READ | PROT_WRITE,
                     MAP_POPULATE, -1, 0);
        interrupters[i].count = PAGESIZE / sizeof(*interrupters[i].evt_ring);
        interrupters[i].next = 0;

        interrupters[i].evt_ring_physaddr =
                mphysaddr(interrupters[i].evt_ring);

        dev_evt_segs[i*4].base =
                interrupters[i].evt_ring_physaddr;

        dev_evt_segs[i*4].trb_count =
                PAGESIZE / sizeof(*interrupters[i].evt_ring);

        // Event ring segment table size
        USBXHCI_ERSTSZ_SZ_SET(mmio_rt->ir[i].erstsz, 1);

        // Event ring dequeue pointer
        mmio_rt->ir[i].erdp = dev_evt_segs[i*4].base;

        // Event ring segment table base address
        mmio_rt->ir[i].erstba = mphysaddr((void*)(dev_evt_segs + i*4));

        // Set interrupt moderation rate (1000 * 250ns = 250us = 4000/sec)
        USBXHCI_INTR_IMOD_IMODI_SET(mmio_rt->ir[i].imod, 1000);

        // Enable interrupt
        USBXHCI_INTR_IMAN_IE_SET(mmio_rt->ir[i].iman, 1);
    }

    for (int i = 0; i < maxports; ++i) {
        if (mmio_op->ports[i].portsc & USBXHCI_PORTSC_CCS) {
            USBXHCI_TRACE("Device is connected to port %d\n", i);

            // Reset the port
            mmio_op->ports[i].portsc |= USBXHCI_PORTSC_PR;

            USBXHCI_TRACE("Waiting for reset on port %d\n", i);

            while (mmio_op->ports[i].portsc & USBXHCI_PORTSC_PR)
                pause();

            USBXHCI_TRACE("Reset finished on port %d\n", i);
        }
    }

    mmio_op->usbcmd |= USBXHCI_USBCMD_INTE;

    mmio_op->usbcmd |= USBXHCI_USBCMD_RUNSTOP;

    port_count = 0;

    // Initialize producer cycle state for command ring
    pcs = 1;
    cr_next = 0;

    for (int port = 1; port <= maxports; ++port) {
        if (!USBXHCI_PORTSC_CCS_GET(mmio_op->ports[port].portsc))
            continue;

        add_device(port, 0);
    }
}

int usbxhci::enable_slot(int port)
{
    usbxhci_cmd_trb_noop_t cmd{};
    cmd.trb_type = USBXHCI_CMD_TRB_TYPE_n(USBXHCI_TRB_TYPE_ENABLESLOTCMD);

    USBXHCI_TRACE("Enabling slot for port %x\n", port);

    usb_blocking_iocp_t block;

    issue_cmd(&cmd, &block);
    block.set_expect(1);

    block.wait();

    USBXHCI_TRACE("enableslot completed: completion code=%x, "
                  "parameter=%x, slotid=%d\n",
                  unsigned(block.get_result().cc),
                  block.get_result().ccp,
                  block.get_result().slotid);

    return block.get_result().slot_or_error();
}

int usbxhci::set_address(int slotid, int port, uint32_t route)
{
    // Create a new device context
    //usbxhci_slotctx_t *ctx;
    //ctx = dev_ctx_ent_slot(slotid);
    //memset(ctx, 0, sizeof(*ctx));

    // Issue a SET_ADDRESS command

    // Allocate an input context
    usbxhci_inpctx_t inp;
    usbxhci_inpctlctx_t *ctlctx;
    usbxhci_slotctx_t *inpslotctx;
    usbxhci_ep_ctx_t *inpepctx;

    if (!dev_ctx_large) {
        inp.any = mmap(0, sizeof(usbxhci_inpctx_small_t),
                   PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
        ctlctx = &inp.small->inpctl;
        inpslotctx = &inp.small->slotctx;
        inpepctx = inp.small->epctx;
    } else {
        inp.any = mmap(0, sizeof(usbxhci_inpctx_large_t),
                   PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
        ctlctx = &inp.large->inpctl;
        inpslotctx = &inp.large->slotctx;
        inpepctx = inp.large->epctx;
    }

    ctlctx->add_bits = (1 << 0) | (1 << 1);

    usbxhci_portreg_t *pr = (usbxhci_portreg_t*)(mmio_op->ports + port);

    // Get speed of port
    uint8_t speed = USBXHCI_PORTSC_SPD_GET(pr->portsc);

    assert(speed == (speed & USBXHCI_SLOTCTX_RSMHC_SPEED_MASK));
    assert(route == (route & USBXHCI_SLOTCTX_RSMHC_ROUTE_MASK));
    inpslotctx->rsmhc = USBXHCI_SLOTCTX_RSMHC_ROUTE_n(route) |
            USBXHCI_SLOTCTX_RSMHC_SPEED_n(speed) |
            USBXHCI_SLOTCTX_RSMHC_CTXENT_n(1);

    // Max wakeup latency
    inpslotctx->max_exit_lat = 0;

    // Number of ports on hub (0 = not a hub)
    inpslotctx->num_ports = 0;

    // Root hub port number
    inpslotctx->root_hub_port_num = port;

    // Device address
    inpslotctx->usbdevaddr = 0;

    // Default max packet size is 8
    inpepctx->max_packet = 8;

    // 6.2.3.1 Input endpoint context 0
    inpepctx->ceh = USBXHCI_EPCTX_CEH_EPTYPE_n(USBXHCI_EPTYPE_CTLBIDIR);

    usbxhci_endpoint_data_t *epdata = add_endpoint(slotid, 0);

    inpepctx->tr_dq_ptr = epdata->xfer_ring_physaddr | epdata->ccs;

    usbxhci_cmd_trb_setaddr_t setaddr{};
    setaddr.input_ctx_physaddr = mphysaddr(inp.any);
    setaddr.trb_type = USBXHCI_CMD_TRB_TYPE_n(USBXHCI_TRB_TYPE_ADDRDEVCMD);

    setaddr.slotid = slotid;

    usb_blocking_iocp_t block;

    issue_cmd(&setaddr, &block);
    block.set_expect(1);

    block.wait();

    USBXHCI_TRACE("setaddr completed: completion code=%x, "
                  "parameter=%x, slotid=%d\n",
                  (unsigned)block.get_result().cc,
                  block.get_result().ccp, slotid);

    if (!dev_ctx_large) {
        munmap(inp.small, sizeof(*inp.small));
        inp.small = nullptr;
    } else {
        munmap(inp.large, sizeof(*inp.large));
        inp.large = nullptr;
    }

    if (unlikely(!block))
        return -int(block.get_result().cc);

    return 0;
}

bool usbxhci::get_pipe(int slotid, int epid, usb_pipe_t &pipe)
{
    if (slotid < 0)
        return false;

    if (likely(lookup_endpoint(slotid, epid))) {
        pipe = usb_pipe_t(this, slotid, epid);
        return true;
    }

    return false;
}

bool usbxhci::alloc_pipe(int slotid, int epid, usb_pipe_t &pipe,
                         int max_packet_sz, int interval, usb_ep_attr ep_type)
{
    if (slotid < 0)
        return false;

    if (likely(lookup_endpoint(slotid, epid))) {
        pipe = usb_pipe_t(this, slotid, epid);
        return true;
    }

    if (!assert(epid != 0))
        return false;

    usbxhci_endpoint_data_t *epd = add_endpoint(slotid, epid);
    if (!epd)
        return false;

    usbxhci_inpctx_t inp;
    usbxhci_inpctlctx_t *ctlctx;
    usbxhci_slotctx_t *inpslotctx;
    usbxhci_ep_ctx_t *inpepctx;

    bool in = epid & 0x80;
    int ep_index = epid & 0xF;

    fetch_inp_ctx(slotid, ep_index, inp, &ctlctx, &inpslotctx, &inpepctx);

    int bit_index;
    if (in)
        bit_index = USBXHCI_DB_VAL_IN_EP_UPD_n(ep_index);
    else
        bit_index = USBXHCI_DB_VAL_OUT_EP_UPD_n(ep_index);

    ctlctx->add_bits = (1 << 0) | (1U << bit_index);

    usbxhci_ep_ctx_t *ep = inpepctx + (bit_index - 1);

    ep->max_packet = max_packet_sz;
    ep->interval = interval;

    ep->tr_dq_ptr = (USBXHCI_EPCTX_TR_DQ_PTR_PTR_MASK &
            epd->xfer_ring_physaddr) |
            USBXHCI_EPCTX_TR_DQ_PTR_DCS_n(epd->ccs);

    uint8_t ep_type_value = uint8_t(ep_type);
    if (in)
        ep_type_value += 4;

    USBXHCI_EPCTX_CEH_EPTYPE_SET(ep->ceh, ep_type_value);

    usb_cc_t cc = commit_inp_ctx(slotid, epid, inp,
                                     USBXHCI_TRB_TYPE_CONFIGUREEPCMD);
    if (cc == usb_cc_t::success) {
        pipe = usb_pipe_t(this, slotid, epid);
        return true;
    }

    pipe = usb_pipe_t(nullptr, -1, -1);
    return false;
}

int usbxhci::send_control(int slotid, uint8_t request_type,
                          uint8_t request, uint16_t value,
                          uint16_t index, uint16_t length, void *data)
{
    usb_blocking_iocp_t block;

    send_control_async(slotid, request_type, request, value,
                       index, length, data, &block);
    block.set_expect(1);

    block.wait();

    return block.get_result().len_or_error();
}

int usbxhci::send_control_async(int slotid, uint8_t request_type,
                                uint8_t request, uint16_t value,
                                uint16_t index, uint16_t length,
                                void *data, usb_iocp_t *iocp)
{
    usbxhci_ctl_trb_t trbs[64/4+2] = {};

    int dir = (request_type & USBXHCI_CTL_TRB_BMREQT_TOHOST) != 0;

    int trb_count = make_setup_trbs(
                trbs, countof(trbs), data, length, dir,
                request_type, request, value, index);

    add_xfer_trbs(slotid, 0, 0, trb_count, dir, trbs, iocp);
    iocp->set_expect(1);

    return 0;
}

int usbxhci::xfer(int slotid, uint8_t epid, uint16_t stream_id,
                  uint32_t length, void *data, int dir)
{
    usb_blocking_iocp_t block;

    xfer_async(slotid, epid, stream_id, length, data, dir, &block);
    block.set_expect(1);

    block.wait();

    return block.get_result();
}

int usbxhci::xfer_async(int slotid, uint8_t epid, uint16_t stream_id,
                        uint32_t length, void *data, int dir, usb_iocp_t *iocp)
{
    // Worst case is 64KB
    usbxhci_ctl_trb_data_t trbs[64/4] = {};

    int data_trb_count = make_data_trbs(trbs, countof(trbs),
                                        data, length, dir, true);

    add_xfer_trbs(slotid, epid, stream_id, data_trb_count, dir, trbs, iocp);

    return 0;
}

usb_ep_state_t usbxhci::get_ep_state(int slotid, uint8_t epid)
{
    usbxhci_ep_ctx_t *epctx = dev_ctx_ent_ep(slotid, epid & 0xF);

    return usb_ep_state_t(USBXHCI_EPCTX_EP_STATE_STATE_GET(epctx->ep_state));
}

bool usbxhci::configure_hub_port(int slotid, int port)
{
    usbxhci_slotctx_t *slotctx = this->dev_ctx_ent_slot(slotid);

    int route = USBXHCI_SLOTCTX_RSMHC_ROUTE_GET(slotctx->rsmhc);

    // Find the bit shift to place the new port number at the end of the route
    int route_bit = route ? (bit_msb_set_32(route) + 4) & -4 : 0;

    // Add the port to the route
    route |= port << route_bit;

    add_device(slotctx->root_hub_port_num, route);

    return true;
}

bool usbxhci::set_hub_port_count(int slotid, usb_hub_desc const& hub_desc)
{
    usbxhci_inpctx_t inp;
    usbxhci_inpctlctx_t *ctlctx;
    usbxhci_slotctx_t *inpslotctx;
    usbxhci_ep_ctx_t *inpepctx;

    usb_cc_t cc = fetch_inp_ctx(slotid, 0, inp,
                                &ctlctx, &inpslotctx, &inpepctx);

    if (cc != usb_cc_t::success)
        return false;

    // Update hub flag
    USBXHCI_SLOTCTX_RSMHC_HUB_SET(inpslotctx->rsmhc, 1);
    inpslotctx->num_ports = hub_desc.num_ports;

    // 4.3.3 Device slot initialization to initialize the
    // slot context and endpoint 0 context
    // Set A0 and A1
    ctlctx->add_bits = (1 << 0) | (1 << 1);

    return commit_inp_ctx(slotid, 0, inp, USBXHCI_TRB_TYPE_EVALCTXCMD) ==
            usb_cc_t::success;
}

int usbxhci::reset_ep(int slotid, uint8_t epid)
{
    usb_blocking_iocp_t block;

    reset_ep_async(slotid, epid, &block);
    block.set_expect(1);

    block.wait();

    return block.get_result().len_or_error();
}

int usbxhci::reset_ep_async(int slotid, uint8_t epid, usb_iocp_t *iocp)
{
    usbxhci_cmd_trb_reset_ep_t cmd{};

    USBXHCI_CTL_TRB_RESETEP_TRBTYPE_TSP_TRB_TYPE_SET(
                cmd.trb_type_tsp, USBXHCI_TRB_TYPE_RESETEPCMD);

    cmd.epid = !epid || epid > 0x80
            ? USBXHCI_DB_VAL_IN_EP_UPD_n(epid & 0xF)
            : USBXHCI_DB_VAL_OUT_EP_UPD_n(epid & 0xF);
    cmd.slotid = slotid;

    issue_cmd(&cmd, iocp);

    return 0;
}

usb_cc_t usbxhci::fetch_inp_ctx(
        int slotid, int epid, usbxhci_inpctx_t &inp,
        usbxhci_inpctlctx_t **p_ctlctx,
        usbxhci_slotctx_t **p_inpslotctx,
        usbxhci_ep_ctx_t **p_inpepctx)
{
    usbxhci_slotctx_t const *ctx = dev_ctx_ent_slot(slotid);
    usbxhci_ep_ctx_t const *ep = dev_ctx_ent_ep(slotid, 0);

    usbxhci_inpctlctx_t *ctlctx = nullptr;
    usbxhci_slotctx_t *inpslotctx = nullptr;
    usbxhci_ep_ctx_t *inpepctx = nullptr;

    if (!dev_ctx_large) {
        inp.any = mmap(0, sizeof(usbxhci_inpctx_small_t),
                   PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
        ctlctx = &inp.small->inpctl;
        inpslotctx = &inp.small->slotctx;
        inpepctx = inp.small->epctx;
    } else {
        inp.any = mmap(0, sizeof(usbxhci_inpctx_large_t),
                   PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
        ctlctx = &inp.large->inpctl;
        inpslotctx = &inp.large->slotctx;
        inpepctx = inp.large->epctx;
    }

    if (inp.any == MAP_FAILED)
        return usb_cc_t::resource_err;

    *inpslotctx = *ctx;
    memcpy(inpepctx, ep, sizeof(*inpepctx) * 32);

    if (p_ctlctx)
        *p_ctlctx = ctlctx;

    if (p_inpslotctx)
        *p_inpslotctx = inpslotctx;

    if (p_inpepctx)
        *p_inpepctx = inpepctx;

    return (usb_cc_t::success);
}

usb_cc_t usbxhci::commit_inp_ctx(int slotid, int epid,
                                     usbxhci_inpctx_t &inp,
                                     uint32_t trb_type)
{
    usbxhci_endpoint_data_t *epd = lookup_endpoint(slotid, epid);

    // Issue evaluate context command
    usbxhci_ctl_trb_evalctx_t *eval = (usbxhci_ctl_trb_evalctx_t *)
            calloc(1, sizeof(*eval));
    eval->trt = USBXHCI_CTL_TRB_TRT_SLOTID_n(slotid);
    eval->input_ctx_ptr = mphysaddr(inp.any);
    eval->flags = USBXHCI_CTL_TRB_FLAGS_TRB_TYPE_n(trb_type) |
            USBXHCI_CTL_TRB_FLAGS_C_n(epd->ccs);

    usb_blocking_iocp_t block;

    issue_cmd(eval, &block);
    block.set_expect(1);

    if (!dev_ctx_large)
        munmap(inp.any, sizeof(usbxhci_inpctx_small_t));
    else
        munmap(inp.any, sizeof(usbxhci_inpctx_large_t));

    block.wait();

    return block.get_result().cc;
}

int usbxhci::update_slot_ctx(uint8_t slotid, usb_desc_device *dev_desc)
{
    USBXHCI_TRACE("Device descriptor: class=%u, sub_class=%u, protocol=%u\n",
                  dev_desc->dev_class, dev_desc->dev_subclass,
                  dev_desc->dev_protocol);

    usbxhci_inpctx_t inp;
    usbxhci_inpctlctx_t *ctlctx;
    usbxhci_slotctx_t *inpslotctx;
    usbxhci_ep_ctx_t *inpepctx;

    fetch_inp_ctx(slotid, 0, inp, &ctlctx, &inpslotctx, &inpepctx);

    // Update max packet size
    if (dev_desc->usb_spec >= 0x300) {
        assert(dev_desc->maxpktsz < 16);
        inpepctx->max_packet = 1U << dev_desc->maxpktsz;
    } else {
        inpepctx->max_packet = dev_desc->maxpktsz;
    }

    // 4.3.3 Device slot initialization to initialize the
    // slot context and endpoint 0 context
    // Set A0 and A1
    ctlctx->add_bits = (1 << 0) | (1 << 1);

    commit_inp_ctx(slotid, 0, inp, USBXHCI_TRB_TYPE_EVALCTXCMD);

    return 0;
}

void usbxhci::evt_handler(usbxhci_interrupter_info_t *ir_info,
                          usbxhci_intr_t *ir, usbxhci_evt_t *evt, size_t ii)
{
    (void)ir_info;
    (void)ir;
    (void)ii;

    uint16_t type = ((evt->flags & USBXHCI_EVT_FLAGS_TYPE) >>
                     USBXHCI_EVT_FLAGS_TYPE_BIT);

    usbxhci_evt_cmdcomp_t *cmdcomp;
    usbxhci_evt_xfer_t *xfer;
    uint64_t cmdaddr = 0;

    switch (type) {
    case USBXHCI_TRB_TYPE_XFEREVT:
        xfer = (usbxhci_evt_xfer_t*)evt;
        cmdaddr = xfer->trb_ptr;
        USBXHCI_TRACE("XFEREVT (CC=%d) (TRB=0x%lx)\n",
                      xfer->cc, xfer->trb_ptr);
        break;

    case USBXHCI_TRB_TYPE_CMDCOMPEVT:
        cmdcomp = (usbxhci_evt_cmdcomp_t*)evt;
        cmdaddr = cmdcomp->command_trb_ptr;
        USBXHCI_TRACE("CMDCOMPEVT (TRB=0x%lx)\n", cmdaddr);
        break;

    case USBXHCI_TRB_TYPE_PORTSTSCHGEVT:
        USBXHCI_TRACE("PORTSTSCHGEVT\n");
        break;

    case USBXHCI_TRB_TYPE_BWREQEVT:
        USBXHCI_TRACE("BWREQEVT\n");
        break;

    case USBXHCI_TRB_TYPE_DBEVT:
        USBXHCI_TRACE("DBEVT\n");
        break;

    case USBXHCI_TRB_TYPE_HOSTCTLEVT:
        USBXHCI_TRACE("HOSTCTLEVT\n");
        break;

    case USBXHCI_TRB_TYPE_DEVNOTIFEVT:
        USBXHCI_TRACE("DEVNOTIFEVT\n");
        break;

    case USBXHCI_TRB_TYPE_MFINDEXWRAPEVT:
        USBXHCI_TRACE("MFINDEXWRAPEVT\n");
        break;

    default:
        USBXHCI_TRACE("Unknown event! \n");
        break;

    }

    if (!cmdaddr)
        return;

    // Lookup pending command
    scoped_lock hold_cmd_lock(lock_cmd);
    refptr<usbxhci_pending_cmd_t> pcp = usbxhci_pending_ht.lookup(&cmdaddr);
    if (pcp)
        usbxhci_pending_ht.del(&cmdaddr);
    hold_cmd_lock.unlock();

    // Invoke completion handler
    cmd_comp(evt, pcp->iocp);
}

isr_context_t *usbxhci::irq_handler(int irq, isr_context_t *ctx)
{
    for (usbxhci* dev : usbxhci_devices)
        dev->irq_handler(irq);

    return ctx;
}

void usbxhci::irq_handler(int irq)
{
    int irq_ofs = irq - irq_range.base;

    // Skip this device if it is not in the irq range
    if (irq_ofs < 0 && irq_ofs >= irq_range.count)
        return;

    // Skip if interrupt is not pending
    if (!USBXHCI_USBSTS_EINT_GET(mmio_op->usbsts))
        return;

    // Acknowledge the IRQ
    mmio_op->usbsts = USBXHCI_USBSTS_EINT;

    // The IRQ is the interrupter index % the number of IRQs
    for (size_t ii = irq_ofs; ii < maxintr; ii += irq_range.count) {
        usbxhci_interrupter_info_t *ir_info = interrupters + ii;
        usbxhci_intr_t *ir = (usbxhci_intr_t*)(mmio_rt->ir + ii);

        if (USBXHCI_INTR_IMAN_IP_GET(ir->iman)) {
            // Interrupt is pending
            USBXHCI_TRACE("Interrupt pending on interrupter %zu\n", ii);

            // Acknowledge the interrupt
            ir->iman |= USBXHCI_INTR_IMAN_IP;

            while (!ir_info->ccs ==
                   !(ir_info->evt_ring[ir_info->next].flags &
                     USBXHCI_EVT_FLAGS_C)) {
                usbxhci_evt_t *evt = (usbxhci_evt_t*)(ir_info->evt_ring +
                        ir_info->next++);

                if (ir_info->next >= ir_info->count) {
                    ir_info->next = 0;
                    ir_info->ccs = !ir_info->ccs;
                }

                evt_handler(ir_info, ir, evt, ii);
            }

            // Notify HC that we have consumed some events
            ir->erdp = (ir_info->evt_ring_physaddr +
                    ir_info->next * sizeof(*ir_info->evt_ring)) |
                    (1<<3);
        }

        // Run completions after consuming events
        for (usb_iocp_t *iocp : completed_iocp)
            iocp->invoke();
        completed_iocp.clear();
    }
}

usbxhci::usbxhci()
{
}

void usbxhci::detect()
{
    pci_dev_iterator_t pci_iter;

    if (!pci_enumerate_begin(&pci_iter, PCI_DEV_CLASS_SERIAL,
                             PCI_SUBCLASS_SERIAL_USB))
        return;

    do {
        if (pci_iter.config.prog_if != PCI_PROGIF_SERIAL_USB_XHCI)
            continue;

        usbxhci *self = new (calloc(1, sizeof(usbxhci))) usbxhci();

        if (!usbxhci_devices.push_back(self)) {
            USBXHCI_TRACE("Out of memory!");
            break;
        }

        self->init(pci_iter);
    } while (pci_enumerate_next(&pci_iter));
}

int usbxhci::make_setup_trbs(
        usbxhci_ctl_trb_t *trbs, size_t trb_capacity,
        void *data, uint16_t length, int dir,
        uint8_t bmreq_type, uint8_t bmreq_recip, bool to_host,
        usb_rqcode_t request, uint16_t value, uint16_t index)
{
    assert(bmreq_type < 4);
    assert(bmreq_recip < 32);
    assert(data || length == 0);

    return make_setup_trbs(trbs, trb_capacity, data, length, dir,
                           USBXHCI_CTL_TRB_BMREQT_RECIP_n(bmreq_recip) |
                           USBXHCI_CTL_TRB_BMREQT_TYPE_n(bmreq_type) |
                           (to_host ? USBXHCI_CTL_TRB_BMREQT_TOHOST : 0),
                           uint8_t(request), value, index);
}

int usbxhci::make_data_trbs(
        usbxhci_ctl_trb_data_t *trbs, size_t trb_capacity,
        void *data, uint32_t length, int dir, bool intr)
{
    // Worst case is 64KB
    mmphysrange_t ranges[64/4];
    size_t range_count = mphysranges(ranges, countof(ranges),
                                     data, length, 64 << 10);

    // TRBs must not cross 64KB boundaries
    if (unlikely(!mphysranges_split(ranges, range_count, countof(ranges), 16)))
        return -1;

    // Caller must provide sufficiently large TRB array
    if (unlikely(trb_capacity < range_count + 2))
        return -1;

    usbxhci_ctl_trb_data_t *trb = trbs;

    for (size_t i = 0; i < range_count; ++i) {
        trb->data_physaddr = ranges[i].physaddr;
        trb->xfer_td_intr =
                USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_n(ranges[i].size) |
                USBXHCI_CTL_TRB_XFERLEN_INTR_TDSZ_n(
                    min(size_t(USBXHCI_CTL_TRB_XFERLEN_INTR_TDSZ_MASK),
                        range_count - i - 1));
        trb->flags = USBXHCI_CTL_TRB_FLAGS_TRB_TYPE_n(USBXHCI_TRB_TYPE_DATA) |
                USBXHCI_CTL_TRB_FLAGS_CH_n(i + 1 < range_count) |
                USBXHCI_CTL_TRB_FLAGS_IOC_n(intr && i + 1 >= range_count) |
                USBXHCI_CTL_TRB_FLAGS_ISP_n(intr);
        trb->dir = dir;
        ++trb;
    }

    return trb - trbs;
}

int usbxhci::make_setup_trbs(
        usbxhci_ctl_trb_t *trbs, int trb_capacity,
        void *data, uint16_t length, int dir,
        uint8_t request_type, uint8_t request,
        uint16_t value, uint16_t index)
{
    if (unlikely(trb_capacity < 2))
        return false;

    int data_trb_count = make_data_trbs(&trbs[1].data, trb_capacity - 1,
                                        data, length, dir, false);

    if (unlikely(trb_capacity < data_trb_count + 2))
        return false;

    usbxhci_ctl_trb_t *trb = trbs;

    trb->setup.bm_req_type = request_type;
    trb->setup.request = usb_rqcode_t(request);
    trb->setup.value = value;
    trb->setup.index = index;
    trb->setup.length = length;
    trb->setup.xferlen_intr = 8;
    trb->setup.flags =
            USBXHCI_CTL_TRB_FLAGS_TRB_TYPE_n(USBXHCI_TRB_TYPE_SETUP) |
            USBXHCI_CTL_TRB_FLAGS_IDT;
    trb->setup.trt = length == 0
            ? USBXHCI_CTL_TRB_TRT_NODATA
            : dir
              ? USBXHCI_CTL_TRB_TRT_IN
              : USBXHCI_CTL_TRB_TRT_OUT;
    ++trb;

    trb += data_trb_count;

    trb->status.flags = USBXHCI_CTL_TRB_FLAGS_TRB_TYPE_n(
                USBXHCI_TRB_TYPE_STATUS) | USBXHCI_CTL_TRB_FLAGS_IOC;
    trb->status.dir = !dir;
    ++trb;

    return trb - trbs;
}

void usbxhci_detect(void *)
{
    usbxhci::detect();
}

REGISTER_CALLOUT(usbxhci_detect, 0, callout_type_t::usb, "000");
