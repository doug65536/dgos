#include "apic.h"
#include "types.h"
#include "bios_data.h"
#include "control_regs.h"
#include "irq.h"
#include "thread_impl.h"
#include "mm.h"
#include "string.h"
#include "atomic.h"

typedef struct mp_table_hdr_t {
    char sig[4];
    uint32_t phys_addr;
    uint8_t length;
    uint8_t spec;
    uint8_t checksum;
    uint8_t features[5];
} mp_table_hdr_t;

static char *mp_tables;

static uint64_t apic_base;
uint32_t volatile *apic_ptr;

#define APIC_BIT(r,n)   (APIC_REG((r) + ((n)>>5)) & \
                            (1<<((n)&31)))

#define APIC_REG(n)     apic_ptr[(n)>>2]
#define APIC_ID         APIC_REG(0x20)
#define APIC_VER        APIC_REG(0x30)
#define APIC_TPR        APIC_REG(0x80)
#define APIC_APR        APIC_REG(0x90)
#define APIC_PPR        APIC_REG(0xA0)
#define APIC_EOI        APIC_REG(0xB0)
#define APIC_LDR        APIC_REG(0xD0)
#define APIC_DFR        APIC_REG(0xE0)
#define APIC_SIR        APIC_REG(0xF0)
#define APIC_ISR_n(n)   APIC_BIT(0x100, n)
#define APIC_TMR_n(n)   APIC_BIT(0x180, n)
#define APIC_IRR_n(n)   APIC_BIT(0x200, n)
#define APIC_ESR        APIC_REG(0x280)
#define APIC_LVT_CMCI   APIC_REG(0x2F0)
#define APIC_ICR_LO     APIC_REG(0x300)
#define APIC_ICR_HI     APIC_REG(0x310)

#define APIC_LVT_TR     APIC_REG(0x320)
#define APIC_LVT_TSR    APIC_REG(0x330)
#define APIC_LVT_PMCR   APIC_REG(0x340)
#define APIC_LVT_LNT0   APIC_REG(0x350)
#define APIC_LVT_LNT1   APIC_REG(0x360)
#define APIC_LVT_ERR    APIC_REG(0x370)
#define APIC_LVT_ICR    APIC_REG(0x380)
#define APIC_LVT_CCR    APIC_REG(0x390)
#define APIC_LVT_DCR    APIC_REG(0x3E0)

#define APIC_CMD        APIC_ICR_LO
#define APIC_DEST       APIC_ICR_HI

#define APIC_CMD_SIPI_PAGE_BIT      0
#define APIC_CMD_VECTOR_BIT         0
#define APIC_CMD_DEST_MODE_BIT      8
#define APIC_CMD_DEST_LOGICAL_BIT   11
#define APIC_CMD_PENDING_BIT        12
#define APIC_CMD_ILD_CLR_BIT        14
#define APIC_CMD_ILD_SET_BIT        15
#define APIC_CMD_DEST_TYPE_BIT      18

#define APIC_CMD_SIPI_PAGE_BITS     8
#define APIC_CMD_DEST_MODE_BITS     3
#define APIC_CMD_DEST_TYPE_BITS     2

#define APIC_CMD_SIPI_PAGE_MASK     ((1 << APIC_CMD_SIPI_PAGE_BITS)-1)
#define APIC_CMD_DEST_MODE_MASK     ((1 << APIC_CMD_DEST_MODE_BITS)-1)
#define APIC_CMD_DEST_TYPE_MASK     ((1 << APIC_CMD_DEST_TYPE_BITS)-1)

#define APIC_CMD_SIPI_PAGE      (APIC_CMD_SIPI_PAGE_MASK << APIC_CMD_SIPI_PAGE_BIT)
#define APIC_CMD_DEST_MODE      (APIC_CMD_DEST_MODE_MASK << APIC_CMD_DEST_MODE_BIT)
#define APIC_CMD_SIPI_PAGE      (APIC_CMD_SIPI_PAGE_MASK << APIC_CMD_SIPI_PAGE_BIT)

#define APIC_CMD_SIPI_PAGE_n(n) ((n) << APIC_CMD_SIPI_PAGE_BIT)
#define APIC_CMD_DEST_MODE_n(n) ((n) << APIC_CMD_DEST_MODE_BIT)
#define APIC_CMD_DEST_TYPE_n(n) ((n) << APIC_CMD_DEST_TYPE_BIT)
#define APIC_CMD_SIPI_PAGE_n(n) ((n) << APIC_CMD_SIPI_PAGE_BIT)

#define APIC_CMD_VECTOR         (1 << APIC_CMD_VECTOR_BIT)
#define APIC_CMD_DEST_LOGICAL   (1 << APIC_CMD_DEST_LOGICAL_BIT)
#define APIC_CMD_PENDING        (1 << APIC_CMD_PENDING_BIT)
#define APIC_CMD_ILD_CLR        (1 << APIC_CMD_ILD_CLR_BIT)
#define APIC_CMD_ILD_SET        (1 << APIC_CMD_ILD_SET_BIT)
#define APIC_CMD_DEST_TYPE      (1 << APIC_CMD_DEST_TYPE_BIT)

