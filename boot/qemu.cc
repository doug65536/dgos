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

bool qemu_fw_cfg_present()
{
    if (likely(fw_cfg_detected))
        return fw_cfg_detected > 0;

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

    return true;
}

int qemu_selector_by_name(const char * restrict name,
                          uint32_t * restrict file_size_out)
{
    if (file_size_out)
        *file_size_out = 0;

    if (!qemu_fw_cfg_present())
        return -1;

    uint32_t file_count;

    outw(FW_CFG_PORT_SEL, FW_CFG_FILE_DIR);
    insb(FW_CFG_PORT_DATA, &file_count, sizeof(file_count));

    file_count = ntohl(file_count);

    FWCfgFile file;

    int sel = -1;

    for (uint32_t i = 0; i < file_count; ++i) {
        insb(FW_CFG_PORT_DATA, &file, sizeof(file));

        if (!strcmp(file.name, name)) {
            sel = ntohs(file.select);
            if (file_size_out)
                *file_size_out = ntohl(file.size);
            break;
        }
    }

    return sel;
}

int qemu_selector_by_name_dma(const char * restrict name,
                              uint32_t * restrict file_size_out)
{
    if (file_size_out)
        *file_size_out = 0;

    if (!qemu_fw_cfg_present())
        return -1;

    uint32_t file_count;

    qemu_fw_cfg_dma(&file_count, sizeof(file_count), FW_CFG_FILE_DIR);
    file_count = ntohl(file_count);

    FWCfgFile *files = new FWCfgFile[file_count];

    int sel = -1;

    if (qemu_fw_cfg_dma(files, sizeof(*files) * file_count,
                        FW_CFG_FILE_DIR, sizeof(file_count))) {
        for (uint32_t i = 0; i < file_count; ++i) {
            if (!strcmp(files[i].name, name)) {
                sel = ntohs(files[i].select);
                if (file_size_out)
                    *file_size_out = ntohl(files[i].size);
                break;
            }
        }
    }

    delete[] files;

    return sel;
}

// Returns how much buffer should have been provided on success
// Limits buffer fill to specified size
// Returns -1 on error or if not running under qemu
ssize_t qemu_fw_cfg(void *buffer, size_t size, char const *name)
{
    uint32_t file_size;
    int sel = qemu_selector_by_name(name, &file_size);

    if (unlikely(sel < 0))
        return -1;

    if (size > file_size)
        size = file_size;

    outw(FW_CFG_PORT_SEL, sel);
    insb(FW_CFG_PORT_DATA, buffer, size);

    return file_size;
}

// fw_cfg DMA commands
enum struct fw_cfg_ctl_t : uint32_t {
    error = 1,
    read = 2,
    skip = 4,
    select = 8,
    write = 16
};

bool qemu_fw_cfg_dma(uintptr_t buffer_addr, uint32_t size,
                     int selector, uint64_t file_offset,
                     bool read)
{
    // Tolerate the caller using qemu_selector_by_name()
    // without checking for error
    if (selector < 0)
        return false;

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
    cmd->control = htonl(uint32_t(fw_cfg_ctl_t::select) | (selector << 16));
    ++cmd;

    // A command to seek to an offset in the file, if it is nonzero
    if (file_offset > 0) {
        cmd->control = htonl(uint32_t(fw_cfg_ctl_t::skip));
        cmd->length = htonl(file_offset);
        ++cmd;
    }

    // A command to perform read or write
    cmd->control = bswap_32(uint32_t(read ? fw_cfg_ctl_t::read
                                          : fw_cfg_ctl_t::write));
    cmd->address = htobe64(buffer_addr);
    cmd->length = htonl(size);
    ++cmd;

    // Guarantee that memory changes thus far are globally visible before
    // telling the hardware to fetch the command data
    __sync_synchronize();

    // Execute all of the commands. Volatile because the hardware may
    // asynchronously change fields
    for (FWCfgDmaAccess volatile *chk = cmd_list; chk < cmd; ++chk) {

        // This must be a physical address
        // This code runs identity mapped so the pointer is the physical address
        uint64_t cmd_physaddr = bswap_64(uintptr_t(chk));

        if (cmd_physaddr & 0xFFFFFFFFU)
            outl(FW_CFG_PORT_DMA, cmd_physaddr & 0xFFFFFFFFU);
        outl(FW_CFG_PORT_DMA + 4, cmd_physaddr >> 32);

        // Guarantee that any recent device memory writes are picked up
        __sync_synchronize();

        uint32_t ctl = chk->control;

        // Wait for the command to complete
        // (this should never loop but someday fw_cfg DMA may become async.
        // As of early 2019, QEMU completes all operations during the write
        // to the DMA I/O port, from the perspective of the quest)
        while (unlikely(!(ctl & uint32_t(fw_cfg_ctl_t::error)) && ctl))
            ctl = chk->control;

        if (unlikely(ctl & uint32_t(fw_cfg_ctl_t::error)))
            return false;
    }

    return true;
}
