#pragma once
#include "types.h"

struct bios_regs_t {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
    uint32_t eflags_out;

    bios_regs_t()
        : ds(0)
        , es(0)
        , fs(0)
        , gs(0)
    {
    }

    // Helper to get carry flag
    bool flags_CF() const
    {
        return eflags_out & (1 << 0);
    }

    // Helper to get zero flag
    bool flags_ZF() const
    {
        return eflags_out & (1 << 6);
    }

    uint8_t ah_if_carry() const
    {
        if (flags_CF())
            return (eax >> 8) & 0xFF;
        return 0;
    }
};

extern "C" void bioscall(bios_regs_t *regs, uint32_t intr);
