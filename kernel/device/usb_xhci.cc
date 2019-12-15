// pci driver: C=SERIAL, S=USB, I=XHCI

#include "kmodule.h"
#include "pci.h"

PCI_DRIVER_BY_CLASS(
        usbxhci,
        PCI_DEV_CLASS_SERIAL, PCI_SUBCLASS_SERIAL_USB,
        PCI_PROGIF_SERIAL_USB_XHCI);

#include "callout.h"
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
#include "time.h"
#include "inttypes.h"
#include "work_queue.h"

#include "usb_xhci.bits.h"

#define USBXHCI_DEBUG   1
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
} _packed;

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
};

C_ASSERT(sizeof(usbxhci_intr_t) == 0x20);

// 5.5 Host Controller Runtime Registers

struct usbxhci_rtreg_t  {
    // 5.5.1 Microframe Index Register
    uint32_t mfindex;

    uint8_t rsvdz[0x20-0x04];

    // 5.5.2 Interrupter Register Set
    usbxhci_intr_t ir[1023];
} _packed;

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
} _packed;

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
} _packed;

C_ASSERT(offsetof(usbxhci_opreg_t, ports) == 0x400);
C_ASSERT(sizeof(usbxhci_opreg_t) == 0x1400);

// 6.2.2 Slot Context

struct usbxhci_slotctx_t {
    // Route string, speed, Multi-TT, hub, last endpoint index
    uint32_t rsmhc;

    // Worst case wakeup latency in microseconds
    uint16_t max_exit_lat;

    // Root hub port number
    uint8_t root_hub_port_num;

    // Number of ports on the hub (0 if not a hub)
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
} _packed;

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

    // LS/FS: delay between consecutive requests. interval * 1ms units
    // SS: delay between consecutive requests. (2**interval) * 125us units
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
} _packed;

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
} _packed;

struct usbxhci_devctx_large_t {
    usbxhci_slotctx_t slotctx;
    uint8_t rsvd[32];

    usbxhci_ep_ctx_t epctx[16];
} _packed;

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
} _packed;

// 6.2.5 Input Context

struct usbxhci_inpctx_small_t {
    usbxhci_inpctlctx_t inpctl;

    usbxhci_slotctx_t slotctx;

    usbxhci_ep_ctx_t epctx[32];
} _packed;

struct usbxhci_inpctx_large_t {
    usbxhci_inpctlctx_t inpctl;

    usbxhci_slotctx_t slotctx;
    uint8_t rsvd[32];

    usbxhci_ep_ctx_t epctx[32];
} _packed;

union usbxhci_inpctx_t {
    void *any;
    usbxhci_inpctx_small_t *small;
    usbxhci_inpctx_large_t *large;
};

// 6.4.3 Command TRB

struct usbxhci_cmd_trb_t {
    uint32_t data[4];
} _packed;

struct usbxhci_cmd_trb_noop_t {
    uint32_t rsvd1[3];
    uint8_t cycle;
    uint8_t trb_type;
    uint16_t rsvd2;
} _packed;

struct usbxhci_cmd_trb_reset_ep_t {
    uint32_t rsvd1[3];
    uint8_t cycle;
    uint8_t trb_type_tsp;
    uint8_t epid;
    uint8_t slotid;
} _packed;

struct usbxhci_cmd_trb_setaddr_t {
    uint64_t input_ctx_physaddr;
    uint32_t rsvd;
    uint8_t cycle;
    uint8_t trb_type;
    uint8_t rsvd2;
    uint8_t slotid;
} _packed;

C_ASSERT(sizeof(usbxhci_cmd_trb_setaddr_t) == 16);

// 6.4.4.1 Link TRB
struct usbxhci_cmd_trb_link_t {
    uint64_t ring_physaddr;
    uint16_t rsvd;
    uint16_t intrtarget;
    uint8_t c_tc_ch_ioc;
    uint8_t trb_type;
    uint16_t rsvd2;
} _packed;

C_ASSERT(sizeof(usbxhci_cmd_trb_link_t) == 16);

// 6.5 Event Ring Segment Table

struct usbxhci_evtring_seg_t {
    // Base address, must be 64-byte aligned
    uint64_t base;

    // Minimum count=16, maximum count=4096
    uint16_t trb_count;

    uint16_t resvd;
    uint32_t resvd2;
} _packed;

C_ASSERT(sizeof(usbxhci_evtring_seg_t) == 0x10);

//
//

// 6.4.2 Event TRBs

struct usbxhci_evt_t {
    uint32_t data[3];
    uint16_t flags;
    uint8_t id;
    uint8_t slotid;
} _packed;

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
} _packed;

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
} _packed;

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
} _packed;

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
} _packed;

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
} _packed;

//
// 6.4.2.5 Doorbell Event TRB

struct usbxhci_evt_db_t {
    uint8_t reason;
    uint8_t rsvd[10];
    uint8_t cc;
    uint16_t flags;
    uint8_t vf_id;
    uint8_t slotid;
} _packed;

//
// 6.4.1.2 Control TRBs

struct usbxhci_ctl_trb_generic_t {
    uint32_t data[3];
    uint16_t flags;
    uint16_t trt;
} _packed;

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
} _packed;

C_ASSERT(sizeof(usbxhci_ctl_trb_setup_t) == 0x10);
C_ASSERT(offsetof(usbxhci_ctl_trb_setup_t, flags) == 0x0c);

struct usbxhci_ctl_trb_data_t {
    uint64_t data_physaddr;
    uint32_t xfer_td_intr;
    uint16_t flags;
    uint16_t dir;
} _packed;

C_ASSERT(sizeof(usbxhci_ctl_trb_data_t) == 0x10);
C_ASSERT(offsetof(usbxhci_ctl_trb_data_t, flags) == 0x0c);

struct usbxhci_ctl_trb_status_t {
    uint64_t rsvd;
    uint16_t rsvd2;
    uint16_t intr;
    uint16_t flags;
    uint16_t dir;
} _packed;

C_ASSERT(sizeof(usbxhci_ctl_trb_status_t) == 0x10);
C_ASSERT(offsetof(usbxhci_ctl_trb_status_t, flags) == 0x0c);

union usbxhci_ctl_trb_t {
    usbxhci_ctl_trb_generic_t generic;
    usbxhci_ctl_trb_setup_t setup;
    usbxhci_ctl_trb_data_t data;
    usbxhci_ctl_trb_status_t status;
} _packed;

C_ASSERT(sizeof(usbxhci_ctl_trb_t) == 0x10);

