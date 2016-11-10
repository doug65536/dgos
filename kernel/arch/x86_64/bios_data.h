#pragma once
// Macros to generalize reading from the BIOS data area
// Allows it to be remapped without having to edit code

// Use identity mapping for now
#define READ_BIOS_DATA_AREA(type,address) (*(type*)address)
