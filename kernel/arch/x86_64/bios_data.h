#pragma once
#include "types.h"
#include "export.h"

// Macros to generalize reading from the BIOS data area
// Allows it to be remapped without having to edit code

extern HIDDEN char *zero_page;

// Use identity mapping for now
#define BIOS_DATA_AREA(type,address) ((type*)(zero_page + address))

#define BIOS_VGA_PORT_BASE   0x463
