#include "types.h"
#include "pxemain.h"
#include "pxemain_abstract.h"
#include "string.h"
#include "pxestruct.h"
#include "../kernel/lib/bswap.h"
#include "boottable.h"
#include "screen.h"
#include "elf64.h"
#include "halt.h"
#include "fs.h"
#include "x86/cpu_x86.h"
#include "gdt_sel_pxe.h"

// iPXE is loading a real mode segment and crashing when doing PM call
#define PXE_USE_PROTECTED_MODE  0

#define DEBUG_TFTP 0
#if DEBUG_TFTP
#define PXE_TFTP_TRACE(...) PRINT("tftp: " __VA_ARGS__)
#else
#define PXE_TFTP_TRACE(...) ((void)0)
#endif

#define PXE_TFTP_ERROR(...) PRINT("tftp error: " __VA_ARGS__)

uint8_t pxe_server_ip[4];
uint8_t pxe_gateway_ip[4];

//#if PXE_USE_PROTECTED_MODE
//_section(".lowdata")
//uint32_t pxe_entry_farp32[2];
//#endif

_section(".lowdata")
uint16_t pxe_entry_farp16[2];  // [ off16, seg16 ] 16-bit far pointer

_section(".lowdata")
uint16_t bang_pxe_farp16[2];   // [ off16, seg16 ] 16-bit far pointer

uint16_t (*pxe_call)(uint16_t op, pxenv_base_t *arg_struct);

static bool pxe_api_op_invoke(uint16_t opcode, pxenv_base_t *op)
{
    uint16_t status = pxe_call(opcode, op);
    return status == PXENV_EXIT_SUCCESS && op->status == PXENV_STATUS_SUCCESS;
}

template<typename T>
static _always_inline bool pxe_api_op_run(T *op)
{
    return pxe_api_op_invoke(pxenv_opcode_t<T>::opcode, op);
}

int64_t pxe_api_tftp_get_fsize(char const *filename)
{
    PXE_TFTP_TRACE("getting filesize of file %s", filename);

    pxenv_tftp_get_fsize_t op{};

    strncpy(op.filename, filename, sizeof(op.filename));
    if (unlikely(op.filename[sizeof(op.filename)-1]))
        return -1;

    C_ASSERT(sizeof(op.server_ip) == sizeof(pxe_server_ip));
    C_ASSERT(sizeof(op.gateway_ip) == sizeof(pxe_gateway_ip));
    memcpy(op.server_ip, pxe_server_ip, sizeof(op.server_ip));
    memcpy(op.gateway_ip, pxe_gateway_ip, sizeof(op.gateway_ip));

    pxe_call(PXENV_TFTP_GET_FSIZE, &op);

    if (unlikely(op.status != 0))
        return -1;

    return op.file_size;
}

// Returns negotiated block size on success, -1 on error
int pxe_api_tftp_open(char const *filename, uint16_t packet_size)
{
    PXE_TFTP_TRACE("opening %s with packet size %u", filename, packet_size);

    pxenv_tftp_open_t op{};

    strncpy((char*)op.filename, filename, sizeof(op.filename));
    if (unlikely(op.filename[countof(op.filename)-1]))
        return -1;

    C_ASSERT(sizeof(op.server_ip) == sizeof(pxe_server_ip));
    memcpy(op.server_ip, pxe_server_ip, sizeof(op.server_ip));

    C_ASSERT(sizeof(op.gateway_ip) == sizeof(pxe_gateway_ip));
    memcpy(op.gateway_ip, pxe_gateway_ip, sizeof(op.gateway_ip));

    // FIXME
    op.tftp_port = htons(69);

    op.packet_size = packet_size;

    if (likely(pxe_api_op_run(&op))) {
        PXE_TFTP_TRACE("open succeeded");
        return op.packet_size;
    }

    PXE_TFTP_ERROR("open failed, status=0x%x", op.status);

    return -1;
}

bool pxe_api_tftp_close()
{
    PXE_TFTP_TRACE("closing transfer");

    pxenv_tftp_close_t op{};

    bool result = pxe_api_op_run(&op);

    if (!result || op.status != PXENV_STATUS_SUCCESS)
        PXE_TFTP_ERROR("closed, status=0x%x", op.status);
    else
        PXE_TFTP_TRACE("closed, result=%d, status=0x%x",
                       result ? "true" : "false", op.status);

    return result;
}

