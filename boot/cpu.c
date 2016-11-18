#include "cpu.h"
#include "paging.h"
#include "bootsect.h"
#include "exception.h"

gdt_entry_t gdt[] = {
    GDT_MAKE_EMPTY(),
    // 64 bit kernel code and data
    GDT_MAKE_CODESEG64(0),
    GDT_MAKE_DATASEG64(0),
    // 32 bit kernel code and data
    GDT_MAKE_CODESEG32(0),
    GDT_MAKE_DATASEG32(0),
    // 16 bit kernel code and data
    GDT_MAKE_CODESEG16(0),
    GDT_MAKE_DATASEG16(0),
    // 64 bit user code and data
    GDT_MAKE_CODESEG64(3),
    GDT_MAKE_DATASEG64(3),
    // 32 bit user code and data
    GDT_MAKE_CODESEG32(3),
    GDT_MAKE_DATASEG32(3)
};

table_register_64_t idtr_64;
table_register_t idtr_16 = {
    8 * 256,
    0,
    0
};

idt_entry_64_t idt[32];

#define GDT_SEL_KERNEL_CODE64   0x08
#define GDT_SEL_KERNEL_DATA64   0x10
#define GDT_SEL_KERNEL_CODE32   0x18
#define GDT_SEL_KERNEL_DATA32   0x20
#define GDT_SEL_KERNEL_CODE16   0x28
#define GDT_SEL_KERNEL_DATA16   0x30
#define GDT_SEL_USER_CODE64     0x38
#define GDT_SEL_USER_DATA64     0x40
#define GDT_SEL_USER_CODE32     0x48
#define GDT_SEL_USER_DATA32     0x50

// See if A20 is blocked or enabled
static uint16_t check_a20()
{
    uint16_t enabled;
    __asm__ __volatile__ (
        "mov $0xFFFF,%%ax\n\t"
        "mov %%ax,%%fs\n\t"
        "pushf\n\t"
        "cli\n\t"
        "wbinvd\n\t"
        "pushw 0\n\t"
        "pushw %%fs:16\n\t"
        "movw $0,0\n\t"
        "wbinvd\n\t"
        "movw $0xface,%%fs:16\n\t"
        "wbinvd\n\t"
        "xorw %%ax,%%ax\n\t"
        "cmpw %%ax,0\n\t"
        "sete %%al\n\t"
        "popw %%fs:16\n\t"
        "popw 0\n\t"
        "popf\n\t"
        : "=a" (enabled)
    );
    return enabled;
}

// Returns BIOS error code, or zero on success
//  01h keyboard controller is in secure mode
//  86h function not supported
static uint16_t toggle_a20(uint8_t enable)
{
    uint8_t value;
    __asm__ __volatile__ (
        "inb $0x92,%1\n\t"
        "andb $~2,%1\n\t"
        "orb %0,%1\n\t"
        "wbinvd\n\t"
        "outb %1,$0x92\n\t"
        : "=c" (enable), "=a" (value)
        : "0" ((enable != 0) << 1)
    );
    return 0;
}

#if 0
// Returns the value that was at the location
// If the returned value is not equal to expect
// then the value was not replaced
int64_t cmpxchg8b(
        int64_t volatile *value, int64_t expect, int64_t replacement);
int64_t cmpxchg8b(
        int64_t volatile *value, int64_t expect, int64_t replacement)
{
    __asm__ __volatile__ (
        "lock cmpxchg8b (%[value])\n\t"
        : "+A" (expect)
        : [value] "SD" (value),
          "b" ((uint32_t)replacement & 0xFFFFFFFF),
          "c" ((uint32_t)(replacement >> 32))
        : "memory"
    );
    return expect;
}

int64_t atomic_inc64(int64_t volatile *value);
int64_t atomic_inc64(int64_t volatile *value)
{
    int64_t stale = *value;
    int64_t fresh;
    for (;;) {
        fresh = cmpxchg8b(value, stale, stale + 1);
        if (fresh == stale)
            return fresh + 1;
        stale = fresh;
    }
}
#endif

// Returns true if the CPU supports that leaf
uint16_t cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx)
{
    // Automatically check for support for the leaf
    if ((eax & 0x7FFFFFFF) != 0) {
        cpuid(output, eax & 0x80000000, 0);
        if (output->eax < eax)
            return 0;
    }

    __asm__ __volatile__ (
        "cpuid"
        : "=a" (output->eax), "=b" (output->ebx),
          "=d" (output->edx), "=c" (output->ecx)
        : "a" (eax), "c" (ecx)
    );

    return 1;
}

