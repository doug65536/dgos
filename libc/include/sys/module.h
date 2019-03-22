#pragma once
#include <stdlib.h>

int init_module(const void *module, size_t module_sz, char const *module_name,
                struct module *mod_user);
