#pragma once
#include "assert.h"

// ACPI Generic Address Structure

// Generic Address Structure
struct acpi_gas_t {
    uint8_t addr_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint32_t addr_lo;
    uint32_t addr_hi;
} __packed;

C_ASSERT(sizeof(acpi_gas_t) == 12);

#define ACPI_GAS_ADDR_SYSMEM    0
#define ACPI_GAS_ADDR_SYSIO     1
#define ACPI_GAS_ADDR_PCICFG    2
#define ACPI_GAS_ADDR_EMBED     3
#define ACPI_GAS_ADDR_SMBUS     4
#define ACPI_GAS_ADDR_FIXED     0x7F

#define ACPI_GAS_ASZ_UNDEF  0
#define ACPI_GAS_ASZ_8      1
#define ACPI_GAS_ASZ_16     2
#define ACPI_GAS_ASZ_32     3
#define ACPI_GAS_ASZ_64     4


class acpi_gas_accessor_t {
public:
    static acpi_gas_accessor_t *from_gas(acpi_gas_t const& gas);
    static acpi_gas_accessor_t *from_sysmem(uint64_t addr, int size,
                                            int bitofs, int bitwidth);
    static acpi_gas_accessor_t *from_pcicfg(uint32_t addr, int size,
                                            int bitofs, int bitwidth);
    static acpi_gas_accessor_t *from_ioport(uint16_t ioport, int size,
                                            int bitofs, int bitwidth);
    static acpi_gas_accessor_t *from_fixed(uint16_t ioport, int size,
                                           int bitofs, int bitwidth);

    virtual ~acpi_gas_accessor_t() {}
    virtual size_t get_size() const = 0;
    virtual int64_t read() const = 0;
    virtual void write(int64_t value) const = 0;
};
