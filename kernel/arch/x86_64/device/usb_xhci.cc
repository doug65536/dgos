#include "usb_xhci.h"

#include "callout.h"
#include "pci.h"
#include "printk.h"
#include "stdlib.h"
#include "mm.h"
#include "cpu/atomic.h"
#include "string.h"
#include "hash_table.h"

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

// 5.3.6 HCCPARAMS1

#define USBXHCI_CAPREG_HCCPARAMS1_AC64_BIT      0
#define USBXHCI_CAPREG_HCCPARAMS1_BNC_BIT       1
#define USBXHCI_CAPREG_HCCPARAMS1_CSZ_BIT       2
#define USBXHCI_CAPREG_HCCPARAMS1_PPC_BIT       3
#define USBXHCI_CAPREG_HCCPARAMS1_PIND_BIT      4
#define USBXHCI_CAPREG_HCCPARAMS1_LHRC_BIT      5
#define USBXHCI_CAPREG_HCCPARAMS1_LTC_BIT       6
#define USBXHCI_CAPREG_HCCPARAMS1_NSS_BIT       7
#define USBXHCI_CAPREG_HCCPARAMS1_PAE_BIT       8
#define USBXHCI_CAPREG_HCCPARAMS1_SPC_BIT       9
#define USBXHCI_CAPREG_HCCPARAMS1_SEC_BIT       10
#define USBXHCI_CAPREG_HCCPARAMS1_CFC_BIT       11
#define USBXHCI_CAPREG_HCCPARAMS1_MAXPSASZ_BIT  12
#define USBXHCI_CAPREG_HCCPARAMS1_MAXPSASZ_BITS 4
#define USBXHCI_CAPREG_HCCPARAMS1_XECP_BIT      16
#define USBXHCI_CAPREG_HCCPARAMS1_XECP_BITS     16

// 64 bit addressing capability
#define USBXHCI_CAPREG_HCCPARAMS1_AC64 \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_AC64_BIT)

// Bandwidth Negotiation Capability
#define USBXHCI_CAPREG_HCCPARAMS1_BNC \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_BNC_BIT)

// Context Size
#define USBXHCI_CAPREG_HCCPARAMS1_CSZ \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_CSZ_BIT)

// Port Power Control
#define USBXHCI_CAPREG_HCCPARAMS1_PPC \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_PPC_BIT)

// Port Indicators
#define USBXHCI_CAPREG_HCCPARAMS1_PIND \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_PIND_BIT)

// Light HC Reset Capability
#define USBXHCI_CAPREG_HCCPARAMS1_LHRC \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_LHRC_BIT)

// Latency Tolerance Messaging Capability
#define USBXHCI_CAPREG_HCCPARAMS1_LTC \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_LTC_BIT)

// No Secondary SID Support
#define USBXHCI_CAPREG_HCCPARAMS1_NSS \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_NSS_BIT)

// Parse All Event Data
#define USBXHCI_CAPREG_HCCPARAMS1_PAE \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_PAE_BIT)

// Stopped Short Packet Capability
#define USBXHCI_CAPREG_HCCPARAMS1_SPC \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_SPC_BIT)

// Stopped EDTLA Capability
#define USBXHCI_CAPREG_HCCPARAMS1_SEC \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_SEC_BIT)

// Contiguous Frame ID Capability
#define USBXHCI_CAPREG_HCCPARAMS1_CFC \
    (1U<<USBXHCI_CAPREG_HCCPARAMS1_CFC_BIT)

// Maximum Primary Stream Array Size
#define USBXHCI_CAPREG_HCCPARAMS1_MAXPSASZ_MASK \
    ((1U<<USBXHCI_CAPREG_HCCPARAMS1_MAXPSASZ_BITS)-1)

#define USBXHCI_CAPREG_HCCPARAMS1_MAXPSASZ \
    (USBXHCI_CAPREG_HCCPARAMS1_MAXPSASZ_MASK<< \
    USBXHCI_CAPREG_HCCPARAMS1_MAXPSASZ_BIT)

#define USBXHCI_CAPREG_HCCPARAMS1_MAXPSASZ_n(n) \
    ((n)<<USBXHCI_CAPREG_HCCPARAMS1_MAXPSASZ_BIT)

// xHCI Extended Capabilities Pointer
#define USBXHCI_CAPREG_HCCPARAMS1_XECP_MASK \
    ((1U<<USBXHCI_CAPREG_HCCPARAMS1_XECP_BITS)-1)

#define USBXHCI_CAPREG_HCCPARAMS1_XECP \
    (USBXHCI_CAPREG_HCCPARAMS1_XECP_MASK<< \
    USBXHCI_CAPREG_HCCPARAMS1_XECP_BIT)

#define USBXHCI_CAPREG_HCCPARAMS1_XECP_n(n) \
    ((n)<<USBXHCI_CAPREG_HCCPARAMS1_XECP_BIT)

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
#define USBXHCI_PORTSC_SPD_BITS     4
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
#define USBXHCI_PORTSC_SPD_MASK     ((1U<<USBXHCI_PORTSC_SPD_BITS)-1)
#define USBXHCI_PORTSC_SPD_n(n)     ((n)<<USBXHCI_PORTSC_SPD_BIT)
#define USBXHCI_PORTSC_SPD \
    (USBXHCI_PORTSC_SPD_MASK<<USBXHCI_PORTSC_SPD_BIT)

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
} __attribute__((packed)) usbxhci_slotctx_t;

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
} __attribute__((packed)) usbxhci_ep_ctx_t;

C_ASSERT(sizeof(usbxhci_ep_ctx_t) == 0x20);

#define USBXHCI_EPCTX_MML_MULT_BIT          0
#define USBXHCI_EPCTX_MML_MULT_BITS         2
#define USBXHCI_EPCTX_MML_MAXPSTREAMS_BIT   2
#define USBXHCI_EPCTX_MML_MAXPSTREAMS_BITS  5
#define USBXHCI_EPCTX_MML_LSA_BIT           7

#define USBXHCI_EPCTX_CEH_CERR_BIT          1
#define USBXHCI_EPCTX_CEH_CERR_BITS         2
#define USBXHCI_EPCTX_CEH_EPTYPE_BIT        3
#define USBXHCI_EPCTX_CEH_EPTYPE_BITS       3
#define USBXHCI_EPCTX_CEH_HID_BIT           7

