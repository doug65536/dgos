#pragma once
#include "types.h"

// Linker script magic
extern char __nofault_text_st[];
extern char __nofault_text_en[];
extern uint64_t __nofault_text_sz;

struct user_gsbase_ip_range_t {
    uintptr_t st;
    uintptr_t en;
};

extern uintptr_t __nofault_tab_st[];
extern uintptr_t __nofault_tab_en[];
extern uintptr_t __nofault_tab_lp[];
extern size_t __nofault_tab_sz;
extern user_gsbase_ip_range_t ___rodata_fixup_swapgs_st[];
extern user_gsbase_ip_range_t ___rodata_fixup_swapgs_en[];
extern size_t ___rodata_fixup_swapgs_cnt;

bool nofault_ip_in_range(uintptr_t ip);
uintptr_t nofault_find_landing_pad(uintptr_t ip);

bool nofault_is_user_gsbase(uintptr_t ip);
