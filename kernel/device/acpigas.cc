#include "acpigas.h"
#include "pci.h"
#include "type_traits.h"
#include "mm.h"

#if defined(__x86_64__) || defined(__i386__)
#include "cpu/ioport.h"
#endif

template<int size>
class acpi_gas_mask_t : public acpi_gas_accessor_t {
protected:
    typedef typename ext::type_from_size<size, true>::type value_type;

    acpi_gas_mask_t(int bitofs, int bitwidth)
        : mask(size != (bitwidth >> 3)
            ? ~(value_type(-1) << bitwidth) : ~value_type(0))
        , bit(bitofs)
    {
    }

    _always_inline
    value_type mask_shift(value_type old, value_type value) const
    {
        return (old & ~mask) | ((value & mask) << bit);
    }

    value_type const mask;
    uint8_t const bit;
};

template<int size>
class acpi_gas_accessor_sysmem_t final : public acpi_gas_mask_t<size> {
private:
    using base = acpi_gas_mask_t<size>;
public:
    using typename base::value_type;

    acpi_gas_accessor_sysmem_t(uint64_t mem_addr, int bitofs, int bitwidth)
        : acpi_gas_mask_t<size>(bitofs, bitwidth)
        , mem((value_type*)mmap((void*)mem_addr, size,
                                PROT_READ | PROT_WRITE, MAP_PHYSICAL))
    {
    }

    ~acpi_gas_accessor_sysmem_t()
    {
        munmap(mem, size);
    }

    size_t get_size() const override final { return size; }

    int64_t read() const override final { return *mem; }

    void write(int64_t value) const override final
    {
        if (base::mask == value_type(-1))
            *mem = value_type(value);
        else
            *mem = base::mask_shift(*mem, value);
    }

private:
    value_type *mem;
};

template<int size>
class acpi_gas_accessor_sysio_t final : public acpi_gas_mask_t<size> {
private:
    using base = acpi_gas_mask_t<size>;
public:
    using typename base::value_type;

    acpi_gas_accessor_sysio_t(uint64_t io_port, int bitofs, int bitwidth)
        : acpi_gas_mask_t<size>(bitofs, bitwidth)
        , port(ioport_t(io_port))
    {
    }

    size_t get_size() const override final { return size; }

    int64_t read() const override final { return inp<size>(port); }

    void write(int64_t value) const override final
    {
        if (base::mask == value_type(-1))
            outp<size>(port, value_type(value));
        else
            outp<size>(port, base::mask_shift(inp<size>(port), value));
    }

private:
    ioport_t port;
};

template<int size>
class acpi_gas_accessor_pcicfg_t final : public acpi_gas_mask_t<size> {
private:
    using base = acpi_gas_mask_t<size>;
public:
    using typename base::value_type;

    acpi_gas_accessor_pcicfg_t(uint64_t pci_dfo, int bitofs, int bitwidth)
        : acpi_gas_mask_t<size>(bitofs, bitwidth)
        , dfo(pci_dfo)
    {
    }

    size_t get_size() const override final { return size; }

    int64_t read() const override final
    {
        value_type result;
        pci_config_copy(pci_addr_t(0, 0, (dfo >> 32) & 0xFF,
                        (dfo >> 16) & 0xFF), &result,
                        dfo & 0xFF, size);
        return result;
    }

    void write(int64_t value) const override final
    {
        pci_config_write(pci_addr_t(0, 0, (dfo >> 32) & 0xFF,
                         (dfo >> 16) & 0xFF), dfo & 0xFF, &value, size);
    }

private:
    uint64_t dfo;
};

acpi_gas_accessor_t *acpi_gas_accessor_t::from_gas(acpi_gas_t const& gas)
{
    uint64_t addr = gas.addr_lo | (uint64_t(gas.addr_hi) << 32);

    switch (gas.addr_space) {
    case ACPI_GAS_ADDR_SYSMEM:
        return from_sysmem(addr, gas.access_size,
                           gas.bit_offset, gas.bit_width);
    case ACPI_GAS_ADDR_FIXED:
        return from_fixed(addr, gas.access_size,
                          gas.bit_offset, gas.bit_width);
    case ACPI_GAS_ADDR_SYSIO:
        return from_ioport(addr, gas.access_size,
                           gas.bit_offset, gas.bit_width);
    case ACPI_GAS_ADDR_PCICFG:
        return from_pcicfg(addr, gas.access_size,
                           gas.bit_offset, gas.bit_width);
    default:
        return nullptr;
    }
}

acpi_gas_accessor_t *acpi_gas_accessor_t::from_sysmem(uint64_t addr, int size,
                                                      int bitofs, int bitwidth)
{
    switch (size) {
    case 1: return new (std::nothrow)
                acpi_gas_accessor_sysmem_t<1>(addr, bitofs, bitwidth);
    case 2: return new (std::nothrow)
                acpi_gas_accessor_sysmem_t<2>(addr, bitofs, bitwidth);
    case 4: return new (std::nothrow)
                acpi_gas_accessor_sysmem_t<4>(addr, bitofs, bitwidth);
    case 8: return new (std::nothrow)
                acpi_gas_accessor_sysmem_t<8>(addr, bitofs, bitwidth);
    default: return nullptr;
    }
}

acpi_gas_accessor_t *acpi_gas_accessor_t::from_pcicfg(uint32_t addr, int size,
                                                      int bitofs, int bitwidth)
{
    switch (size) {
    case 1: return new (std::nothrow)
                acpi_gas_accessor_pcicfg_t<1>(addr, bitofs, bitwidth);
    case 2: return new (std::nothrow)
                acpi_gas_accessor_pcicfg_t<2>(addr, bitofs, bitwidth);
    case 4: return new (std::nothrow)
                acpi_gas_accessor_pcicfg_t<4>(addr, bitofs, bitwidth);
    case 8: return new (std::nothrow)
                acpi_gas_accessor_pcicfg_t<8>(addr, bitofs, bitwidth);
    default: return nullptr;
    }
}

acpi_gas_accessor_t *acpi_gas_accessor_t::from_ioport(uint16_t ioport, int size,
                                                      int bitofs, int bitwidth)
{
#if defined(__x86_64__) || defined(__i386__)
    switch (size) {
    case 1: return new (std::nothrow)
                acpi_gas_accessor_sysio_t<1>(ioport, bitofs, bitwidth);
    case 2: return new (std::nothrow)
                acpi_gas_accessor_sysio_t<2>(ioport, bitofs, bitwidth);
    case 4: return new (std::nothrow)
                acpi_gas_accessor_sysio_t<4>(ioport, bitofs, bitwidth);
    case 8: return nullptr;
    default: return nullptr;
    }
#else
    return nullptr;
#endif
}

acpi_gas_accessor_t *acpi_gas_accessor_t::from_fixed(uint16_t ioport, int size,
                                                     int bitofs, int bitwidth)
{
    return from_ioport(ioport, size, bitofs, bitwidth);
}

acpi_gas_accessor_t::~acpi_gas_accessor_t()
{
}