#define USBXHCI_EPCTX_TR_DQ_PTR_DCS_BIT     0
#define USBXHCI_EPCTX_TR_DQ_PTR_PTR_BIT     4
#define USBXHCI_EPCTX_TR_DQ_PTR_PTR_BITS    28

#define USBXHCI_EPCTX_MML_LSA               (1<<USBXHCI_EPCTX_MML_LSA_BIT)
#define USBXHCI_EPCTX_CEH_HID               (1<<USBXHCI_EPCTX_CEH_HID_BIT)
#define USBXHCI_EPCTX_2_DCS                 (1<<USBXHCI_EPCTX_2_DCS_BIT)

#define USBXHCI_EPCTX_MML_MULT_MASK         ((1U<<USBXHCI_EPCTX_MML_MULT_BITS)-1)
#define USBXHCI_EPCTX_MML_MULT_n(n)         ((n)<<USBXHCI_EPCTX_MML_MULT_BIT)
#define USBXHCI_EPCTX_MML_MULT \
    (USBXHCI_EPCTX_MML_MULT_MASK<<USBXHCI_EPCTX_MML_MULT_BIT)

#define USBXHCI_EPCTX_MML_MAXPSTREAMS_MASK \
    ((1U<<USBXHCI_EPCTX_MML_MAXPSTREAMS_BITS)-1)
#define USBXHCI_EPCTX_MML_MAXPSTREAMS_n(n) \
    ((n)<<USBXHCI_EPCTX_MML_MAXPSTREAMS_BIT)
#define USBXHCI_EPCTX_MML_MAXPSTREAMS \
    (USBXHCI_EPCTX_MML_MAXPSTREAMS_MASK<<USBXHCI_EPCTX_MML_MAXPSTREAMS_BIT)

#define USBXHCI_EPCTX_CEH_CERR_MASK \
    ((1U<<USBXHCI_EPCTX_CEH_CERR_BITS)-1)
#define USBXHCI_EPCTX_CEH_CERR_n(n) \
    ((n)<<USBXHCI_EPCTX_CEH_CERR_BIT)
#define USBXHCI_EPCTX_CEH_CERR \
    (USBXHCI_EPCTX_CEH_CERR_MASK<<USBXHCI_EPCTX_CEH_CERR_BIT)

#define USBXHCI_EPCTX_CEH_EPTYPE_MASK \
    ((1U<<USBXHCI_EPCTX_CEH_EPTYPE_BITS)-1)
#define USBXHCI_EPCTX_CEH_EPTYPE_n(n) \
    ((n)<<USBXHCI_EPCTX_CEH_EPTYPE_BIT)
#define USBXHCI_EPCTX_CEH_EPTYPE \
    (USBXHCI_EPCTX_CEH_EPTYPE_MASK<<USBXHCI_EPCTX_CEH_EPTYPE_BIT)

#define USBXHCI_EPCTX_TR_DQ_PTR_PTR_MASK \
    ((1U<<USBXHCI_EPCTX_TR_DQ_PTR_PTR_BITS)-1)
#define USBXHCI_EPCTX_TR_DQ_PTR_ptr(ptr) \
    ((n)&USBXHCI_EPCTX_TR_DQ_PTR_PTR_MASK)
#define USBXHCI_EPCTX_TR_DQ_PTR \
    (USBXHCI_EPCTX_TR_DQ_PTR_PTR_MASK<<USBXHCI_EPCTX_TR_DQ_PTR_PTR_BIT)

#define USBXHCI_EPTYPE_INVALID    0
#define USBXHCI_EPTYPE_ISOCHOUT   1
#define USBXHCI_EPTYPE_BULKOUT    2
#define USBXHCI_EPTYPE_INTROUT    3
#define USBXHCI_EPTYPE_CTLBIDIR   4
#define USBXHCI_EPTYPE_ISOCHIN    5
#define USBXHCI_EPTYPE_BULKIN     6
#define USBXHCI_EPTYPE_INTRIN     7

// 6.2.1 Device Context

typedef struct usbxhci_devctx_small_t {
    usbxhci_slotctx_t slotctx;
    usbxhci_ep_ctx_t epctx[16];
} __attribute__((packed)) usbxhci_devctx_small_t;

typedef struct usbxhci_devctx_large_t {
    usbxhci_slotctx_t slotctx;
    uint8_t rsvd[32];

    usbxhci_ep_ctx_t epctx[16];
} __attribute__((packed)) usbxhci_devctx_large_t;

typedef union usbxhci_devctx_t {
    void *any;
    usbxhci_devctx_small_t *small;
    usbxhci_devctx_large_t *large;
} usbxhci_devctx_t;

// 6.2.5.1 Input Control Context

typedef struct usbxhci_inpctlctx_t {
    uint32_t drop_bits;
    uint32_t add_bits;
    uint32_t rsvd[5];
    uint8_t cfg;
    uint8_t iface_num;
    uint8_t alternate;
    uint8_t rsvd2;
} __attribute__((packed)) usbxhci_inpctlctx_t;

// 6.2.5 Input Context

typedef struct usbxhci_inpctx_small_t {
    usbxhci_inpctlctx_t inpctl;

    usbxhci_slotctx_t slotctx;

    usbxhci_ep_ctx_t epctx[32];
} __attribute__((packed)) usbxhci_inpctx_small_t;

typedef struct usbxhci_inpctx_large_t {
    usbxhci_inpctlctx_t inpctl;

    usbxhci_slotctx_t slotctx;
    uint8_t rsvd[32];

    usbxhci_ep_ctx_t epctx[32];
} __attribute__((packed)) usbxhci_inpctx_large_t;

typedef union usbxhci_inpctx_t {
    void *any;
    usbxhci_inpctx_small_t *small;
    usbxhci_inpctx_large_t *large;
} usbxhci_inpctx_t;

// 6.4.3 Command TRB

typedef struct usbxhci_cmd_trb_t {
    uint32_t data[4];
} __attribute__((packed)) usbxhci_cmd_trb_t;

typedef struct usbxhci_cmd_trb_noop_t {
    uint32_t rsvd1[3];
    uint8_t cycle;
    uint8_t trb_type;
    uint16_t rsvd2;
} __attribute__((packed)) usbxhci_cmd_trb_noop_t;

