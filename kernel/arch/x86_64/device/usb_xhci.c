#include "usb_xhci.h"

#include "callout.h"
#include "pci.h"
#include "printk.h"
#include "stdlib.h"
#include "mm.h"
#include "cpu/atomic.h"
#include "string.h"

#define USBXHCI_DEBUG   1
#if USBXHCI_DEBUG
#define USBXHCI_TRACE(...) printdbg("xhci: " __VA_ARGS__)
#else
#define USBXHCI_TRACE(...) (void)0
#endif

//
// 5.3 Host Controller Capability Registers

typedef struct usbxhci_capreg_t {
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
} __attribute__((packed)) usbxhci_capreg_t;

C_ASSERT(sizeof(usbxhci_capreg_t) == 0x20);

//
// 5.3.3 Structural Parameters 1 (HCSPARAMS1)

#define USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_BIT   0
#define USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_BITS  8
#define USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_BIT       8
#define USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_BITS      11
#define USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_BIT      24
#define USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_BITS     8

#define USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_MASK \
    ((1U<<USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_BITS)-1)
#define USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_n(n) \
    ((n)<<USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_BIT)
#define USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS \
    (USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_MASK<< \
    USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_BIT)

#define USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_MASK \
    ((1U<<USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_BITS)-1)
#define USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_n(n) \
    ((n)<<USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_BIT)
#define USBXHCI_CAPREG_HCSPARAMS1_MAXINTR \
    (USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_MASK<< \
    USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_BIT)

#define USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_MASK \
    ((1U<<USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_BITS)-1)
#define USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_n(n) \
    ((n)<<USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_BIT)
#define USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS \
    (USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_MASK<< \
    USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_BIT)

// 5.5.2 Interrupt Register Set

typedef struct usbxhci_intr_t {
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
} __attribute__((packed)) usbxhci_intr_t;

#define USBXHCI_INTR_IMAN_IP_BIT    0
#define USBXHCI_INTR_IMAN_IE_BIT    1

// Interrupt Pending
#define USBXHCI_INTR_IMAN_IP        (1U<<USBXHCI_INTR_IMAN_IP_BIT)

// Interrupt Enable
#define USBXHCI_INTR_IMAN_IE        (1U<<USBXHCI_INTR_IMAN_IE_BIT)

C_ASSERT(sizeof(usbxhci_intr_t) == 0x20);

// 5.5 Host Controller Runtime Registers

typedef struct usbxhci_rtreg_t  {
    // 5.5.1 Microframe Index Register
    uint32_t mfindex;

    uint8_t rsvdz[0x20-0x04];

    // 5.5.2 Interrupter Register Set
    usbxhci_intr_t ir[1023];
} __attribute__((packed)) usbxhci_rtreg_t;

C_ASSERT(sizeof(usbxhci_rtreg_t) == 0x8000);

typedef uint32_t usbxhci_dbreg_t;



typedef struct usbxhci_portreg_t {
    // 5.4.8 0h Port Status and Control
    uint32_t portsc;

    // 5.4.9 4h Port Power Management Status and Control
    uint32_t portpmsc;

    // 5.4.10 8h Port Link Info
    uint32_t portli;

    // 5.4.11 Ch Port Hardware LPM Control
    uint32_t porthlpmc;
} __attribute__((packed)) usbxhci_portreg_t;

C_ASSERT(sizeof(usbxhci_portreg_t) == 0x10);

typedef struct usbxhci_opreg_t {
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
} __attribute__((packed)) usbxhci_opreg_t;

C_ASSERT(sizeof(usbxhci_opreg_t) == 0x1400);

//
// 5.4.1 USB Command Register (USBCMD)

#define USBXHCI_USBCMD_RUNSTOP_BIT  0
#define USBXHCI_USBCMD_HCRST_BIT    1
#define USBXHCI_USBCMD_INTE_BIT     2
#define USBXHCI_USBCMD_HSEE_BIT     3
#define USBXHCI_USBCMD_LHCRST_BIT   7
#define USBXHCI_USBCMD_CSS_BIT      8
#define USBXHCI_USBCMD_CRS_BIT      9
#define USBXHCI_USBCMD_EWE_BIT      10
#define USBXHCI_USBCMD_EU3S_BIT     11
#define USBXHCI_USBCMD_SPE_BIT      12
#define USBXHCI_USBCMD_CME_BIT      13

// RW Run/Stop: 1=run, 0=stop
#define USBXHCI_USBCMD_RUNSTOP      (1U<<USBXHCI_USBCMD_RUNSTOP_BIT)

// RW Reset: 1=reset, xHC resets it to 0 when reset is complete
#define USBXHCI_USBCMD_HCRST        (1U<<USBXHCI_USBCMD_HCRST_BIT)

// RW Interrupter Enable: 1=enabled
#define USBXHCI_USBCMD_INTE         (1U<<USBXHCI_USBCMD_INTE_BIT)

// RW Host System Error Enable: 1=enable
#define USBXHCI_USBCMD_HSEE         (1U<<USBXHCI_USBCMD_HSEE_BIT)

// RO or RW Light Host Controller Reset: 1=Reset,
// xHC resets it to 0 when reset is complete
#define USBXHCI_USBCMD_LHCRST       (1U<<USBXHCI_USBCMD_LHCRST_BIT)

// RW Controller Save State: 0=Normal, 1=Save
#define USBXHCI_USBCMD_CSS          (1U<<USBXHCI_USBCMD_CSS_BIT)

// RW Controller Restore State: 0=Normal, 1=Restore
#define USBXHCI_USBCMD_CRS          (1U<<USBXHCI_USBCMD_CRS_BIT)

// RW Event Wrap Enable: 0=Normal, 1=Generate MFINDEX wrap event
#define USBXHCI_USBCMD_EWE          (1U<<USBXHCI_USBCMD_EWE_BIT)

// RW Enable U3 MFINDEX Stop: 0=normal, 1=Stop counting if all ports off
#define USBXHCI_USBCMD_EU3S         (1U<<USBXHCI_USBCMD_EU3S_BIT)

// Stopped - Short Packet Enable: 0=normal, 1=Generate completion code
#define USBXHCI_USBCMD_SPE          (1U<<USBXHCI_USBCMD_SPE_BIT)

// RW CEM Enable: 0=normal, 1=Enable Max Exit Latency Too Large Capability Error
#define USBXHCI_USBCMD_CME          (1U<<USBXHCI_USBCMD_CME_BIT)

//
// 5.4.2 USB Status Register (USBSTS)

