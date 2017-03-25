#include "gdt.h"
#include "control_regs.h"
#include "mm.h"
#include "string.h"
#include "spinlock.h"
#include "printk.h"

#define TSS_STACK_SIZE (PAGESIZE*2)

static gdt_entry_combined_t gdt[] = {
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
    GDT_MAKE_DATASEG32(3),
    GDT_MAKE_EMPTY(),
    // CPU task selector
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t))
};

// Holds exclusive access to TSS segment descriptor
// while loading task register
static spinlock_t gdt_tss_lock;
static tss_t *tss_list;

void gdt_init(void)
{
    table_register_64_t gdtr;
    uintptr_t gdt_addr = (uintptr_t)gdt;
    gdtr.base = gdt_addr;
    gdtr.limit = sizeof(gdt) - 1;
    cpu_set_gdtr(gdtr);
}

static void gdt_set_tss_base(tss_t *base)
{
    gdt_entry_combined_t gdt_ent_lo =
            GDT_MAKE_TSS_DESCRIPTOR(
                (uintptr_t)base,
                sizeof(*base)-1, 1, 0, 0);

    gdt_entry_combined_t gdt_ent_hi =
            GDT_MAKE_TSS_HIGH_DESCRIPTOR(
                (uintptr_t)base);

    gdt[GDT_SEL_TSS >> 3] = gdt_ent_lo;
    gdt[(GDT_SEL_TSS >> 3) + 1] = gdt_ent_hi;
}

void gdt_init_tss(int cpu_count)
{
    tss_list = (tss_t*)mmap(0, sizeof(*tss_list) * cpu_count,
                           PROT_READ | PROT_WRITE,
                           MAP_POPULATE, -1, 0);
    memset(tss_list, 0, sizeof(*tss_list) * cpu_count);

    for (int i = 0; i < cpu_count; ++i) {
        for (int st = 0; st < 8; ++st) {
            void *stack = mmap(0, TSS_STACK_SIZE,
                               PROT_READ | PROT_WRITE,
                               MAP_POPULATE, -1, 0);

            printdbg("Allocated IST cpu=%d slot=%d at %lx\n",
                     i, st, (uintptr_t)stack);

            tss_list[i].stack[st] = stack;

            if (st) {
                tss_list[i].ist[st].lo = (uint32_t)
                        (uintptr_t)stack + TSS_STACK_SIZE;
                tss_list[i].ist[st].hi = (uint32_t)
                        (((uintptr_t)stack + TSS_STACK_SIZE) >> 32);
            } else {
                tss_list[i].rsp[0].lo = (uint32_t)
                        (uintptr_t)stack + TSS_STACK_SIZE;
                tss_list[i].rsp[0].hi = (uint32_t)
                        (((uintptr_t)stack + TSS_STACK_SIZE) >> 32);
            }
        }
    }
}

void gdt_load_tr(int cpu_number)
{
    spinlock_lock_noirq(&gdt_tss_lock);

    gdt_set_tss_base(tss_list + cpu_number);
    cpu_set_tr(GDT_SEL_TSS);

    spinlock_unlock_noirq(&gdt_tss_lock);
}
