#pragma once

#include "farptr.h"
#include "vesainfo.h"
#include "physmem_data.h"
#include "boottable.h"
#include "assert.h"

// Helper class to enforce 64 bit pointer in 32 bit code
template<typename T>
class alignas(8) ptr64_t {
public:
    ptr64_t()
        : addr(0)
    {
    }

    ptr64_t &operator=(uintptr_t ptr)
    {
        addr = uint64_t(ptr);
        return *this;
    }

    ptr64_t &operator=(T *ptr)
    {
        addr = uint64_t(ptr);
        return *this;
    }

    T *operator->()
    {
        return reinterpret_cast<T *>(addr);
    }

    T const *operator->() const
    {
        return reinterpret_cast<T const *>(addr);
    }

    T& operator*()
    {
        return *reinterpret_cast<T *>(addr);
    }

    operator uintptr_t()
    {
        return addr;
    }

    T& operator*() const
    {
        return *reinterpret_cast<T const *>(addr);
    }

    operator T*()
    {
        return reinterpret_cast<T *>(addr);
    }

    operator T const *() const
    {
        return reinterpret_cast<T const *>(addr);
    }

    operator uintptr_t() const
    {
        return addr;
    }

private:
    uint64_t addr;
} _packed;

struct alignas(8) kernel_params_t {
    uint64_t size;
    ptr64_t<physmem_range_t> phys_mem_table;
    uint64_t phys_mem_table_size;
    ptr64_t<void(*)()> ap_entry;
    ptr64_t<vbe_info_t> vbe_info;
    ptr64_t<vbe_selected_mode_t> vbe_selected_mode;
    // 3 * 8 bytes
    boottbl_acpi_info_t acpi_rsdt;
    // 8 bytes
    boottbl_mptables_info_t mptables;
    // 4 * 8 bytes
    boottbl_nodes_info_t numa;
    uint64_t boot_drv_serial;
    uint64_t initrd_st;
    uint64_t initrd_sz;
    uint64_t phys_mapping;
    uint64_t phys_mapping_sz;
    ptr64_t<char> command_line;

    uint8_t gdb_port;
    uint8_t testrun_port;
    uint8_t serial_debugout;
    uint8_t serial_baud;

    uint8_t smp_enable;
    uint8_t acpi_enable;
    uint8_t mps_enable;
    uint8_t msi_enable;
    uint8_t msix_enable;
    uint8_t e9_enable;
} _packed;

// Ensure that all of the architectures have the same layout
C_ASSERT(sizeof(kernel_params_t) == 22 * 8);