#define USBXHCI_CMD_TRB_TYPE_BIT    2
#define USBXHCI_CMD_TRB_TYPE_BITS   6
#define USBXHCI_CMD_TRB_TYPE_MASK   ((1U<<USBXHCI_CMD_TRB_TYPE_BITS)-1)
#define USBXHCI_CMD_TRB_TYPE_n(n)   ((n)<<USBXHCI_CMD_TRB_TYPE_BIT)
#define USBXHCI_CMD_TRB \
    (USBXHCI_CMD_TRB_TYPE_MASK<<USBXHCI_CMD_TRB_TYPE_BIT)

typedef struct usbxhci_cmd_trb_setaddr_t {
    uint64_t input_ctx_physaddr;
    uint32_t rsvd;
    uint8_t cycle;
    uint8_t trb_type;
    uint8_t rsvd2;
    uint8_t slotid;
} __attribute__((packed)) usbxhci_cmd_trb_setaddr_t;

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
} __attribute__((packed)) usbxhci_evt_cmdcomp_t;

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
} __attribute__((packed)) usbxhci_evt_xfer_t;

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
} __attribute__((packed)) usbxhci_evt_completion_t;

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
} __attribute__((packed)) usbxhci_evt_portstchg_t;

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
} __attribute__((packed)) usbxhci_evt_bwreq_t;

//
// 6.4.2.5 Doorbell Event TRB

typedef struct usbxhci_evt_db_t {
    uint8_t reason;
    uint8_t rsvd[10];
    uint8_t cc;
    uint16_t flags;
    uint8_t vf_id;
    uint8_t slotid;
} __attribute__((packed)) usbxhci_evt_db_t;

//
// 6.4.1.2 Control TRBs

typedef struct usbxhci_ctl_trb_generic_t {
    uint32_t data[3];
    uint8_t flags;
    uint8_t trb_type;
    uint16_t trt;
} __attribute__((packed)) usbxhci_ctl_trb_generic_t;

C_ASSERT(sizeof(usbxhci_ctl_trb_generic_t) == 0x10);

// 6.4.1.2.1 Setup stage TRB

typedef struct usbxhci_ctl_trb_setup_t {
    uint8_t bm_req_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
    uint32_t xferlen_intr;
    uint8_t flags;
    uint8_t trb_type;
    uint16_t trt;
} __attribute__((packed)) usbxhci_ctl_trb_setup_t;

C_ASSERT(sizeof(usbxhci_ctl_trb_setup_t) == 0x10);

typedef struct usbxhci_ctl_trb_data_t {
    uint64_t data_physaddr;
    uint32_t xfer_td_intr;
    uint8_t flags;
    uint8_t trb_type;
    uint16_t dir;
} __attribute__((packed)) usbxhci_ctl_trb_data_t;

C_ASSERT(sizeof(usbxhci_ctl_trb_data_t) == 0x10);

typedef struct usbxhci_ctl_trb_status_t {
    uint64_t rsvd;
    uint16_t rsvd2;
    uint16_t intr;
    uint8_t flags;
    uint8_t trb_type;
    uint16_t dir;
} __attribute__((packed)) usbxhci_ctl_trb_status_t;

C_ASSERT(sizeof(usbxhci_ctl_trb_status_t) == 0x10);

typedef union usbxhci_ctl_trb_t {
    usbxhci_ctl_trb_generic_t generic;
    usbxhci_ctl_trb_setup_t setup;
    usbxhci_ctl_trb_data_t data;
    usbxhci_ctl_trb_status_t status;
} __attribute__((packed)) usbxhci_ctl_trb_t;

C_ASSERT(sizeof(usbxhci_ctl_trb_t) == 0x10);

#define USBXHCI_CTL_TRB_BMREQT_RECIP_BIT    0
#define USBXHCI_CTL_TRB_BMREQT_RECIP_BITS   5
#define USBXHCI_CTL_TRB_BMREQT_TYPE_BIT     5
#define USBXHCI_CTL_TRB_BMREQT_TYPE_BITS    2
#define USBXHCI_CTL_TRB_BMREQT_TOHOST_BIT   7

#define USBXHCI_CTL_TRB_BMREQT_TOHOST       (1U<<USBXHCI_CTL_TRB_BMREQT_TOHOST_BIT)

#define USBXHCI_CTL_TRB_BMREQT_RECIP_MASK   ((1U<<USBXHCI_CTL_TRB_BMREQT_RECIP_BITS)-1)
#define USBXHCI_CTL_TRB_BMREQT_RECIP_n(n)   ((n)<<USBXHCI_CTL_TRB_BMREQT_RECIP_BIT)
#define USBXHCI_CTL_TRB_BMREQT_RECIP \
    (USBXHCI_CTL_TRB_BMREQT_RECIP_MASK<<USBXHCI_CTL_TRB_BMREQT_RECIP_BIT)

#define USBXHCI_CTL_TRB_BMREQT_TYPE_MASK   ((1U<<USBXHCI_CTL_TRB_BMREQT_TYPE_BITS)-1)
#define USBXHCI_CTL_TRB_BMREQT_TYPE_n(n)   ((n)<<USBXHCI_CTL_TRB_BMREQT_TYPE_BIT)
#define USBXHCI_CTL_TRB_BMREQT_TYPE \
    (USBXHCI_CTL_TRB_BMREQT_TYPE_MASK<<USBXHCI_CTL_TRB_BMREQT_TYPE_BIT)

#define USBXHCI_CTL_TRB_BMREQT_TYPE_STD         0
#define USBXHCI_CTL_TRB_BMREQT_TYPE_CLASS       1
#define USBXHCI_CTL_TRB_BMREQT_TYPE_VENDOR      2

#define USBXHCI_CTL_TRB_BMREQT_RECIP_DEVICE     0
#define USBXHCI_CTL_TRB_BMREQT_RECIP_INTERFACE  1
#define USBXHCI_CTL_TRB_BMREQT_RECIP_ENDPOINT   2
#define USBXHCI_CTL_TRB_BMREQT_RECIP_OTHER      3
#define USBXHCI_CTL_TRB_BMREQT_RECIP_VENDOR     31

#define USBXHCI_CTL_TRB_FLAGS_C_BIT     0
#define USBXHCI_CTL_TRB_FLAGS_IOC_BIT   5
#define USBXHCI_CTL_TRB_FLAGS_IDT_BIT   6

// Cycle flag
#define USBXHCI_CTL_TRB_FLAGS_C         (1<<USBXHCI_CTL_TRB_FLAGS_C_BIT)

