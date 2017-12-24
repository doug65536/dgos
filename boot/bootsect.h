#pragma once

#include "types.h"

extern "C" uint8_t boot_drive;
extern "C" uint32_t vbe_info_vector;

extern "C" void halt(char const*);

uint8_t read_lba_sectors(char *buf, uint8_t drive,
                         uint32_t lba, uint16_t count);