#define USBXHCI_USBSTS_HCH_BIT      0
#define USBXHCI_USBSTS_HSE_BIT      2
#define USBXHCI_USBSTS_EINT_BIT     3
#define USBXHCI_USBSTS_PCD_BIT      4
#define USBXHCI_USBSTS_SSS_BIT      8
#define USBXHCI_USBSTS_RSS_BIT      9
#define USBXHCI_USBSTS_SRE_BIT      10
#define USBXHCI_USBSTS_CNR_BIT      11
#define USBXHCI_USBSTS_HCE_BIT      12

// RO HC Halted: 0=running, 1=halted
#define USBXHCI_USBSTS_HCH          (1U<<USBXHCI_USBSTS_HCH_BIT)

// RW1C Host System Error: 0=ok, 1=serious error
#define USBXHCI_USBSTS_HSE          (1U<<USBXHCI_USBSTS_HSE_BIT)

// RW1C Event Interrupt: 1=interrupt pending
#define USBXHCI_USBSTS_EINT         (1U<<USBXHCI_USBSTS_EINT_BIT)

// RW1C Port Change Detect: 1=changed
#define USBXHCI_USBSTS_PCD          (1U<<USBXHCI_USBSTS_PCD_BIT)

// RO Save State Status: 1=still saving
#define USBXHCI_USBSTS_SSS          (1U<<USBXHCI_USBSTS_SSS_BIT)

// RO Restore State Status: 1=still restoring
#define USBXHCI_USBSTS_RSS          (1U<<USBXHCI_USBSTS_RSS_BIT)

// RW1C Save/Restore Error: 1=error
#define USBXHCI_USBSTS_SRE          (1U<<USBXHCI_USBSTS_SRE_BIT)

// RO Controller Not Ready: 1=not ready
#define USBXHCI_USBSTS_CNR          (1U<<USBXHCI_USBSTS_CNR_BIT)

// RO Host Controller Error: 1=xHC error
#define USBXHCI_USBSTS_HCE          (1U<<USBXHCI_USBSTS_HCE_BIT)

//
// 5.4.3 Page Size Register (PAGESIZE)

// One bit may be set. The value of the register times 4096 is the page size
#define USBXHCI_PAGESIZE_BIT        0
#define USBXHCI_PAGESIZE_BITS       16
#define USBXHCI_PAGESIZE_MASK       ((1U<<USBXHCI_PAGESIZE_BITS)-1)
#define USBXHCI_PAGESIZE \
    (USBXHCI_PAGESIZE_MASK<<USBXHCI_PAGESIZE_BIT)
#define USBXHCI_PAGESIZE_n(n)       ((n)<<USBXHCI_PAGESIZE_BIT)

//
// 5.4.4 Device Notification Control Register (DNCTRL)

// RW Notification enable (only bit 1 should be set, normally)
#define USBXHCI_DNCTRL_N_BITS       16
#define USBXHCI_DNCTRL_N_MASK       ((1U<<USBXHCI_DNCTRL_N_BITS)-1)
#define USBXHCI_DNCTRL_N \
    (USBXHCI_DNCTRL_N_MASK<<USBXHCI_DNCTRL_N_BITS)
#define USBXHCI_DNCTRL_N_n(n)       (1U<<(n))

//
// 5.4.5 Command Ring Control Register (CRCR)

#define USBXHCI_CRCR_RCS_BIT        0
#define USBXHCI_CRCR_CS_BIT         1
#define USBXHCI_CRCR_CA_BIT         2
#define USBXHCI_CRCR_CRR_BIT        3

// RW Command ring pointer, 512-bit aligned start address of command ring
#define USBXHCI_CRCR_CRPTR_BIT      6
#define USBXHCI_CRCR_CRPTR_BITS     58
#define USBXHCI_CRCR_CRPTR_MASK     ((1UL<<USBXHCI_CRCR_CRPTR_BITS)-1)
#define USBXHCI_CRCR_CRPTR \
    (USBXHCI_CRCR_CRPTR_MASK<<USBXHCI_CRCR_CRPTR_BIT)
#define USBXHCI_CRCR_CRPTR_n(ptr)   ((uintptr_t)(ptr) & USBXHCI_CRCR_CRPTR)

// RW Ring Cycle State: ?
#define USBXHCI_CRCR_RCS            (1U<<USBXHCI_CRCR_RCS_BIT)

// RW1S Command Stop: 0=normal, 1=stop command ring
#define USBXHCI_CRCR_CS             (1U<<USBXHCI_CRCR_CS_BIT)

// RW1S Command Abort: 0=normal, 1=abort
#define USBXHCI_CRCR_CA             (1U<<USBXHCI_CRCR_CA_BIT)

// RO Command Ring Running, 0=normal, 1=running
#define USBXHCI_CRCR_CRR            (1U<<USBXHCI_CRCR_CRR_BIT)
#define USBXHCI_CRCR_CRPTR_BITS     58

//
// 5.4.6 Device Context Base Address Array Pointer Register (DCBAAP)

// RW 512 bit aligned address of device context base address array
#define USBXHCI_DCBAAP_BIT          6
#define USBXHCI_DCBAAP_BITS         58
#define USBXHCI_DCBAAP_MASK         ((1UL<<USBXHCI_DCBAAP_BITS)-1)
#define USBXHCI_DCBAAP              (USBXHCI_DCBAAP_MASK<<USBXHCI_DCBAAP_BIT)
#define USBXHCI_DCBAAP_n(ptr)       ((uintptr_t)(ptr) & USBXHCI_DCBAAP)

//
// 5.4.7 Configure Register (CONFIG)

#define USBXHCI_CONFIG_MAXSLOTSEN_BIT   0
#define USBXHCI_CONFIG_MAXSLOTSEN_BITS  8
#define USBXHCI_CONFIG_U3E_BIT          8
#define USBXHCI_CONFIG_CIE_BIT          9

// RW Maximum device slots enabled, 0=all disabled, n=n slots enabled
#define USBXHCI_CONFIG_MAXSLOTSEN_BITS  8
#define USBXHCI_CONFIG_MAXSLOTSEN_MASK \
    ((1U<<USBXHCI_CONFIG_MAXSLOTSEN_BITS)-1)
#define USBXHCI_CONFIG_MAXSLOTSEN \
    (USBXHCI_CONFIG_MAXSLOTSEN_MASK<<USBXHCI_CONFIG_MAXSLOTSEN_BIT)
#define USBXHCI_CONFIG_MAXSLOTSEN_n(n) \
    ((n)<<USBXHCI_CONFIG_MAXSLOTSEN_BIT)

// RW U3 Entry Enable: 0=normal, 1=assert PLC flag on transition to the U3 state
#define USBXHCI_CONFIG_U3E              (1U<<USBXHCI_CONFIG_U3E_BIT)