// Interrupt on completion
#define USBXHCI_CTL_TRB_FLAGS_IOC       (1<<USBXHCI_CTL_TRB_FLAGS_IOC_BIT)

// Immediate data (the pointer field actually contains
// up to 8 bytes of data, not a pointer)
#define USBXHCI_CTL_TRB_FLAGS_IDT       (1<<USBXHCI_CTL_TRB_FLAGS_IDT_BIT)

#define USBXHCI_CTL_TRB_TRB_TYPE_BIT    2
#define USBXHCI_CTL_TRB_TRB_TYPE_BITS   6

#define USBXHCI_CTL_TRB_TRB_TYPE_MASK   ((1U<<USBXHCI_CTL_TRB_TRB_TYPE_BITS)-1)
#define USBXHCI_CTL_TRB_TRB_TYPE_n(n)   ((n)<<USBXHCI_CTL_TRB_TRB_TYPE_BIT)
#define USBXHCI_CTL_TRB_TRB_TYPE \
    (USBXHCI_CTL_TRB_TRB_TYPE_MASK<<USBXHCI_CTL_TRB_TRB_TYPE_BIT)

#define USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_BIT    0
#define USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_BITS   17
#define USBXHCI_CTL_TRB_XFERLEN_INTR_INTR_BIT       22
#define USBXHCI_CTL_TRB_XFERLEN_INTR_INTR_BITS      10

#define USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_MASK \
    ((1U<<USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_BITS)-1)
#define USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_n(n) \
    ((n)<<USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_BIT)
#define USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN \
    (USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_MASK<< \
    USBXHCI_CTL_TRB_XFERLEN_INTR_XFERLEN_BIT)

#define USBXHCI_CTL_TRB_TRT_NODATA  0
#define USBXHCI_CTL_TRB_TRT_OUT     2
#define USBXHCI_CTL_TRB_TRT_IN      3

//
// USB Descriptor types (USB 3.1 spec, Table 9.5)

#define USB_RQCODE_
#define USB_RQCODE_GET_STATUS           0
#define USB_RQCODE_CLEAR_FEATURE        1
#define USB_RQCODE_SET_FEATURE          3
#define USB_RQCODE_SET_ADDRESS          5
#define USB_RQCODE_GET_DESCRIPTOR       6
#define USB_RQCODE_SET_DESCRIPTOR       7
#define USB_RQCODE_GET_CONFIGURATION    8
#define USB_RQCODE_SET_CONFIGURATION    9
#define USB_RQCODE_GET_INTERFACE        10
#define USB_RQCODE_SET_INTERFACE        11
#define USB_RQCODE_SYNCH_FRAME          12
#define USB_RQCODE_SET_ENCRYPTION       13
#define USB_RQCODE_GET_ENCRYPTION       14
#define USB_RQCODE_SET_HANDSHAKE        15
#define USB_RQCODE_GET_HANDSHAKE        16
#define USB_RQCODE_SET_CONNECTION       17
#define USB_RQCODE_SET_SECURITY_DATA    18
#define USB_RQCODE_GET_SECURITY_DATA    19
#define USB_RQCODE_SET_WUSB_DATA        20
#define USB_RQCODE_LOOPBACK_DATA_WRITE  21
#define USB_RQCODE_LOOPBACK_DATA_READ   22
#define USB_RQCODE_SET_INTERFACE_DS     23
#define USB_RQCODE_SET_SEL              48
#define USB_RQCODE_SET_ISOCH_DELAY      49

//
// Descriptor Types (USB 3.1 spec, Table 9-6)

#define USB_DESCTYPE_DEVICE                     1
#define USB_DESCTYPE_CONFIGURATION              2
#define USB_DESCTYPE_STRING                     3
#define USB_DESCTYPE_INTERFACE                  4
#define USB_DESCTYPE_ENDPOINT                   5
#define USB_DESCTYPE_INTERFACE_POWER            8
#define USB_DESCTYPE_OTG                        9
#define USB_DESCTYPE_DEBUG                      10
#define USB_DESCTYPE_INTERFACE_ASSOCIATION      11
#define USB_DESCTYPE_BOS                        15
#define USB_DESCTYPE_DEVICE_CAPABILITY          16
#define USB_DESCTYPE_SS_EP_COMPANION            48
#define USB_DESCTYPE_SSPLUS_ISOCH_EP_COMPANION  49

//
// USB Device Descriptor

typedef struct usb_desc_device {
    // Length of descriptor
    uint8_t len;

    // USB_DESCTYPE_*
    uint8_t desc_type;

    // USB spec, USB2.1=0x210, etc
    uint16_t usb_spec;

    // Device class, subclass, and protocol, else 0 if interface specific
    uint8_t dev_class;
    uint8_t dev_subclass;
    uint8_t dev_protocol;

    // 4=max 16 bytes, 12=max 4kb, 64=unlimited, etc
    uint8_t log2_maxpktsz;

    // Vendor, product, revision
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t product_ver;

    // Indices into string descriptor
    uint8_t manufact_stridx;
    uint8_t vendor_stridx;
    uint8_t serial_stridx;

    // Number of possible configurations
    uint8_t num_config;
} __attribute__((packed)) usb_desc_device;

C_ASSERT(sizeof(usb_desc_device) == 18);

//
// Doorbells

#define USBXHCI_DB_VAL_CTL_EP_UPD       1
#define USBXHCI_DB_VAL_OUT_EP_UPD_n(n)  ((n)*2+0)
#define USBXHCI_DB_VAL_IN_EP_UPD_n(n)   ((n)*2+1)

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

typedef struct usbxhci_endpoint_target_t {
    uint8_t slotid;
    uint8_t epid;
} usbxhci_endpoint_target_t;

typedef struct usbxhci_endpoint_data_t {
    usbxhci_endpoint_target_t target;

    usbxhci_evt_xfer_t *xfer_ring;
    uint64_t xfer_ring_physaddr;
    uint32_t xfer_next;
    uint32_t xfer_count;
    uint8_t ccs;
} usbxhci_endpoint_data_t;

