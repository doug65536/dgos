#pragma once

#define PXENV_EXIT_SUCCESS 0x0000
#define PXENV_EXIT_FAILURE 0x0001

// Status codes returned in the status word of PXENV API parameter
#define PXENV_STATUS_SUCCESS 0x00
#define PXENV_STATUS_FAILURE 0x01

// Invalid function number
#define PXENV_STATUS_BAD_FUNC 0x02

// Function is not supported
#define PXENV_STATUS_UNSUPPORTED 0x03

// UNDI must not be unloaded from base memory
#define PXENV_STATUS_KEEP_UNDI 0x04

#define PXENV_STATUS_KEEP_ALL           0x05
#define PXENV_STATUS_OUT_OF_RESOURCES   0x06

#define PXENV_STATUS_ARP_TIMEOUT  0x11
#define PXENV_STATUS_UDP_CLOSED   0x18
#define PXENV_STATUS_UDP_OPEN     0x19
#define PXENV_STATUS_TFTP_CLOSED  0x1A
#define PXENV_STATUS_TFTP_OPEN    0x1B

/* BIOS/system errors (0x20 to 0x2F) */
#define PXENV_STATUS_MCOPY_PROBLEM         0x20
#define PXENV_STATUS_BIS_INTEGRITY_FAILURE 0x21
#define PXENV_STATUS_BIS_VALIDATE_FAILURE  0x22
#define PXENV_STATUS_BIS_INIT_FAILURE      0x23
#define PXENV_STATUS_BIS_SHUTDOWN_FAILURE  0x24
#define PXENV_STATUS_BIS_GBOA_FAILURE      0x25
#define PXENV_STATUS_BIS_FREE_FAILURE      0x26
#define PXENV_STATUS_BIS_GSI_FAILURE       0x27
#define PXENV_STATUS_BIS_BAD_CKSUM         0x28

/* TFTP/MTFTP errors (0x30 to 0x3F) */
#define PXENV_STATUS_TFTP_CANNOT_ARP_ADDRESS 0x30
#define PXENV_STATUS_TFTP_OPEN_TIMEOUT       0x32

#define PXENV_STATUS_TFTP_UNKNOWN_OPCODE                0x33
#define PXENV_STATUS_TFTP_READ_TIMEOUT                  0x35
#define PXENV_STATUS_TFTP_ERROR_OPCODE                  0x36
#define PXENV_STATUS_TFTP_CANNOT_OPEN_CONNECTION        0x38
#define PXENV_STATUS_TFTP_CANNOT_READ_FROM_CONNECTION   0x39
#define PXENV_STATUS_TFTP_TOO_MANY_PACKAGES             0x3A
#define PXENV_STATUS_TFTP_FILE_NOT_FOUND                0x3B
#define PXENV_STATUS_TFTP_ACCESS_VIOLATION              0x3C
#define PXENV_STATUS_TFTP_NO_MCAST_ADDRESS              0x3D
#define PXENV_STATUS_TFTP_NO_FILESIZE                   0x3E
#define PXENV_STATUS_TFTP_INVALID_PACKET_SIZE           0x3F

/* Reserved errors 0x40 to 0x4F) */
/* DHCP/BOOTP errors (0x50 to 0x5F) */
#define PXENV_STATUS_DHCP_TIMEOUT          0x51
#define PXENV_STATUS_DHCP_NO_IP_ADDRESS    0x52
#define PXENV_STATUS_DHCP_NO_BOOTFILE_NAME 0x53
#define PXENV_STATUS_DHCP_BAD_IP_ADDRESS   0x54

/* Driver errors (0x60 to 0x6F) */
/* These errors are for UNDI compatible NIC drivers. */
#define PXENV_STATUS_UNDI_INVALID_FUNCTION           0x60
#define PXENV_STATUS_UNDI_MEDIATEST_FAILED           0x61
#define PXENV_STATUS_UNDI_CANNOT_INIT_NIC_FOR_MCAST  0x62
#define PXENV_STATUS_UNDI_CANNOT_INITIALIZE_NIC      0x63
#define PXENV_STATUS_UNDI_CANNOT_INITIALIZE_PHY      0x64
#define PXENV_STATUS_UNDI_CANNOT_READ_CONFIG_DATA    0x65
#define PXENV_STATUS_UNDI_CANNOT_READ_INIT_DATA      0x66
#define PXENV_STATUS_UNDI_BAD_MAC_ADDRESS            0x67
#define PXENV_STATUS_UNDI_BAD_EEPROM_CHECKSUM        0x68
#define PXENV_STATUS_UNDI_ERROR_SETTING_ISR          0x69
#define PXENV_STATUS_UNDI_INVALID_STATE              0x6A
#define PXENV_STATUS_UNDI_TRANSMIT_ERROR             0x6B
#define PXENV_STATUS_UNDI_INVALID_PARAMETER          0x6C

