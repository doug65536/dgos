#pragma once
#include "types.h"

enum struct bootparam_t {
	ap_entry_point,
	vbe_mode_info,
	boot_device
};

extern "C" uintptr_t bootinfo_parameter(bootparam_t param);