// RW Configuration Information Enable: 1=enable
#define USBXHCI_CONFIG_CIE              (1U<<USBXHCI_CONFIG_CIE_BIT)

//
// 5.4.8 Port Status and Control Register (PORTSC)

#define USBXHCI_PORTSC_CCS_BIT      0
#define USBXHCI_PORTSC_PED_BIT      1
#define USBXHCI_PORTSC_OCA_BIT      3
#define USBXHCI_PORTSC_PR_BIT       4
#define USBXHCI_PORTSC_PLS_BIT      5
#define USBXHCI_PORTSC_PLS_BITS     4
#define USBXHCI_PORTSC_PP_BIT       9
#define USBXHCI_PORTSC_SPD_BIT      10
#define USBXHCI_PORTSC_PIC_BIT      14
#define USBXHCI_PORTSC_LWS_BIT      16
#define USBXHCI_PORTSC_CSC_BIT      17
#define USBXHCI_PORTSC_PEC_BIT      18
#define USBXHCI_PORTSC_WRC_BIT      19
#define USBXHCI_PORTSC_OCC_BIT      20
#define USBXHCI_PORTSC_PRC_BIT      21
#define USBXHCI_PORTSC_PLC_BIT      22
#define USBXHCI_PORTSC_CEC_BIT      23
#define USBXHCI_PORTSC_CAS_BIT      24
#define USBXHCI_PORTSC_WCE_BIT      25
#define USBXHCI_PORTSC_WDE_BIT      26
#define USBXHCI_PORTSC_WOE_BIT      27
#define USBXHCI_PORTSC_DR_BIT       30
#define USBXHCI_PORTSC_WPR_BIT      31

#define USBXHCI_PORTSC_PLS_MASK     ((1U<<USBXHCI_PORTSC_PLS_BITS)-1)

// ROS Current Connect Status, 0=no device, 1=device is connected
#define USBXHCI_PORTSC_CCS          (1U<<USBXHCI_PORTSC_CCS_BIT)

// RW1CS Port Enabled/Disabled: 1=enabled
#define USBXHCI_PORTSC_PED          (1U<<USBXHCI_PORTSC_PED_BIT)

// RO Overcurrent Active: 1=overcurrent
#define USBXHCI_PORTSC_OCA          (1U<<USBXHCI_PORTSC_OCA_BIT)

// RW1S Port Reset: 1=reset, xHC clears bit when reset is finished
#define USBXHCI_PORTSC_PR           (1U<<USBXHCI_PORTSC_PR_BIT)

// RWS Port Link State: 0=U0 state, 2=U2 state, 3=U3 state, 5=detect state,
// 10=compliance state, 15=resume state (page 311)
#define USBXHCI_PORTSC_PLS \
    (USBXHCI_PORTSC_PLS_MASK<<USBXHCI_PORTSC_PLS_BIT)
#define USBXHCI_PORTSC_PLS_n(n)     ((n)<<USBXHCI_PORTSC_PLS_BIT)

// RWS Port Power: 1=on
#define USBXHCI_PORTSC_PP           (1U<<USBXHCI_PORTSC_PP_BIT)

// ROS Port Speed: 0=undefined, 1-15=protocol speed ID
#define USBXHCI_PORTSC_SPD          (1U<<USBXHCI_PORTSC_SPD_BIT)

// RWS Port Indicator Control: 0=off, 1=amber, 2=green, 3=undefined
#define USBXHCI_PORTSC_PIC          (1U<<USBXHCI_PORTSC_PIC_BIT)

// RW Link State Write Strobe: 1=enable PLS field writes
#define USBXHCI_PORTSC_LWS          (1U<<USBXHCI_PORTSC_LWS_BIT)

// RW1CS Connect Status Change: 1=changed
#define USBXHCI_PORTSC_CSC          (1U<<USBXHCI_PORTSC_CSC_BIT)

// RW1CS Port Enable/Disable Change: 1=changed
#define USBXHCI_PORTSC_PEC          (1U<<USBXHCI_PORTSC_PEC_BIT)

// WRC Warm Reset Change: 1=reset complete
#define USBXHCI_PORTSC_WRC          (1U<<USBXHCI_PORTSC_WRC_BIT)

// RW1CS Overcurrent Change: 1=changed
#define USBXHCI_PORTSC_OCC          (1U<<USBXHCI_PORTSC_OCC_BIT)

// RW1CS Port Reset Change: 1=reset complete
#define USBXHCI_PORTSC_PRC          (1U<<USBXHCI_PORTSC_PRC_BIT)

// RW1CS Port Link State Change: 1=changed
#define USBXHCI_PORTSC_PLC          (1U<<USBXHCI_PORTSC_PLC_BIT)

// RW1CS/RsvdZ Port Config Error Change: 1=failed
#define USBXHCI_PORTSC_CEC          (1U<<USBXHCI_PORTSC_CEC_BIT)

// RO Cold Attach Status: 1=terminated
#define USBXHCI_PORTSC_CAS          (1U<<USBXHCI_PORTSC_CAS_BIT)

// RWS Wake on Connect Enable: 1=enable
#define USBXHCI_PORTSC_WCE          (1U<<USBXHCI_PORTSC_WCE_BIT)

// RWS Wake on Disconnect Enable: 1=enable
#define USBXHCI_PORTSC_WDE          (1U<<USBXHCI_PORTSC_WDE_BIT)

// RWS Wake on Overcurrent Enable: 1=enable
#define USBXHCI_PORTSC_WOE          (1U<<USBXHCI_PORTSC_WOE_BIT)

// RO Device Removable: 1=not removable
#define USBXHCI_PORTSC_DR           (1U<<USBXHCI_PORTSC_DR_BIT)

// EW1S/RsvdZ Warm Port Reset, 1=initiate warm reset
#define USBXHCI_PORTSC_WPR          (1U<<USBXHCI_PORTSC_WPR_BIT)

#define USBXHCI_USB3_PORTPMSC_U1TO_BIT  0
#define USBXHCI_USB3_PORTPMSC_U2TO_BIT  8
#define USBXHCI_USB3_PORTPMSC_U1TO_BITS 8
#define USBXHCI_USB3_PORTPMSC_U2TO_BITS 8
#define USBXHCI_USB3_PORTPMSC_FLA_BIT   16

// RWS U1 Timeout: n=timeout in microseconds
#define USBXHCI_USB3_PORTPMSC_U1TO_MASK \
    ((1U<<USBXHCI_USB3_PORTPMSC_U1TO_BITS)-1)
#define USBXHCI_USB3_PORTPMSC_U1TO_n(n) \
    ((n)<<USBXHCI_USB3_PORTPMSC_U1TO_BIT)
