#pragma once
#include "types.h"

// Linker script magic
extern char __nofault_text_st[];
extern char __nofault_text_en[];
extern uint64_t __nofault_text_sz;

extern uintptr_t __nofault_tab_st[];
extern uintptr_t __nofault_tab_en[];
extern uintptr_t __nofault_tab_lp[];
extern uint64_t __nofault_tab_sz;

bool nofault_ip_in_range(uintptr_t ip);
uintptr_t nofault_find_landing_pad(uintptr_t ip);