typedef struct usbxhci_dev_t {
    usbxhci_device_vtbl_t *vtbl;

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

    spinlock_t endpoints_lock;
    usbxhci_endpoint_data_t **endpoints;
    uint32_t endpoint_count;

    // Endpoint data keyed on usbxhci_endpoint_target_t
    hashtbl_t endpoint_lookup;

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

    // 0 for 32 byte usbxhci_devctx_t, 1 for 64 byte usbxhci_devctx_large_t
    uint32_t dev_ctx_large;

    pci_irq_range_t irq_range;

    // Command issue lock
    spinlock_t lock_cmd;
} usbxhci_dev_t;

typedef struct usbxhci_pending_cmd_t usbxhci_pending_cmd_t;

typedef void (*usbxhci_complete_handler_t)(usbxhci_dev_t *self,
                                           usbxhci_cmd_trb_t *cmd,
                                           usbxhci_evt_t *evt,
                                           uintptr_t data);

struct usbxhci_pending_cmd_t {
    uint64_t cmd_physaddr;
    usbxhci_complete_handler_t handler;
    uintptr_t data;
};

typedef struct usbxhci_port_init_data_t {
    usbxhci_inpctx_t inpctx;
    usbxhci_ctl_trb_t *trbs;
    usb_desc_device descriptor;
    uint8_t port;
    uint8_t slotid;
    uint8_t current_descriptor;
    uint8_t descriptor_count;
} usbxhci_port_init_data_t;

static usbxhci_dev_t *usbxhci_devices;
static unsigned usbxhci_device_count;

static hashtbl_t usbxhci_pending_ht;

// Handle 32 or 64 byte device context size
static usbxhci_slotctx_t *usbxhci_dev_ctx_ent_slot(usbxhci_dev_t *self, size_t i)
{
    if (self->dev_ctx_large)
        return &self->dev_ctx.large[i].slotctx;
    return &self->dev_ctx.small[i].slotctx;
}

// Handle 32 or 64 byte device context size
__attribute__((used))
static usbxhci_ep_ctx_t *usbxhci_dev_ctx_ent_ep(usbxhci_dev_t *self,
                                                size_t slot, size_t i)
{
    if (self->dev_ctx_large)
        return &self->dev_ctx.large[slot].epctx[i];
    return &self->dev_ctx.small[slot].epctx[i];
}

static void usbxhci_ring_doorbell(usbxhci_dev_t *self,
                                  uint32_t doorbell, uint8_t value,
                                  uint16_t stream_id)
{
    self->mmio_db[doorbell] = value | (stream_id << 16);
}

static void usbxhci_insert_pending_command(
        uint64_t cmd_physaddr,
        usbxhci_complete_handler_t handler,
        uintptr_t data)
{
    usbxhci_pending_cmd_t *pc = (usbxhci_pending_cmd_t *)
            malloc(sizeof(usbxhci_pending_cmd_t));
    pc->cmd_physaddr = cmd_physaddr;
    pc->handler = handler;
    pc->data = data;
    htbl_insert(&usbxhci_pending_ht, pc);
}

static void usbxhci_issue_cmd(usbxhci_dev_t *self, void *cmd,
                       usbxhci_complete_handler_t handler,
                       uintptr_t data)
{
    spinlock_lock_noirq(&self->lock_cmd);

    usbxhci_cmd_trb_t *s =
            (usbxhci_cmd_trb_t *)&self->dev_cmd_ring[self->cr_next++];

    memcpy(s, cmd, sizeof(*s));

    size_t offset = (uint64_t)s - (uint64_t)self->dev_cmd_ring;

    usbxhci_insert_pending_command(self->cmd_ring_physaddr + offset,
                                   handler, data);

    spinlock_unlock_noirq(&self->lock_cmd);
}

static void usbxhci_add_xfer_trbs(usbxhci_dev_t *self,
                                  uint8_t slotid,
                                  size_t count,
                                  usbxhci_ctl_trb_t *trbs,
                                  usbxhci_complete_handler_t handler,
                                  uintptr_t handler_data)
{
    //usbxhci_slotctx_t *slot = usbxhci_dev_ctx_ent_slot(self, slotid);
    //usbxhci_ep_ctx_t *ep = usbxhci_dev_ctx_ent_ep(self, slotid, 0);

    usbxhci_endpoint_target_t ept;
    ept.slotid = slotid;
    ept.epid = 0;

    usbxhci_endpoint_data_t *epd = (usbxhci_endpoint_data_t *)
            htbl_lookup(&self->endpoint_lookup, &ept);

    for (size_t i = 0; i < count; ++i) {
        USBXHCI_TRACE("Writing TRB to %zx\n",
                      mphysaddr(&epd->xfer_ring[epd->xfer_next]));
        trbs[i].generic.flags |= (!!epd->ccs) << USBXHCI_CTL_TRB_FLAGS_C_BIT;
        memcpy(&epd->xfer_ring[epd->xfer_next], trbs + i, sizeof(*trbs));

        if (handler && ((i + 1) == count)) {
            usbxhci_insert_pending_command(
                        epd->xfer_ring_physaddr + epd->xfer_next *
                        sizeof(*epd->xfer_ring), handler, handler_data);
        }

        ++epd->xfer_next;
    }
}

static usbxhci_ctl_trb_t *usbxhci_make_setup_trbs(
        void *response, uint16_t length,
        uint8_t bmreq_type, uint8_t bmreq_recip,
        uint8_t request, uint16_t value, uint16_t index)
{
    usbxhci_ctl_trb_t *trbs = (usbxhci_ctl_trb_t *)
            calloc(3, sizeof(*trbs));

    trbs[0].setup.trb_type = USBXHCI_CTL_TRB_TRB_TYPE_n(
                USBXHCI_TRB_TYPE_SETUP);
    trbs[0].setup.trt = USBXHCI_CTL_TRB_TRT_IN;
    trbs[0].setup.bm_req_type =
            USBXHCI_CTL_TRB_BMREQT_RECIP_n(
                bmreq_recip) |
            USBXHCI_CTL_TRB_BMREQT_TYPE_n(
                bmreq_type) |
            USBXHCI_CTL_TRB_BMREQT_TOHOST;
    trbs[0].setup.request = request;
    trbs[0].setup.value = value;
    trbs[0].setup.index = index;
    trbs[0].setup.length = length;
    trbs[0].setup.xferlen_intr = 8;
    trbs[0].setup.flags = USBXHCI_CTL_TRB_FLAGS_IDT;

    trbs[1].data.trb_type = USBXHCI_CTL_TRB_TRB_TYPE_n(
                USBXHCI_TRB_TYPE_DATA);
    trbs[1].data.dir = 1;
    trbs[1].data.xfer_td_intr = length;
    trbs[1].data.data_physaddr = mphysaddr(response);

    trbs[2].status.trb_type = USBXHCI_CTL_TRB_TRB_TYPE_n(
                USBXHCI_TRB_TYPE_STATUS);
    trbs[2].status.dir = 0;
    trbs[2].status.flags = USBXHCI_CTL_TRB_FLAGS_IOC;

    return trbs;
}

