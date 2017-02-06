#define PART_T iso9660
#define STORAGE_IMPL
#include "string.h"
#include "iso9660_part.h"
#include "dev_storage.h"

DECLARE_part_DEVICE(iso9660);

#include "iso9660_decl.h"

//typedef struct part_dev_t part_dev_t;

#define MAX_PARTITIONS  128
static part_dev_t partitions[MAX_PARTITIONS];
static size_t partition_count;

static if_list_t iso9660_part_detect(storage_dev_base_t *drive)
{
    unsigned start_at = partition_count;

    if_list_t list = {
        partitions + start_at,
        sizeof(*partitions),
        0
    };

    long sector_size = drive->vtbl->info(drive, STORAGE_INFO_BLOCKSIZE);
    char sig[5];

    long sector_mul = 2048 / sector_size;

    if (sector_mul < 1)
        sector_mul = 1;

    char sector[sector_size * sector_mul];
    iso9660_pvd_t *pvd = (void*)sector;

    int err = drive->vtbl->read_blocks(drive, sector,
                             1 * sector_mul,
                             16 * sector_mul);

    if (err == 0)
        memcpy(sig, sector + 1, sizeof(sig));

    if (err == 0 && !memcmp(sig, "CD001", 5)) {
        part_dev_t *part = partitions + partition_count++;

        part->drive = drive;
        part->vtbl = &iso9660_part_device_vtbl;
        part->lba_st = 0;
        part->lba_len = pvd->block_count.le;
        part->name = "iso9660";
    }

    list.count = partition_count - start_at;

    return list;
}

REGISTER_part_DEVICE(iso9660);

