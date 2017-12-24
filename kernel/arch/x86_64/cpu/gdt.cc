#include "gdt.h"
#include "control_regs.h"
#include "mm.h"
#include "string.h"
#include "spinlock.h"
#include "printk.h"
#include "assert.h"
#include "callout.h"

C_ASSERT(sizeof(gdt_entry_t) == 8);
C_ASSERT(sizeof(gdt_entry_tss_ldt_t) == 8);
C_ASSERT(sizeof(gdt_entry_combined_t) == 8);

#define TSS_STACK_SIZE (32 << 10)

gdt_entry_combined_t gdt[13] = {
    GDT_MAKE_EMPTY(),

    // Must be in this order for syscall instruction
    // 64 bit kernel code and data
    GDT_MAKE_CODESEG64(0),
    GDT_MAKE_DATASEG64(0),

    // Arbitrary
    // 32 bit kernel code and data
    GDT_MAKE_CODESEG32(0),
    GDT_MAKE_DATASEG32(0),
    // 16 bit kernel code and data
    GDT_MAKE_CODESEG16(0),
    GDT_MAKE_DATASEG16(0),

    // Must be in this order for sysret instruction
    // 32 bit user code
    GDT_MAKE_CODESEG32(3),
    // User data
    GDT_MAKE_DATASEG64(3),
    // 64 bit user code
    GDT_MAKE_CODESEG32(3),

    GDT_MAKE_EMPTY(),

    // CPU task selector
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t))
};

C_ASSERT(GDT_SEL_KERNEL_CODE64 == 1*8);
C_ASSERT(GDT_SEL_USER_CODE32 == 7*8);
C_ASSERT(GDT_SEL_TSS == 11*8);
C_ASSERT(sizeof(gdt) == GDT_SEL_END);

// Holds exclusive access to TSS segment descriptor
// while loading task register
static spinlock_t gdt_tss_lock;
tss_t *tss_list;

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
    tss_list = (tss_t*)mmap(0, sizeof(*tss_list) * cpu_count,
                           PROT_READ | PROT_WRITE,
                           MAP_POPULATE, -1, 0);

    for (int i = 0; i < cpu_count; ++i) {
        tss_t *tss = tss_list + i;

        for (int st = 0; st < 3; ++st) {
            void *stack = mmap(0, TSS_STACK_SIZE,
                               PROT_READ | PROT_WRITE,
                               MAP_POPULATE, -1, 0);

            printdbg("Allocated IST cpu=%d slot=%d at %lx\n",
                     i, st, (uintptr_t)stack);

            tss->stack[st] = stack;

            if (st) {
                tss->ist[st] = (uint64_t)stack + TSS_STACK_SIZE;
            } else {
                tss->rsp[0] = (uint64_t)stack + TSS_STACK_SIZE;
            }

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
    spinlock_lock_noirq(&gdt_tss_lock);

    gdt_set_tss_base(tss_list + cpu_number);
    cpu_set_tr(GDT_SEL_TSS);

    spinlock_unlock_noirq(&gdt_tss_lock);
}
