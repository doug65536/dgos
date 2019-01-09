#include "tui.h"
#include "bioscall.h"
#include "ctors.h"
#include "farptr.h"

extern char __int15_handler[];
extern "C" far_ptr_t __int15_old;

__asm__(
".section .early\n"
".code16\n"
".align 16\n"
".global __int15_handler\n"
"__int15_handler:"
"cmp $0x5305,%ax\n"
"jnz 0f\n"
"hlt\n"
"0:\n"
"ljmpw %cs:*__int15_old\n"
".global __int15_old\n"
"__int15_old:\n"
".space 4\n"
".code32\n"
".section .text\n"
);

_constructor(ctor_console) void init_console()
{
    // Hook idle interrupt
    far_ptr_t *int15_vec = (far_ptr_t*)(0x15 * 4);
    __int15_old = *int15_vec;
    *int15_vec = __int15_handler;
    int15_vec->adj_seg(0);
}

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

static constexpr int tick24h = (86400 * 182) / 10;

// Get ticks since midnight (54.9ms units)
int64_t systime()
{
    static int days;

    bios_regs_t regs{};
    regs.eax = 0;
    bioscall(&regs, 0x1A);

    // Tick count in CX:DX
    int this_time = (regs.ecx << 16) | (regs.edx & 0xFFFF);

    // Make sure tick count wraps to zero every day
    // Because we separately track days with AL flag
    this_time %= tick24h;

    // AL == 1 if midnight has passed
    days += (regs.eax & 0xFF) == 1;

    return int64_t(days * tick24h) + this_time;
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
