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

// Returns how much buffer should have been provided on success
// Limits buffer fill to specified size
// Returns -1 on error or if not running under qemu
ssize_t qemu_fw_cfg(void *buffer, size_t size, const char *name)
{
    if (!qemu_present())
        return -1;

    char sig[4];

    // Probe for fw_cfg hardware
    outw(FW_CFG_PORT_SEL, FW_CFG_SIGNATURE);
    insb(FW_CFG_PORT_DATA, sig, 4);

    if (memcmp(sig, "QEMU", 4))
        return -1;

    uint32_t file_count;

    outw(FW_CFG_PORT_SEL, FW_CFG_FILE_DIR);
    insb(FW_CFG_PORT_DATA, &file_count, sizeof(file_count));

    file_count = ntohl(file_count);

    FWCfgFile file;

    uint16_t sel = 0;
    uint32_t file_size;

    for (uint32_t i = 0; i < file_count; ++i) {
        insb(FW_CFG_PORT_DATA, &file, sizeof(file));

        if (!strcmp(file.name, name)) {
            sel = ntohs(file.select);
            file_size = ntohl(file.size);
            break;
        }
    }

    if (!sel)
        return -1;

    if (size > file_size)
        size = file_size;

    outw(FW_CFG_PORT_SEL, sel);
    insb(FW_CFG_PORT_DATA, buffer, size);

    return file_size;
}
