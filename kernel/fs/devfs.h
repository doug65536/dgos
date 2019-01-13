#pragma once
#include "dev_storage.h"

struct dev_fs_t;

dev_fs_t *devfs_create();
void devfs_delete(dev_fs_t *dev_fs);
fs_base_t *devfs_resolve(dev_fs_t *dev_fs, char const *path);
