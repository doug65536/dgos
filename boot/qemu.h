#pragma once
#include "types.h"

bool qemu_present();
ssize_t qemu_fw_cfg(void *buffer, size_t size, char const *name);
