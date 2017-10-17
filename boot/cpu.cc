#include "cpu.h"
#include "paging.h"
#include "bootsect.h"
#include "exception.h"
#include "farptr.h"
#include "string.h"

extern gdt_entry_t gdt[];

table_register_64_t idtr_64;
static table_register_t idtr_16 = {
    4 * 256,
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

uint16_t toggle_a20(uint8_t enable)
{
    uint8_t value;
    enable = (enable != 0) << 1;
    __asm__ __volatile__ (
        "inb $0x92,%1\n\t"
        "andb $~2,%1\n\t"
        "orb %0,%1\n\t"
        "wbinvd\n\t"
        "outb %1,$0x92\n\t"
        : "+c" (enable)
        , "=a" (value)
    );
    return 0;
}

// Returns true if the CPU supports that leaf
uint16_t cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx)
{
    // Automatically check for support for the leaf
    if ((eax & 0x7FFFFFFF) != 0) {
        cpuid(output, eax & 0x80000000U, 0);
        if (output->eax < eax)
            return 0;
    }

    __asm__ __volatile__ (
        "cpuid"
        : "=a" (output->eax), "=c" (output->ecx)
        , "=d" (output->edx), "=b" (output->ebx)
        : "a" (eax), "c" (ecx)
    );

    return 1;
}

uint16_t cpu_has_long_mode()
{
    cpuid_t cpuinfo;
    return cpuid(&cpuinfo, 0x80000001U, 0) &&
            (cpuinfo.edx & (1<<29));
}

uint16_t cpu_has_no_execute()
{
    cpuid_t cpuinfo;
    return cpuid(&cpuinfo, 0x80000001U, 0) &&
            (cpuinfo.edx & (1<<20));
}

uint16_t cpu_has_global_pages()
{
    cpuid_t cpuinfo;
    return cpuid(&cpuinfo, 1U, 0) &&
            (cpuinfo.edx & (1<<13));
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

bool need_a20_toggle;
extern table_register_64_t gdtr;

bool nx_available;
uint32_t gp_available;

void cpu_init()
{
	nx_available = cpu_has_no_execute();
	gp_available = cpu_has_global_pages() ? (1 << 7) : 0;
}

void cpu_a20_enterpm()
{
    if (need_a20_toggle)    
        toggle_a20(1);
}

void cpu_a20_exitpm()
{
    if (need_a20_toggle)
        toggle_a20(0);
}

// If src is > 0, copy size bytes from src to address
// If src is == 0, jumps to entry point in address, passing size as an argument
void copy_or_enter(uint64_t address, uint32_t src, uint32_t size)
{
    // Fast-path copies to below 4GB line
    //if (src > 0 && address < 0x100000000 && address + size <= 0x100000000) {
    //    memcpy((void*)address, (void*)src, size);
    //    return;
    //}
    
    uint16_t intf = disable_interrupts();

    uint32_t pdbr = paging_root_addr();

	struct {
		uint64_t address;
		uint32_t src;
		uint32_t size;
	} params = {
		address,
		src,
		size
	};

    __asm__ __volatile__ (
        // Enable CR4.PAE (bit 5)
        "movl %%cr4,%%eax\n\t"
        "btsl $5,%%eax\n\t"
        "orl %[gp_available],%%eax\n\t"
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

        "lidtq (%%eax)\n\t"

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
		// (before screwing up stack pointer with call)
		"mov (%[params]),%%rdi\n\t"
		"movl 8(%[params]),%%esi\n\t"
		"movl 12(%[params]),%%ecx\n\t"

		// Check whether it is copy or entry
        "testl %%esi,%%esi\n\t"
        "jz 2f\n\t"

        // Copy memory
        "cld\n\t"
        "rep movsb\n\t"
        "jmp 3f\n\t"

        // Enter kernel
        "2:\n\t"
        "mov %%rsp,%%r15\n\t"
        "andq $-16,%%rsp\n\t"
        "call *%%rdi\n\t"
        // Should not be possible to reach here
        "mov %%r15,%%rsp\n\t"

        "3:\n\t"

        // Far return to 32 bit compatibility mode code segment
		"lret\n\t"

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

        // Load 32 bit selectors
        "movl %[gdt_data32],%%eax\n\t"
        "movl %%eax,%%ds\n\t"
        "movl %%eax,%%es\n\t"
        "movl %%eax,%%fs\n\t"
        "movl %%eax,%%ss\n\t"

        // Load 32-bit code segment
        "ljmp %[gdt_code32],$0f\n\t"
        "nop\n\t"

        // 32-bit addressing mode reenabled
        ".code32\n\t"
        "0:\n\t"

        "lea %[idtr_16],%%esi\n\t"
        "lidt (%%esi)\n\t"
        :
		: [params] "b" (&params)
        , [pdbr] "m" (pdbr)
        , [gdtr] "m" (gdtr)
        , [idtr_64] "m" (idtr_64)
        , [idtr_16] "m" (idtr_16)
        , [gdt_code64] "i" (GDT_SEL_KERNEL_CODE64)
        , [gdt_data64] "i" (GDT_SEL_KERNEL_DATA64)
        , [gdt_code32] "i" (GDT_SEL_KERNEL_CODE32)
        , [gdt_data32] "i" (GDT_SEL_KERNEL_DATA32)
        , [gdt_code16] "i" (GDT_SEL_KERNEL_CODE16)
        , [gdt_data16] "i" (GDT_SEL_KERNEL_DATA16)
        , [nx_available] "m" (nx_available)
        , [gp_available] "m" (gp_available)
        : "eax", "ecx", "edx", "esi", "edi", "memory"
    );

    toggle_interrupts(intf);
}
