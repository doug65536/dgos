#include "gdt.h"
#include "mm.h"
#include "string.h"
#include "printk.h"
#include "assert.h"
#include "callout.h"
#include "mutex.h"
#include "inttypes.h"
#include "stdlib.h"

C_ASSERT(sizeof(gdt_entry_t) == 8);
C_ASSERT(sizeof(gdt_entry_tss_ldt_t) == 8);
C_ASSERT(sizeof(gdt_entry_combined_t) == 8);

#define TSS_STACK_SIZE (64 << 10)

// Must match control_regs_constants.h GDT_SEL_* defines!
__aligned(64) gdt_entry_combined_t gdt[24] = {
    // --- cache line 0x00 ---

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

    // --- cache line 0x40 ---

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

    // 0x70, 0x78
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),

    // --- cache line 0x80 ---

    // CPU task selector
    // 0x80-0x8F
    GDT_MAKE_TSSSEG(0, 0, sizeof(tss_t)),

    // 0x90-0xBF
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY(),
    GDT_MAKE_EMPTY()

    // --- cache line 0xc0 ---
};

C_ASSERT(GDT_SEL_KERNEL_CODE64 == 12*8);
C_ASSERT(GDT_SEL_USER_CODE32 == 8*8);
C_ASSERT(GDT_SEL_TSS == 16*8);
C_ASSERT(sizeof(gdt) == GDT_SEL_END);

// Holds exclusive access to TSS segment descriptor
// while loading task register
static ext::spinlock gdt_tss_lock;
tss_t tss_list[MAX_CPUS];
table_register_64_t gdtr;

void gdt_init(int)
{
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uintptr_t)gdt;
    cpu_gdtr_set(gdtr);
}

static void gdt_set_tss_base(tss_t *base)
{
    if (base) {
        gdt_entry_t tss_lo;
        gdt_entry_tss_ldt_t tss_hi;

        tss_lo.set_type(GDT_TYPE_TSS);
        uintptr_t tss_addr = uintptr_t(&base->reserved0);

        // Get low and high halves of address
        uint32_t tss_addr_lo = tss_addr & 0xFFFFFFFFU;
        uint32_t tss_addr_hi = (tss_addr >> 32) & 0xFFFFFFFFU;

        tss_lo.set_base(tss_addr_lo);
        tss_hi.set_base_hi(tss_addr_hi);
        tss_lo.set_limit(sizeof(*base) - 1);
        tss_lo.set_present(true);

        gdt[GDT_SEL_TSS >> 3] = tss_lo;
        gdt[(GDT_SEL_TSS >> 3) + 1] = tss_hi;
    } else {
        gdt[GDT_SEL_TSS >> 3].raw = 0;
        gdt[(GDT_SEL_TSS >> 3) + 1].raw = 0;
    }
}

void gdt_init_tss(size_t cpu_count)
{
    //tss_list = (tss_t*)mmap(nullptr, sizeof(*tss_list) * cpu_count,
    //                       PROT_READ | PROT_WRITE, MAP_POPULATE);

    size_t constexpr stack_count_per_cpu = 5;

    size_t stacks_nr = cpu_count * stack_count_per_cpu;
    size_t stacks_sz = TSS_STACK_SIZE * stacks_nr;

    // Map space for all the stacks
    char *stacks_base = (char*)mmap(
                nullptr, stacks_sz, PROT_NONE,
                MAP_NOCOMMIT);
    if (unlikely(stacks_base == MAP_FAILED))
        panic_oom();
    char *stacks_alloc = stacks_base;

    for (size_t i = 0; i < cpu_count; ++i) {
        tss_t *tss = tss_list + i;

        tss->iomap_base = uint16_t(uintptr_t(tss + 1) - uintptr_t(tss));

        for (size_t st = 0; st < stack_count_per_cpu; ++st) {
            char *stack = stacks_alloc;

            stacks_alloc += TSS_STACK_SIZE;

            char *stack_st = stack + PAGE_SIZE;
            char *stack_en = stacks_alloc;

            mprotect(stack_st, stack_en - stack_st, PROT_READ | PROT_WRITE);
            madvise(stack_st, stack_en - stack_st, MADV_WILLNEED);

            printdbg("Allocated IST cpu=%zu slot=%zu at %#zx\n",
                     i, st, (uintptr_t)stack);

            tss->stack[st] = stack;

            if (st)
                tss->ist[st] = uintptr_t(stack) + TSS_STACK_SIZE;
            else
                tss->rsp[0] = uintptr_t(stack) + TSS_STACK_SIZE;

            assert(tss->reserved0 == 0);
            assert(tss->reserved3 == 0);
            assert(tss->reserved4 == 0);
            assert(tss->reserved5 == 0);
        }
    }

   callout_call(callout_type_t::tss_list_ready);
}

extern tss_t early_tss;

void gdt_init_tss_early()
{
    gdt_set_tss_base(&early_tss);
    assert(gdt[GDT_SEL_TSS >> 3].mem.get_type() == GDT_TYPE_TSS);
    cpu_tr_set(GDT_SEL_TSS);
    gdt[GDT_SEL_TSS >> 3].raw = 0;
}

void gdt_load_tr(int cpu_number)
{
    std::unique_lock<ext::spinlock> lock(gdt_tss_lock);

    gdt_set_tss_base(tss_list + cpu_number);
    assert(gdt[GDT_SEL_TSS >> 3].mem.get_type() == GDT_TYPE_TSS);
    cpu_tr_set(GDT_SEL_TSS);
    gdt[GDT_SEL_TSS >> 3].mem.set_type(GDT_TYPE_TSS);
}
