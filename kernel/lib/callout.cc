#include "callout.h"

extern callout_t const ___callout_array_st[];
extern callout_t const ___callout_array_en[];

size_t callout_call(callout_type_t type)
{
    for (callout_t const *callout = ___callout_array_st;
         callout < ___callout_array_en; ++callout) {
        if (callout->type == type)
            callout->fn(callout->userarg);
    }

    return ___callout_array_en - ___callout_array_st;
}