#define USBXHCI_USB3_PORTPMSC_U1TO \
    (USBXHCI_USB3_PORTPMSC_U1TO_MASK<<USBXHCI_USB3_PORTPMSC_U1TO_BIT)

// RWS U1 Timeout: n=timeout in microseconds
#define USBXHCI_USB3_PORTPMSC_U2TO_MASK \
    ((1U<<USBXHCI_USB3_PORTPMSC_U2TO_BITS)-1)
#define USBXHCI_USB3_PORTPMSC_U2TO_n(n) \
    ((n)<<USBXHCI_USB3_PORTPMSC_U2TO_BIT)
#define USBXHCI_USB3_PORTPMSC_U2TO \
    (USBXHCI_USB3_PORTPMSC_U2TO_MASK<<USBXHCI_USB3_PORTPMSC_U2TO_BITS)
// RW Force Link PM Accept (for testing)
#define USBXHCI_USB3_PORTPMSC_FLA       (1U<<USBXHCI_USB3_PORTPMSC_FLA_BIT)

#define USBXHCI_USB2_PORTPMSC_L1S_BIT   0
#define USBXHCI_USB2_PORTPMSC_L1S_BITS  3
#define USBXHCI_USB2_PORTPMSC_RWE_BIT   3
#define USBXHCI_USB2_PORTPMSC_BESL_BIT  4
#define USBXHCI_USB2_PORTPMSC_BESL_BITS 4
#define USBXHCI_USB2_PORTPMSC_L1DS_BIT  8
#define USBXHCI_USB2_PORTPMSC_L1DS_BITS 8
#define USBXHCI_USB2_PORTPMSC_HLE_BIT   16
#define USBXHCI_USB2_PORTPMSC_PTC_BIT   28
#define USBXHCI_USB2_PORTPMSC_PTC_BITS  4

// RO L1 Status, 0=ignore, 1=success, 2=not yet, 3=not supported, 4=timeout
#define USBXHCI_USB2_PORTPMSC_L1S \
    (1U<<USBXHCI_USB2_PORTPMSC_L1S_BIT)
#define USBXHCI_USB2_PORTPMSC_MASK \
    ((1U<<USBXHCI_USB2_PORTPMSC_L1S_BITS)-1)
#define USBXHCI_USB2_PORTPMSC_n(n) \
    ((n)<<USBXHCI_USB2_PORTPMSC_L1S_BIT)
#define USBXHCI_USB2_PORTPMSC \
    (USBXHCI_USB2_PORTPMSC_MASK<<USBXHCI_USB2_PORTPMSC_L1S_BITS)

// RWE Remote Wake Enable, 1=enable
#define USBXHCI_USB2_PORTPMSC_RWE       (1U<<USBXHCI_USB2_PORTPMSC_RWE_BIT)

// RW Best Effort Service Latency: 0=default
#define USBXHCI_USB2_PORTPMSC_BESL_MASK \
    ((1U<<USBXHCI_USB2_PORTPMSC_BESL_BITS)-1)
#define USBXHCI_USB2_PORTPMSC_BESL_n(n) \
    ((n)<<USBXHCI_USB2_PORTPMSC_BESL_BIT)
#define USBXHCI_USB2_PORTPMSC_BESL \
    (USBXHCI_USB2_PORTPMSC_BESL_MASK<<USBXHCI_USB2_PORTPMSC_BESL_BIT)

// RW L1 Device slot
#define USBXHCI_USB2_PORTPMSC_L1DS_MASK \
    ((1U<<USBXHCI_USB2_PORTPMSC_L1DS_BITS)-1)
#define USBXHCI_USB2_PORTPMSC_L1DS_n(n) \
    ((n)<<USBXHCI_USB2_PORTPMSC_L1DS_BIT)
#define USBXHCI_USB2_PORTPMSC_L1DS \
    (USBXHCI_USB2_PORTPMSC_L1DS_MASK<<USBXHCI_USB2_PORTPMSC_L1DS_BIT)

// RW Hardware LPM Enable 1=enable
#define USBXHCI_USB2_PORTPMSC_HLE       (1U<<USBXHCI_USB2_PORTPMSC_HLE_BIT)

// RW Port Test Control: 0=no test
#define USBXHCI_USB2_PORTPMSC_PTC_MASK \
    ((1U<<USBXHCI_USB2_PORTPMSC_PTC_BITS)-1)
#define USBXHCI_USB2_PORTPMSC_PTC_n(n)  ((n)<<USBXHCI_USB2_PORTPMSC_PTC_BIT)
#define USBXHCI_USB2_PORTPMSC_PTC \
    (USBXHCI_USB2_PORTPMSC_PTC_MASK<<USBXHCI_USB2_PORTPMSC_PTC_BIT)

#define USBXHCI_PORTLI_LEC_BIT  0
#define USBXHCI_PORTLI_LEC_BITS 16
#define USBXHCI_PORTLI_RLC_BIT  16
#define USBXHCI_PORTLI_RLC_BITS 4
#define USBXHCI_PORTLI_TLC_BIT  20
#define USBXHCI_PORTLI_TLC_BITS 4

// RO Link Error Count:
#define USBXHCI_PORTLI_LEC_MASK ((1U<<USBXHCI_PORTLI_LEC_BITS)-1)
#define USBXHCI_PORTLI_LEC_n(n) ((n)<<USBXHCI_PORTLI_LEC_BIT)
#define USBXHCI_PORTLI_LEC \
    (USBXHCI_PORTLI_LEC_MASK<<USBXHCI_PORTLI_LEC_BIT)

// RO Rx Lane Count
#define USBXHCI_PORTLI_RLC_MASK ((1U<<USBXHCI_PORTLI_RLC_BITS)-1)
#define USBXHCI_PORTLI_RLC_n(n) ((n)<<USBXHCI_PORTLI_RLC_BIT)
#define USBXHCI_PORTLI_RLC \
    (USBXHCI_PORTLI_RLC_MASK<<USBXHCI_PORTLI_RLC_BIT)

// RO Tx Lane Count
#define USBXHCI_PORTLI_TLC_MASK ((1U<<USBXHCI_PORTLI_TLC_BITS)-1)
#define USBXHCI_PORTLI_TLC_n(n) ((n)<<USBXHCI_PORTLI_TLC_BIT)
#define USBXHCI_PORTLI_TLC \
    (USBXHCI_PORTLI_TLC_MASK<<USBXHCI_PORTLI_TLC_BIT)

#define USBXHCI_PORTHLPMC_HIRDM_BIT     0
#define USBXHCI_PORTHLPMC_HIRDM_BITS    2
#define USBXHCI_PORTHLPMC_L1TO_BIT      2
#define USBXHCI_PORTHLPMC_L1TO_BITS     8
#define USBXHCI_PORTHLPMC_BESLD_BIT     10
#define USBXHCI_PORTHLPMC_BESLD_BITS    4