#define APIC_CMD_DEST_MODE_NORMAL   APIC_CMD_DEST_MODE_n(0)
#define APIC_CMD_DEST_MODE_LOWPRI   APIC_CMD_DEST_MODE_n(1)
#define APIC_CMD_DEST_MODE_SMI      APIC_CMD_DEST_MODE_n(2)
#define APIC_CMD_DEST_MODE_NMI      APIC_CMD_DEST_MODE_n(4)
#define APIC_CMD_DEST_MODE_INIT     APIC_CMD_DEST_MODE_n(5)
#define APIC_CMD_DEST_MODE_SIPI     APIC_CMD_DEST_MODE_n(6)

#define APIC_CMD_DEST_TYPE_BYID     APIC_CMD_DEST_TYPE_n(0)
#define APIC_CMD_DEST_TYPE_SELF     APIC_CMD_DEST_TYPE_n(1)
#define APIC_CMD_DEST_TYPE_ALL      APIC_CMD_DEST_TYPE_n(2)
#define APIC_CMD_DEST_TYPE_OTHER    APIC_CMD_DEST_TYPE_n(3)

// Divide configuration register
#define APIC_LVT_DCR_BY_2          0
#define APIC_LVT_DCR_BY_4          1
#define APIC_LVT_DCR_BY_8          2
#define APIC_LVT_DCR_BY_16         3
#define APIC_LVT_DCR_BY_32         (8+0)
#define APIC_LVT_DCR_BY_64         (8+1)
#define APIC_LVT_DCR_BY_128        (8+2)
#define APIC_LVT_DCR_BY_1          (8+3)

#define APIC_BASE_MSR  0x1B

static int parse_mp_tables(void)
{
    void const *mem_top =
            (uint16_t*)((uintptr_t)*BIOS_DATA_AREA(
                uint16_t, 0x40E) << 4);
    void const *ranges[4] = {
        mem_top, (uint32_t*)0xA0000,
        (uint32_t*)0xE0000, (uint32_t*)0x100000
    };
    for (size_t pass = 0; !mp_tables && pass < 4; pass += 2) {
        if (pass == 2) {
            ranges[2] = mmap((void*)0xE0000, 0x20000, PROT_READ,
                             MAP_PHYSICAL, -1, 0);
            ranges[3] = (char*)ranges[2] + 0x20000;
        }
        for (mp_table_hdr_t const* sig_srch = ranges[pass];
             (void*)sig_srch < ranges[pass+1]; ++sig_srch) {
            // Check signature
            if (memcmp(sig_srch->sig, "_MP_", 4))
                continue;

            // Check checksum
            char *checked_sum_ptr = (char*)sig_srch;
            uint8_t checked_sum = 0;
            for (size_t i = 0; i < sizeof(*sig_srch); ++i)
                checked_sum += *checked_sum_ptr++;
            if (checked_sum != 0)
                continue;

            mp_tables = (char*)(uintptr_t)sig_srch->phys_addr;
            break;
        }
    }

    return !!mp_tables;
}

static void *apic_timer_handler(int irq, void *ctx)
{
    APIC_EOI = irq + 48;
    return thread_schedule(ctx);
}

int apic_init(int ap)
{
    if (ap) {
        APIC_SIR |= (1<<8);
        APIC_LVT_DCR = APIC_LVT_DCR_BY_128;
        APIC_LVT_ICR = ((3600000000U>>7)/60);
        APIC_LVT_TR = 73 | (1<<17);

        irq_hook(41, apic_timer_handler);

        return 1;
    }

    if (!parse_mp_tables())
        return 0;

    apic_base = msr_get(APIC_BASE_MSR) & -(intptr_t)4096;

    apic_ptr = mmap((void*)apic_base, 4096,
         PROT_READ | PROT_WRITE, MAP_PHYSICAL, -1, 0);

    // Read address of MP entry trampoline from boot sector
    uint32_t mp_trampoline_addr = *(uint32_t*)0x7c40;
    uint32_t mp_trampoline_page = mp_trampoline_addr >> 12;

    APIC_DEST = 0;

    APIC_CMD =
            APIC_CMD_DEST_MODE_INIT |
            APIC_CMD_DEST_LOGICAL |
            APIC_CMD_DEST_TYPE_OTHER;

    APIC_CMD = APIC_CMD_SIPI_PAGE_n(mp_trampoline_page) |
            APIC_CMD_DEST_MODE_SIPI |
            APIC_CMD_DEST_LOGICAL |
            APIC_CMD_DEST_TYPE_OTHER;

    while (APIC_CMD & APIC_CMD_PENDING)
        pause();

    return 1;
}
