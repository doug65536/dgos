#define PART_T mbr
#define STORAGE_IMPL
#include "string.h"
#include "mbr.h"
#include "dev_storage.h"

DECLARE_part_DEVICE(mbr);

typedef struct part_dev_t part_dev_t;

typedef struct partition_tbl_ent_t {
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
} __attribute__((packed)) partition_tbl_ent_t;

#define MAX_PARTITIONS  128
static part_dev_t partitions[MAX_PARTITIONS];
static size_t partition_count;

static if_list_t mbr_part_detect(storage_dev_base_t *drive)
{
    unsigned start_at = partition_count;

    if_list_t list = {
        partitions + start_at,
        sizeof(*partitions),
        0
    };

    long sector_size = drive->vtbl->info(drive, STORAGE_INFO_BLOCKSIZE);
    char sig[2];

    if (sector_size >= 512) {
        char sector[sector_size];

        drive->vtbl->read_blocks(drive, sector, 1, 0);

        memcpy(sig, sector + 512 - sizeof(sig), sizeof(sig));

        if ((unsigned)sector[510] == 0x55 &&
                (unsigned)sector[511] == 0xAA) {
            partition_tbl_ent_t ptbl[4];
            memcpy(ptbl, sector + 446, sizeof(ptbl));

            for (int i = 0;
                 i < 4 && partition_count < MAX_PARTITIONS; ++i) {
                if (ptbl[i].system_id == 0x0C) {
                    part_dev_t *part = partitions + partition_count++;
                    part->drive = drive;
                    part->vtbl = &mbr_part_device_vtbl;
                    part->lba_st = ptbl[i].start_lba;
                    part->lba_len = ptbl[i].total_sectors;
                    part->name = "fat32";
                }
            }
        }
    }

    list.count = partition_count - start_at;

    return list;
}

REGISTER_part_DEVICE(mbr);

