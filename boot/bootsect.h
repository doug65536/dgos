#pragma once

#include "types.h"

extern "C" uint8_t boot_drive;
extern "C" uint32_t vbe_info_vector;

extern "C" void halt(tchar const*);

bool read_lba_sectors(char *buf, uint64_t lba, uint16_t count);

#ifdef __efi
#define HALT(s) halt(TEXT(s))
#else
#define HALT(s) halt(s)
#endif
