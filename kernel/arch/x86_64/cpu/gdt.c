#include "gdt.h"
#include "control_regs.h"
#include "mm.h"
#include "string.h"

#define TSS_STACK_SIZE (PAGESIZE*2)

gdt_entry_combined_t gdt[] = {
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
    // CPU task selectors (64 CPU limit)
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t)),
    GDT_MAKE_TSSSEG(0L, sizeof(tss_t)), GDT_MAKE_TSSSEG(0L, sizeof(tss_t))
};

void gdt_init(void)
{
    table_register_64_t gdtr;
    uintptr_t gdt_addr = (uintptr_t)gdt;
    gdtr.base_lo = gdt_addr & 0xFFFFFFFF;
    gdtr.base_hi = (gdt_addr >> 32) & 0xFFFFFFFF;
    gdtr.limit = sizeof(gdt) - 1;
    cpu_set_gdtr(gdtr);
}

static void gdt_set_tss_base(int cpu_number, tss_t *base)
{
    gdt_entry_combined_t gdt_ent_lo =
            GDT_MAKE_TSS_DESCRIPTOR(
                (uintptr_t)base,
                sizeof(*base)-1, 1, 0, 0);

    gdt_entry_combined_t gdt_ent_hi =
            GDT_MAKE_TSS_HIGH_DESCRIPTOR(
                (uintptr_t)base);

    gdt[GDT_SEL_TSS_n(cpu_number) >> 3] = gdt_ent_lo;
    gdt[(GDT_SEL_TSS_n(cpu_number) >> 3) + 1] = gdt_ent_hi;
}

void gdt_init_tss(int cpu_count)
{
    tss_t *tss_list = mmap(0, sizeof(*tss_list) * cpu_count,
                      PROT_READ | PROT_WRITE, 0, -1, 0);
    memset(tss_list, 0, sizeof(*tss_list) * cpu_count);

    for (int i = 0; i < cpu_count; ++i) {
        gdt_set_tss_base(i, tss_list + i);

        for (int st = 0; st < 8; ++st) {
            void *stack = mmap(0, TSS_STACK_SIZE,
                               PROT_READ | PROT_WRITE,
                               0, -1, 0);

            tss_list[i].stack[st] = stack;

            if (st) {
                tss_list[i].rsp[st].lo = (uint32_t)
                        (uintptr_t)stack + TSS_STACK_SIZE;
                tss_list[i].rsp[st].hi = (uint32_t)
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
    cpu_set_tr(GDT_SEL_TSS_n(cpu_number));
}