// RWS Host Initiated Resume Duration Mode
#define USBXHCI_PORTHLPMC_HIRDM_MASK    ((1U<<USBXHCI_PORTHLPMC_HIRDM_BITS)-1)
#define USBXHCI_PORTHLPMC_HIRDM_n(n)    ((n)<<USBXHCI_PORTHLPMC_HIRDM_BIT)
#define USBXHCI_PORTHLPMC_HIRDM \
    (USBXHCI_PORTHLPMC_HIRDM_MASK<<USBXHCI_PORTHLPMC_HIRDM_BIT)

// RWS L1 Timeout
#define USBXHCI_PORTHLPMC_L1TO_MASK     ((1U<<USBXHCI_PORTHLPMC_L1TO_BITS)-1)
#define USBXHCI_PORTHLPMC_L1TO_n(n)     ((n)<<USBXHCI_PORTHLPMC_L1TO_BIT)
#define USBXHCI_PORTHLPMC_L1TO \
    (USBXHCI_PORTHLPMC_L1TO_MASK<<USBXHCI_PORTHLPMC_L1TO_BIT)

// RWS Best Effort Service Latency Deep, n=how long to drive resume on exit from U2
#define USBXHCI_PORTHLPMC_BESLD_MASK    ((1U<<USBXHCI_PORTHLPMC_BESLD_BITS)-1)
#define USBXHCI_PORTHLPMC_BESLD_n(n)    ((n)<<USBXHCI_PORTHLPMC_BESLD_BIT)
#define USBXHCI_PORTHLPMC_BESLD \
    (USBXHCI_PORTHLPMC_BESLD_MASK<<USBXHCI_PORTHLPMC_BESLD_BIT)

// 6.2.2 Slot Context

typedef struct usbxhci_slotctx_t {
    uint32_t rsmhc;

    uint16_t max_exit_lat;
    uint8_t root_hub_num;
    uint8_t num_ports;

    uint8_t tthub_slotid;
    uint8_t ttportnum;
    uint16_t ttt_intrtarget;

    uint8_t usbdevaddr;
    uint8_t rsvd[2];
    uint8_t slotstate;

    uint32_t rsvd2[4];
} usbxhci_slotctx_t;

C_ASSERT(sizeof(usbxhci_slotctx_t) == 0x20);

#define USBXHCI_SLOTCTX_RSMHC_ROUTE_BIT         0
#define USBXHCI_SLOTCTX_RSMHC_ROUTE_BITS        20
#define USBXHCI_SLOTCTX_RSMHC_SPEED_BIT         20
#define USBXHCI_SLOTCTX_RSMHC_SPEED_BITS        4
#define USBXHCI_SLOTCTX_RSMHC_MTT_BIT           25
#define USBXHCI_SLOTCTX_RSMHC_HUB_BIT           26
#define USBXHCI_SLOTCTX_RSMHC_CTXENT_BIT        27
#define USBXHCI_SLOTCTX_RSMHC_CTXENT_BITS       4

#define USBXHCI_SLOTCTX_RSMHC_ROUTE_MASK \
    ((1U<<USBXHCI_SLOTCTX_RSMHC_ROUTE_BITS)-1)
#define USBXHCI_SLOTCTX_RSMHC_ROUTE_n(n) \
    ((n)<<USBXHCI_SLOTCTX_RSMHC_ROUTE_BIT)
#define USBXHCI_SLOTCTX_RSMHC_ROUTE \
    (USBXHCI_SLOTCTX_RSMHC_ROUTE_MASK<<USBXHCI_SLOTCTX_RSMHC_ROUTE_BIT)

#define USBXHCI_SLOTCTX_RSMHC_SPEED_MASK \
    ((1U<<USBXHCI_SLOTCTX_RSMHC_SPEED_BITS)-1)
#define USBXHCI_SLOTCTX_RSMHC_SPEED_n(n) \
    ((n)<<USBXHCI_SLOTCTX_RSMHC_SPEED_BIT)
#define USBXHCI_SLOTCTX_RSMHC_SPEED \
    (USBXHCI_SLOTCTX_RSMHC_SPEED_MASK<<USBXHCI_SLOTCTX_RSMHC_SPEED_BIT)

#define USBXHCI_SLOTCTX_RSMHC_CTXENT_MASK \
    ((1U<<USBXHCI_SLOTCTX_RSMHC_CTXENT_BITS)-1)
#define USBXHCI_SLOTCTX_RSMHC_CTXENT_n(n) \
    ((n)<<USBXHCI_SLOTCTX_RSMHC_CTXENT_BIT)
#define USBXHCI_SLOTCTX_RSMHC_CTXENT \
    (USBXHCI_SLOTCTX_RSMHC_CTXENT_MASK<<USBXHCI_SLOTCTX_RSMHC_CTXENT_BIT)

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

typedef struct usbxhci_ep_ctx_t {
    uint32_t ctx[5];
    uint32_t rsvd[3];
} usbxhci_ep_ctx_t;

C_ASSERT(sizeof(usbxhci_ep_ctx_t) == 0x20);

#define USBXHCI_EPCTX_0_EPSTATE_BIT         0
#define USBXHCI_EPCTX_0_EPSTATE_BITS        3
#define USBXHCI_EPCTX_0_MULT_BIT            8
#define USBXHCI_EPCTX_0_MULT_BITS           2
#define USBXHCI_EPCTX_0_MAXPSTREAMS_BIT     10
#define USBXHCI_EPCTX_0_MAXPSTREAMS_BITS    5
#define USBXHCI_EPCTX_0_LSA_BIT             15
#define USBXHCI_EPCTX_0_INTERVAL_BIT        16
#define USBXHCI_EPCTX_0_INTERVAL_BITS       8
#define USBXHCI_EPCTX_0_MAXESITHI_BIT       24
#define USBXHCI_EPCTX_0_MAXESITHI_BITS      8

#define USBXHCI_EPCTX_1_CERR_BIT            1
#define USBXHCI_EPCTX_1_CERR_BITS           2
#define USBXHCI_EPCTX_1_EPTYPE_BIT          3
#define USBXHCI_EPCTX_1_EPTYPE_BITS         3
#define USBXHCI_EPCTX_1_HID_BIT             7
#define USBXHCI_EPCTX_1_MAXBURST_BIT        8
#define USBXHCI_EPCTX_1_MAXBURST_BITS       8
#define USBXHCI_EPCTX_1_MAXPKT_BIT          16
#define USBXHCI_EPCTX_1_MAXPKT_BITS         16