//
// 6.4.3.6 Evaluate Context command

struct usbxhci_ctl_trb_evalctx_t {
    uint64_t input_ctx_ptr;
    uint32_t rsvd[1];
    uint16_t flags;
    uint16_t trt;
} _packed;

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

class usbxhci;
class usbxhci_pending_cmd_t;

template<typename T>
struct usbxhci_ring_data_t {
    usbxhci_pending_cmd_t *pending;
    T volatile *ptr;
    uint64_t physaddr;
    uint32_t next;
    uint32_t count;
    uint8_t cycle;

    void insert(usbxhci *ctrl, usbxhci_cmd_trb_t *src, usb_iocp_t *iocp,
                bool ins_pending);

    bool alloc(uint32_t trb_count);

    void reserve_link();
};

using usbxhci_trb_ring_data_t = usbxhci_ring_data_t<usbxhci_cmd_trb_t>;
using usbxhci_interrupter_info_t = usbxhci_ring_data_t<usbxhci_evt_t>;

struct usbxhci_portinfo_t {
    usbxhci_evtring_seg_t volatile *dev_evt_segs;
    usbxhci_evt_t volatile *dev_evt_ring;
};

struct usbxhci_endpoint_target_t {
    uint8_t slotid;
    uint8_t epid;
};

struct usbxhci_endpoint_data_t {
    usbxhci_endpoint_target_t target;

    usbxhci_trb_ring_data_t ring;
};

struct usbxhci_pending_cmd_t {
    uint64_t cmd_physaddr;
    usb_iocp_t *iocp;

    size_t hash() const;
};

struct usbxhci_slot_data_t {
    usbxhci_slot_data_t();

    int parent_slot;
    uint8_t port;

    // Flags
    bool is_hub:1;
    bool is_multi_tt:1;
};

class usbxhci final : public usb_bus_t {
public:
    usbxhci();
    usbxhci(usbxhci const&) = delete;
    usbxhci(usbxhci&&) = delete;
    usbxhci& operator=(usbxhci) = delete;

    static void detect();

    void dump_config_desc(usb_config_helper const& cfg_hlp);

    bool add_device(int parent_slot, int port, int route);

    template<typename T>
    friend class usbxhci_ring_data_t;

    void *operator new(size_t sz, std::nothrow_t const&) noexcept;
    void operator delete(void *p) noexcept;

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