/* ROM and NBP Bootstrap errors (0x70 to 0x7F) */
#define PXENV_STATUS_BSTRAP_PROMPT_MENU   0x74
#define PXENV_STATUS_BSTRAP_MCAST_ADDR    0x76
#define PXENV_STATUS_BSTRAP_MISSING_LIST  0x77
#define PXENV_STATUS_BSTRAP_NO_RESPONSE   0x78
#define PXENV_STATUS_BSTRAP_FILE_TOO_BIG  0x79

/* Environment NBP errors (0x80 to 0x8F) */
/* Reserved errors (0x90 to 0x9F) */
/* Misc. errors (0xA0 to 0xAF) */
#define PXENV_STATUS_BINL_CANCELED_BY_KEYSTROKE  0xA0
#define PXENV_STATUS_BINL_NO_PXE_SERVER          0xA1
#define PXENV_STATUS_NOT_AVAILABLE_IN_PMODE      0xA2
#define PXENV_STATUS_NOT_AVAILABLE_IN_RMODE      0xA3

/* BUSD errors (0xB0 to 0xBF) */
#define PXENV_STATUS_BUSD_DEVICE_NOT_SUPPORTED 0xB0
/* Loader errors (0xC0 to 0xCF) */
#define PXENV_STATUS_LOADER_NO_FREE_BASE_MEMORY     0xC0
#define PXENV_STATUS_LOADER_NO_BC_ROMID             0xC1
#define PXENV_STATUS_LOADER_BAD_BC_ROMID            0xC2
#define PXENV_STATUS_LOADER_BAD_BC_RUNTIME_IMAGE    0xC3
#define PXENV_STATUS_LOADER_NO_UNDI_ROMID           0xC4
#define PXENV_STATUS_LOADER_BAD_UNDI_ROMID          0xC5
#define PXENV_STATUS_LOADER_BAD_UNDI_DRIVER_IMAGE   0xC6
#define PXENV_STATUS_LOADER_NO_PXE_STRUCT           0xC8
#define PXENV_STATUS_LOADER_NO_PXENV_STRUCT         0xC9
#define PXENV_STATUS_LOADER_UNDI_START              0xCA
#define PXENV_STATUS_LOADER_BC_START                0xCB

/* Vendor errors (0xD0 to 0xFF) */

//
// Offsets used in assembly
//

#define PXE_ENTRY_SP_OFS 0x10

#if !defined(__ASSEMBLER__)

#include "types.h"
#include "assert.h"

typedef uint16_t ofs16_t;
typedef uint16_t seg16_t;

struct pxe_segdesc_t {
    // Real mode segment or protected mode selector
    seg16_t seg;

    // Offset within the segment
    uint32_t ofs;

    // Size of the segment
    uint16_t size;
} _packed;

struct bangpxe_t {
    char sig[4];

    // Length of this structure
    uint8_t StructLength;

    // Checksum value set to make sum of bytes in this structure equal zero
    uint8_t StructCksum;

    // Revision
    uint8_t StructRev;

    uint8_t reserved;

    // Real mode seg:ofs of UNDI ROM ID structure
    ofs16_t UNDIROMID_ofs;
    seg16_t UNDIROMID_seg;

    // Real mode seg:ofs of BC ROM ID structure
    ofs16_t BaseROMID_ofs;
    seg16_t BaseROMID_seg;

    ofs16_t EntryPointSP_ofs;
    seg16_t EntryPointSP_seg;

    ofs16_t EntryPointESP_ofs;
    seg16_t EntryPointESP_seg;

    // Real mode far pointer to status call-out handler function
    // Must be filled in before making any base-code API calls in PM
    ofs16_t StatusCallout_ofs;
    seg16_t StatusCallout_seg;

    uint8_t reserved2;
    uint8_t SegDescCnt;
    uint16_t FirstSelector;

    pxe_segdesc_t Stack;
    pxe_segdesc_t UNDIData;
    pxe_segdesc_t UNDICode;
    pxe_segdesc_t UNDICodeWrite;
    pxe_segdesc_t BC_Data;
    pxe_segdesc_t BC_Code;
    pxe_segdesc_t BC_CodeWrite;
};

C_ASSERT(sizeof(bangpxe_t) == 0x58);
C_ASSERT(offsetof(bangpxe_t, EntryPointSP_ofs) == PXE_ENTRY_SP_OFS);

typedef uint16_t pxenv_status_t;

//
// PXENV+

struct pxenv_plus_t {
    char sig[6];
    uint16_t version;
    uint8_t length;
    uint8_t checksum;

