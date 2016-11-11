#include "idt.h"
#include "gdt.h"

idt_entry_64_t idt[0x80];

void *(*irq_dispatcher)(int irq, void *stack_pointer);

// Exception handlers
extern void isr_entry_0(void);
extern void isr_entry_1(void);
extern void isr_entry_2(void);
extern void isr_entry_3(void);
extern void isr_entry_4(void);
extern void isr_entry_5(void);
extern void isr_entry_6(void);
extern void isr_entry_7(void);
extern void isr_entry_8(void);
extern void isr_entry_9(void);
extern void isr_entry_10(void);
extern void isr_entry_11(void);
extern void isr_entry_12(void);
extern void isr_entry_13(void);
extern void isr_entry_14(void);
extern void isr_entry_15(void);
extern void isr_entry_16(void);
extern void isr_entry_17(void);
extern void isr_entry_18(void);
extern void isr_entry_19(void);
extern void isr_entry_20(void);
extern void isr_entry_21(void);
extern void isr_entry_22(void);
extern void isr_entry_23(void);
extern void isr_entry_24(void);
extern void isr_entry_25(void);
extern void isr_entry_26(void);
extern void isr_entry_27(void);
extern void isr_entry_28(void);
extern void isr_entry_29(void);
extern void isr_entry_30(void);
extern void isr_entry_31(void);

// PIC IRQs
extern void isr_entry_32(void);
extern void isr_entry_33(void);
extern void isr_entry_34(void);
extern void isr_entry_35(void);
extern void isr_entry_36(void);
extern void isr_entry_37(void);
extern void isr_entry_38(void);
extern void isr_entry_39(void);
extern void isr_entry_40(void);
extern void isr_entry_41(void);
extern void isr_entry_42(void);
extern void isr_entry_43(void);
extern void isr_entry_44(void);
extern void isr_entry_45(void);
extern void isr_entry_46(void);
extern void isr_entry_47(void);

// APIC IRQs
extern void isr_entry_48(void);
extern void isr_entry_49(void);
extern void isr_entry_50(void);
extern void isr_entry_51(void);
extern void isr_entry_52(void);
extern void isr_entry_53(void);
extern void isr_entry_54(void);
extern void isr_entry_55(void);
extern void isr_entry_56(void);
extern void isr_entry_57(void);
extern void isr_entry_58(void);
extern void isr_entry_59(void);
extern void isr_entry_60(void);
extern void isr_entry_61(void);
extern void isr_entry_62(void);
extern void isr_entry_63(void);
extern void isr_entry_64(void);
extern void isr_entry_65(void);
extern void isr_entry_66(void);
extern void isr_entry_67(void);
extern void isr_entry_68(void);
extern void isr_entry_69(void);
extern void isr_entry_70(void);
extern void isr_entry_71(void);

typedef void (*isr_entry_t)(void);

const isr_entry_t isr_entry_points[72] = {
    isr_entry_0,
    isr_entry_1,
    isr_entry_2,
    isr_entry_3,
    isr_entry_4,
    isr_entry_5,
    isr_entry_6,
    isr_entry_7,
    isr_entry_8,
    isr_entry_9,
    isr_entry_10,
    isr_entry_11,
    isr_entry_12,
    isr_entry_13,
    isr_entry_14,
    isr_entry_15,
    isr_entry_16,
    isr_entry_17,
    isr_entry_18,
    isr_entry_19,
    isr_entry_20,
    isr_entry_21,
    isr_entry_22,
    isr_entry_23,
    isr_entry_24,
    isr_entry_25,
    isr_entry_26,
    isr_entry_27,
    isr_entry_28,
    isr_entry_29,
    isr_entry_30,
    isr_entry_31,

    isr_entry_32,
    isr_entry_33,
    isr_entry_34,
    isr_entry_35,
    isr_entry_36,
    isr_entry_37,
    isr_entry_38,
    isr_entry_39,
    isr_entry_40,
    isr_entry_41,
    isr_entry_42,
    isr_entry_43,
    isr_entry_44,
    isr_entry_45,
    isr_entry_46,
    isr_entry_47,

    isr_entry_48,
    isr_entry_49,
    isr_entry_50,
    isr_entry_51,
    isr_entry_52,
    isr_entry_53,
    isr_entry_54,
    isr_entry_55,
    isr_entry_56,
    isr_entry_57,
    isr_entry_58,
    isr_entry_59,
    isr_entry_60,
    isr_entry_61,
    isr_entry_62,
    isr_entry_63,
    isr_entry_64,
    isr_entry_65,
    isr_entry_66,
    isr_entry_67,
    isr_entry_68,
    isr_entry_69,
    isr_entry_70,
    isr_entry_71
};

extern void isr_entry_0xC0(void);

static void load_idtr(table_register_64_t *table_reg)
{
    __asm__ __volatile__ (
        "lidtq %[table_reg]\n\t"
        :
        : [table_reg] "m" (*table_reg)
    );
}

int idt_init(void)
{
    uint64_t addr;

    for (size_t i = 0; i < countof(isr_entry_points); ++i) {
        addr = (uint64_t)isr_entry_points[i];
        idt[i].offset_lo = (uint16_t)(addr & 0xFFFF);
        idt[i].offset_hi = (uint16_t)((addr >> 16) & 0xFFFF);
        idt[i].offset_64_31 = (uint16_t)((addr >> 32) & 0xFFFFFFFF);

        idt[i].type_attr = IDT_PRESENT | IDT_INTR;

        idt[i].selector = IDT_SEL;
    }

    table_register_64_t idtr;

    addr = (uint64_t)idt;
    idtr.base_lo = (uint16_t)(addr & 0xFFFF);
    idtr.base_hi = (uint16_t)((addr >> 16) & 0xFFFF);
    idtr.base_hi1 = (uint16_t)((addr >> 32) & 0xFFFF);
    idtr.base_hi2 = (uint16_t)((addr >> 48) & 0xFFFF);
    idtr.limit = sizeof(idt) - 1;

    load_idtr(&idtr);

    return 0;
}

static void *exception_handler(interrupt_info_t *info, void *stack_pointer)
{
    (void)info;
    return stack_pointer;
}

isr_full_context_t *exception_isr_handler(isr_full_context_t *ctx)
{
    ctx->fpr->xmm[0].qword[0] = 0xfeedbaadbeeff00d;
    ctx->fpr->xmm[1].word[5] = 0xface;
    return ctx;
}

void *isr_handler(interrupt_info_t *info, void *stack_pointer)
{
    if (info->interrupt >= 32 &&
               info->interrupt < 48) {
        //
        // PIC IRQ
        stack_pointer = irq_dispatcher(
                    info->interrupt - 32,
                    stack_pointer);
    } else if (info->interrupt >= 48 &&
               info->interrupt < 72) {
        //
        // APIC IRQ
        stack_pointer = irq_dispatcher(
                    info->interrupt - 48,
                    stack_pointer);
    } else {
        // System call later...
    }

    return stack_pointer;
}
