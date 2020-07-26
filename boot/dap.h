#pragma once

#include "asmstruct.h"

struct_begin dap
struct_field 1 dap_sizeof_packet
struct_field 1 dap_reserved
struct_field 2 dap_block_count
struct_field 4 dap_address
struct_field 8 dap_lba
struct_end dap

struct_begin ddp
struct_field 2 ddp_struct_size
struct_field 2 ddp_info_flags
struct_field 4 ddp_cylinders
struct_field 4 ddp_heads
struct_field 4 ddp_sectors
struct_field 8 ddp_total_sectors
struct_field 2 ddp_sector_size
struct_field 2 ddp_dpte_ofs
struct_field 2 ddp_dpte_seg
struct_field 2 ddp_dpi_sig
struct_field 1 ddp_dpi_len
struct_field 1 ddp_reserved1
struct_field 2 ddp_reserved2
struct_field 4 ddp_bus_type
struct_field 8 ddp_if_type
struct_field 8 ddp_if_path
struct_field 8 ddp_dev_path
struct_field 1 ddp_reserved3
struct_field 1 ddp_dpi_checksum
struct_end ddp
