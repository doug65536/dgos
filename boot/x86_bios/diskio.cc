#include "types.h"
#include "diskio.h"
#include "bioscall.h"
#include "likely.h"
#include "assert.h"
#include "malloc.h"
#include "string.h"
#include "halt.h"

extern uint8_t boot_drive;

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
};

C_ASSERT(sizeof(disk_address_packet_t) == 16);

__BEGIN_ANONYMOUS
struct disk_read_bouncebuffer_t {
    void *data = nullptr;
    size_t size = 0;

    disk_read_bouncebuffer_t() noexcept = default;

    disk_read_bouncebuffer_t(disk_read_bouncebuffer_t const&) = delete;

    disk_read_bouncebuffer_t(disk_read_bouncebuffer_t &&rhs) noexcept
    {
        data = rhs.data;
        rhs.data = nullptr;
        size = rhs.size;
        rhs.size = 0;
    }

    ~disk_read_bouncebuffer_t() noexcept
    {
        reset();
    }

    operator bool() const noexcept
    {
        return data;
    }

    void reset() noexcept
    {
        if (data) {
            free(data);
            data = nullptr;
            size = 0;
        }
    }

    bool alloc_up_to(size_t alloc_size, uint8_t log2_alignment) noexcept
    {
        reset();

        size_t alignment = size_t(1) << log2_alignment;

        if (alloc_size > (64 << 10))
            alloc_size = (64 << 10);

        do {
            size = alloc_size;
            data = malloc_aligned(alloc_size, alignment);

            // If allocation failed, try again with smaller buffer
            if (unlikely(!data))
                alloc_size >>= 1;

            // Can't transfer less than one sector
            if (unlikely(alloc_size < (size_t(1) << log2_alignment))) {
                data = nullptr;
                size = 0;
                break;
            }
        } while (unlikely(!data));

        return data && size;
    }
};

__END_ANONYMOUS

bool disk_read_lba(uint64_t addr, uint64_t lba,
                   uint8_t log2_sector_size, unsigned count)
{
    // Extended Read LBA sectors
    // INT 13h AH=42h
    disk_address_packet_t pkt;
    pkt.sizeof_packet = sizeof(pkt);
    pkt.reserved = 0;
    pkt.lba = lba;

    disk_read_bouncebuffer_t buffer;

    size_t max_sectors = 32768 >> log2_sector_size;

    uint64_t size = count << log2_sector_size;

    uint64_t end = addr + size;

    // Detect crossing a 64KB boundary with one sector read and force
    // bounce buffering in that case (unlikely)
    bool force_bounce_buffer =
            (addr & -(UINT64_C(64) << 10)) !=
            ((addr + (size_t(1) << log2_sector_size) - 1) &
             -(UINT64_C(64) << 10));

    uint32_t read_addr;
    if (force_bounce_buffer || end > 0x100000) {
        if (unlikely(!buffer.alloc_up_to(size, log2_sector_size)))
            PANIC_OOM();

        max_sectors = buffer.size >> log2_sector_size;

        // Point reads at bounce buffer
        read_addr = uint32_t(buffer.data);
    } else {
        // Read directly to destination
        read_addr = addr;
    }

    // Point the read at the bounce buffer or straight to result buffer
    pkt.segoff = ((uint32_t(read_addr) >> 4) << 16) |
            (uint32_t(read_addr) & 0xF);

    while (count) {
        // Try to read the remaining sectors
        unsigned read_count = count;

        // Limit LBA count
        if (read_count > max_sectors)
            read_count = max_sectors;

        // Calculate read size in bytes
        size_t read_size = read_count << log2_sector_size;

        // Limit to half of a 64KB region, since a full 64KB is impossible
        if (read_count > (1U << (15 - log2_sector_size))) {
            read_count = (1U << (15 - log2_sector_size));
            read_size = read_count << log2_sector_size;

            // This should be guaranteed, see force_bounce_buffer above
            assert(read_count > 0);
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

        assert(read_count <= 0xFFFF);

        pkt.block_count = read_count;

        bios_regs_t regs;
        regs.eax = 0x4200;
        regs.edx = boot_drive;
        regs.esi = uint32_t(uintptr_t(&pkt)) & 0xF;
        regs.ds = uint32_t(uintptr_t(&pkt)) >> 4;

        bioscall(&regs, 0x13);

        uint8_t bios_error = regs.ah_if_carry();

        if (unlikely(bios_error != 0)) {
            // Error occurred...

            // pkt updated with number of sectors successfully transfered
            read_count = pkt.block_count;
            read_size = read_count << log2_sector_size;

            // Failed if no sectors transferred
            if (unlikely(pkt.block_count == 0))
                return false;
        }

        // If using bounce buffer, copy data from bounce buffer to destination
        if (buffer)
            memcpy((void*)addr, buffer.data, read_size);

        count -= read_count;
        lba += read_count;
        addr += read_size;

        // If not using bounce buffer, advance disk read address
        if (!buffer) {
            pkt.segoff = ((uint32_t(addr) >> 4) << 16) |
                    (uint32_t(addr) & 0xF);
        }
    }

    return true;
}
