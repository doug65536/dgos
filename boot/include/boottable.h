#pragma once
#include "types.h"

struct boottbl_acpi_info_t {
    uint64_t rsdt_addr;
    uint64_t rsdt_size;
    uint64_t ptrsz;
} _packed;

struct boottbl_mptables_info_t {
    uint64_t mp_addr;
} _packed;

_pure
void *boottbl_ebda_ptr();

_pure
uint8_t boottbl_checksum(char const *bytes, size_t len);

boottbl_acpi_info_t boottbl_find_acpi_rsdp();
boottbl_mptables_info_t boottbl_find_mptables();