#define USBXHCI_EPCTX_2_DCS_BIT             0
#define USBXHCI_EPCTX_2_DRQUEUEPTRLO_BIT    4
#define USBXHCI_EPCTX_2_DRQUEUEPTRLO_BITS   28

//#define USBXHCI_EPCTX_3_DRQUEUEPTRHI_BIT

#define USBXHCI_EPCTX_4_AVGTRBLEN_BIT       0
#define USBXHCI_EPCTX_4_AVGTRBLEN_BITS      16
#define USBXHCI_EPCTX_4_MAXESITLO_BIT       16
#define USBXHCI_EPCTX_4_MAXESITLO_BITS      16

// 6.2.1 Device Context

typedef struct usbxhci_devctx_t {
    usbxhci_slotctx_t slotctx;
    usbxhci_ep_ctx_t epctx[16];
} usbxhci_devctx_t;

// 6.4.3 Command TRB

typedef struct usbxhci_cmd_trb_t {
    uint32_t data[4];
} usbxhci_cmd_trb_t;

typedef struct usbxhci_cmd_trb_noop_t {
    uint32_t rsvd1[3];
    uint8_t cycle;
    uint8_t trb_type;
    uint16_t rsvd2;
} usbxhci_cmd_trb_noop_t;

#define USBXHCI_CMD_TRB_TYPE_BIT    2
#define USBXHCI_CMD_TRB_TYPE_BITS   6
#define USBXHCI_CMD_TRB_TYPE_MASK   ((1U<<USBXHCI_CMD_TRB_TYPE_BITS)-1)
#define USBXHCI_CMD_TRB_TYPE_n(n)   ((n)<<USBXHCI_CMD_TRB_TYPE_BIT)
#define USBXHCI_CMD_TRB \
    (USBXHCI_CMD_TRB_TYPE_MASK<<USBXHCI_CMD_TRB_TYPE_BIT)

// 6.5 Event Ring Segment Table

typedef struct usbxhci_evtring_seg_t {
    // Base address, must be 64-byte aligned
    uint64_t base;

    // Minimum count=16, maximum count=4096
    uint16_t trb_count;

    uint16_t resvd;
    uint32_t resvd2;
} __attribute__((packed)) usbxhci_evtring_seg_t;

C_ASSERT(sizeof(usbxhci_evtring_seg_t) == 0x10);

//
//

// 6.4.2 Event TRBs

typedef struct usbxhci_evt_t {
    uint32_t data[3];
    uint16_t flags;
    uint8_t id;
    uint8_t slotid;
} __attribute__((packed)) usbxhci_evt_t;

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

typedef struct usbxhci_evt_cmdcomp_t {
    uint64_t command_trb_ptr;
    uint32_t info;
    uint16_t flags;
    uint8_t id;
    uint8_t slotid;
} usbxhci_evt_cmdcomp_t;

// Command completion parameter
#define USBXHCI_EVT_CMDCOMP_INFO_CCP_BIT    0
#define USBXHCI_EVT_CMDCOMP_INFO_CCP_BITS   24
#define USBXHCI_EVT_CMDCOMP_INFO_CCP_MASK \
    ((1U<<USBXHCI_EVT_CMDCOMP_INFO_CCP_BITS)-1)
#define USBXHCI_EVT_CMDCOMP_INFO_CCP_n(n) \
    ((n)<<USBXHCI_EVT_CMDCOMP_INFO_CCP_BIT)
#define USBXHCI_EVT_CMDCOMP_INFO_CCP \
    (USBXHCI_EVT_CMDCOMP_INFO_CCP_MASK<<USBXHCI_EVT_CMDCOMP_INFO_CCP_BIT)

// Command completion code
#define USBXHCI_EVT_CMDCOMP_INFO_CC_BIT     24
#define USBXHCI_EVT_CMDCOMP_INFO_CC_BITS    8
#define USBXHCI_EVT_CMDCOMP_INFO_CC_MASK \
    ((1U<<USBXHCI_EVT_CMDCOMP_INFO_CC_BITS)-1)
#define USBXHCI_EVT_CMDCOMP_INFO_CC_n(n) \
    ((n)<<USBXHCI_EVT_CMDCOMP_INFO_CC_BIT)
#define USBXHCI_EVT_CMDCOMP_INFO_CC \
    (USBXHCI_EVT_CMDCOMP_INFO_CC_MASK<<USBXHCI_EVT_CMDCOMP_INFO_CC_BIT)

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

typedef struct usbxhci_evt_xfer_t {
    uint64_t trb_ptr;
    uint16_t len_lo;
    uint8_t len_hi;
    uint8_t cc;
    uint8_t flags;
    uint8_t trb_type;
    uint8_t ep_id;
    uint8_t slotid;
} usbxhci_evt_xfer_t;

//
// 6.4.2.2 Command Completion Event TRB

typedef struct usbxhci_evt_completion_t {
    uint64_t trb_ptr;
    uint16_t len_lo;
    uint8_t len_hi;
    uint8_t cc;
    uint8_t flags;
    uint8_t trb_type;
    uint8_t ep_id;
    uint8_t slotid;
} usbxhci_evt_completion_t;

//
// 6.4.2.3 Port Status Change Event TRB

typedef struct usbxhci_evt_portstchg_t {
    uint16_t rsvd1;
    uint8_t rsvd2;
    uint8_t portid;
    uint32_t rsvd3;
    uint16_t rsvd4;
    uint8_t cc;
    uint16_t flags;
    uint16_t rsvd5;
} usbxhci_evt_portstchg_t;

//
// 6.4.2.4 Bandwidth Request Event TRB

typedef struct usbxhci_evt_bwreq_t {
    uint32_t rsvd1;
    uint32_t rsvd2;
    uint16_t rsvd3;
    uint8_t rsvd4;
    uint8_t cc;
    uint16_t flags;
    uint8_t rsvd5;
    uint8_t slotid;
} usbxhci_evt_bwreq_t;

//
// 6.4.2.5 Doorbell Event TRB

typedef struct usbxhci_evt_db_t {
    uint8_t reason;
    uint8_t rsvd[10];
    uint8_t cc;
    uint16_t flags;
    uint8_t vf_id;
    uint8_t slotid;
} usbxhci_evt_db_t;

//
// Interface Interface

typedef struct usbxhci_device_vtbl_t {
    void (*init)(void);
} usbxhci_device_vtbl_t;

typedef struct usbxhci_interrupter_info_t {
    usbxhci_evt_t volatile *evt_ring;
    uint64_t evt_ring_physaddr;
    uint32_t next;
    uint32_t count;
    uint8_t ccs;
} usbxhci_interrupter_info_t;

