#pragma once
#include "types.h"

struct module;
struct kernel_sym;

int sys_init_module(char const *module, ptrdiff_t module_sz,
                    char const *module_name, char const *module_params);
int sys_delete_module(char const *name_user);
int sys_query_module(char const *name_user, int which, char *buf,
                     size_t bufsize, size_t *ret);
int sys_get_kernel_syms(struct kernel_sym *table);
int sys_probe_pci_for(int32_t vendor,
                      int32_t device,
                      int32_t devclass,
                      int32_t subclass,
                      int32_t prog_if);
