#include "types.h"
#include "diskio.h"
#include "bioscall.h"
#include "likely.h"
#include "assert.h"
#include "malloc.h"
#include "string.h"

extern uint8_t boot_drive;

static int disk_64bit;

static bool disk_support_64bit_addr_bioscall()
{
    bios_regs_t regs;

    regs.eax = 0x4100;
    regs.ebx = 0x55AA;
    regs.edx = boot_drive;

    bioscall(&regs, 0x13);

    if (regs.flags_CF())
        return false;

    bool signature = ((regs.ebx & 0xFFFF) == 0xAA55);
    int version = ((regs.eax >> 8) & 0xFF);

    disk_64bit = (signature && version >= 3) ? 1 : -1;

    return disk_64bit > 0;
}

bool disk_support_64bit_addr()
{
    if (disk_64bit)
        return disk_64bit > 0;

    return disk_support_64bit_addr_bioscall();
}

struct disk_drive_parameters_t {
    uint16_t struct_size;
    uint16_t info_flags;
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors;
    uint64_t total_sectors;
    uint16_t sector_size;
    uint16_t dpte_ofs;
    uint16_t dpte_seg;
    uint16_t dpi_sig;
    uint8_t dpi_len;
    uint8_t reserved1;
    uint16_t reserved2;
    char bus_type[4];
    char if_type[8];
    uint64_t if_path;
    uint64_t dev_path;
    uint8_t reserved3;
    uint8_t dpi_checksum;
} _packed;

C_ASSERT(sizeof(disk_drive_parameters_t) == 66);

int disk_sector_size()
{
    bios_regs_t regs;
    disk_drive_parameters_t params{};

    params.struct_size = sizeof(disk_drive_parameters_t);

    regs.eax = 0x4800;
    regs.edx = boot_drive;
    regs.esi = uint32_t(&params) & 0xF;
    regs.ds = uint32_t(&params) >> 4;

    bioscall(&regs, 0x13);

    if (regs.flags_CF())
        return -1;

    return params.sector_size;
}

struct disk_address_packet_t {
    uint8_t sizeof_packet;
    uint8_t reserved;
    uint16_t block_count;
    uint32_t segoff;
    uint64_t lba;
} _packed;

struct disk_address_packet_64_t {
    disk_address_packet_t hdr;
    uint64_t address;
} _packed;

static bool disk_read_lba_bouncebuffer(
        uint64_t addr, uint64_t lba,
        uint8_t log2_sector_size, unsigned count)
{
    size_t buf_size = count << log2_sector_size;

    // Cap to 64KB
    if (buf_size > (64 << 10))
        buf_size = (64 << 10);

    void *buffer;
    do {
        buffer = malloc(buf_size);

        // If allocation failed, try again with smaller buffer
        if (unlikely(!buffer))
            buf_size >>= 1;

        // Can't transfer less than one sector
        if (unlikely(buf_size < (size_t(1) << log2_sector_size)))
            return false;
    } while (unlikely(!buffer));

    int buf_sectors = buf_size >> log2_sector_size;

    while (likely(count)) {
        // Try to read the rest
        int read_count = count;

        // Cap read count to buffer size
        if (read_count > buf_sectors)
            read_count = buf_sectors;

        if (!disk_read_lba(uint64_t(buffer), lba,
                           log2_sector_size, read_count))
            break;

        size_t read_size = read_count << log2_sector_size;

        memcpy((void*)addr, buffer, read_size);

        addr += read_size;
        lba += read_count;
        count -= read_count;
    }

    free(buffer);

    return count == 0;
}

bool disk_read_lba(uint64_t addr, uint64_t lba,
                   uint8_t log2_sector_size, unsigned count)
{
    // Extended Read LBA sectors
    // INT 13h AH=42h
    disk_address_packet_64_t pkt;
    pkt.hdr.reserved = 0;
    pkt.hdr.lba = lba;

    if (addr < 0x100000) {
        pkt.hdr.sizeof_packet = sizeof(pkt.hdr);
        pkt.hdr.segoff = ((uint32_t(addr) >> 4) << 16) |
                (uint32_t(addr) & 0xF);
    } else if (0 && disk_support_64bit_addr()) {
        pkt.hdr.sizeof_packet = sizeof(pkt);
        pkt.hdr.segoff = 0xFFFFFFFF;
        pkt.address = addr;
    } else {
        // Automatic bounce buffering
        return disk_read_lba_bouncebuffer(addr, lba, log2_sector_size, count);
    }

    // At first, assume 64KB boundaries are irrelevant
    // and large sector counts are okay
    bool careful = false;

    while (count) {
        // Try to read the remaining sectors
        int read_count = count;

        // The LBA count field is 16 bit
        if (read_count > 0xFFFF)
            read_count = 0xFFFF;

        // Calculate read size in bytes
        size_t read_size = read_count << log2_sector_size;

        if (unlikely(careful)) {
            // Limit to half of a 64KB region, since a full 64KB is impossible
            if (read_count > 0x40) {
                read_count = 0x40;
                read_size = read_count << log2_sector_size;
            }

            // Don't cross 64KB boundary
            if (((addr + read_size) & -(UINT64_C(64) << 10)) !=
                    (addr & -(UINT64_C(64) << 10))) {
                // Read up to next 64KB boundary
                read_size = ((addr + read_size) & -(UINT64_C(64) << 10)) -
                        addr;
                read_count = read_size >> log2_sector_size;
                read_size = read_count << log2_sector_size;
            }
        }

        assert(read_count <= 0xFFFF);

        pkt.hdr.block_count = read_count;

        bios_regs_t regs;
        regs.eax = 0x4200;
        regs.edx = boot_drive;
        regs.esi = uint32_t(&pkt) & 0xF;
        regs.ds = uint32_t(&pkt) >> 4;

        bioscall(&regs, 0x13);

        uint8_t bios_error = regs.ah_if_carry();

        if (unlikely(bios_error != 0)) {
            // Error occurred...

            // pkt updated with number of sectors successfully transfered
            read_count = pkt.hdr.block_count;
            read_size = read_count << log2_sector_size;

            // Failed if no sectors transferred and already being careful
            if (unlikely(pkt.hdr.block_count == 0) && unlikely(careful))
                return false;

            // Start being careful
            careful = true;
        }

        count -= read_count;
        lba += read_count;
        addr += read_size;

        if (addr < 0x100000) {
            pkt.hdr.segoff = ((uint32_t(addr) >> 4) << 16) |
                    (uint32_t(addr) & 0xF);
        } else {
            pkt.address = addr;
        }
    }

    return true;
}
