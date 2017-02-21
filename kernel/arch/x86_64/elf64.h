#pragma once

typedef int (*module_entry_fn_t)(void);

void modload_init(void);
module_entry_fn_t modload_load(char const *path);