uint16_t cpu_has_long_mode(void)
{
    cpuid_t cpuinfo;
    return cpuid(&cpuinfo, 0x80000001, 0) &&
            (cpuinfo.edx & (1<<29));
}

uint16_t cpu_has_no_execute(void)
{
    cpuid_t cpuinfo;
    return cpuid(&cpuinfo, 0x8000001, 0) &&
            (cpuinfo.edx & (1<<20));
}

static uint16_t disable_interrupts()
{
    uint32_t int_enabled;
    __asm__ __volatile__ (
        "pushfl\n"
        "popl %0\n"
        "shrl $9,%0\n\t"
        "andl $1,%0\n\t"
        "cli\n\t"
        : "=r" (int_enabled)
    );
    return !!int_enabled;
}

static void enable_interrupts()
{
    __asm__ __volatile__ ("sti");
}

static void toggle_interrupts(uint16_t enable)
{
    if (enable)
        enable_interrupts();
    else
        disable_interrupts();
}

static void idt_init()
{
    for (size_t i = 0; i < 32; ++i) {
        idt[i].type_attr = IDT_PRESENT | IDT_INTR;
        idt[i].selector = IDT_SEL;
        idt[i].offset_lo = isr_table[i];
    }

    idtr_64.base_lo = (uint16_t)(uint32_t)idt;
    idtr_64.limit = 32 * sizeof(*idt) - 1;
}

