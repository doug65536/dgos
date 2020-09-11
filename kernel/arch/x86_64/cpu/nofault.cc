#include "nofault.h"

bool nofault_ip_in_range(uintptr_t ip)
{
    return ip - uintptr_t(__nofault_text_st) < __nofault_text_sz;
}

uintptr_t nofault_find_landing_pad(uintptr_t ip)
{
    for (size_t i = 0, e = __nofault_tab_sz >> 3; i < e; ++i) {
        if (ip >= __nofault_tab_st[i] && ip < __nofault_tab_en[i]) {
            return __nofault_tab_lp[i];
        }
    }
    return 0;
}
