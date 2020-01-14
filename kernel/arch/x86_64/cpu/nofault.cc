#include "nofault.h"

bool nofault_ip_in_range(uintptr_t ip)
{
    return ip - uintptr_t(__nofault_text_st) < __nofault_text_sz;
}

uintptr_t nofault_find_landing_pad(uintptr_t ip)
{
    for (size_t i = 0, e = __nofault_tab_sz >> 3; i < e; ++i) {
        if (ip >= __nofault_tab_st[i] && ip < __nofault_tab_en[i]) {
            // Put CPU on landing pad with rax == -1
            return __nofault_tab_lp[i];
        }
    }
    return 0;
}

size_t ___rodata_fixup_swapgs_cnt =
        ___rodata_fixup_swapgs_en -
        ___rodata_fixup_swapgs_st;

bool nofault_is_user_gsbase(uintptr_t ip)
{
    size_t st = 0;
    size_t en = ___rodata_fixup_swapgs_cnt;
    while (st < en) {
        size_t md = st + ((en - st) >> 1);
        user_gsbase_ip_range_t &item = ___rodata_fixup_swapgs_st[md];
        en = (ip < item.st) ? md : en;
        st = (ip >= item.st) ? st : (md + 1);
    }
    user_gsbase_ip_range_t &item = ___rodata_fixup_swapgs_st[st];
    return ip >= item.st && ip < item.en;
}
