#include "qemu.h"
#include "string.h"
#include "cpu.h"
#include "cpuid.h"
#include "../kernel/lib/bswap.h"

bool qemu_present()
{
    cpuid_t info{};

    cpuid(&info, 0x40000000, 0);

    char str[12];
    memcpy(str + 0, &info.ebx, 4);
    memcpy(str + 4, &info.ecx, 4);
    memcpy(str + 8, &info.edx, 4);

    return !memcmp(str, "TCGTCGTCGTCG", 12) ||
            !memcmp(str, "KVMKVMKVM\0\0\0", 12);
}

// From https://github.com/qemu/qemu/blob/master/docs/specs/fw_cfg.txt
struct FWCfgFile {      /* an individual file entry, 64 bytes total */
    uint32_t size;      /* size of referenced fw_cfg item, big-endian */
    uint16_t select;    /* selector key of fw_cfg item, big-endian */
    uint16_t reserved;
    char name[56];      /* fw_cfg item name, NUL-terminated ascii */
};

#define FW_CFG_PORT_SEL     0x510
#define FW_CFG_PORT_DATA    0x511
#define FW_CFG_PORT_DMA     0x514

#define FW_CFG_SIGNATURE    0x0000
#define FW_CFG_ID           0x0001
#define FW_CFG_FILE_DIR     0x0019

// -1 not present, 0=unknown, 1=present
static int fw_cfg_detected;

// returns: 0=not present, 1=present (no dma), 2=present (with dma)

static int qemu_fw_cfg_detect()
{
    // Guess not detected until proven otherwise so we can early out
    fw_cfg_detected = -1;

    if (!qemu_present())
        return false;

    // Probe for fw_cfg hardware
    char sig[4];
    outw(FW_CFG_PORT_SEL, FW_CFG_SIGNATURE);
    insb(FW_CFG_PORT_DATA, sig, 4);

    if (memcmp(sig, "QEMU", 4))
        return false;

    // Proven
    fw_cfg_detected = 1;

    // Look for DMA
    uint32_t dma_port_signature[2];
    dma_port_signature[0] = inl(FW_CFG_PORT_DMA);
    dma_port_signature[1] = inl(FW_CFG_PORT_DMA + 4);
    if (!memcmp(&dma_port_signature, "QEMU CFG", sizeof(uint32_t) * 2))
        fw_cfg_detected = 2;

    return fw_cfg_detected;
}

int qemu_fw_cfg_present()
{
    if (likely(fw_cfg_detected))
        return fw_cfg_detected > 0 ? fw_cfg_detected : 0;

    return qemu_fw_cfg_detect();
}

static inline bool qemu_fw_cfg_has_dma()
{
    return qemu_fw_cfg_present() > 1;
}

int qemu_selector_by_name(const char * restrict name,
                          uint32_t * restrict file_size_out)
{
    if (file_size_out)
        *file_size_out = 0;

    if (!qemu_fw_cfg_present())
        return -1;

    uint32_t file_count;

    qemu_fw_cfg(&file_count, sizeof(file_count),
                sizeof(file_count), FW_CFG_FILE_DIR);

    file_count = ntohl(file_count);

    size_t dir_file_size = sizeof(FWCfgFile) * file_count + sizeof(file_count);

    FWCfgFile file;

    int sel = -1;

    for (uint32_t i = 0; i < file_count; ++i) {
        qemu_fw_cfg(&file, sizeof(file), dir_file_size);

        if (!strcmp(file.name, name)) {
            sel = ntohs(file.select);
            if (file_size_out)
                *file_size_out = ntohl(file.size);
            break;
        }
    }

    return sel;
}

// fw_cfg DMA commands
enum struct fw_cfg_ctl_t : uint32_t {
    error = 1,
    read = 2,
    skip = 4,
    select = 8,
    write = 16
};

