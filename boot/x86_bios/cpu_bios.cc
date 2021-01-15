#include "x86/cpu_x86.h"
#include "paging.h"
#include "x86/gdt_sel.h"
#include "screen.h"
#include "bioscall.h"
#include "elf64decl.h"

_section(".lowdata")
bool cpu_a20_need_toggle;

void cpu_a20_enterpm()
{
    if (cpu_a20_need_toggle) {
        cpu_a20_toggle(true);
        cpu_a20_wait(true);
    }
}

void cpu_a20_exitpm()
{
    if (cpu_a20_need_toggle) {
        cpu_a20_toggle(false);
        cpu_a20_wait(false);
    }
}

void cpu_a20_init()
{
    //bios_regs_t regs{};
}

bool cpu_a20_toggle(bool enabled)
{
    enum struct a20_method {
        unknown,
        bios,
        port92,
        keybd,
        unspecified
    };

    static a20_method method = a20_method::unknown;

    bios_regs_t regs{};

    if (method == a20_method::unknown) {
        // Ask the BIOS which method to use
        regs.eax = 0x2403;
        bioscall(&regs, 0x15, false);
        if (!regs.flags_CF() && (regs.ebx & 3)) {
            if (regs.ebx & 2) {
                // Use port 0x92
                method = a20_method::port92;
            } else if (regs.ebx & 1) {
                // Use the keyboard controller
                method = a20_method::keybd;
            } else {
                // Try to use the BIOS
                regs.eax = enabled ? 0x2401 : 0x2400;
                bioscall(&regs, 0x15, false);
                if (!regs.flags_CF()) {
                    method = a20_method::bios;
                    return true;
                }
            }
        }

        // Still don't know? Guess!
        if (method == a20_method::unknown) {
            PRINT("BIOS doesn't support A20! Guessing port 0x92...");
            method = a20_method::port92;
        }
    }

    switch (method) {
    case a20_method::port92:
        uint8_t temp;
        __asm__ __volatile__ (
            "inb $ 0x92,%b[temp]\n\t"
            "andb $ ~2,%b[temp]\n\t"
            "orb %b[bit],%b[temp]\n\t"
            "outb %b[temp],$ 0x92\n\t"
            : [temp] "=&a" (temp)
            : [bit] "ri" (enabled ? 2 : 0)
        );
        return true;

    case a20_method::keybd:
        // Command write
        while (inb(0x64) & 2);
        outb(0x64, 0xD1);

        // Write command
        while (inb(0x64) & 2);
        outb(0x60, enabled ? 0xDF : 0xDD);

        // Wait for empty and cover signal propagation delay
        while (inb(0x64) & 2);

        return true;

    case a20_method::bios:
        regs.eax = enabled ? 0x2401 : 0x2400;
        bioscall(&regs, 0x15, false);
        return !regs.flags_CF();

    default:
        return false;

    }
}

extern "C" void run_code64(void (*fn)(void *), void *arg);
