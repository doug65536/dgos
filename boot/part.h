#pragma once

#include "types.h"

typedef struct {
	uint8_t  boot;					//0: Boot indicator bit flag: 0 = no, 0x80 = bootable (or "active")
	uint8_t  start_head;			// H

	uint16_t start_sector : 6;		// S
	uint16_t start_cyl : 10;		// C

	uint8_t  system_id;
	uint8_t  end_head;				// H

	uint16_t end_sector : 6;		// S
	uint16_t end_cyl : 10;			// C

	uint32_t start_lba;
	uint32_t total_sectors;
} __attribute__((packed)) PartitionTableEntry;

extern PartitionTableEntry partition_table[];
