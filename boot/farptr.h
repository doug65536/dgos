#pragma once

#include "types.h"

typedef struct far_ptr_t {
    uint16_t offset;
    uint16_t segment;
} far_ptr_t;

void far_zero(far_ptr_t dest, uint16_t paragraphs);

void far_copy_to(void *near, far_ptr_t src, uint16_t size);
void far_copy_from(far_ptr_t dest, void *near, uint16_t size);

void far_copy(far_ptr_t dest,
              far_ptr_t src,
              uint16_t size);

far_ptr_t far_ptr2(uint16_t seg, uint16_t ofs);

far_ptr_t far_ptr(uint32_t addr);

far_ptr_t far_adj(far_ptr_t ptr, int32_t distance);
