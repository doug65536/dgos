#pragma once

#include "asmstruct.h"

struct_begin    gpt_lba1_hdr
struct_field  8 gpt_lba1_hdr_sig
struct_field  4 gpt_lba1_hdr_rev
struct_field  4 gpt_lba1_hdr_sz_le
struct_field  4 gpt_lba1_hdr_crc32
struct_field  4 gpt_lba1_hdr_reserved1
struct_field  8 gpt_lba1_hdr_cur_lba
struct_field  8 gpt_lba1_hdr_bkp_lba
struct_field  8 gpt_lba1_hdr_st_lba
struct_field  8 gpt_lba1_hdr_en_lba
struct_field 16 gpt_lba1_hdr_guid
struct_field  8 gpt_lba1_hdr_st_pent
struct_field  4 gpt_lba1_hdr_nr_pent
struct_field  4 gpt_lba1_hdr_sz_pent
struct_field  4 gpt_lba1_hdr_crc_pent_le
// Rest must be zeros (420 bytes on 512 byte sector)
struct_end      gpt_lba1_hdr

struct_begin    gpt_lba2_part
struct_field 16 gpt_lba2_part_type_guid
struct_field 16 gpt_lba2_part_uniq_guid
struct_field  8 gpt_lba2_part_lba_st
struct_field  8 gpt_lba2_part_lba_en
struct_field  8 gpt_lba2_part_attr
struct_field 72 gpt_lba2_part_utf16le_name
struct_end      gpt_lba2_part

.set gpt_lba2_part_attr_reqd_bit, 0
.set gpt_lba2_part_attr_ign_bit,  1
.set gpt_lba2_part_attr_bios_bit, 2
.set gpt_lba2_part_attr_ro_bit, 60
.set gpt_lba2_part_attr_shadow_bit, 61
.set gpt_lba2_part_attr_hidden_bit, 62
.set gpt_lba2_part_attr_nomount_bit, 63
.set gpt_lba2_part_attr_booted_bit, 56
.set gpt_lba2_part_attr_tries_bit, 52
.set gpt_lba2_part_attr_tries_bits, 4
.set gpt_lba2_part_attr_prio_bit, 48
.set gpt_lba2_part_attr_prio_bits, 4

.set gpt_lba2_part_attr_reqd, \
	(1ULL<<gpt_lba2_part_attr_reqd_bit)

.set gpt_lba2_part_attr_ign, \
	(1ULL<<gpt_lba2_part_attr_ign_bit)

.set gpt_lba2_part_attr_bios, \
	(1ULL<<gpt_lba2_part_attr_bios_bit)

.set gpt_lba2_part_attr_ro, \
	(1ULL<<gpt_lba2_part_attr_ro_bit)

.set gpt_lba2_part_attr_shadow, \
	(1ULL<<gpt_lba2_part_attr_shadow_bit)

.set gpt_lba2_part_attr_hidden, \
	(1ULL<<gpt_lba2_part_attr_hidden_bit)

.set gpt_lba2_part_attr_nomount, \
	(1ULL<<gpt_lba2_part_attr_nomount_bit)

.set gpt_lba2_part_attr_booted, \
	(1ULL<<gpt_lba2_part_attr_booted_bit)

.set gpt_lba2_part_attr_tries_bits, 4

.set gpt_lba2_part_attr_tries_mask, \
	((1ULL<<gpt_lba2_part_attr_tries_bits)-1)

.set gpt_lba2_part_attr_tries, \
	(gpt_lba2_part_attr_tries_mask<<gpt_lba2_part_attr_tries_bit)

.set gpt_lba2_part_attr_prio_mask, \
	((1ULL<<gpt_lba2_part_attr_prio_bits)-1)

.set gpt_lba2_part_attr_prio, \
	(gpt_lba2_part_attr_prio_mask<<gpt_lba2_part_attr_prio_bit)

.set gpt_lba2_part_attr_prio_bits, 4
