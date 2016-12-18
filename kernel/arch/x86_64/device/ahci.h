#pragma once

#define STORAGE_DEV_NAME ahci
#include "dev_storage.h"
#ifndef STORAGE_IMPL
#undef STORAGE_DEV_NAME
#endif
