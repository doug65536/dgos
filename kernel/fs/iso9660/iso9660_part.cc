#include "dev_storage.h"
#include "string.h"
#include "unique_ptr.h"

#include "iso9660_decl.h"

__BEGIN_ANONYMOUS

//struct part_dev_t;

struct iso9660_part_factory_t : public part_factory_t {
    iso9660_part_factory_t();
    ext::vector<part_dev_t*> detect(storage_dev_base_t *drive) override;
};

static ext::vector<part_dev_t*> partitions;

iso9660_part_factory_t::iso9660_part_factory_t()
    : part_factory_t("iso9660")
{
    part_register_factory(this);
}

ext::vector<part_dev_t *>
iso9660_part_factory_t::detect(storage_dev_base_t *drive)
{
    ext::vector<part_dev_t *> list;

    long sector_size = drive->info(STORAGE_INFO_BLOCKSIZE);
    char sig[5];

    long sector_mul = 2048 / sector_size;

    if (sector_mul < 1)
        sector_mul = 1;

    ext::unique_ptr<char[]> sector =
            new (ext::nothrow) char[sector_size * sector_mul];
    iso9660_pvd_t *pvd = (iso9660_pvd_t*)sector.get();

    int err = drive->read_blocks(sector, 1 * sector_mul, 16 * sector_mul);

    if (err >= 0)
        memcpy(sig, sector + 1, sizeof(sig));

    if (err >= sector_mul && !memcmp(sig, "CD001", 5)) {
        ext::unique_ptr<part_dev_t> part(new (ext::nothrow) part_dev_t{});

        part->drive = drive;
        part->lba_st = 0;
        part->lba_len = pvd->block_count.le;
        part->name = "iso9660";

        if (unlikely(!partitions.push_back(part)))
            panic_oom();

        if (likely(list.push_back(part.get())))
            part.release();
    }

    return list;
}

static iso9660_part_factory_t iso9660_part_factory;

__END_ANONYMOUS
