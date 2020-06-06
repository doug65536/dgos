#pragma once

#include "types.h"
#include "string.h"

struct far_ptr_t {
    uint16_t offset;
    uint16_t segment;

    far_ptr_t()
        : offset{}
        , segment{}
    {
    }

    template<typename T>
    far_ptr_t(T *init)
    {
        *this = init;
    }

    template<typename T>
    operator T*()
    {
        return (T*)(uintptr_t(segment) << 4) + offset;
    }

    template<typename T>
    operator T const*() const
    {
        return (T*)(uintptr_t(segment) << 4) + offset;
    }

    far_ptr_t& adj_seg(uint_fast16_t new_seg)
    {
        uint32_t addr = (segment << 4) + offset;
        uint32_t seg_addr = new_seg << 4;
        segment = new_seg;
        offset = addr - seg_addr;
        return *this;
    }

    template<typename T>
    far_ptr_t& operator=(T *p)
    {
        segment = uintptr_t(p) >> 4;
        offset = uintptr_t(p) & 0xF;
        return *this;
    }
} _packed;