    ofs16_t rm_entry_ofs;
    seg16_t rm_entry_seg;

    uint32_t pm32_ofs;
    seg16_t pm32_sel;

    seg16_t stack_seg;
    uint16_t stack_size;

    seg16_t bc_code_seg;
    uint16_t bc_code_size;

    seg16_t bc_data_seg;
    uint16_t bc_data_size;

    seg16_t undi_data_seg;
    uint16_t undi_data_size;

    seg16_t undi_code_seg;
    uint16_t undi_code_size;

    ofs16_t pxe_ptr_ofs;
    seg16_t pxe_ptr_seg;
} _packed;

C_ASSERT(sizeof(pxenv_plus_t) == 0x2C);

struct pxenv_base_t {
    pxenv_status_t status;
};

template<typename T>
struct pxenv_opcode_t
{
    static uint16_t opcode;
};

#define PXE_DEFINE_OPCODE(type_, opcode_) \
    template<> \
    struct pxenv_opcode_t<type_> { \
        static constexpr uint16_t opcode = opcode_; \
    }

//
// OP: get cached info

#define PXENV_GET_CACHED_INFO   0x71

#define PXENV_PACKET_TYPE_DHCP_DISCOVER     1
#define PXENV_PACKET_TYPE_DHCP_ACK          2
#define PXENV_PACKET_TYPE_DHCP_CACHED_REPLY 3

struct pxenv_get_cached_info_t : public pxenv_base_t {
    uint16_t packet_type;
    uint16_t buffer_size;
    uint16_t buffer_ofs;
    uint16_t buffer_seg;
    uint16_t buffer_limit;
};

PXE_DEFINE_OPCODE(pxenv_get_cached_info_t, PXENV_GET_CACHED_INFO);

extern "C" uint16_t pxe_call_bangpxe_rm(
        uint16_t opcode, pxenv_base_t *arg_struct);
extern "C" uint16_t pxe_call_bangpxe_pm(
        uint16_t opcode, pxenv_base_t *arg_struct);
extern "C" uint16_t pxe_call_pxenv(
        uint16_t opcode, pxenv_base_t *arg_struct);

//
// OP: UNDI startup



//
// OP: TFTP open

#define PXENV_TFTP_OPEN 0x20

struct pxenv_tftp_open_t : public pxenv_base_t {
    uint8_t server_ip[4];
    uint8_t gateway_ip[4];
    char filename[128];
    uint16_t tftp_port;
    uint16_t packet_size;
} _packed;

PXE_DEFINE_OPCODE(pxenv_tftp_open_t, PXENV_TFTP_OPEN);

//
// OP: TFTP close

#define PXENV_TFTP_CLOSE 0x21

struct pxenv_tftp_close_t : public pxenv_base_t {
} _packed;

PXE_DEFINE_OPCODE(pxenv_tftp_close_t, PXENV_TFTP_CLOSE);

//
// OP: TFTP read

#define PXENV_TFTP_READ 0x22

struct pxenv_tftp_read_t : public pxenv_base_t {
    uint16_t sequence;
    uint16_t buffer_size;
    uint16_t buffer_ofs;
    uint16_t buffer_seg;
} _packed;

PXE_DEFINE_OPCODE(pxenv_tftp_read_t, PXENV_TFTP_READ);

//
// OP: TFTP get file size

#define PXENV_TFTP_GET_FSIZE    0x25

struct pxenv_tftp_get_fsize_t : public pxenv_base_t {
    uint8_t server_ip[4];
    uint8_t gateway_ip[4];
    char filename[128];
    uint32_t file_size;
} _packed;

PXE_DEFINE_OPCODE(pxenv_tftp_get_fsize_t, PXENV_TFTP_GET_FSIZE);

struct pxe_cached_info_t {
    // REQ or REP
    uint8_t opcode;

    // See ARP internet assigned numbers
    uint8_t hardware;

    // Hardware address length
    uint8_t hard_len;

    // Gateway hops
    uint8_t gate_hops;

    // ident
    uint32_t ident;

    uint16_t seconds;
    uint16_t flags;

    // Client IP
    uint8_t cip[4];

    // Your IP
    uint8_t yip[4];

    // Boot Server IP
    uint8_t sip[4];

    // Gateway IP
    uint8_t gip[4];

    // Client MAC address
    uint8_t mac_addr[16];

    // Server host name
    char server_name[64];

    // Boot file name
    char boot_file[128];

    // DHCP extensions
    // Expect
    char dhcp_ops[4*64];
};

C_ASSERT(sizeof(pxe_cached_info_t) < 512);

#endif  // !defined(__ASSEMBLER__)
