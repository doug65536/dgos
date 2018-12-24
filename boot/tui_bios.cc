#include "tui.h"
#include "bioscall.h"

int readkey()
{
    bios_regs_t regs{};
    regs.eax = 0x1000;
    bioscall(&regs, 0x16);

    return regs.eax & 0xFFFF;
}

mouse_evt readmouse()
{
    return {};
}

// Get ticks since midnight (54.9ms units)
int systime()
{
    bios_regs_t regs{};
    regs.eax = 0;
    bioscall(&regs, 0x1A);
    return (regs.ecx << 16) | (regs.edx & 0xFFFF);
}

bool wait_input(uint32_t ms_timeout)
{
    uint32_t expire = systime() + ((ms_timeout * 10) / 549);

    while (systime() < expire) {
        if (pollkey())
            return true;
        idle();
    }
    return false;
}

void idle()
{
    // Perform APM "CPU Idle" call
    bios_regs_t regs{};
    regs.eax = 0x5305;
    bioscall(&regs, 0x15);
}

// Returns true if a key is availble
bool pollkey()
{
    bios_regs_t regs{};

    regs.eax = 0x1100;
    bioscall(&regs, 0x16);

    if (!regs.flags_ZF())
        return true;

    return false;
}
