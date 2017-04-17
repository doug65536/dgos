#include "dev_storage.h"
#include "string.h"
#include "unique_ptr.h"
#include "printk.h"

#define DEBUG_MBR   1
#if DEBUG_MBR
#define MBR_TRACE(...) printdbg("mbr: " __VA_ARGS__)
#else
#define MBR_TRACE(...) ((void)0)
#endif

struct part_dev_t;

struct partition_tbl_ent_t {
    uint8_t  boot;					//0: Boot indicator bit flag: 0 = no,
                                    // 0x80 = bootable (or "active")
    uint8_t  start_head;			// H

    uint16_t start_sector : 6;		// S
    uint16_t start_cyl : 10;		// C

    uint8_t  system_id;
    uint8_t  end_head;				// H

    uint16_t end_sector : 6;		// S
    uint16_t end_cyl : 10;			// C

    uint32_t start_lba;
    uint32_t total_sectors;
} __packed;

struct mbr_part_factory_t : public part_factory_t {
    mbr_part_factory_t() : part_factory_t("mbr") {}
    if_list_t detect(storage_dev_base_t *drive);
};

static mbr_part_factory_t mbr_part_factory;

#define MAX_PARTITIONS  128
static part_dev_t partitions[MAX_PARTITIONS];
static size_t partition_count;

if_list_t mbr_part_factory_t::detect(storage_dev_base_t *drive)
{
    unsigned start_at = partition_count;

    if_list_t list = {
        partitions + start_at,
        sizeof(*partitions),
        0
    };

    long sector_size = drive->info(STORAGE_INFO_BLOCKSIZE);
    char sig[2];

    if (sector_size >= 512) {
        unique_ptr<uint8_t> sector = new uint8_t[sector_size];

        if (drive->read_blocks(sector, 1, 0) >= 0) {
            memcpy(sig, sector + 512 - sizeof(sig), sizeof(sig));

            if (sector[510] == 0x55 &&
                    sector[511] == 0xAAU) {
                partition_tbl_ent_t ptbl[4];
                memcpy(ptbl, sector + 446, sizeof(ptbl));

                for (int i = 0;
                     i < 4 && partition_count < MAX_PARTITIONS; ++i) {
                    if (ptbl[i].system_id == 0x0C) {
                        part_dev_t *part = partitions + partition_count++;
                        part->drive = drive;
                        part->lba_st = ptbl[i].start_lba;
                        part->lba_len = ptbl[i].total_sectors;
                        part->name = "fat32";
                    }
                }
            }
        }
    }

    list.count = partition_count - start_at;

    return list;
}