    bool alloc_pipe(int slotid, usb_pipe_t &pipe,
                    int epid, int cfg_value,
                    int iface_num, int alt_iface,
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
    using lock_type = ext::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;

    errno_t cc_to_errno(usb_cc_t cc);

    usbxhci_slotctx_t *dev_ctx_ent_slot(size_t slotid);

    usbxhci_ep_ctx_t *dev_ctx_ent_ep(size_t slot, size_t i);

    void ring_doorbell(uint32_t doorbell, uint8_t value,
                       uint16_t stream_id);

    void issue_cmd(usbxhci_cmd_trb_t *cmd, usb_iocp_t *iocp);

    void add_xfer_trbs(uint8_t slotid, uint8_t epid,
                       uint16_t stream_id, size_t count, int dir,
                       void *trbs, usb_iocp_t *iocp);

    void insert_pending_command(usbxhci_pending_cmd_t *pc,
                                uint64_t cmd_physaddr, usb_iocp_t *iocp);

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

    bool remove_endpoint(uint8_t slotid, uint8_t epid);

    void evt_handler(usbxhci_interrupter_info_t *ir_info,
                     usbxhci_intr_t *ir, usbxhci_evt_t *evt, size_t ii);

    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    void irq_handler(int irq_ofs);

    void walk_caps(char const volatile *caps);

    bool init(const pci_dev_iterator_t &pci_iter, size_t busid);

    //

    uint64_t mmio_addr;

    char volatile *mmio_base;
    usbxhci_capreg_t volatile *mmio_cap;
    usbxhci_opreg_t volatile *mmio_op;
    usbxhci_rtreg_t volatile *mmio_rt;
    usbxhci_dbreg_t volatile *mmio_db;

    uint64_t volatile *dev_ctx_ptrs;
    usbxhci_devctx_t dev_ctx;

    usbxhci_trb_ring_data_t cmd_ring;

    usbxhci_evtring_seg_t volatile *dev_evt_segs;

    usbxhci_interrupter_info_t *interrupters;

    size_t busid;

    lock_type endpoints_lock;
    using endpoint_collection_t = std::vector<
        std::unique_ptr<usbxhci_endpoint_data_t>>;
    endpoint_collection_t endpoints;

    // Endpoint data keyed on usbxhci_endpoint_target_t
    hashtbl_t<usbxhci_endpoint_data_t, usbxhci_endpoint_target_t,
    &usbxhci_endpoint_data_t::target> endpoint_lookup;

    // Maximums
    uint32_t maxslots;
    uint32_t maxintr;
    int maxports;

    usbxhci_portinfo_t *ports;
    unsigned port_count;

    bool use_msi;

    // 0 for 32 byte usbxhci_devctx_t, 1 for 64 byte usbxhci_devctx_large_t
    uint32_t dev_ctx_large;

    pci_irq_range_t irq_range;

    hashtbl_t<usbxhci_pending_cmd_t,
    uint64_t, &usbxhci_pending_cmd_t::cmd_physaddr> usbxhci_pending_ht;

    std::vector<void*> scratch_buffers;

    std::vector<usb_iocp_t*> completed_iocp;

    std::vector<usbxhci_slot_data_t> slot_data;

    // Command issue lock
    lock_type lock_cmd;
};

static std::vector<std::unique_ptr<usbxhci>> usbxhci_devices;

// Handle 32 or 64 byte device context size
usbxhci_slotctx_t *usbxhci::dev_ctx_ent_slot(size_t slotid)
{
    if (dev_ctx_large)
        return &dev_ctx.large[slotid].slotctx;
    return &dev_ctx.small[slotid].slotctx;
}

// Handle 32 or 64 byte device context size
_used
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

void usbxhci::insert_pending_command(usbxhci_pending_cmd_t *pc,
                                     uint64_t cmd_physaddr, usb_iocp_t *iocp)
{
    pc->cmd_physaddr = cmd_physaddr;
    pc->iocp = iocp;
    usbxhci_pending_ht.insert(pc);
}

void usbxhci::issue_cmd(usbxhci_cmd_trb_t *cmd, usb_iocp_t *iocp)
{
    scoped_lock hold_cmd_lock(lock_cmd);

    USBXHCI_TRACE("Writing command to command ring at %u\n", cmd_ring.next);

    cmd_ring.insert(this, cmd, iocp, true);

    // Ring controller command doorbell
    ring_doorbell(0, 0, 0);
}

void usbxhci::add_xfer_trbs(uint8_t slotid, uint8_t epid, uint16_t stream_id,
                            size_t count, int dir, void *trbs,
                            usb_iocp_t *iocp)
{
    scoped_lock hold_cmd_lock(lock_cmd);

    usbxhci_endpoint_data_t *epd = lookup_endpoint(slotid, epid);

    for (size_t i = 0; i < count; ++i) {
        // Get pointer to source TRB
        auto src = (usbxhci_cmd_trb_t *)trbs + i;

        epd->ring.insert(this, src, iocp, iocp && (i + 1) == count);
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

    USBXHCI_TRACE("getting descriptor, slot=%u, epid=%u\n", slotid, epid);

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

    USBXHCI_TRACE("got descriptor, slot=%u, epid=%u\n", slotid, epid);

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
    usbxhci_endpoint_data_t *newepd =
            new (std::nothrow) usbxhci_endpoint_data_t;
    if (!newepd)
        return nullptr;

    newepd->target.slotid = slotid;
    newepd->target.epid = epid;

    scoped_lock hold_endpoints_lock(endpoints_lock);

    if (!endpoints.emplace_back(newepd))
        return nullptr;

    if (unlikely(!newepd->ring.alloc(PAGESIZE / sizeof(*newepd->ring.ptr)))) {
        endpoints.pop_back();
        return nullptr;
    }
    newepd->ring.reserve_link();

    if (!endpoint_lookup.insert(newepd))
        return nullptr;

    return newepd;
}

usbxhci_endpoint_data_t *usbxhci::lookup_endpoint(uint8_t slotid, uint8_t epid)
{
    scoped_lock hold_endpoints_lock(endpoints_lock);

    usbxhci_endpoint_target_t key{ slotid, epid };
    usbxhci_endpoint_data_t *data = endpoint_lookup.lookup(&key);

    return data;
}

bool usbxhci::remove_endpoint(uint8_t slotid, uint8_t epid)
{
    scoped_lock hold_endpoints_lock(endpoints_lock);

    for (auto it = endpoints.begin(), en = endpoints.end(); it != en; ++it) {
        auto& endpoint = *it;
        if (endpoint->target.slotid == slotid &&
                endpoint->target.epid == epid) {
            endpoints.erase(it);
            usbxhci_endpoint_target_t key{ slotid, epid };
            bool found = endpoint_lookup.del(&key);
            assert(found);
            return found;
        }
    }

    return false;
}

void usbxhci::dump_config_desc(usb_config_helper const& cfg_hlp)
{
    USBXHCI_TRACE("Dumping configuration descriptors\n");

    for (uint8_t const *base = (uint8_t const *)(cfg_hlp.find_config(0)),
         *raw = base;
         *raw && *raw != 255; raw += *raw)
        hex_dump(raw, *raw, raw - base);

    usb_desc_config const *cfg;
    usb_desc_iface const *iface;
    usb_desc_ep const *ep;

    for (int cfg_idx = 0;
         (cfg = cfg_hlp.find_config(cfg_idx)) != nullptr;
         ++cfg_idx) {

        USBXHCI_TRACE("cfg #%#x max_power=%umA {\n",
                      cfg_idx, cfg->max_power * 2);

        for (int iface_idx = 0;
             (iface = cfg_hlp.find_iface(cfg, iface_idx)) != nullptr;
             ++iface_idx) {
            USBXHCI_TRACE("  iface #%#x: class=%#x (%s)"
                          " subclass=%#x, proto=%#x {\n",
                          iface->iface_num, iface->iface_class,
                          cfg_hlp.class_code_text(iface->iface_class),
                          iface->iface_subclass, iface->iface_proto);

            for (int ep_idx = 0;
                 (ep = cfg_hlp.find_ep(iface, ep_idx)) != nullptr;
                 ++ep_idx) {
                USBXHCI_TRACE("    ep %#x: dir=%s attr=%s,"
                              " maxpktsz=%#x, interval=%u\n",
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

bool usbxhci::add_device(int parent_slot, int port, int route)
{
    USBXHCI_TRACE("Adding device, port=%d, route=%#x\n", port, route);

    int slotid = enable_slot(port);

    // Remember the parent hub slot
    slot_data.at(parent_slot).parent_slot = slotid;

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

    err = get_descriptor(slotid, 0, &dev_desc, sizeof(dev_desc),
                         usb_req_type::STD, usb_req_recip_t::DEVICE,
                         usb_desctype_t::DEVICE, 0);
    if (err < 0)
        return false;

    // Get first 8 bytes of device descriptor to get max packet size
    union {
        usb_desc_config cfg_buf[1];
        char filler[128];
    };

    err = get_descriptor(slotid, 0, cfg_buf, 128, usb_req_type::STD,
                         usb_req_recip_t::DEVICE,
                         usb_desctype_t::CONFIGURATION, 0);
    if (err < 0)
        return false;

    // 9.2.6.6 - Speed Dependent Descriptors
    // Devices with a value of at least 0210H in the bcdUSB field of
    // their device descriptor shall support GetDescriptor (BOS Descriptor)
    // requests
    usb_desc_bos bos_hdr{};
    std::unique_ptr<usb_desc_bos> bos;
    do {
        if (dev_desc.usb_spec >= 0x210) {
            err = get_descriptor(slotid, 0, &bos_hdr, sizeof(bos_hdr),
                                 usb_req_type::STD, usb_req_recip_t::DEVICE,
                                 usb_desctype_t::BOS, 0);

            if (err < 0)
                break;

            void *bos_mem = malloc(bos_hdr.total_len);

            if (unlikely(!bos_mem))
                break;

            bos.reset(new (bos_mem) usb_desc_bos{});

            err = get_descriptor(slotid, 0, bos.get(), bos_hdr.total_len,
                                 usb_req_type::STD, usb_req_recip_t::DEVICE,
                                 usb_desctype_t::BOS, 0);
        }
    } while (false);

    usb_config_helper cfg_hlp(slotid, dev_desc, bos, cfg_buf, 128);

    USBXHCI_TRACE("Searching for driver for %#04x:%#04x\n",
                  cfg_hlp.device().vendor_id, cfg_hlp.device().product_id);

    dump_config_desc(cfg_hlp);

    usb_class_drv_t * drv = usb_class_drv_t::find_driver(&cfg_hlp, this);
    (void)drv;

    USBXHCI_TRACE("Driver: %s\n", drv ? drv->name() : "<not found>");

    return true;
}

void *usbxhci::operator new(size_t sz, std::nothrow_t const&) noexcept
{
    return calloc(1, sz);
}

void usbxhci::operator delete(void *p) noexcept
{
    free(p);
}

// 7 xHCI extended capabilities

#define USBXHCI_EXTCAPID_LEGACY 1
#define USBXHCI_EXTCAPID_PROTO  2
#define USBXHCI_EXTCAPID_EXTPM  3
#define USBXHCI_EXTCAPID_IOV    4
#define USBXHCI_EXTCAPID_MSI    5
#define USBXHCI_EXTCAPID_LMEM   6
#define USBXHCI_EXTCAPID_DEBUG  10
#define USBXHCI_EXTCAPID_XMI    17

void usbxhci::walk_caps(char const volatile *caps)
{
    for (uint32_t const *cap = (uint32_t const*)caps;
         USBXHCI_EXTCAP_CAPID_GET(*cap);
         cap += USBXHCI_EXTCAP_NEXT_GET(*cap)) {
        unsigned capid = USBXHCI_EXTCAP_CAPID_GET(*cap);

        switch (capid) {
        case USBXHCI_EXTCAPID_LEGACY:
            // Perform BIOS->OS handoff
            USBXHCI_TRACE("Performing legacy handoff\n");

            uint8_t volatile *bios_owned;
            uint8_t volatile *os_owned;
            bios_owned = (uint8_t volatile *)caps + 2;
            os_owned = (uint8_t volatile *)caps + 3;

            atomic_st_rel(os_owned, atomic_ld_acq(os_owned) | 1);

            uint64_t now;
            uint64_t timeout_ns;
            now = time_ns();
            timeout_ns = now + 1000000000;

            while (atomic_ld_acq(bios_owned) & 1) {
                now = time_ns();
                if (now < timeout_ns) {
                    pause();
                } else {
                    USBXHCI_TRACE("Legacy handoff timed out!\n");
                }
            }

            USBXHCI_TRACE("Legacy handoff completed in %" PRId64 "ms\n",
                          (1000000000 - (timeout_ns - now)) / 1000000);

            break;

        case USBXHCI_EXTCAPID_PROTO:
            USBXHCI_TRACE("USB %#2x.%#02x \"%4.4s\"\n",
                          (*cap >> 24) & 0xFF,
                          (*cap >> 16) & 0xFF,
                          (char const*)(cap+1));
            break;

        case USBXHCI_EXTCAPID_EXTPM:
            break;

        case USBXHCI_EXTCAPID_IOV:
            break;

        case USBXHCI_EXTCAPID_MSI:
            break;

        case USBXHCI_EXTCAPID_LMEM:
            break;

        case USBXHCI_EXTCAPID_DEBUG:
            break;

        case USBXHCI_EXTCAPID_XMI:
            break;

        default:
            break;

        }

        if (!USBXHCI_EXTCAP_NEXT_GET(*cap))
            break;
    }
}

bool usbxhci::init(pci_dev_iterator_t const& pci_iter, size_t busid)
{
    uint64_t timeout;

    this->busid = busid;

    // Bus master enable, memory space enable, I/O space disable
    pci_adj_control_bits(pci_iter, PCI_CMD_BME | PCI_CMD_MSE, PCI_CMD_IOSE);

    mmio_addr = pci_iter.config.get_bar(0);

    mmio_base = (char*)mmap(
                (void*)mmio_addr, 64<<10, PROT_READ | PROT_WRITE,
                MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU, -1, 0);

    mmio_cap = (usbxhci_capreg_t*)mmio_base;
    mmio_op = (usbxhci_opreg_t*)(mmio_base + mmio_cap->caplength);
    mmio_rt = (usbxhci_rtreg_t*)(mmio_base + (mmio_cap->rtsoff & -32));
    mmio_db = (usbxhci_dbreg_t*)(mmio_base + (mmio_cap->dboff & -4));

    USBXHCI_TRACE("Waiting for controller not ready == zero\n");

    // 20 seconds is the point where no working controller is that slow
    uint64_t now = time_ns();
    uint64_t time_st = now;
    timeout = time_st + 20000000000;

    // Wait for CNR (controller not ready) to be zero
    while ((mmio_op->usbsts & USBXHCI_USBSTS_CNR) &&
           ((now = time_ns()) < timeout))
        pause();

    if (unlikely(now >= timeout)) {
        printk("XHCI device timed out waiting for controller ready\n");
        return false;
    }

    USBXHCI_TRACE("Controller is ready\n");

    // Perform BIOS handoff
    size_t xecp = USBXHCI_CAPREG_HCCPARAMS1_XECP_GET(mmio_cap->hccparams1);

    walk_caps(mmio_base + (xecp * 4));

    // 4.2 Host Controller Initialization

    USBXHCI_TRACE("Stopping controller\n");

    // 2 seconds is the point where no working controller is that slow
    now = time_ns();
    time_st = now;
    timeout = time_st + 2000000000;

    // Stop the controller
    mmio_op->usbcmd &= ~USBXHCI_USBCMD_RUNSTOP;

    // Wait for controller to stop
    while ((!(mmio_op->usbsts & USBXHCI_USBSTS_HCH)) &&
           ((now = time_ns()) < timeout))
        pause();

    if (unlikely(now >= timeout)) {
        printk("XHCI controller not responding to stop request\n");
        return false;
    }

    USBXHCI_TRACE("Stop completed in %" PRIu64 "ms\n",
                  (now - time_st) / 1000000);

    USBXHCI_TRACE("Resetting controller\n");

    // 20 seconds is the point where no working controller is that slow
    now = time_ns();
    time_st = now;
    timeout = time_st + 200000000000;

    // Reset the controller
    mmio_op->usbcmd |= USBXHCI_USBCMD_HCRST;

    // Wait for reset to complete
    while ((mmio_op->usbcmd & USBXHCI_USBCMD_HCRST) &&
           ((now = time_ns()) < timeout))
        pause();

    if (unlikely(now >= timeout)) {
        printk("XHCI device, host controller reset will not complete\n");
        return false;
    }

    USBXHCI_TRACE("Reset completed in %" PRIu64 "ms\n",
                  (time_ns() - time_st) / 1000000);

    uint32_t hcsparams1 = mmio_cap->hcsparams1;
    maxslots = USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_GET(hcsparams1);
    maxintr = USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_GET(hcsparams1);
    maxports = USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_GET(hcsparams1);

    dev_ctx_large = USBXHCI_CAPREG_HCCPARAMS1_CSZ_GET(mmio_cap->hccparams1);

    USBXHCI_CONFIG_MAXSLOTSEN_SET(mmio_op->config, maxslots);

    USBXHCI_TRACE("devslots=%d, maxintr=%d, maxports=%d\n",
                  maxslots, maxintr, maxports);

    slot_data.resize(maxslots);

    size_t cpu_count = thread_get_cpu_count();

    // Share the same vector on all CPUs if using multiple interrupters
    std::vector<int> target_cpus;
    std::vector<int> vector_offsets;

    if (maxintr >= cpu_count) {
        USBXHCI_TRACE("Device supports per-CPU IRQs\n");
        maxintr = cpu_count;

        // One MSI-X vector per CPU, all using same interrupt vector
        target_cpus.resize(cpu_count);
        for (size_t i = 0; i < cpu_count; ++i)
            target_cpus[i] = i;
        vector_offsets.resize(cpu_count, 0);
    } else {
        USBXHCI_TRACE("Device cannot support per-CPU IRQs, using one IRQ\n");
        maxintr = 1;
    }

    use_msi = pci_try_msi_irq(pci_iter, &irq_range,
                              0, true, maxintr,
                              &usbxhci::irq_handler,
                              "usb_xhci", !target_cpus.empty()
                              ? target_cpus.data() : nullptr,
                              !vector_offsets.empty()
                              ? vector_offsets.data() : nullptr);

    // 17. Interrupters "when using PCI pin interrupt,
    // Interrupters 1 to MaxIntrs-1 shall be disabled."
    if (!use_msi)
        maxintr = 1;

    // Don't bother using multiple IRQs if no MSI-X for per CPU routing
    if (!irq_range.msix)
        maxintr = 1;

    USBXHCI_TRACE("Using IRQs %s=%d, base=%u, count=%u\n",
                  irq_range.msix ? "msix" : "msi", use_msi,
                  irq_range.base, irq_range.count);

    size_t dev_ctx_size = dev_ctx_large
            ? sizeof(usbxhci_devctx_large_t)
            : sizeof(usbxhci_devctx_small_t);

    // Program device context address array pointer
    dev_ctx_ptrs = (uint64_t*)
            mmap(nullptr, sizeof(*dev_ctx_ptrs) * maxslots,
                 PROT_READ | PROT_WRITE,
                 MAP_POPULATE, -1, 0);

    dev_ctx.any = mmap(nullptr, dev_ctx_size * maxslots,
                        PROT_READ | PROT_WRITE,
                        MAP_POPULATE, -1, 0);

    // Device Context Base Address Array (idx 0 reserved for scratchpad array)
    for (size_t i = 1; i < maxslots; ++i)
        dev_ctx_ptrs[i] = mphysaddr(dev_ctx_ent_slot(i));

    // Allocate scratchpad buffers for controller if required
    int maxscratchpad =
            USBXHCI_CAPREG_HCSPARAMS2_MAXSCRBUFLO_GET(mmio_cap->hcsparams2) |
            (USBXHCI_CAPREG_HCSPARAMS2_MAXSCRBUFHI_GET(
                 mmio_cap->hcsparams2) << 5);

    if (maxscratchpad > 0) {
        USBXHCI_TRACE("device requests %d scratchpad pages (%dKB)\n",
                      maxscratchpad, (maxscratchpad << PAGE_SCALE) >> 10);
        // Not doing contiguous allocation - can only handle 512 pages
        assert(maxscratchpad <= int(PAGESIZE / sizeof(uint64_t)));

        // Array of scratch buffer pointers
        uint64_t *scratchbufarr = (uint64_t*)mmap(
                    nullptr, maxscratchpad * sizeof(uint64_t),
                    PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

        for (int i = 0; i < maxscratchpad; ++i) {
            void *scratch_buffer = mmap(nullptr, PAGESIZE, PROT_READ,
                                        MAP_POPULATE, -1, 0);
            scratchbufarr[i] = mphysaddr(scratch_buffer);
        }

        // If the Max Scratchpad Buffers field of the HCSPARAMS2 register
        // is > ‘0’, then the first entry (entry_0) in the DCBAA shall
        // contain a pointer to the Scratchpad Buffer Array.
        dev_ctx_ptrs[0] = mphysaddr(scratchbufarr);
    } else {
        // 6.1  If the Max Scratchpad Buffers field of the HCSPARAMS2
        // register is = ‘0’, then the first entry (entry_0) in the DCBAA
        // is reserved and shall be cleared to ‘0’ by software.
        dev_ctx_ptrs[0] = 0;
    }

    // Device Context Base Address Array Pointer
    mmio_op->dcbaap = mphysaddr(dev_ctx_ptrs);

    // Command ring size in entries
    cmd_ring.alloc(PAGESIZE / sizeof(usbxhci_cmd_trb_t));
    cmd_ring.reserve_link();

    // Command Ring Control Register
    mmio_op->crcr = cmd_ring.physaddr;

    // Event segments
    dev_evt_segs = (usbxhci_evtring_seg_t*)mmap(
                nullptr, sizeof(*dev_evt_segs) * maxintr * 4,
                PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

    interrupters = (usbxhci_interrupter_info_t*)
            malloc(sizeof(*interrupters) * maxintr);

    for (size_t i = 0; i < maxintr; ++i) {
        interrupters[i].alloc(PAGESIZE / sizeof(*interrupters[i].ptr));

        dev_evt_segs[i*4].base = interrupters[i].physaddr;

        dev_evt_segs[i*4].trb_count =
                PAGESIZE / sizeof(*interrupters[i].ptr);

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

        //// Acknowldge possible pending interrupt
        //mmio_rt->ir[i].iman |= USBXHCI_INTR_IMAN_IP;
    }

    // Acknowledge possible pending interrupt
    mmio_op->usbsts = USBXHCI_USBSTS_EINT;

    mmio_op->usbcmd |= USBXHCI_USBCMD_INTE;

    // Enable PCI IRQ
    pci_set_irq_unmask(pci_iter, true);

    mmio_op->usbcmd |= USBXHCI_USBCMD_RUNSTOP;

    while (mmio_op->usbsts & USBXHCI_USBSTS_HCH)
        pause();

    for (int port = 0; port < maxports; ++port) {
        if (mmio_op->ports[port].portsc & USBXHCI_PORTSC_CCS) {
            USBXHCI_TRACE("Device is connected to port %d\n", port);

            // Reset the port
            mmio_op->ports[port].portsc |= USBXHCI_PORTSC_PR;

            USBXHCI_TRACE("Waiting for reset on port %d\n", port);

            while (mmio_op->ports[port].portsc & USBXHCI_PORTSC_PR)
                pause();

            USBXHCI_TRACE("Reset finished on port %d\n", port);
        }
    }

    port_count = 0;

    for (int port = 0; port < maxports; ++port) {
        if (USBXHCI_PORTSC_CCS_GET(mmio_op->ports[port].portsc)) {
            USBXHCI_TRACE("Adding device on port %d\n", port);
            add_device(0, port, 0);
        } else {
            USBXHCI_TRACE("No device on port %d\n", port);
        }
    }

    return true;
}

int usbxhci::enable_slot(int port)
{
    usbxhci_cmd_trb_noop_t cmd{};
    cmd.trb_type = USBXHCI_CMD_TRB_TYPE_n(USBXHCI_TRB_TYPE_ENABLESLOTCMD);

    USBXHCI_TRACE("Enabling slot for port %#x\n", port);

    usb_blocking_iocp_t block;

    issue_cmd((usbxhci_cmd_trb_t*)&cmd, &block);
    block.set_expect(1);

    block.wait();

    USBXHCI_TRACE("enableslot completed: completion code=%#x, "
                  "parameter=%#x, slotid=%d\n",
                  unsigned(block.get_result().cc),
                  block.get_result().ccp,
                  block.get_result().slotid);

    return block.get_result().slot_or_error();
}

int usbxhci::set_address(int slotid, int port, uint32_t route)
{
    // Issue a SET_ADDRESS command

    usbxhci_slot_data_t &slot = slot_data.at(slotid);

    slot.port = port;

    // Follow parent chain and find the root hub port number for this slot
    int root_port_slot = slotid;
    while (slot_data[root_port_slot].parent_slot)
        root_port_slot = slot_data[root_port_slot].parent_slot;

    uint8_t root_port = slot_data[root_port_slot].port;

    // Allocate an input context
    usbxhci_inpctx_t inp;
    usbxhci_inpctlctx_t *ctlctx;
    usbxhci_slotctx_t *inpslotctx;
    usbxhci_ep_ctx_t *inpepctx;

    if (!dev_ctx_large) {
        inp.any = mmap(nullptr, sizeof(usbxhci_inpctx_small_t),
                   PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
        ctlctx = &inp.small->inpctl;
        inpslotctx = &inp.small->slotctx;
        inpepctx = inp.small->epctx;
    } else {
        inp.any = mmap(nullptr, sizeof(usbxhci_inpctx_large_t),
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
    inpslotctx->root_hub_port_num = root_port + 1;

    // Device address
    inpslotctx->usbdevaddr = 0;

    // Default max packet size is 8
    inpepctx->max_packet = 8;

    // 6.2.3.1 Input endpoint context 0
    inpepctx->ceh = USBXHCI_EPCTX_CEH_EPTYPE_n(USBXHCI_EPTYPE_CTLBIDIR);

    usbxhci_endpoint_data_t *epdata = add_endpoint(slotid, 0);

    inpepctx->tr_dq_ptr = epdata->ring.physaddr | epdata->ring.cycle;

    usbxhci_cmd_trb_setaddr_t setaddr{};
    setaddr.input_ctx_physaddr = mphysaddr(inp.any);
    setaddr.trb_type = USBXHCI_CMD_TRB_TYPE_n(USBXHCI_TRB_TYPE_ADDRDEVCMD);

    setaddr.slotid = slotid;

    usb_blocking_iocp_t block;

    issue_cmd((usbxhci_cmd_trb_t*)&setaddr, &block);
    block.set_expect(1);

    block.wait();

    USBXHCI_TRACE("setaddr completed: completion code=%#x, "
                  "parameter=%#x, slotid=%d\n",
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

bool usbxhci::alloc_pipe(int slotid, usb_pipe_t &pipe,
                         int epid, int cfg_value,
                         int iface_num, int alt_iface,
                         int max_packet_sz, int interval,
                         usb_ep_attr ep_type)
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

    ctlctx->cfg = cfg_value;
    ctlctx->iface_num = iface_num;
    ctlctx->alternate = alt_iface;

    usbxhci_ep_ctx_t *ep = inpepctx + (bit_index - 1);

    ep->max_packet = max_packet_sz;
    ep->interval = interval;

    ep->tr_dq_ptr = (USBXHCI_EPCTX_TR_DQ_PTR_PTR_MASK &
            epd->ring.physaddr) |
            USBXHCI_EPCTX_TR_DQ_PTR_DCS_n(epd->ring.cycle);

    uint8_t ep_type_value = uint8_t(ep_type);
    if (in)
        ep_type_value += 4;

    USBXHCI_EPCTX_CEH_EPTYPE_SET(ep->ceh, ep_type_value);

    usb_cc_t cc = commit_inp_ctx(slotid, epid, inp,
                                 USBXHCI_TRB_TYPE_CONFIGUREEPCMD);
    if (unlikely(cc != usb_cc_t::success)) {
        pipe = usb_pipe_t(nullptr, -1, -1);
        return false;
    }

    pipe = usb_pipe_t(this, slotid, epid);
    return true;
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

    add_device(slotid, slotctx->root_hub_port_num, route);

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

    issue_cmd((usbxhci_cmd_trb_t*)&cmd, iocp);

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
        inp.any = mmap(nullptr, sizeof(usbxhci_inpctx_small_t),
                   PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
        ctlctx = &inp.small->inpctl;
        inpslotctx = &inp.small->slotctx;
        inpepctx = inp.small->epctx;
    } else {
        inp.any = mmap(nullptr, sizeof(usbxhci_inpctx_large_t),
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
    // Issue evaluate context command
    usbxhci_ctl_trb_evalctx_t *eval = (usbxhci_ctl_trb_evalctx_t *)
            calloc(1, sizeof(*eval));
    eval->trt = USBXHCI_CTL_TRB_TRT_SLOTID_n(slotid);
    eval->input_ctx_ptr = mphysaddr(inp.any);
    eval->flags = USBXHCI_CTL_TRB_FLAGS_TRB_TYPE_n(trb_type);

    usb_blocking_iocp_t block;

    issue_cmd((usbxhci_cmd_trb_t*)eval, &block);
    block.set_expect(1);

    block.wait();

    if (!dev_ctx_large)
        munmap(inp.any, sizeof(usbxhci_inpctx_small_t));
    else
        munmap(inp.any, sizeof(usbxhci_inpctx_large_t));

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
    inpepctx->max_packet = dev_desc->maxpktsz;

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
    usbxhci_evt_portstchg_t *portscevt;
    uint64_t cmdaddr = 0;

    switch (type) {
    case USBXHCI_TRB_TYPE_XFEREVT:
        xfer = (usbxhci_evt_xfer_t*)evt;
        cmdaddr = xfer->trb_ptr;
        USBXHCI_TRACE("XFEREVT (CC=%d) (TRB=0x%#" PRIx64 ")\n",
                      xfer->cc, xfer->trb_ptr);
        break;

    case USBXHCI_TRB_TYPE_CMDCOMPEVT:
        cmdcomp = (usbxhci_evt_cmdcomp_t*)evt;
        cmdaddr = cmdcomp->command_trb_ptr;
        USBXHCI_TRACE("CMDCOMPEVT (TRB=0x%#" PRIx64 ")\n", cmdaddr);
        break;

    case USBXHCI_TRB_TYPE_PORTSTSCHGEVT:
        // Port status change
        USBXHCI_TRACE("PORTSTSCHGEVT\n");

        portscevt = (usbxhci_evt_portstchg_t*)evt;

        size_t portid;
        portid = portscevt->portid;

        uint32_t volatile *portsc;
        portsc = &mmio_op->ports[portid].portsc;

        bool connect_status_chg;
        connect_status_chg = USBXHCI_PORTSC_CSC_GET(*portsc);

        if (connect_status_chg) {
            bool connected = USBXHCI_PORTSC_CCS_GET(*portsc);

            if (connected) {
                USBXHCI_TRACE("PORTSTSCHGEVT detected"
                              " port %zu device %sconnect\n", portid, "");
            } else {
                USBXHCI_TRACE("PORTSTSCHGEVT detected"
                              " port %zu device %sconnect\n", portid, "dis");
            }

            // Clear connect status changed
            *portsc = USBXHCI_PORTSC_CSC;
        }
        break;

    case USBXHCI_TRB_TYPE_BWREQEVT:
        // Bandwidth request
        USBXHCI_TRACE("BWREQEVT\n");
        break;

    case USBXHCI_TRB_TYPE_DBEVT:
        // Doorbell
        USBXHCI_TRACE("DBEVT\n");
        break;

    case USBXHCI_TRB_TYPE_HOSTCTLEVT:
        // Host controller
        USBXHCI_TRACE("HOSTCTLEVT\n");
        break;

    case USBXHCI_TRB_TYPE_DEVNOTIFEVT:
        // Device notification
        USBXHCI_TRACE("DEVNOTIFEVT\n");
        break;

    case USBXHCI_TRB_TYPE_MFINDEXWRAPEVT:
        // Microframe index wrap
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
    usbxhci_pending_cmd_t *pcp = usbxhci_pending_ht.lookup(&cmdaddr);
    assert(pcp);
    if (pcp) {
        usbxhci_pending_ht.del(&cmdaddr);

        // Invoke completion handler
        cmd_comp(evt, pcp->iocp);
    }
}

// Runs in IRQ handler
isr_context_t *usbxhci::irq_handler(int irq, isr_context_t *ctx)
{
    for (usbxhci* dev : usbxhci_devices) {
        int irq_ofs = irq - dev->irq_range.base;

        if (dev->maxintr != 1)
            irq_ofs = thread_cpu_number();

        if (irq_ofs >= 0 && irq_ofs < dev->irq_range.count) {
            //dev->irq_handler(irq_ofs);
            workq::enqueue([=]() {
                dev->irq_handler(irq_ofs);
            });
        }
    }

    return ctx;
}

// Runs in CPU worker thread
void usbxhci::irq_handler(int irq_ofs)
{
    uint32_t usbsts = mmio_op->usbsts;

    bool event_intr = USBXHCI_USBSTS_EINT_GET(usbsts);

    bool port_chg_intr = USBXHCI_USBSTS_PCD_GET(usbsts);

#if USBXHCI_DEBUG
    bool host_err_intr = USBXHCI_USBSTS_HSE_GET(usbsts);
    bool save_err_intr = USBXHCI_USBSTS_SRE_GET(usbsts);
#endif

    USBXHCI_TRACE("irq: evt=%d, hosterr=%d, portchg=%d, saveerr=%d\n",
                  event_intr, host_err_intr, port_chg_intr, save_err_intr);

    if (port_chg_intr) {
        for (int port = 0; port < maxports; ++port) {
            auto volatile& p = mmio_op->ports[port];

            uint32_t portsc = p.portsc;

#if USBXHCI_DEBUG
            bool conn_chg = USBXHCI_PORTSC_CSC_GET(portsc);
            bool enab_chg = USBXHCI_PORTSC_PEC_GET(portsc);
            bool wrst_chg = USBXHCI_PORTSC_WRC_GET(portsc);
            bool curr_chg = USBXHCI_PORTSC_OCC_GET(portsc);
            bool rset_chg = USBXHCI_PORTSC_PRC_GET(portsc);
            bool link_chg = USBXHCI_PORTSC_PLC_GET(portsc);
            bool cerr_chg = USBXHCI_PORTSC_CEC_GET(portsc);
#endif

            USBXHCI_TRACE("pchg: port=%u, conn=%u, enab=%u wrst=%u"
                          ", curr=%u, rset=%u, link=%u, cerr=%u\n",
                          port, conn_chg, enab_chg, wrst_chg,
                          curr_chg, rset_chg, link_chg, cerr_chg);

            uint32_t portsc_ack = portsc &
                    (USBXHCI_PORTSC_CSC | USBXHCI_PORTSC_PEC |
                     USBXHCI_PORTSC_WRC | USBXHCI_PORTSC_OCC |
                     USBXHCI_PORTSC_PRC | USBXHCI_PORTSC_PLC |
                     USBXHCI_PORTSC_CEC);

            p.portsc = portsc_ack;
        }
    }

    uint32_t ack = usbsts &
            (USBXHCI_USBSTS_HSE | USBXHCI_USBSTS_EINT |
             USBXHCI_USBSTS_PCD | USBXHCI_USBSTS_SRE);

    // Acknowledge pending IRQs
    if (ack)
        atomic_st_rel(&mmio_op->usbsts, ack);

    // Skip if interrupt is not pending
    if (!event_intr)
        return;

    // The IRQ is the interrupter index % the number of IRQs
    for (size_t ii = irq_ofs; ii < maxintr; ii += irq_range.count) {
        usbxhci_interrupter_info_t *ir_info = interrupters + ii;
        usbxhci_intr_t *ir = (usbxhci_intr_t*)(mmio_rt->ir + ii);

        if (USBXHCI_INTR_IMAN_IP_GET(atomic_ld_acq(&ir->iman))) {
            // Interrupt is pending
            USBXHCI_TRACE("Interrupt pending on interrupter %zu\n", ii);

            // Acknowledge the interrupt
            //  4.17.5: "If MSI or MSI-X interrupts are enabled, IP shall be
            //  cleared to ‘0’ automatically when the PCI Dword write
            //  generated by the Interrupt assertion is complete."
            if (!use_msi)
                ir->iman |= USBXHCI_INTR_IMAN_IP;

            bool any_consumed = false;

            while (!ir_info->cycle ==
                   !(ir_info->ptr[ir_info->next].flags &
                     USBXHCI_EVT_FLAGS_C)) {
                usbxhci_evt_t *evt = (usbxhci_evt_t*)(ir_info->ptr +
                        ir_info->next++);

                if (ir_info->next >= ir_info->count) {
                    ir_info->next = 0;
                    ir_info->cycle = !ir_info->cycle;
                }

                any_consumed = true;

                evt_handler(ir_info, ir, evt, ii);
            }

            if (any_consumed) {
                // Notify HC that we have consumed some events
                ir->erdp = (ir_info->physaddr +
                        ir_info->next * sizeof(*ir_info->ptr)) |
                        USBXHCI_ERDP_EHB;
            }
        }

        // Run completions after consuming events
        for (usb_iocp_t *iocp : completed_iocp)
            iocp->invoke();
        completed_iocp.clear();
    }
}

int module_main(int argc, char const * const * argv)
{
    usbxhci::detect();
    return 0;
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

        std::unique_ptr<usbxhci> self(new (std::nothrow) usbxhci);

        if (!usbxhci_devices.emplace_back(self.get())) {
            USBXHCI_TRACE("Out of memory!");
            break;
        }

        if (self->init(pci_iter, usbxhci_devices.size()-1))
            self.release();
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

    // Route the completions to this CPU if we are using multiple interrupters
    unsigned interrupter = maxintr > 1 ? thread_cpu_number() : 0;

    for (size_t i = 0; i < range_count; ++i) {
        trb->data_physaddr = ranges[i].physaddr;
        trb->xfer_td_intr =
                USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_n(ranges[i].size) |
                USBXHCI_CTL_TRB_XFERLEN_INTR_TDSZ_n(
                    std::min(size_t(USBXHCI_CTL_TRB_XFERLEN_INTR_TDSZ_MASK),
                        range_count - i - 1)) |
                USBXHCI_CTL_TRB_XFERLEN_INTR_INTR_n(interrupter);
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

template<typename T>
void usbxhci_ring_data_t<T>::insert(usbxhci *ctrl, usbxhci_cmd_trb_t *src,
                                    usb_iocp_t *iocp, bool ins_pending)
{
    USBXHCI_CTL_TRB_FLAGS_C_SET(src->data[3], !cycle);
    usbxhci_cmd_trb_t volatile *dst = &ptr[next];

    // Copy the TRB carefully, ensuring cycle bit is set last
    dst->data[0] = src->data[0];
    dst->data[1] = src->data[1];
    dst->data[2] = src->data[2];

    // Verify that the cycle bit of the existing TRB is as expected
    assert(USBXHCI_CTL_TRB_FLAGS_C_GET(dst->data[3]) != (cycle != 0));

    // Initially set the cycle bit to the value that prevents TRB execution
    dst->data[3] = (src->data[3] & ~USBXHCI_CTL_TRB_FLAGS_C) |
            USBXHCI_CTL_TRB_FLAGS_C_n(!cycle);

    // Guarantee ordering and set cycle bit last
    atomic_st_rel(&dst->data[3],
            (src->data[3] & ~USBXHCI_CTL_TRB_FLAGS_C) |
            USBXHCI_CTL_TRB_FLAGS_C_n(cycle));

    if (ins_pending) {
        usbxhci_pending_cmd_t *pending_cmd = pending + next;
        ctrl->insert_pending_command(pending_cmd, physaddr +
                                     next * sizeof(*ptr), iocp);
    }

    if (++next >= count) {
        USBXHCI_TRACE("Wrapping ring\n");

        // Update link TRB cycle bit
        usbxhci_cmd_trb_t volatile *link = ptr + next;

        // Make sure the cycle bit is as expected
        assert(USBXHCI_CTL_TRB_FLAGS_C_GET(link->data[3]) != cycle);

        // Copy the chain bit from the last TRB to propagate possible
        // chain across the link TRB
        bool chain = USBXHCI_CTL_TRB_FLAGS_CH_GET(src->data[3]);
        USBXHCI_CTL_TRB_FLAGS_CH_SET(link->data[3], chain);

        // Guarantee ordering and set cycle bit last
        atomic_st_rel(&link->data[3],
                (link->data[3] & ~USBXHCI_CTL_TRB_FLAGS_C) |
                USBXHCI_CTL_TRB_FLAGS_C_n(cycle));

        cycle = !cycle;
        next = 0;
    }
}

template<typename T>
bool usbxhci_ring_data_t<T>::alloc(uint32_t trb_count)
{
    pending = (usbxhci_pending_cmd_t*)
                 mmap(nullptr, sizeof(*pending) * trb_count,
                      PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

    ptr = (T *)mmap(nullptr, sizeof(*ptr) * trb_count,
                    PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
    physaddr = mphysaddr(ptr);

    next = 0;
    count = trb_count;
    cycle = 1;

    return pending != MAP_FAILED && ptr != MAP_FAILED;
}

template<typename T>
void usbxhci_ring_data_t<T>::reserve_link()
{
    // Reserve entry for link TRB
    --count;

    auto link = (usbxhci_cmd_trb_link_t *)&ptr[count];

    *link = {};
    link->ring_physaddr = physaddr;
    link->c_tc_ch_ioc = USBXHCI_CMD_TRB_TC | USBXHCI_CMD_TRB_C_n(!cycle);
    link->trb_type = USBXHCI_CMD_TRB_TYPE_n(USBXHCI_TRB_TYPE_LINK);
}

size_t usbxhci_pending_cmd_t::hash() const
{
    return cmd_physaddr;
}

usbxhci_slot_data_t::usbxhci_slot_data_t()
    : parent_slot(0)
    , port(0)
    , is_hub(false)
    , is_multi_tt(false)
{
}
