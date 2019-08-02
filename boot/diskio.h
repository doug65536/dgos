#pragma once

#include "types.h"

__BEGIN_DECLS

extern uint8_t boot_drive;

bool disk_support_64bit_addr();

int disk_sector_size();

bool disk_read_lba(uint64_t addr, uint64_t lba,
                      uint8_t log2_sector_size, unsigned sector_count);

__END_DECLS
