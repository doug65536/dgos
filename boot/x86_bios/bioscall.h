#pragma once
#include "types.h"

struct bios_regs_t {
    // offset 0
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    uint32_t esi = 0;
    uint32_t edi = 0;
    uint32_t ebp = 0;
    // offset 28
    uint16_t ds = 0;
    uint16_t es = 0;
    uint16_t fs = 0;
    uint16_t gs = 0;
    // offset 36
    uint32_t eflags = 0x202;    // IF=1 bit1=1

    // Helper to get carry flag
    bool flags_CF() const
    {
        return eflags & (1 << 0);
    }

    // Helper to get zero flag
    bool flags_ZF() const
    {
        return eflags & (1 << 6);
    }

    uint8_t ah_if_carry() const
    {
        if (flags_CF())
            return (eax >> 8) & 0xFF;
        return 0;
    }
};

static_assert(sizeof(bios_regs_t) == 40, "Unexpected structure size");

extern "C" void bioscall(bios_regs_t *regs, uint32_t intr,
                         bool cpu_a20_toggle = true);
