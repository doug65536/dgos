#include "apic.h"
#include "types.h"
#include "bios_data.h"
#include "control_regs.h"
#include "irq.h"
#include "thread_impl.h"
#include "mm.h"
#include "cpuid.h"
#include "string.h"
#include "atomic.h"
#include "printk.h"
#include "likely.h"

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

#define APIC_REG(n)     apic_ptr[(n)>>2]
#define APIC_BIT(r,n)   (APIC_REG((r) + (((n)>>5)<<2)) & \
                            (1<<((n)&31)))

// APIC ID
#define APIC_ID         APIC_REG(0x20)

// APIC version
#define APIC_VER        APIC_REG(0x30)

// Task Priority Register
#define APIC_TPR        APIC_REG(0x80)

// Arbitration Priority Register
#define APIC_APR        APIC_REG(0x90)

// Processor Priority Register
#define APIC_PPR        APIC_REG(0xA0)

// End Of Interrupt register
#define APIC_EOI        APIC_REG(0xB0)

// Logical Destination Register
#define APIC_LDR        APIC_REG(0xD0)

// Destination Format Register
#define APIC_DFR        APIC_REG(0xE0)

// Spuriout Interrupt Register
#define APIC_SIR        APIC_REG(0xF0)

// In Service Registers (256 individual bits)
#define APIC_ISR_n(n)   APIC_BIT(0x100, n)

// Trigger Mode Registers (256 bits)
#define APIC_TMR_n(n)   APIC_BIT(0x180, n)

// Interrupt Request Registers (256 bits)
#define APIC_IRR_n(n)   APIC_BIT(0x200, n)

// Error Status Register
#define APIC_ESR        APIC_REG(0x280)

// Local Vector Table Corrected Machine Check Interrupt
#define APIC_LVT_CMCI   APIC_REG(0x2F0)

// Local Vector Table Interrupt Command Register Low
#define APIC_ICR_LO     APIC_REG(0x300)

// Local Vector Table Interrupt Command Register High
#define APIC_ICR_HI     APIC_REG(0x310)

// Local Vector Table Timer Register
#define APIC_LVT_TR     APIC_REG(0x320)

// Local Vector Table Thermal Sensor Register
#define APIC_LVT_TSR    APIC_REG(0x330)

// Local Vector Table Performance Monitoring Counter Register
#define APIC_LVT_PMCR   APIC_REG(0x340)

// Local Vector Table Local Interrupt 0 Register
#define APIC_LVT_LNT0   APIC_REG(0x350)

// Local Vector Table Local Interrupt 1 Register
#define APIC_LVT_LNT1   APIC_REG(0x360)

// Local Vector Table Error Register
#define APIC_LVT_ERR    APIC_REG(0x370)

// Local Vector Table Timer Initial Count Register
#define APIC_LVT_ICR    APIC_REG(0x380)

// Local Vector Table Timer Current Count Register
#define APIC_LVT_CCR    APIC_REG(0x390)

// Local Vector Table Timer Divide Configuration Register
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

#define APIC_CMD_VECTOR_BITS        8
#define APIC_CMD_SIPI_PAGE_BITS     8
#define APIC_CMD_DEST_MODE_BITS     3
#define APIC_CMD_DEST_TYPE_BITS     2

#define APIC_CMD_VECTOR_MASK        ((1 << APIC_CMD_VECTOR_BITS)-1)
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

#define APIC_CMD_VECTOR_n(n)    (((n) & APIC_CMD_VECTOR_MASK) << APIC_CMD_VECTOR_BIT)

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
#define APIC_LVT_DCR_BY_2           0
#define APIC_LVT_DCR_BY_4           1
#define APIC_LVT_DCR_BY_8           2
#define APIC_LVT_DCR_BY_16          3
#define APIC_LVT_DCR_BY_32          (8+0)
#define APIC_LVT_DCR_BY_64          (8+1)
#define APIC_LVT_DCR_BY_128         (8+2)
#define APIC_LVT_DCR_BY_1           (8+3)

#define APIC_SIR_APIC_ENABLE_BIT    8

#define APIC_SIR_APIC_ENABLE        (1<<APIC_SIR_APIC_ENABLE_BIT)

#define APIC_LVT_TR_MODE_BIT        17
#define APIC_LVT_TR_MODE_BITS       2
#define APIC_LVT_TR_VECTOR_BIT      0
#define APIC_LVT_TR_MODE_MASK       ((1<<APIC_LVT_TR_MODE_BITS)-1)
#define APIC_LVT_TR_MODE_n(n)       ((n)<<APIC_LVT_TR_MODE_BIT)
#define APIC_LVT_TR_VECTOR_n(n)     ((n)<<APIC_LVT_TR_VECTOR_BIT)