static usbxhci_ctl_trb_t *usbxhci_get_descriptor(
        usbxhci_dev_t *self, usb_desc_device *desc,
        uint8_t desc_index, uint8_t slotid,
        usbxhci_complete_handler_t handler, uintptr_t handler_data)
{
    usbxhci_ctl_trb_t *trbs = usbxhci_make_setup_trbs(
                desc, sizeof(*desc),
                USBXHCI_CTL_TRB_BMREQT_TYPE_STD,
                USBXHCI_CTL_TRB_BMREQT_RECIP_DEVICE,
                USB_RQCODE_GET_DESCRIPTOR,
                (USB_DESCTYPE_DEVICE << 8) | desc_index, 0);

    usbxhci_add_xfer_trbs(self, slotid, 3, trbs,
                          handler, handler_data);

    usbxhci_ring_doorbell(self, 1, USBXHCI_DB_VAL_CTL_EP_UPD, 0);

    return trbs;
}

static usbxhci_ctl_trb_t *usbxhci_get_config(
        usbxhci_dev_t *self, usb_desc_device *desc,
        uint8_t desc_index, uint8_t slotid,
        usbxhci_complete_handler_t handler, uintptr_t handler_data)
{
    usbxhci_ctl_trb_t *trbs = usbxhci_make_setup_trbs(
                desc, sizeof(*desc),
                USBXHCI_CTL_TRB_BMREQT_TYPE_STD,
                USBXHCI_CTL_TRB_BMREQT_RECIP_DEVICE,
                USB_RQCODE_GET_DESCRIPTOR,
                (USB_DESCTYPE_DEVICE << 8) | desc_index, 0);

    usbxhci_add_xfer_trbs(self, slotid, 3, trbs,
                          handler, handler_data);

    usbxhci_ring_doorbell(self, USBXHCI_DB_VAL_OUT_EP_UPD_n(slotid),
                          USBXHCI_DB_VAL_CTL_EP_UPD, 0);

    return trbs;
}

static void usbxhci_get_descriptor_handler(
        usbxhci_dev_t *self, usbxhci_cmd_trb_t *cmd,
        usbxhci_evt_t *evt, uintptr_t data)
{
    usbxhci_port_init_data_t *init_data = (usbxhci_port_init_data_t*)data;
    usbxhci_evt_xfer_t *xferevt = (usbxhci_evt_xfer_t*)evt;
    xferevt->len_hi = xferevt->len_hi;

    USBXHCI_TRACE("Device descriptor: \n");

    if (init_data->descriptor_count == 0) {
        init_data->descriptor_count = init_data->descriptor.num_config;
        init_data->current_descriptor = 1;
    } else {
        ++init_data->current_descriptor;
    }

    if (init_data->descriptor.dev_class == 0) {
        free(init_data->trbs);
        init_data->trbs = usbxhci_get_config(
                    self, &init_data->descriptor,
                    init_data->current_descriptor - 1,
                    init_data->slotid,
                    usbxhci_get_descriptor_handler, (uintptr_t)init_data);
    }

    (void)xferevt;
    (void)self;
    (void)cmd;
    (void)data;
}

static void usbxhci_cmd_comp_setaddr(
        usbxhci_dev_t *self, usbxhci_cmd_trb_t *cmd,
        usbxhci_evt_t *evt, uintptr_t data)
{
    usbxhci_port_init_data_t *init_data = (usbxhci_port_init_data_t*)data;

    usbxhci_evt_cmdcomp_t *cmdcomp = (usbxhci_evt_cmdcomp_t*)evt;

    uint32_t cc = ((cmdcomp->info & USBXHCI_EVT_CMDCOMP_INFO_CC) >>
                   USBXHCI_EVT_CMDCOMP_INFO_CC_BIT);
    uint32_t ccp = ((cmdcomp->info & USBXHCI_EVT_CMDCOMP_INFO_CCP) >>
                    USBXHCI_EVT_CMDCOMP_INFO_CCP_BIT);
    USBXHCI_TRACE("setaddr completed: completion code=%x, "
                  "parameter=%x, slotid=%d\n", cc, ccp, cmdcomp->slotid);

    if (self->dev_ctx_large) {
        munmap(init_data->inpctx.large, sizeof(*init_data->inpctx.large));
    } else {
        munmap(init_data->inpctx.small, sizeof(*init_data->inpctx.small));
    }
    init_data->inpctx.any = 0;

    memset(&init_data->descriptor, 0, sizeof(init_data->descriptor));

    init_data->trbs = usbxhci_get_descriptor(
                self, &init_data->descriptor, 0, init_data->slotid,
                usbxhci_get_descriptor_handler, (uintptr_t)init_data);

    (void)cmd;
}