typedef struct usbxhci_portinfo_t {
    usbxhci_evtring_seg_t volatile *dev_evt_segs;
    usbxhci_evt_t volatile *dev_evt_ring;
} usbxhci_portinfo_t;

typedef struct usbxhci_dev_t {
    usbxhci_device_vtbl_t *vtbl;

    uint64_t mmio_addr;

    usbxhci_capreg_t volatile *mmio_cap;
    usbxhci_opreg_t volatile *mmio_op;
    usbxhci_rtreg_t volatile *mmio_rt;
    usbxhci_dbreg_t volatile *mmio_db;

    uint64_t volatile *dev_ctx_ptrs;
    usbxhci_devctx_t volatile *dev_ctx;
    usbxhci_cmd_trb_t volatile *dev_cmd_ring;
    uint64_t cmd_ring_physaddr;

    usbxhci_evtring_seg_t volatile *dev_evt_segs;

    usbxhci_interrupter_info_t *interrupters;

    // Maximums
    uint32_t maxslots;
    uint32_t maxintr;

    // Next command slot
    uint32_t cr_next;

    // Command ring size
    uint32_t cr_size;

    usbxhci_portinfo_t *ports;
    unsigned port_count;

    uint8_t use_msi;

    // Producer Cycle State
    uint8_t pcs;

    pci_irq_range_t irq_range;
} usbxhci_dev_t;

static usbxhci_dev_t *usbxhci_devices;
static unsigned usbxhci_device_count;

static void usbxhci_init(usbxhci_dev_t *self)
{
    // 4.2 Host Controller Initialization

    // Stop the controller
    self->mmio_op->usbcmd &= ~USBXHCI_USBCMD_RUNSTOP;

    // Wait for controller to stop
    while (!(self->mmio_op->usbsts & USBXHCI_USBSTS_HCH))
        pause();

    // Reset the controller
    self->mmio_op->usbcmd |= USBXHCI_USBCMD_HCRST;

    // Wait for reset to complete
    while (self->mmio_op->usbcmd & USBXHCI_USBCMD_HCRST)
        pause();

    uint32_t hcsparams1 = self->mmio_cap->hcsparams1;
    self->maxslots = (hcsparams1 >>
                      USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_BIT) &
            USBXHCI_CAPREG_HCSPARAMS1_MAXDEVSLOTS_MASK;
    self->maxintr = (hcsparams1 >>
                     USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_BIT) &
            USBXHCI_CAPREG_HCSPARAMS1_MAXINTR_MASK;
    uint32_t maxports = (hcsparams1 >>
                    USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_BIT) &
            USBXHCI_CAPREG_HCSPARAMS1_MAXPORTS_MASK;

    self->mmio_op->config = (self->mmio_op->config &
            ~USBXHCI_CONFIG_MAXSLOTSEN) |
            USBXHCI_CONFIG_MAXSLOTSEN_n(self->maxslots);

    USBXHCI_TRACE("devslots=%d, maxintr=%d, maxports=%d\n",
                  self->maxslots, self->maxintr, maxports);

    // Program device context address array pointer
    self->dev_ctx_ptrs = mmap(0, sizeof(*self->dev_ctx) * self->maxslots,
                        PROT_READ | PROT_WRITE,
                        MAP_POPULATE, -1, 0);

    self->dev_ctx = mmap(0, sizeof(*self->dev_ctx) * self->maxslots,
                        PROT_READ | PROT_WRITE,
                        MAP_POPULATE, -1, 0);

    // Device Context Base Address Array
    for (size_t i = 0; i < self->maxslots; ++i)
        self->dev_ctx_ptrs[i] = mphysaddr(self->dev_ctx + i);

    // Device Context Base Address Array Pointer
    self->mmio_op->dcbaap = mphysaddr(self->dev_ctx_ptrs);

    // Command Ring
    self->dev_cmd_ring = mmap(0, sizeof(usbxhci_cmd_trb_t) * self->maxslots,
                              PROT_READ | PROT_WRITE,
                              MAP_POPULATE, -1, 0);

    self->cmd_ring_physaddr = mphysaddr(self->dev_cmd_ring);

    // Command Ring Control Register
    self->mmio_op->crcr = self->cmd_ring_physaddr;

    // Event segments
    self->dev_evt_segs = mmap(0, sizeof(*self->dev_evt_segs) *
                              self->maxintr * 4,
                             PROT_READ | PROT_WRITE,
                             MAP_POPULATE, -1, 0);

    self->interrupters = malloc(sizeof(*self->interrupters) * self->maxintr);

    for (size_t i = 0; i < self->maxintr; ++i) {
        // Initialize Consumer Cycle State
        self->interrupters[i].ccs = 1;

        // Event ring
        self->interrupters[i].evt_ring = mmap(0, 4096,
                                 PROT_READ | PROT_WRITE,
                                 MAP_POPULATE, -1, 0);
        self->interrupters[i].count =
                4096 / sizeof(*self->interrupters[i].evt_ring);
        self->interrupters[i].next = 0;

        self->interrupters[i].evt_ring_physaddr =
                mphysaddr(self->interrupters[i].evt_ring);

        self->dev_evt_segs[i*4].base =
                self->interrupters[i].evt_ring_physaddr;

        self->dev_evt_segs[i*4].trb_count =
                4096 / sizeof(*self->interrupters[i].evt_ring);

        // Event ring segment table size
        self->mmio_rt->ir[i].erstsz = 1;

        // Event ring dequeue pointer
        self->mmio_rt->ir[i].erdp = self->dev_evt_segs[i*4].base;

        // Event ring segment table base address
        self->mmio_rt->ir[i].erstba =
                mphysaddr((void*)(self->dev_evt_segs + i*4));

        // Set interrupt moderation rate (1000 * 250ns = 250us = 4000/sec)
        self->mmio_rt->ir[i].imod = 1000;

        // Enable interrupt
        self->mmio_rt->ir[i].iman = USBXHCI_INTR_IMAN_IE;
    }

    for (size_t i = 0; i < maxports; ++i) {
        if (self->mmio_op->ports[i].portsc & USBXHCI_PORTSC_CCS) {
            USBXHCI_TRACE("Device is connected to port %zd\n", i);

            // Reset the port
            self->mmio_op->ports[i].portsc |= USBXHCI_PORTSC_PR;

            USBXHCI_TRACE("Waiting for reset on port %zd\n", i);

            while (self->mmio_op->ports[i].portsc & USBXHCI_PORTSC_PR)
                pause();

            USBXHCI_TRACE("Reset finished on port %zd\n", i);
        }
    }

    self->mmio_op->usbcmd |= USBXHCI_USBCMD_INTE;

    self->mmio_op->usbcmd |= USBXHCI_USBCMD_RUNSTOP;

    // Initialize producer cycle state for command ring
    self->pcs = 1;
    self->cr_next = 0;

    for (size_t i = 0; i < maxports; ++i) {
        if (self->mmio_op->ports[i].portsc & USBXHCI_PORTSC_CCS) {
            usbxhci_cmd_trb_noop_t *s =
                    (void*)&self->dev_cmd_ring[self->cr_next++];

            memset(s, 0, sizeof(*s));
            s->cycle = self->pcs;
            s->trb_type = USBXHCI_CMD_TRB_TYPE_n(
                        USBXHCI_TRB_TYPE_ENABLESLOTCMD);
        }
    }

    // Ring controller command doorbell
    self->mmio_db[0] = 0;

    self->port_count = 0;
}

