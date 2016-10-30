#include "code16gcc.h"
#include "cpu.h"
#include "paging.h"
#include "bootsect.h"

gdt_entry_t gdt[] = {
    GDT_MAKE_EMPTY(),
    // Kernel code and data
    GDT_MAKE_CODESEG32(0),  // 0x08
    GDT_MAKE_DATASEG32(0),  // 0x10
    // User code and data
    GDT_MAKE_CODESEG32(3),  // 0x18
    GDT_MAKE_DATASEG32(3),  // 0x20
    // 16 bit code and data
    GDT_MAKE_CODESEG16(0),  // 0x28
    GDT_MAKE_DATASEG16(0),  // 0x30
    // 64 bit kernel code and data
    GDT_MAKE_CODESEG64(0),  // 0x38
    GDT_MAKE_DATASEG64(0),  // 0x40
    // 64 bit user code and data
    GDT_MAKE_CODESEG64(3),  // 0x48
    GDT_MAKE_DATASEG64(3)  // 0x50
};

// See if A20 is blocked or enabled
uint16_t check_a20()
{
    uint16_t enabled;
    __asm__ __volatile__ (
        "mov $0xFFFF,%%ax\n\t"
        "mov %%ax,%%fs\n\t"
        "pushf\n\t"
        "cli\n\t"
        "pushw 0\n\t"
        "movw $0,0\n\t"
        "movw $1,%%fs:16\n\t"
        "xorw %%ax,%%ax\n\t"
        "cmpw $0,0\n\t"
        "sete %%al\n\t"
        "popw 0\n\t"
        "popf\n\t"
        : "=a" (enabled)
    );
    return enabled;
}

// Returns BIOS error code, or zero on success
//  01h keyboard controller is in secure mode
//  86h function not supported
uint16_t toggle_a20(uint8_t enable)
{
    uint8_t value;
    __asm__ __volatile__ (
        "inb $0x92,%1\n\t"
        "andb $~2,%1\n\t"
        "orb %0,%1\n"
        "outb %1,$0x92\n\t"
        : "=c" (enable), "=a" (value)
        : "0" ((enable != 0) << 1)
    );
    return 0;
//    uint16_t ax = 0x2400 | (enable != 0);
//
//    // INT 15 AX=2401
//    __asm__ __volatile__ (
//        // Flush cache
//        "wbinvd\n\t"
//        //"outb %%al,$0x80\n\t"
//        // Enable A20
//        "int $0x15\n\t"
//        "setc %%al\n\t"
//        "negb %%al\n\t"
//        "andb %%al,%%ah\n\t"
//        "movzbw %%ah,%%ax\n\t"
//        //"out %%al,$0x80\n\t"
//        : "=a" (ax)
//        : "a" (ax)
//    );
//
//    return ax;
}

// Returns true if the CPU supports that leaf
uint16_t cpuid(cpuid_t *output, uint32_t eax, uint32_t ecx)
{
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

static uint16_t cpu_has_apic(void)
{
    cpuid_t cpuinfo;
    return cpuid(&cpuinfo, 9, 0) && (cpuinfo.edx & (1<<9));
}

void outb(uint16_t dx, uint8_t al)
{
    __asm__ __volatile__ (
        "outb %b1,%w0\n\t"
        :
        : "d" (dx), "a" (al)
    );
}

void outw(uint16_t dx, uint16_t ax)
{
    __asm__ __volatile__ (
        "outw %w1,%w0\n\t"
        :
        : "d" (dx), "a" (ax)
    );
}

void outl(uint16_t dx, uint32_t eax)
{
    __asm__ __volatile__ (
        "outl %1,%w0\n\t"
        :
        : "d" (dx), "a" (eax)
    );
}

uint8_t inb(uint16_t dx)
{
    int8_t al;
    __asm__ __volatile__ (
        "inb %w1,%b0\n\t"
        : "=a" (al)
        : "d" (dx)
    );
    return al;
}

uint16_t inw(uint16_t dx)
{
    int16_t ax;
    __asm__ __volatile__ (
        "inw %w1,%w0\n\t"
        : "=a" (ax)
        : "d" (dx)
    );
    return ax;
}

uint32_t inl(uint16_t dx)
{
    int32_t eax;
    __asm__ __volatile__ (
        "inl %w1,%0\n\t"
        : "=a" (eax)
        : "d" (dx)
    );
    return eax;
}

#define PIC1            0x20	// IO base address for master PIC
#define PIC2            0xA0	// IO base address for slave PIC
#define PIC1_COMMAND	PIC1
#define PIC1_DATA       (PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA       (PIC2+1)

#define ICW1_ICW4       0x01	// ICW4 (not) needed
#define ICW1_SINGLE     0x02	// Single (cascade) mode
#define ICW1_INTERVAL4	0x04	// Call address interval 4 (8)
#define ICW1_LEVEL      0x08	// Level triggered (edge) mode
#define ICW1_INIT       0x10	// Initialization - required!

#define ICW4_8086       0x01	// 8086/88 (MCS-80/85) mode
#define ICW4_AUTO       0x02	// Auto (normal) EOI
#define ICW4_BUF_SLAVE	0x08	// Buffered mode/slave
#define ICW4_BUF_MASTER	0x0C	// Buffered mode/master
#define ICW4_SFNM       0x10	// Special fully nested (not)

void io_wait()
{
    __asm__ __volatile__ (
        "outb %al,$0x80\n\t"
    );
}

void init_8259_pic(uint8_t pic1_base, uint8_t pic2_base)
{
    unsigned char a1, a2;

    // save masks
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);
    io_wait();

    // starts the initialization sequence (in cascade mode)
    outb(PIC1_COMMAND, ICW1_INIT+ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT+ICW1_ICW4);
    io_wait();

    // ICW2: Master PIC vector offset
    outb(PIC1_DATA, pic1_base);
    // ICW2: Slave PIC vector offset
    outb(PIC2_DATA, pic2_base);
    io_wait();

    // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    outb(PIC1_DATA, 4);
    // ICW3: tell Slave PIC its cascade identity (0000 0010)
    outb(PIC2_DATA, 2);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, a1);   // restore saved masks.
    outb(PIC2_DATA, a2);
    io_wait();
}

