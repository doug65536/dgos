#include "fs.h"
#include "malloc.h"
#include "likely.h"

fs_api_t fs_api;

tchar const *boot_name()
{
    return fs_api.name;
}

int boot_open(tchar const *filename)
{
    return fs_api.boot_open(filename);
}

off_t boot_filesize(int file)
{
    return fs_api.boot_filesize(file);
}

int boot_close(int file)
{
    return fs_api.boot_close(file);
}

ssize_t boot_pread(int file, void *buf, size_t bytes, off_t ofs)
{
    return fs_api.boot_pread(file, buf, bytes, ofs);
}

uint64_t boot_serial()
{
    return fs_api.boot_drv_serial();
}

disk_io_plan_t::disk_io_plan_t(void *dest, uint8_t log2_sector_size)
    : dest(dest)
    , vec(nullptr)
    , count(0)
    , capacity(0)
    , log2_sector_size(log2_sector_size)
{
}

disk_io_plan_t::~disk_io_plan_t()
{
    free(vec);
    vec = nullptr;
    count = 0;
    capacity = 0;
}

bool disk_io_plan_t::add(uint32_t lba, uint16_t sector_count,
                         uint16_t sector_ofs, uint16_t byte_count)
{
    if (count > 0) {
        // See if we can coalesce with previous entry

        disk_vec_t &prev = vec[count - 1];

        uint_fast16_t sector_size = 1 << log2_sector_size;

        if (prev.lba + prev.count == lba &&
                prev.sector_ofs == 0 &&
                sector_ofs == 0 &&
                prev.byte_count == sector_size &&
                byte_count == sector_size &&
                0xFFFFFFFFU - count > prev.count) {
            // Added entry is a sequential run of full sector-aligned sector
            // which is contiguous with previous run of full sector-aligned
            // sectors and the sector count won't overflow
            prev.count += sector_count;
            return true;
        }
    }

    if (count + 1 > capacity) {
        size_t new_capacity = capacity >= 16 ? capacity * 2 : 16;
        disk_vec_t *new_vec = (disk_vec_t*)realloc(
                    vec, new_capacity * sizeof(*vec));
        if (unlikely(!new_vec))
            return false;
        vec = new_vec;
        capacity = new_capacity;
    }

    disk_vec_t &item = vec[count++];

    item.lba = lba;
    item.count = sector_count;
    item.sector_ofs = sector_ofs;
    item.byte_count = byte_count;

    return true;
}