static usbxhci_endpoint_data_t *usbxhci_add_endpoint(
        usbxhci_dev_t * self, uint8_t slotid, uint8_t epid)
{
    usbxhci_endpoint_data_t *newepd = (usbxhci_endpoint_data_t*)
            malloc(sizeof(*newepd));
    if (!newepd)
        return 0;

    newepd->target.slotid = slotid;
    newepd->target.epid = epid;

    spinlock_lock_noirq(&self->endpoints_lock);

    usbxhci_endpoint_data_t **new_endpoints = (usbxhci_endpoint_data_t **)
            realloc(self->endpoints,
                    sizeof(*self->endpoints) *
                    (self->endpoint_count + 1));
    if (unlikely(!new_endpoints)) {
        spinlock_unlock_noirq(&self->endpoints_lock);
        free(newepd);
        return 0;
    }

    self->endpoints = new_endpoints;
    self->endpoints[self->endpoint_count++] = newepd;

    newepd->xfer_next = 0;
    newepd->xfer_count = PAGESIZE / sizeof(*newepd->xfer_ring);
    newepd->xfer_ring = (usbxhci_evt_xfer_t *)
            mmap(0, sizeof(*newepd->xfer_ring) * newepd->xfer_count,
                 PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
    if (unlikely(newepd->xfer_ring == MAP_FAILED || !newepd->xfer_ring)) {
        free(self->endpoints[--self->endpoint_count]);
        spinlock_lock_noirq(&self->endpoints_lock);
        return 0;
    }
    newepd->ccs = 1;

    uint64_t xfer_ring_physaddr = mphysaddr(newepd->xfer_ring);

    USBXHCI_TRACE("Transfer ring physical address for slot=%d, ep=%d: %lx\n",
                  slotid, epid, xfer_ring_physaddr);

    newepd->xfer_ring_physaddr = xfer_ring_physaddr;

    htbl_insert(&self->endpoint_lookup, newepd);

    spinlock_unlock_noirq(&self->endpoints_lock);

    return newepd;
}

static void usbxhci_cmd_comp_enableslot(usbxhci_dev_t * self,
                                        usbxhci_cmd_trb_t * cmd,
                                        usbxhci_evt_t * evt,
                                        uintptr_t data)
{
    usbxhci_port_init_data_t *init_data = (usbxhci_port_init_data_t*)data;

    usbxhci_cmd_trb_noop_t *escmd = (usbxhci_cmd_trb_noop_t*)cmd;
    (void)escmd;

    usbxhci_evt_cmdcomp_t *cmdcomp = (usbxhci_evt_cmdcomp_t*)evt;

    init_data->slotid = cmdcomp->slotid;

    uint32_t cc = ((cmdcomp->info & USBXHCI_EVT_CMDCOMP_INFO_CC) >>
                   USBXHCI_EVT_CMDCOMP_INFO_CC_BIT);
    uint32_t ccp = ((cmdcomp->info & USBXHCI_EVT_CMDCOMP_INFO_CCP) >>
                    USBXHCI_EVT_CMDCOMP_INFO_CCP_BIT);
    USBXHCI_TRACE("enableslot completed: completion code=%x, "
                  "parameter=%x, slotid=%d\n", cc, ccp, cmdcomp->slotid);

    // Create a new device context
    usbxhci_slotctx_t *ctx;

    ctx = usbxhci_dev_ctx_ent_slot(self, cmdcomp->slotid);

    memset(ctx, 0, sizeof(*ctx));

    // Issue a SET_ADDRESS command

    // Allocate an input context
    usbxhci_inpctx_t inp;
    usbxhci_inpctlctx_t *ctlctx;
    usbxhci_slotctx_t *inpslotctx;
    usbxhci_ep_ctx_t *inpepctx;

    if (self->dev_ctx_large) {
        inp.any = mmap(0, sizeof(usbxhci_inpctx_large_t),
                   PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
        ctlctx = &inp.large->inpctl;
        inpslotctx = &inp.large->slotctx;
        inpepctx = inp.large->epctx;
    } else {
        inp.any = mmap(0, sizeof(usbxhci_inpctx_small_t),
                   PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
        ctlctx = &inp.small->inpctl;
        inpslotctx = &inp.small->slotctx;
        inpepctx = inp.small->epctx;
    }

    init_data->inpctx = inp;

    ctlctx->add_bits = 3;

    usbxhci_portreg_t *pr = (usbxhci_portreg_t*)
            (self->mmio_op->ports + init_data->port);

    // Get speed of port
    uint8_t speed = ((pr->portsc & USBXHCI_PORTSC_SPD) >>
                     USBXHCI_PORTSC_SPD_BIT);

    inpslotctx->rsmhc = USBXHCI_SLOTCTX_RSMHC_ROUTE_n(0) |
            USBXHCI_SLOTCTX_RSMHC_SPEED_n(speed) |
            USBXHCI_SLOTCTX_RSMHC_CTXENT_n(1);

    // Max wakeup latency
    inpslotctx->max_exit_lat = 0;

    // Number of ports on hub (0 = not a hub)
    inpslotctx->num_ports = 0;

    // Root hub port number
    inpslotctx->root_hub_port_num = init_data->port;

    // Device address
    inpslotctx->usbdevaddr = 0;

    // 6.2.3.1 Input endpoint context 0
    inpepctx->ceh = USBXHCI_EPCTX_CEH_EPTYPE_n(USBXHCI_EPTYPE_CTLBIDIR);

    usbxhci_endpoint_data_t *epdata =
            usbxhci_add_endpoint(self, cmdcomp->slotid, 0);

    inpepctx->tr_dq_ptr = epdata->xfer_ring_physaddr | epdata->ccs;

    usbxhci_cmd_trb_setaddr_t setaddr;
    memset(&setaddr, 0, sizeof(setaddr));
    setaddr.input_ctx_physaddr = mphysaddr(inp.any);
    setaddr.cycle = self->pcs;
    setaddr.trb_type = USBXHCI_CMD_TRB_TYPE_n(
                USBXHCI_TRB_TYPE_ADDRDEVCMD);
    setaddr.slotid = evt->slotid;
    usbxhci_issue_cmd(self, &setaddr,
                      usbxhci_cmd_comp_setaddr, (uintptr_t)init_data);
    usbxhci_ring_doorbell(self, 0, 0, 0);
}

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

    self->dev_ctx_large = !!(self->mmio_cap->hccparams1 &
                             USBXHCI_CAPREG_HCCPARAMS1_CSZ);

    self->mmio_op->config = (self->mmio_op->config &
            ~USBXHCI_CONFIG_MAXSLOTSEN) |
            USBXHCI_CONFIG_MAXSLOTSEN_n(self->maxslots);

    USBXHCI_TRACE("devslots=%d, maxintr=%d, maxports=%d\n",
                  self->maxslots, self->maxintr, maxports);

    size_t dev_ctx_size = self->dev_ctx_large
            ? sizeof(usbxhci_devctx_large_t)
            : sizeof(usbxhci_devctx_small_t);

    // Program device context address array pointer
    self->dev_ctx_ptrs = (uint64_t*)
            mmap(0, sizeof(*self->dev_ctx_ptrs) * self->maxslots,
                 PROT_READ | PROT_WRITE,
                 MAP_POPULATE, -1, 0);

    self->dev_ctx.any = mmap(0, dev_ctx_size * self->maxslots,
                        PROT_READ | PROT_WRITE,
                        MAP_POPULATE, -1, 0);

    // Device Context Base Address Array
    for (size_t i = 0; i < self->maxslots; ++i) {
        self->dev_ctx_ptrs[i] = mphysaddr(usbxhci_dev_ctx_ent_slot(self, i));
    }

    // Device Context Base Address Array Pointer
    self->mmio_op->dcbaap = mphysaddr(self->dev_ctx_ptrs);

    // Command Ring
    self->dev_cmd_ring = (usbxhci_cmd_trb_t*)
            mmap(0, sizeof(usbxhci_cmd_trb_t) * self->maxslots,
                 PROT_READ | PROT_WRITE,
                 MAP_POPULATE, -1, 0);

    self->cmd_ring_physaddr = mphysaddr(self->dev_cmd_ring);

    // Command Ring Control Register
    self->mmio_op->crcr = self->cmd_ring_physaddr;

    // Event segments
    self->dev_evt_segs = (usbxhci_evtring_seg_t*)
            mmap(0, sizeof(*self->dev_evt_segs) *
                 self->maxintr * 4,
                 PROT_READ | PROT_WRITE,
                 MAP_POPULATE, -1, 0);

    self->interrupters = (usbxhci_interrupter_info_t*)
            malloc(sizeof(*self->interrupters) * self->maxintr);

    for (size_t i = 0; i < self->maxintr; ++i) {
        // Initialize Consumer Cycle State
        self->interrupters[i].ccs = 1;

        // Event ring
        self->interrupters[i].evt_ring = (usbxhci_evt_t*)
                mmap(0, PAGESIZE, PROT_READ | PROT_WRITE,
                     MAP_POPULATE, -1, 0);
        self->interrupters[i].count =
                PAGESIZE / sizeof(*self->interrupters[i].evt_ring);
        self->interrupters[i].next = 0;

        self->interrupters[i].evt_ring_physaddr =
                mphysaddr(self->interrupters[i].evt_ring);

        self->dev_evt_segs[i*4].base =
                self->interrupters[i].evt_ring_physaddr;

        self->dev_evt_segs[i*4].trb_count =
                PAGESIZE / sizeof(*self->interrupters[i].evt_ring);

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
            usbxhci_cmd_trb_noop_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.cycle = self->pcs;
            cmd.trb_type = USBXHCI_CMD_TRB_TYPE_n(
                        USBXHCI_TRB_TYPE_ENABLESLOTCMD);

            usbxhci_port_init_data_t *init_data = (usbxhci_port_init_data_t*)
                    calloc(1, sizeof(*init_data));

            init_data->port = i + 1;

            usbxhci_issue_cmd(self, &cmd,
                              usbxhci_cmd_comp_enableslot,
                              (uintptr_t)init_data);
        }
    }

    // Ring controller command doorbell
    usbxhci_ring_doorbell(self, 0, 0, 0);

    self->port_count = 0;
}

static void usbxhci_evt_handler(usbxhci_dev_t *self,
                                usbxhci_interrupter_info_t *ir_info,
                                usbxhci_intr_t *ir,
                                usbxhci_evt_t *evt,
                                size_t ii)
{
    (void)self;
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
        USBXHCI_TRACE("XFEREVT\n");
        xfer = (usbxhci_evt_xfer_t*)evt;
        cmdaddr = xfer->trb_ptr;
        break;

    case USBXHCI_TRB_TYPE_CMDCOMPEVT:
        cmdcomp = (usbxhci_evt_cmdcomp_t*)evt;
        cmdaddr = cmdcomp->command_trb_ptr;

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
    spinlock_lock_noirq(&self->lock_cmd);
    usbxhci_pending_cmd_t *pcp = (usbxhci_pending_cmd_t*)
            htbl_lookup(&usbxhci_pending_ht, &cmdaddr);
    assert(pcp);
    usbxhci_pending_cmd_t pc = *pcp;
    htbl_delete(&usbxhci_pending_ht, &cmdaddr);
    uint64_t cmd_physaddr = pcp->cmd_physaddr;
    free(pcp);
    spinlock_unlock_noirq(&self->lock_cmd);

    // Invoke completion handler
    pc.handler(self, (usbxhci_cmd_trb_t*)((char*)self->dev_cmd_ring +
               (cmd_physaddr - self->cmd_ring_physaddr)),
               (usbxhci_evt_t*)evt, pc.data);
}

static isr_context_t*usbxhci_irq_handler(int irq, isr_context_t *ctx)
{
    (void)irq;
    USBXHCI_TRACE("IRQ %d!\n", irq);

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
            usbxhci_intr_t *ir = (usbxhci_intr_t*)(self->mmio_rt->ir + ii);

            if (ir->iman & USBXHCI_INTR_IMAN_IP) {
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

        usbxhci_dev_t *dev_list = (usbxhci_dev_t*)realloc(
                    usbxhci_devices, sizeof(*usbxhci_devices) *
                    (usbxhci_device_count + 1));
        if (!dev_list)
            panic("Out of memory!");
        usbxhci_devices = dev_list;

        usbxhci_dev_t *self = dev_list + usbxhci_device_count++;

        memset(self, 0, sizeof(*self));

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

        self->mmio_cap = (usbxhci_capreg_t*)
                mmap((void*)(uintptr_t)self->mmio_addr,
                     64<<10, PROT_READ | PROT_WRITE,
                     MAP_PHYSICAL, -1, 0);

        self->mmio_op = (usbxhci_opreg_t*)((char*)self->mmio_cap +
                               self->mmio_cap->caplength);

        self->mmio_rt = (usbxhci_rtreg_t*)((char*)self->mmio_cap +
                               (self->mmio_cap->rtsoff & -32));

        self->mmio_db = (usbxhci_dbreg_t*)((char*)self->mmio_cap +
                               (self->mmio_cap->dboff & -4));

        self->mmio_db = self->mmio_db;

        // Pending commands hash table
        htbl_create(&usbxhci_pending_ht,
                    offsetof(usbxhci_pending_cmd_t, cmd_physaddr),
                    sizeof(uint64_t));

        // Endpoint lookup hash table
        htbl_create(&self->endpoint_lookup,
                    offsetof(usbxhci_endpoint_data_t, target),
                    sizeof(usbxhci_endpoint_target_t));

        usbxhci_init(self);
    } while (pci_enumerate_next(&pci_iter));
}

REGISTER_CALLOUT(usbxhci_detect, 0, 'U', "000");