// Returns amount of data received, or -1 on error
int pxe_api_tftp_read(void *buffer, uint16_t sequence, uint16_t size)
{
    PXE_TFTP_TRACE("reading sequence=%u, size=%u, buffer=0x%p",
                   sequence, size, buffer);

    pxenv_tftp_read_t op{};

    op.buffer_ofs = uintptr_t(buffer) & 0xF;
    op.buffer_seg = uintptr_t(buffer) >> 4;
    op.sequence = sequence;
    op.buffer_size = size;

    if (likely(pxe_api_op_run(&op))) {
        PXE_TFTP_TRACE("read succeeded, got %u", op.buffer_size);
        return op.buffer_size;
    }

    PXE_TFTP_ERROR("read failed, status=0x%x", op.status);
    return -1;
}

static bool pxe_set_api(bangpxe_t *bp)
{
    if (unlikely(memcmp(bp->sig, "!PXE", 4))) {
        PRINT("!PXE struct has a bad signature");
        return false;
    }

    if (unlikely(boottbl_checksum((char const *)bp, bp->StructLength))) {
        PRINT("!PXE struct has a checksum mismatch");
        return false;
    }

#if PXE_USE_PROTECTED_MODE
    //
    // Initialize 16-bit protected mode GDT entries

    gdt[GDT_SEL_PXE_STACK >> 3].set_base(bp->Stack.ofs)
            .set_limit(bp->Stack.size-1)
            .set_access_rwdata(0)
            .set_flags_16bit();

    gdt[GDT_SEL_PXE_UD >> 3].set_base(bp->UNDIData.ofs)
            .set_limit(bp->UNDIData.size-1)
            .set_access_rwdata(0)
            .set_flags_16bit();

    gdt[GDT_SEL_PXE_UC >> 3].set_base(bp->UNDICode.ofs)
            .set_limit(bp->UNDICode.size-1)
            .set_access_code(0)
            .set_flags_16bit();

    gdt[GDT_SEL_PXE_UCW >> 3].set_base(bp->UNDICodeWrite.ofs)
            .set_limit(bp->UNDICodeWrite.size-1)
            .set_access_rwdata(0)//.access(true, 0, true, false, true)
            .set_flags_16bit();

    gdt[GDT_SEL_PXE_BD >> 3].set_base(bp->BC_Data.ofs)
            .set_limit(bp->BC_Data.size-1)
            .set_access_rwdata(0)
            .set_flags_16bit();

    gdt[GDT_SEL_PXE_BC >> 3].set_base(bp->BC_Code.ofs)
            .set_limit(bp->BC_Code.size-1)
            .set_access(true, 0, true, false, false)
            .set_flags_16bit();

    gdt[GDT_SEL_PXE_BCW >> 3].set_base(bp->BC_CodeWrite.ofs)
            .set_limit(bp->BC_CodeWrite.size-1)
            .set_access_rwdata(0)//.set_access(true, 0, true, false, true)
            .set_flags_16bit();

    gdt[GDT_SEL_PXE_ENTRY >> 3].set_base(bp->EntryPointSP_seg << 4)
            .set_limit(0xFFFF)
            .set_access(true, 0, true, false, true)
            .set_flags_16bit();

    gdt[GDT_SEL_PXE_TEMP >> 3].set_base(bp->EntryPointSP_seg << 4)
            .set_limit(0xFFFF)
            .set_access(true, 0, false, false, true)
            .set_flags_16bit();

//    pep->bc_code_seg = GDT_SEL_PXE_BC;
//    pep->bc_data_seg = GDT_SEL_PXE_BD;
//    pep->stack_seg = GDT_SEL_PXE_STACK;
//    pep->undi_code_seg = GDT_SEL_PXE_UC;
//    pep->undi_data_seg = GDT_SEL_PXE_UD;

    bp->EntryPointSP_seg = GDT_SEL_PXE_ENTRY;
    bp->EntryPointESP_seg = GDT_SEL_PXE_ENTRY;

    bp->Stack.seg = GDT_SEL_PXE_STACK;
    bp->UNDIData.seg = GDT_SEL_PXE_UD;
    bp->UNDICode.seg = GDT_SEL_PXE_UC;
    bp->UNDICodeWrite.seg = GDT_SEL_PXE_UCW;
    bp->BC_Data.seg = GDT_SEL_PXE_BD;
    bp->BC_Code.seg = GDT_SEL_PXE_BC;
    bp->BC_CodeWrite.seg = GDT_SEL_PXE_BCW;

    bp->FirstSelector = GDT_SEL_PXE_1ST;

    bp->StatusCallout_ofs = -1;
    bp->StatusCallout_seg = -1;

    bang_pxe_farp16[0] = uintptr_t(bp) - bp->UNDICode.ofs;
    bang_pxe_farp16[1] = GDT_SEL_PXE_UC;

    pxe_call = pxe_call_bangpxe_pm;

    pxe_entry_farp16[0] = bp->EntryPointSP_ofs;
    pxe_entry_farp16[1] = bp->EntryPointSP_seg;

    PRINT("Using protected mode PXE API entry point at %x:%x",
        pxe_entry_farp16[1], pxe_entry_farp16[0]);
#else
    pxe_call = pxe_call_bangpxe_rm;

    // 16-bit far pointer, seg:off little endian
    pxe_entry_farp16[0] = bp->EntryPointSP_ofs;
    pxe_entry_farp16[1] = bp->EntryPointSP_seg;

    PRINT("Using real mode PXE API entry point at %x:%x",
        pxe_entry_farp16[1], pxe_entry_farp16[0]);
#endif

    PRINT("Using %s API", "!PXE");

    return true;
}