// If src is > 0, copy size bytes from src to address
// If src is == 0, jumps to entry point in address, passing size as an argument
void copy_or_enter(uint64_t address, uint32_t src, uint32_t size)
{
    if (idtr_64.base_lo == 0)
        idt_init();

    uint16_t intf = disable_interrupts();
    uint16_t was_a20 = check_a20();
    if (!was_a20) {
        toggle_a20(1);
        if (!check_a20()) {
            halt("A20 not enabled!");
        }
    }

    table_register_64_t gdtr = {
        sizeof(gdt) - 1,
        (uint16_t)(uint32_t)gdt,
        0,
        0,
        0
    };

    uint32_t pdbr = paging_root_addr();

    uint16_t nx_available = cpu_has_no_execute();

    __asm__ __volatile__ (
        "lgdt %[gdtr]\n\t"

        // Enable protected mode
        "movl %%cr0,%%eax\n\t"
        "incl %%eax\n\t"
        "movl %%eax,%%cr0\n\t"

        // Clear prefetch queue
        "jmp 0f\n\t"

        // Far jump to load cs selector
        "0:\n\t"
        "ljmpl %[gdt_code32],$0f\n\t"

        // In protected mode
        // Switch assembler to assume 32 bit mode
        ".code32\n\t"
        "0:\n\t"

        // Load 32-bit data segments
        "movl %[gdt_data32],%%eax\n\t"
        "movl %%eax,%%ds\n\t"
        "movl %%eax,%%es\n\t"
        "movl %%eax,%%fs\n\t"
        "movl %%eax,%%ss\n\t"

        // Enable CR4.PAE (bit 5)
        "movl %%cr4,%%eax\n\t"
        "btsl $5,%%eax\n\t"
        "movl %%eax,%%cr4\n\t"

        // Load PDBR
        "movl %[pdbr],%%eax\n\t"
        "movl %%eax,%%cr3\n\t"

        // Enable long mode IA32_EFER.LME (bit 8) MSR 0xC0000080
        "movl $0xC0000080,%%ecx\n"
        "rdmsr\n\t"
        "btsl $8,%%eax\n\t"
        "cmpw $0,%[nx_available]\n\t"
        "jz 0f\n\t"
        "btsl $11,%%eax\n\t"
        "0:"
        "wrmsr\n\t"

        // Enable paging (CR0.PG (bit 31)
        "movl %%cr0,%%eax\n\t"
        "btsl $31,%%eax\n\t"
        "movl %%eax,%%cr0\n\t"

        "jmp 0f\n\t"
        "0:"

        // Now in 64 bit compatibility mode (still really 32 bit)

        "lgdt %[gdtr]\n\t"

        // Far jump to selector that has L bit set (64 bit)
        "lea %[idtr_64],%%eax\n\t"
        "lcall %[gdt_code64],$0f\n\t"

        // Returned from long mode, back to compatibility mode

        // Load 32 bit data segments
        "movl %[gdt_data32],%%eax\n\t"
        "movl %%eax,%%ds\n\t"
        "movl %%eax,%%es\n\t"
        "movl %%eax,%%fs\n\t"
        "movl %%eax,%%ss\n\t"

        // Jump over 64 bit code
        "jmp 1f\n\t"

        // Long mode
        ".code64\n\t"
        "0:\n\t"

        "lidt (%%eax)\n\t"

        // Deliberate crash to test exception handlers
        //"movl $0x56363,%%eax\n\t"
        //"decl (%%eax)\n\t"

        // Load 64 bit data segments
        "movl %[gdt_data64],%%eax\n\t"
        "movl %%eax,%%ds\n\t"
        "movl %%eax,%%es\n\t"
        "movl %%eax,%%fs\n\t"
        "movl %%eax,%%ss\n\t"

        // Load copy/entry parameters
        "movq %[address],%%rdi\n\t"
        "movl %[size],%%ecx\n\t"
        "movl %[src],%%esi\n\t"

        // Check whether it is copy or entry
        "testl %%esi,%%esi\n\t"
        "jz 2f\n\t"

        // Copy memory
        "cld\n\t"
        "rep movsb\n\t"
        "jmp 3f\n\t"

        // Enter kernel
        "2:\n\t"
        "call *%%rdi\n\t"
        // Should not be possible to reach here

        "3:\n\t"

        // Far return to 32 bit compatibility mode code segment
        "retf\n\t"

        "1:\n\t"
        ".code32\n\t"

        // Disable paging
        "mov %%cr0,%%eax\n\t"
        "btr $31,%%eax\n\t"
        "mov %%eax,%%cr0\n\t"

        // Disable long mode
        "movl $0xC0000080,%%ecx\n"
        "rdmsr\n\t"
        "andl $~0x100,%%eax\n\t"
        "wrmsr\n\t"

        "lgdt %[gdtr]\n\t"

        // Load 16 bit selectors
        "movl %[gdt_data16],%%eax\n\t"
        "movl %%eax,%%ds\n\t"
        "movl %%eax,%%es\n\t"
        "movl %%eax,%%fs\n\t"
        "movl %%eax,%%ss\n\t"

        // Load 16-bit code segment
        "ljmp %[gdt_code16],$0f\n\t"
        "nop\n\t"

        // 16-bit addressing mode reenabled
        ".code16gcc\n\t"
        "0:\n\t"

        // Disable protected mode
        "movl %%cr0,%%eax\n\t"
        "decl %%eax\n\t"
        "movl %%eax,%%cr0\n\t"

        // Clear prefetch queue
        "jmp 0f\n\t"
        "nop\n\t"
        "0:\n\t"

        // Jump to real mode
        "ljmp $0,$0f\n\t"
        "nop\n\t"
        "0:"

        // In real mode
        // Load real mode segments
        "xorw %%ax,%%ax\n\t"
        "movw %%ax,%%ds\n\t"
        "movw %%ax,%%es\n\t"
        "movw %%ax,%%fs\n\t"
        "movw %%ax,%%ss\n\t"

        "leaw %[idtr_16],%%si\n\t"
        "lidt (%%si)\n\t"
        :
        :
                [address] "m" (address),
                [src] "m" (src),
                [size] "m" (size),
                [pdbr] "m" (pdbr),
                [gdtr] "m" (gdtr),
                [idtr_64] "m" (idtr_64),
                [idtr_16] "m" (idtr_16),
                [gdt_code64] "i" (GDT_SEL_KERNEL_CODE64),
                [gdt_data64] "i" (GDT_SEL_KERNEL_DATA64),
                [gdt_code32] "i" (GDT_SEL_KERNEL_CODE32),
                [gdt_data32] "i" (GDT_SEL_KERNEL_DATA32),
                [gdt_code16] "i" (GDT_SEL_KERNEL_CODE16),
                [gdt_data16] "i" (GDT_SEL_KERNEL_DATA16),
                [nx_available] "m" (nx_available)
        : "eax", "ecx", "edx", "esi", "edi", "memory"
    );

    if (!was_a20)
        toggle_a20(0);

    toggle_interrupts(intf);
}