// If the selector is >= 0, the selector is selected and seek offset is reset,
// otherwise, the current file and seek position is preserved.
// If the file_offset is nonzero, then the seek position is advanced that far,
// If the size is nonzero, then a data transfer is performed
// This can be used to individually select, seek, and read files
//  select: qemu_fw_cfg_dma(nullptr, 0, sel, 0)
//  seek: qemu_fw_cfg_dma(nullptr, 0, -1, offset)
//  read: qemu_fw_cfg_dma(buffer, size, -1, 0)
//  select+read: qemu_fw_cfg_dma(buffer, size, sel, 0)
//  select+seek+read: qemu_fw_cfg_dma(buffer, size, sel, offset)
//  any combination
bool qemu_fw_cfg(void *buffer, uint32_t size, uint32_t file_size,
                 int selector, off_t file_offset)
{
    int present = qemu_fw_cfg_present();

    if (!present)
        return false;

    // DMA has this behaviour, touching one out of bounds byte = fail fast
    if (uint64_t(file_offset) + size > file_size)
        return false;

    if (unlikely(present == 1)) {
        // No DMA

        if (selector >= 0)
            outw(FW_CFG_PORT_SEL, selector);

        // Read bytes and throw them away to seek
        while (unlikely(file_offset--))
            inb(FW_CFG_PORT_DATA);

        insb(FW_CFG_PORT_DATA, buffer, size);

        // Return how much is left
        return true;
    }

    // The address register is big-endian
    // The most significant half is at offset 0 from the dma register
    // The least significant half is at offset 4 from the dma register
    // Operations trigger on the write to the low half, the high half
    // is simply a latch. When an operation triggers, both halves are
    // reset to zero. 32-bit operation addresses can be triggered
    // with a single write to the low half.

    // Aligning to a 16 byte boundary guarantees it won't cross a page boundary
    typedef struct alignas(16) FWCfgDmaAccess {
        uint32_t control;
        uint32_t length;
        uint64_t address;
    } FWCfgDmaAccess;

    FWCfgDmaAccess cmd_list[3] = {}, *cmd = cmd_list;

    // A command to select a file
    if (selector >= 0) {
        cmd->control = htonl(uint32_t(fw_cfg_ctl_t::select) | (selector << 16));
        ++cmd;
    }

    // A command to seek to an offset in the file, if it is nonzero
    if (file_offset > 0) {
        cmd->control = htonl(uint32_t(fw_cfg_ctl_t::skip));
        cmd->length = htonl(file_offset);
        ++cmd;
    }

    // A command to perform read
    if (size > 0) {
        cmd->control = bswap_32(uint32_t(fw_cfg_ctl_t::read));
        cmd->address = htobe64(uintptr_t(buffer));
        cmd->length = htonl(size);
        ++cmd;
    }

    // x86 I/O instructions are serializing. On platforms with MMIO
    // a memory barrier may be required here.

    // Execute all of the commands. Volatile because the hardware may
    // asynchronously change fields
    for (FWCfgDmaAccess volatile *iter = cmd_list; iter < cmd; ++iter) {
        // This must be a physical address
        // This code runs identity mapped so the pointer is the physical address
        uint32_t cmd_physaddr_lo = uint32_t(uintptr_t(iter) & 0xFFFFFFFFU);
        uint32_t cmd_physaddr_hi = sizeof(uintptr_t) > sizeof(uint32_t)
                ? uint32_t(uintptr_t(iter) >> 32)
                : 0;

        if (cmd_physaddr_hi)
            outl(FW_CFG_PORT_DMA, htonl(cmd_physaddr_hi));
        outl(FW_CFG_PORT_DMA + 4, htonl(cmd_physaddr_lo));

        // x86 I/O instructions are serializing. On platforms with MMIO
        // a memory barrier may be required here.

        uint32_t ctl = iter->control;

        // Wait for the command to complete
        // (this should never loop but someday fw_cfg DMA may become async.
        // As of early 2019, QEMU completes all operations during the write
        // to the DMA I/O port, from the perspective of the quest)
        while (unlikely(!(ctl & uint32_t(fw_cfg_ctl_t::error)) && ctl))
            ctl = iter->control;

        if (unlikely(ctl & uint32_t(fw_cfg_ctl_t::error)))
            return false;
    }

    return true;
}