static bool pxe_set_api(pxenv_plus_t *pep)
{
    if (unlikely(memcmp(pep->sig, "PXENV+", 6) ||
            boottbl_checksum((char const *)pep, pep->length)))
        return false;

    if (likely(pep->version >= 0x201)) {
        bang_pxe_farp16[0] = pep->pxe_ptr_ofs;
        bang_pxe_farp16[1] = pep->pxe_ptr_seg;

        bangpxe_t *bangpxe = (bangpxe_t*)
            ((uintptr_t(pep->pxe_ptr_seg) << 4) + pep->pxe_ptr_ofs);

        return pxe_set_api(bangpxe);
    }

    pxe_call = pxe_call_pxenv;

    pxe_entry_farp16[0] = pep->rm_entry_ofs;
    pxe_entry_farp16[1] = pep->rm_entry_seg;

    PRINT("Using %s API", "PXENV+");

    return true;
}

void pxe_init_tftp()
{
    pxenv_get_cached_info_t gci{};
    union alignas(16) {
        char buf[1500];
        pxe_cached_info_t cached_info;
    };

    gci.buffer_ofs = uintptr_t(buf) & 0xF;
    gci.buffer_seg = uintptr_t(buf) >> 4;
    gci.buffer_size = sizeof(buf);

    uint16_t result;

    memset(buf, 0, sizeof(buf));
    gci.buffer_size = sizeof(buf);
    gci.packet_type = PXENV_PACKET_TYPE_DHCP_CACHED_REPLY;
    result = pxe_call(PXENV_GET_CACHED_INFO, &gci);

    if (unlikely(result != PXENV_EXIT_SUCCESS))
        PANIC("Failed to get cached network info!");

    C_ASSERT(sizeof(pxe_server_ip) == sizeof(cached_info.sip));
    memcpy(pxe_server_ip, cached_info.sip, sizeof(pxe_server_ip));

    C_ASSERT(sizeof(pxe_gateway_ip) == sizeof(cached_info.gip));
    memcpy(pxe_gateway_ip, cached_info.gip, sizeof(pxe_gateway_ip));

    PRINT("Booting from server %u.%u.%u.%u, gateway %u.%u.%u.%u\n",
          pxe_server_ip[0], pxe_server_ip[1],
          pxe_server_ip[2], pxe_server_ip[3],
          pxe_gateway_ip[0], pxe_gateway_ip[1],
          pxe_gateway_ip[2], pxe_gateway_ip[3]);
}

extern "C" void pxe_main(pxenv_plus_t *pep, bangpxe_t *)
{
    PRINT("PXE bootloader started...");

    if (unlikely(!pxe_set_api(pep)))
        return;

    pxe_init_fs();

    elf64_boot();
}
