#pragma once
#include "types.h"

bool qemu_present();
int qemu_fw_cfg_present();

int qemu_selector_by_name(char const * restrict name,
                          uint32_t * restrict file_size_out = nullptr);

bool qemu_fw_cfg(void *buffer, uint32_t size, uint32_t file_size,
                 int selector = -1, off_t file_offset = 0);