#define PIC_EOI		0x20

void ack_irq(uint8_t irq)
{
    // Ack PIC 2 if IRQ was from PIC2
    if(irq >= 8)
        outb(PIC2_COMMAND,PIC_EOI);

    // Always ack PIC 1 due to cascade
    outb(PIC1_COMMAND,PIC_EOI);
}

void load_gdt(void *gdt, size_t size)
{
    table_register_t dtr;

    dtr.limit = size - 1;
    dtr.base_lo = (uint16_t)(uint32_t)gdt;
    dtr.base_hi = (uint16_t)((uint32_t)gdt >> 16);

    __asm__ __volatile__ (
        "lgdtl %0"
        :
        : "m" (dtr)
    );
}

void init_cpu()
{
    __asm__ __volatile__ ( "cli" );

    toggle_a20(1);

    init_8259_pic(0x20, 0x28);

    __asm__ __volatile__ ( "sti" );
}

uint16_t disable_interrupts()
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

void enable_interrupts()
{
    __asm__ __volatile__ ("sti");
}

void toggle_interrupts(uint16_t enable)
{
    if (enable)
        enable_interrupts();
    else
        disable_interrupts();
}

void copy_to_address(uint64_t *address, void *src, uint32_t size)
{
    uint16_t intf = disable_interrupts();
    toggle_a20(1);
    if (!check_a20()) {
        halt("A20 not enabled!");
    }

    load_gdt(&gdt, sizeof(gdt));
    uint32_t pdbr = paging_root_addr();

    __asm__ __volatile__ (
        // Enable protected mode
        "movl %%cr0,%%eax\n\t"
        "orl $0x21,%%eax\n\t"
        "movl %%eax,%%cr0\n\t"

        // Clear prefetch queue
        "jmp 0f\n\t"
        "nop\n\t"
        "0:\n\t"

        // Far jump to load cs selector
        "ljmpl $8,$0f\n\t"
        "nop\n\t"

        // In protected mode
        // Switch assembler to assume 32 bit mode
        ".code32\n\t"
        "0:\n\t"

        // Load 32-bit data segments
        "mov $0x10,%%ax\n\t"
        "movw %%ax,%%ds\n\t"
        "movw %%ax,%%es\n\t"

        // Enable CR4.PAE (bit 5)
        "movl %%cr4,%%eax\n\t"
        "orl $0x20,%%eax\n\t"
        "movl %%eax,%%cr4\n\t"

        // Load PDBR
        "movl %%ebx,%%cr3\n\t"

        // Enable long mode IA32_EFER.LME (bit 8) MSR 0xC0000080
        "pushl %%ecx\n\t"
        "movl $0xC0000080,%%ecx\n"
        "rdmsr\n\t"
        "orl $0x100,%%eax\n\t"
        "wrmsr\n\t"
        "popl %%ecx\n\t"

        // Enable paging (CR0.PG (bit 31)
        "movl %%cr0,%%eax\n\t"
        "orl $0x80000000,%%eax\n\t"
        "movl %%eax,%%cr0\n\t"

        // Now in 64 bit compatibility mode (still really 32 bit)

        // Far jump to selector that has L bit set (64 bit)
        "lcall $0x38,$0f\n\t"
        "jmp 1f\n\t"

        ".code64\n\t"
        "0:\n\t"
        "movq (%%edi),%%rdi\n\t"

        // Copy memory
        "cld\n\t"
        "rep movsb\n\t"

        // Far return to 32 bit compatibility mode code segment
        "retf\n\t"

        "1:\n\t"
        ".code32\n\t"

        "addl $16,%%esp\n\t"

        // Disable paging
        "mov %%cr0,%%eax\n\t"
        "btr $31,%%eax\n\t"
        "mov %%eax,%%cr0\n\t"

        // Switch back to 64 bit compatibility mode
        "movl $0xC0000080,%%ecx\n"
        "rdmsr\n\t"
        "andl $~0x100,%%eax\n\t"
        "wrmsr\n\t"

        // Load 16 bit selectors
        "movw $0x30,%%ax\n\t"
        "movw %%ax,%%ds\n\t"
        "movw %%ax,%%es\n\t"

        // Load 16-bit code segment
        "ljmp $0x28,$0f\n\t"

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

        // In real mode
        "0:\n\t"

        // Load real mode segments
        "xorw %%ax,%%ax\n\t"
        "movw %%ax,%%ds\n\t"
        "movw %%ax,%%es\n\t"
        "ljmp $0,$0f\n\t"
        "0:"
        : "=D" (address), "=S" (src), "=c" (size)
        : "0" (address), "1" (src), "2" (size), "b" (pdbr)
        : "eax"
    );

    toggle_a20(0);
    toggle_interrupts(intf);
}
