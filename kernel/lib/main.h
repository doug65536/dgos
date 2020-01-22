#pragma once
#include "types.h"
#include "bootloader.h"

extern kernel_params_t *kernel_params;

// Boundaries of kernel image
extern char __image_start[];
extern char ___init_brk[];

extern char kernel_stack[];
extern size_t const kernel_stack_size;

size_t kernel_get_size();
