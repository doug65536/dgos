#pragma once

#include "types.h"
#include "cpu/isr.h"
#include "cxxexception.h"

typedef uintptr_t physaddr_t;
typedef uintptr_t linaddr_t;

extern physaddr_t root_physaddr;

extern "C" void mmu_init();
uintptr_t mm_create_process(void);
void mm_destroy_process(void);

extern "C" isr_context_t *mmu_page_fault_handler(int intr, isr_context_t *ctx);
