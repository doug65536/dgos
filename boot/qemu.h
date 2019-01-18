#pragma once
#include "types.h"

bool qemu_present();
bool qemu_fw_cfg_present();

int qemu_selector_by_name(char const * restrict name,
                          uint32_t * restrict file_size_out = nullptr);
int qemu_selector_by_name_dma(const char * restrict name,
                              uint32_t * restrict file_size_out = nullptr);

ssize_t qemu_fw_cfg(void *buffer, size_t size, char const *name);

bool qemu_fw_cfg_dma(uintptr_t buffer_addr, uint32_t size,
                     int selector, uint64_t file_offset = 0,
                     bool read = true);

static inline bool qemu_fw_cfg_dma(void *buffer_ptr, uint32_t size,
                                   int selector, uint64_t file_offset = 0,
                                   bool read = true)
{
    return qemu_fw_cfg_dma(uintptr_t(buffer_ptr), size,
                           selector, file_offset, read);
}
