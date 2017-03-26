#define PART_T iso9660
#define STORAGE_IMPL
#include "string.h"
#include "iso9660_part.h"
#include "dev_storage.h"
#include "unique_ptr.h"

#include "iso9660_decl.h"

//typedef struct part_dev_t part_dev_t;

struct iso9660_part_factory_t : public part_factory_t {
    iso9660_part_factory_t() : part_factory_t("iso9660") {}
    if_list_t detect(storage_dev_base_t *drive);
};

static iso9660_part_factory_t iso9660_factory;

#define MAX_PARTITIONS  128
static part_dev_t partitions[MAX_PARTITIONS];
static size_t partition_count;

if_list_t iso9660_part_factory_t::detect(storage_dev_base_t *drive)
{
    unsigned start_at = partition_count;

    if_list_t list = {
        partitions + start_at,
        sizeof(*partitions),
        0
    };

    long sector_size = drive->info(STORAGE_INFO_BLOCKSIZE);
    char sig[5];

    long sector_mul = 2048 / sector_size;

    if (sector_mul < 1)
        sector_mul = 1;

    unique_ptr<char> sector(new char[sector_size * sector_mul]);
    iso9660_pvd_t *pvd = (iso9660_pvd_t*)sector.get();

    int err = drive->read_blocks(sector,
                             1 * sector_mul,
                             16 * sector_mul);

    if (err == 0)
        memcpy(sig, sector + 1, sizeof(sig));

    if (err == 0 && !memcmp(sig, "CD001", 5)) {
        part_dev_t *part = partitions + partition_count++;

        part->drive = drive;
        part->lba_st = 0;
        part->lba_len = pvd->block_count.le;
        part->name = "iso9660";
    }

    list.count = partition_count - start_at;

    return list;
}
