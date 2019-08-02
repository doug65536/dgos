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

// Node array entry
struct boottbl_mem_affinity_t {
    uint64_t base;
    uint64_t len;
    uint32_t domain;
    uint32_t reserved;
};

// CPU-to-node
struct boottbl_apic_affinity_t {
    uint32_t domain;
    uint32_t apic_id;
};

// Physical address of and size of numa boottbl arrays
struct boottbl_nodes_info_t {
    uint64_t mem_affinity_addr;
    uint64_t mem_affinity_count;
    uint64_t apic_affinity_addr;
    uint64_t apic_affinity_count;
};

_pure
void *boottbl_ebda_ptr();

_pure
uint8_t boottbl_checksum(char const *bytes, size_t len);

boottbl_acpi_info_t boottbl_find_acpi_rsdp();
boottbl_mptables_info_t boottbl_find_mptables();
boottbl_nodes_info_t boottbl_find_numa(const boottbl_acpi_info_t &acpi_info);

