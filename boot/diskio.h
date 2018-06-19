#pragma once

#include "types.h"

extern "C" uint8_t boot_drive;
extern "C" uint32_t vbe_info_vector;

bool disk_support_64bit_addr();

bool disk_read_lba(uint64_t addr, uint64_t lba,
                      uint8_t log2_sector_size, unsigned sector_count);
