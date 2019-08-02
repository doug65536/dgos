#pragma once
#include "types.h"
#include "assert.h"
#include "isr.h"

struct idt_entry_t {
    uint16_t offset_lo; // offset bits 0..15
    uint16_t selector;  // a code segment selector in GDT or LDT
    uint8_t zero;       // unused, set to 0
    uint8_t type_attr;  // type and attributes
    uint16_t offset_hi; // offset bits 16..31
};

C_ASSERT(sizeof(idt_entry_t) == 8);

struct idt_entry_64_t {
    uint16_t offset_lo;     // offset bits 15:0
    uint16_t selector;      // a code segment selector in GDT
    uint8_t ist;            // interrupt stack table index
    uint8_t type_attr;      // type and attributes
    uint16_t offset_hi;     // offset bits 31:16

    uint32_t offset_64_31;  // offset bits 63:32
    uint32_t reserved;
};

C_ASSERT(sizeof(idt_entry_64_t) == 16);

extern "C" idt_entry_64_t idt[];

// Buffer large enough for worst case flags description
#define CPU_MAX_FLAGS_DESCRIPTION    58

size_t cpu_describe_eflags(char *buf, size_t buf_size, uintptr_t rflags);
size_t cpu_describe_mxcsr(char *buf, size_t buf_size, uintptr_t mxcsr);
size_t cpu_describe_fpucw(char *buf, size_t buf_size, uint16_t fpucw);
size_t cpu_describe_fpusw(char *buf, size_t buf_size, uint16_t fpusw);

typedef isr_context_t *(*irq_dispatcher_handler_t)(
        int intr, isr_context_t *ctx);

typedef irq_dispatcher_handler_t idt_unhandled_exception_handler_t;

extern "C" isr_context_t *debug_exception_handler(int intr, isr_context_t *ctx);

extern "C" void idt_xsave_detect(int ap);

extern "C" isr_context_t *unhandled_exception_handler(isr_context_t *ctx);

extern "C" void idt_set_unhandled_exception_handler(
        idt_unhandled_exception_handler_t handler);

int idt_init(int ap);

extern "C" void idt_override_vector(int intr, irq_dispatcher_handler_t handler);
extern "C" void idt_clone_debug_exception_dispatcher(void);

extern "C" uint32_t xsave_supported_states;
extern "C" uint32_t xsave_enabled_states;
extern "C" void dump_context(isr_context_t *ctx, int to_screen);

// Direct call stub that will not stall on retpoline
#define FAST_VECTOR(name, init_target) \
    __asm__ __volatile__ ( \
        ".global " #name "\n\t" \
        #name ":" "\n\t" \
        "jmp " #init_target "\n\t"\
        ".global " #name "_patch" "\n\t" \
        #name "_patch:" "\n\t" \
    ); \
    extern char name##_patch[]


enum struct xsave_support_t {
    FXSAVE,
    XSAVE,
    XSAVEC,
    XSAVEOPT,
    XSAVES
};

extern xsave_support_t xsave_support;

void idt_ist_adjust(int cpu, size_t ist, ptrdiff_t adj);

#define IDT_IST_SLOT_STACK          1
#define IDT_IST_SLOT_DBLFAULT       2
#define IDT_IST_SLOT_FLUSH_TRACE    3
#define IDT_IST_SLOT_NMI            4
