#include "callout.h"

extern callout_t ___callout_array_st[];
extern callout_t ___callout_array_en[];

size_t callout_call(int32_t type)
{
    for (callout_t *callout = ___callout_array_st;
         callout < ___callout_array_en; ++callout) {
        if (callout->type == type)
            callout->fn(callout->userarg);
    }

    return ___callout_array_en - ___callout_array_st;
}