static void usbxhci_evt_handler(usbxhci_dev_t *self,
                                usbxhci_interrupter_info_t *ir_info,
                                usbxhci_intr_t volatile *ir,
                                usbxhci_evt_t volatile *evt,
                                size_t ii)
{
    (void)self;
    (void)ir_info;
    (void)ir;
    (void)ii;

    uint16_t type = ((evt->flags & USBXHCI_EVT_FLAGS_TYPE) >>
                     USBXHCI_EVT_FLAGS_TYPE_BIT);

    usbxhci_evt_cmdcomp_t volatile *cmdcomp;

    switch (type) {
    case USBXHCI_TRB_TYPE_XFEREVT:
        USBXHCI_TRACE("XFEREVT\n");
        break;

    case USBXHCI_TRB_TYPE_CMDCOMPEVT:
        cmdcomp = (void*)evt;
        uint64_t cmdaddr = cmdcomp->command_trb_ptr;
        uint32_t cmdidx = (cmdaddr - self->cmd_ring_physaddr) /
                sizeof(*self->dev_cmd_ring);
        uint32_t cc = ((cmdcomp->info & USBXHCI_EVT_CMDCOMP_INFO_CC) >>
                       USBXHCI_EVT_CMDCOMP_INFO_CC_BIT);
        uint32_t ccp = ((cmdcomp->info & USBXHCI_EVT_CMDCOMP_INFO_CCP) >>
                        USBXHCI_EVT_CMDCOMP_INFO_CCP_BIT);
        USBXHCI_TRACE("CMDCOMPEVT, cmdaddr=%lx, cmdidx=%d, "
                      "completion code=%x, parameter=%x\n",
                      cmdaddr, cmdidx, cc, ccp);
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
}

static void *usbxhci_irq_handler(int irq, void *ctx)
{
    (void)irq;
    USBXHCI_TRACE("IRQ!\n");

    for (size_t i = 0; i < usbxhci_device_count; ++i) {
        usbxhci_dev_t *self = usbxhci_devices + i;

        int irq_ofs = irq - self->irq_range.base;

        // Skip this device if it is not in the irq range
        if (irq_ofs < 0 || irq_ofs >= self->irq_range.count)
            continue;

        // Skip if interrupt is not pending
        if (!(self->mmio_op->usbsts & USBXHCI_USBSTS_EINT))
            continue;

        // Acknowledge the IRQ
        self->mmio_op->usbsts = USBXHCI_USBSTS_EINT;

        // The IRQ is the interrupter index % the number of IRQs
        for (size_t ii = irq_ofs; ii < self->maxintr;
             ii += self->irq_range.count) {
            usbxhci_interrupter_info_t *ir_info = self->interrupters + ii;
            usbxhci_intr_t volatile *ir = self->mmio_rt->ir + ii;

            if (ir->iman & USBXHCI_INTR_IMAN_IP) {
                // Interrupt is pending
                USBXHCI_TRACE("Interrupt pending on interrupter %zu\n", ii);

                // Acknowledge the interrupt
                ir->iman |= USBXHCI_INTR_IMAN_IP;

                while (!ir_info->ccs ==
                       !(ir_info->evt_ring[ir_info->next].flags &
                         USBXHCI_EVT_FLAGS_C)) {
                    usbxhci_evt_t volatile *evt = ir_info->evt_ring +
                            ir_info->next++;

                    if (ir_info->next >= ir_info->count) {
                        ir_info->next = 0;
                        ir_info->ccs = !ir_info->ccs;
                    }

                    usbxhci_evt_handler(self, ir_info, ir, evt, ii);
                }

                // Notify HC that we have consumed some events
                ir->erdp = ir_info->evt_ring_physaddr +
                        ir_info->next * sizeof(*ir_info->evt_ring);
            }
        }
    }

    return ctx;
}

void usbxhci_detect(void *arg);
void usbxhci_detect(void *arg)
{
    (void)arg;

    pci_dev_iterator_t pci_iter;

    if (!pci_enumerate_begin(
                &pci_iter,
                PCI_DEV_CLASS_SERIAL,
                PCI_SUBCLASS_SERIAL_USB))
        return;

    do {
        if (pci_iter.config.prog_if != PCI_PROGIF_SERIAL_USB_XHCI)
            continue;

        usbxhci_dev_t *dev_list = realloc(
                    usbxhci_devices, sizeof(*usbxhci_devices) *
                    (usbxhci_device_count + 1));
        if (!dev_list)
            panic("Out of memory!");
        usbxhci_devices = dev_list;

        usbxhci_dev_t *self = dev_list + usbxhci_device_count++;

        self->irq_range.base = pci_iter.config.irq_line;
        self->irq_range.count = 1;

        self->use_msi = pci_set_msi_irq(
                    pci_iter.bus, pci_iter.slot, pci_iter.func,
                    &self->irq_range, 1, 0, 1, usbxhci_irq_handler);

        if (!self->use_msi) {
            // Fall back to pin based IRQ
            irq_hook(self->irq_range.base, usbxhci_irq_handler);
            irq_setmask(self->irq_range.base, 1);
        }

        self->mmio_addr = (pci_iter.config.base_addr[0] & -16) |
                ((uint64_t)pci_iter.config.base_addr[1] << 32);

        self->mmio_cap = mmap((void*)(uintptr_t)self->mmio_addr,
                64<<10, PROT_READ | PROT_WRITE,
                MAP_PHYSICAL, -1, 0);

        self->mmio_op = (void*)((char*)self->mmio_cap +
                               self->mmio_cap->caplength);

        self->mmio_rt = (void*)((char*)self->mmio_cap +
                               (self->mmio_cap->rtsoff & -32));

        self->mmio_db = (void*)((char*)self->mmio_cap +
                               (self->mmio_cap->dboff & -4));

        self->mmio_db = self->mmio_db;

        usbxhci_init(self);
    } while (pci_enumerate_next(&pci_iter));
}

REGISTER_CALLOUT(usbxhci_detect, 0, 'U', "000");
