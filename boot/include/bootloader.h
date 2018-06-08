#pragma once

#include "farptr.h"
#include "vesainfo.h"
#include "physmem_data.h"
#include "bootdev_info.h"

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

    ptr64_t &operator=(far_ptr_t const& ptr)
    {
        addr = uint64_t((ptr.segment << 4) + ptr.offset);
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
};

struct alignas(8) kernel_params_t {
    size_t size;
    ptr64_t<physmem_range_t> phys_mem_table;
    uint64_t phys_mem_table_size;
    ptr64_t<void(*)()> ap_entry;
    ptr64_t<vbe_info_t> vbe_info;
    ptr64_t<vbe_mode_info_t> vbe_mode_info;
    ptr64_t<vbe_selected_mode_t> vbe_selected_mode;
    uint64_t boot_drv_serial;
    uint8_t wait_gdb;
    uint8_t serial_debugout;
};