#define APIC_LVT_TR_MODE_ONESHOT    0
#define APIC_LVT_TR_MODE_PERIODIC   1
#define APIC_LVT_TR_MODE_DEADLINE   2

#define APIC_BASE_MSR  0x1B

static int parse_mp_tables(void)
{
    void *mem_top =
            (uint16_t*)((uintptr_t)*BIOS_DATA_AREA(
                uint16_t, 0x40E) << 4);
    void *ranges[4] = {
        mem_top, (uint32_t*)0xA0000,
        0, 0
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

    if (ranges[2] != 0)
        munmap(ranges[2], 0x20000);

    return !!mp_tables;
}

static void *apic_timer_handler(int irq, void *ctx)
{
    APIC_EOI = irq + 48;
    return thread_schedule(ctx);
}

unsigned apic_get_id(void)
{
    if (likely(apic_ptr))
        return APIC_ID;

    cpuid_t cpuid_info;
    cpuid(&cpuid_info, CPUID_INFO_FEATURES, 0);
    unsigned apic_id = cpuid_info.ebx >> 24;
    return apic_id;
}

static void apic_send_command(uint32_t dest, uint32_t cmd)
{
    APIC_DEST = dest;
    APIC_CMD = cmd;
    while (APIC_CMD & APIC_CMD_PENDING)
        pause();
}

// if target_apic_id is == -1, sends to other CPUs
// if target_apic_id is <= -2, sends to all CPUs
// if target_apid_id is >= 0, sends to specific APIC ID
void apic_send_ipi(int target_apic_id, uint8_t intr)
{
    uint32_t dest_type = target_apic_id < -1
            ? APIC_CMD_DEST_TYPE_ALL
            : target_apic_id < 0
            ? APIC_CMD_DEST_TYPE_OTHER
            : APIC_CMD_DEST_TYPE_BYID;

    apic_send_command(target_apic_id >= 0
                      ? target_apic_id << 24
                      : 0,
                      APIC_CMD_VECTOR_n(intr) |
                      dest_type |
                      APIC_CMD_DEST_MODE_NORMAL);
}

static void apic_enable(int enabled)
{
    if (enabled)
        APIC_SIR |= APIC_SIR_APIC_ENABLE;
    else
        APIC_SIR &= ~APIC_SIR_APIC_ENABLE;
}

static void apic_configure_timer(
        uint32_t dcr, uint32_t icr, uint8_t timer_mode,
        uint8_t intr)
{
    APIC_LVT_DCR = dcr;
    APIC_LVT_ICR = icr;
    APIC_LVT_TR = APIC_LVT_TR_VECTOR_n(intr) |
            APIC_LVT_TR_MODE_n(timer_mode);
}

int apic_init(int ap)
{
    if (ap) {
        apic_enable(1);
        intr_hook(73, apic_timer_handler);
        apic_configure_timer(APIC_LVT_DCR_BY_128,
                             ((3600000000U>>7)/60),
                             APIC_LVT_TR_MODE_PERIODIC,
                             73);

        return 1;
    }

    if (!parse_mp_tables())
        return 0;

    printk("Found MP tables\n");

    apic_base = msr_get(APIC_BASE_MSR) & -(intptr_t)4096;

    apic_ptr = mmap((void*)apic_base, 4096,
         PROT_READ | PROT_WRITE, MAP_PHYSICAL, -1, 0);

    // Read address of MP entry trampoline from boot sector
    uint32_t *mp_trampoline_ptr = (uint32_t*)0x7c40;
    uint32_t mp_trampoline_addr = *mp_trampoline_ptr;
    uint32_t mp_trampoline_page = mp_trampoline_addr >> 12;

    // Send INIT to all other CPUs (FIXME)
    apic_send_command(0,
                      APIC_CMD_DEST_MODE_INIT |
                      APIC_CMD_DEST_LOGICAL |
                      APIC_CMD_DEST_TYPE_OTHER);

    // Send SIPI to all other CPUs (FIXME)
    apic_send_command(0,
                      APIC_CMD_SIPI_PAGE_n(mp_trampoline_page) |
                      APIC_CMD_DEST_MODE_SIPI |
                      APIC_CMD_DEST_LOGICAL |
                      APIC_CMD_DEST_TYPE_OTHER);

    return 1;
}
