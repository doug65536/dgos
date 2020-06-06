#pragma once
#include "assert.h"
#include "acpi_decl.h"

// ACPI Generic Address Structure

class acpi_gas_accessor_t {
public:
    static acpi_gas_accessor_t *from_gas(acpi_gas_t const& gas);
    static acpi_gas_accessor_t *from_sysmem(uint_fast64_t addr, int size,
                                            int bitofs, int bitwidth);
    static acpi_gas_accessor_t *from_pcicfg(uint_fast32_t addr, int size,
                                            int bitofs, int bitwidth);
    static acpi_gas_accessor_t *from_ioport(uint_fast16_t ioport, int size,
                                            int bitofs, int bitwidth);
    static acpi_gas_accessor_t *from_fixed(uint_fast16_t ioport, int size,
                                           int bitofs, int bitwidth);

    virtual ~acpi_gas_accessor_t() = 0;
    virtual size_t get_size() const = 0;
    virtual int64_t read() const = 0;
    virtual void write(int64_t value) const = 0;
};
