#include "boottable.h"
#include "boottable_decl.h"
#include "assert.h"
#include "string.h"
#include "screen.h"

#define DEBUG_BOOTTBL   1
#if DEBUG_BOOTTBL
#define BOOTTBL_TRACE(...) PRINT(TSTR "boottbl: " __VA_ARGS__)
#else
#define BOOTTBL_TRACE(...) ((void)0)
#endif


static void *boottbl_ebda_ptr()
{
    // Read address of EBDA from BIOS data area
    return (void*)uintptr_t(*(uint16_t*)0x40E << 4);
}

static uint8_t boottbl_checksum(char const *bytes, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
        sum += uint8_t(bytes[i]);
    return sum;
}

template<typename R, typename T, size_t N>
static bool boottbl_run_searches(R &result, T (&search_data)[N])
{
    for (size_t i = 0; i < N; ++i) {
        if (unlikely(!search_data[i].start))
            continue;

        if (search_data[i].search_fn(
                    result, search_data[i].start, search_data[i].len))
            return true;
    }

    return false;
}

static bool boottbl_rsdp_search(
        boottbl_acpi_info_t &result, void const *start, size_t len)
{
    for (size_t offset = 0; offset < len; offset += 16) {
        acpi_rsdp2_t *rsdp2 = (acpi_rsdp2_t*)((char*)start + offset);

        // Check for ACPI 2.0+ RSDP
        if (!memcmp(rsdp2->rsdp1.sig, "RSD PTR ", 8)) {
            // Check checksum
            if (rsdp2->rsdp1.rev != 0 &&
                    boottbl_checksum((char*)rsdp2, sizeof(*rsdp2)) == 0 &&
                    boottbl_checksum((char*)&rsdp2->rsdp1,
                                   sizeof(rsdp2->rsdp1)) == 0) {
                if (rsdp2->xsdt_addr_lo | rsdp2->xsdt_addr_hi) {
                    BOOTTBL_TRACE("Found 64-bit XSDT\n");
                    result.rsdt_addr = rsdp2->xsdt_addr_lo |
                            ((uint64_t)rsdp2->xsdt_addr_hi << 32);
                    result.ptrsz = sizeof(uint64_t);
                } else {
                    BOOTTBL_TRACE("Found 32-bit RSDT\n");
                    result.rsdt_addr = rsdp2->rsdp1.rsdt_addr;
                    result.ptrsz = sizeof(uint32_t);
                }

                result.rsdt_size = rsdp2->length;

                BOOTTBL_TRACE("Found ACPI 2.0+ RSDP at 0x%" PRIx64 "\n",
                              result.rsdt_addr);

                return true;
            }
        }

        // Check for ACPI 1.0 RSDP
        acpi_rsdp_t *rsdp = (acpi_rsdp_t*)rsdp2;
        if (rsdp->rev == 0 &&
                !memcmp(rsdp->sig, "RSD PTR ", 8)) {
            // Check checksum
            if (boottbl_checksum((char*)rsdp, sizeof(*rsdp)) == 0) {
                result.rsdt_addr = rsdp->rsdt_addr;
                result.rsdt_size = 0;
                result.ptrsz = sizeof(uint32_t);

                // Leave acpi_rsdt_len 0 in this case, it is
                // handled later

                BOOTTBL_TRACE("Found ACPI 1.0 RSDP at 0x%" PRIx64 "\n",
                              result.rsdt_addr);

                // Keep looking, might find ACPI 2.0+ RSDP...
            }
        }
    }

    return result.rsdt_addr != 0;
}

boottbl_acpi_info_t boottbl_find_acpi_rsdp()
{
    boottbl_acpi_info_t result{};

    // ACPI RSDP can be found:
    //  - in the first 1KB of the EBDA
    //  - in the 128KB range starting at 0xE0000

    void *p_ebda = boottbl_ebda_ptr();
    void *p_E0000 = (void*)0xE0000;

    struct range {
        void *start;
        size_t len;
        bool (*search_fn)(boottbl_acpi_info_t &result,
                          void const *start, size_t len);
    } const search_data[] = {
        range{ p_ebda, 0x400, boottbl_rsdp_search },
        range{ p_E0000, 0x20000, boottbl_rsdp_search },
    };

    boottbl_run_searches(result, search_data);

    return result;
}

static bool boottbl_mptable_search(
        boottbl_mptables_info_t &result, void const *start, size_t len)
{
    for (size_t offset = 0; offset < len; offset += 16) {
        mp_table_hdr_t *sig_srch = (mp_table_hdr_t*)
                ((char*)start + offset);

        // Check for MP tables signature
        if (!memcmp(sig_srch->sig, "_MP_", 4)) {
            // Check checksum
            if (boottbl_checksum((char*)sig_srch, sizeof(*sig_srch)) == 0) {
                result.mp_addr = sig_srch->phys_addr;

                BOOTTBL_TRACE(TSTR "Found MPS tables at %" PRIx64 "\n",
                              result.mp_addr);

                return true;
            }
        }
    }

    return false;
}

boottbl_mptables_info_t boottbl_find_mptables()
{
    boottbl_mptables_info_t result{};

    void *p_ebda = boottbl_ebda_ptr();
    void *p_9FC00 = (void*)0x9FC00;
    void *p_F0000 = (void*)0xE0000;

    struct range {
        void *start;
        size_t len;
        bool (*search_fn)(boottbl_mptables_info_t &result,
                          void const *start, size_t len);
    } const search_data[] = {
        range{ p_ebda, 0x400, boottbl_mptable_search },
        range{ p_9FC00 != p_ebda ? p_9FC00 : nullptr,
                    0x400, boottbl_mptable_search },
        range{ p_F0000, 0x10000, boottbl_mptable_search }
    };

    boottbl_run_searches(result, search_data);

    return result;
}
