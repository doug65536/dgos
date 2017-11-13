#pragma once
#include <stdint.h>

void __byte_bitmap(uint32_t (&bitmap)[256>>5], char const *s);
