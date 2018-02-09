#include "gdt.h"
#include "control_regs.h"
#include "mm.h"
#include "string.h"
#include "printk.h"
#include "assert.h"
#include "callout.h"
#include "mutex.h"

C_ASSERT(sizeof(gdt_entry_t) == 8);
C_ASSERT(sizeof(gdt_entry_tss_ldt_t) == 8);
C_ASSERT(sizeof(gdt_entry_combined_t) == 8);

#define TSS_STACK_SIZE (32 << 10)

// Must match control_regs_constants.h GDT_SEL_* defines!
__aligned(64) gdt_entry_combined_t gdt[24] = {
    // --- cache line ---

    // 0x00
    GDT_MAKE_EMPTY(),

    // 0x8, 0x10
    GDT_MAKE_CODESEG16(0),
    GDT_MAKE_DATASEG16(0),

    // 0x18, 0x20
    GDT_MAKE_CODESEG32(0),
    GDT_MAKE_DATASEG(0),

    // 0x28, 0x30, 0x38
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),

    // --- cache line ---

    // 32 bit user code
    // 0x40, 0x48, 0x50
    GDT_MAKE_CODESEG32(3),
    GDT_MAKE_DATASEG(3),
    GDT_MAKE_CODESEG64(3),

    // 0x58
    GDT_MAKE_EMPTY(),

    // 64 bit kernel code and data
    // 0x60, 0x68
    GDT_MAKE_CODESEG64(0),
    GDT_MAKE_DATASEG(0),

    // 0x68, 0x70, 0x78
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),

    // --- cache line ---

    // CPU task selector
    // 0x80-0x8F
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),

    // 0x90-0xBF
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY()

    // --- cache line ---
};

C_ASSERT(GDT_SEL_KERNEL_CODE64 == 12*8);
C_ASSERT(GDT_SEL_USER_CODE32 == 8*8);
C_ASSERT(GDT_SEL_TSS == 16*8);
C_ASSERT(sizeof(gdt) == GDT_SEL_END);

// Holds exclusive access to TSS segment descriptor
// while loading task register
static ticketlock gdt_tss_lock;
tss_t tss_list[];

void gdt_init(int)
{
    table_register_64_t gdtr;
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uintptr_t)gdt;
    cpu_set_gdtr(gdtr);
}

static void gdt_set_tss_base(tss_t *base)
{
    gdt_entry_combined_t gdt_ent_lo =
            GDT_MAKE_TSS_DESCRIPTOR(
                uintptr_t(&base->reserved0),
                sizeof(*base)-1, 1, 0, 0);

    gdt_entry_combined_t gdt_ent_hi =
            GDT_MAKE_TSS_HIGH_DESCRIPTOR(
                uintptr_t(&base->reserved0));

    gdt[GDT_SEL_TSS >> 3] = gdt_ent_lo;
    gdt[(GDT_SEL_TSS >> 3) + 1] = gdt_ent_hi;
}

void gdt_init_tss(int cpu_count)
{
    //tss_list = (tss_t*)mmap(0, sizeof(*tss_list) * cpu_count,
    //                       PROT_READ | PROT_WRITE,
    //                       MAP_POPULATE, -1, 0);

    for (int i = 0; i < cpu_count; ++i) {
        tss_t *tss = tss_list + i;

        for (int st = 0; st < 3; ++st) {
            void *stack = mmap(0, TSS_STACK_SIZE,
                               PROT_READ | PROT_WRITE,
                               MAP_POPULATE, -1, 0);
            madvise(stack, PAGESIZE, MADV_DONTNEED);
            mprotect(stack, PAGESIZE, PROT_NONE);

            printdbg("Allocated IST cpu=%d slot=%d at %lx\n",
                     i, st, (uintptr_t)stack);

            tss->stack[st] = stack;

            if (st)
                tss->ist[st] = uintptr_t(stack) + TSS_STACK_SIZE;
            else
                tss->rsp[0] = uintptr_t(stack) + TSS_STACK_SIZE;

            tss->iomap_base = uint16_t(uintptr_t(tss + 1) - uintptr_t(tss));

            assert(tss->reserved0 == 0);
            assert(tss->reserved3 == 0);
            assert(tss->reserved4 == 0);
            assert(tss->reserved5 == 0);
        }
    }

   callout_call(callout_type_t::tss_list_ready);
}

void gdt_load_tr(int cpu_number)
{
    unique_lock<ticketlock> lock(gdt_tss_lock);

    gdt_set_tss_base(tss_list + cpu_number);
    cpu_set_tr(GDT_SEL_TSS);
}
