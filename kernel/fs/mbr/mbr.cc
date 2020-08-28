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
} _packed;

struct mbr_part_factory_t : public part_factory_t {
    constexpr mbr_part_factory_t();
    ext::vector<part_dev_t*> detect(storage_dev_base_t *drive) override;
};

static ext::vector<part_dev_t*> partitions;

constexpr mbr_part_factory_t::mbr_part_factory_t()
    : part_factory_t("mbr")
{
    part_register_factory(this);
}

ext::vector<part_dev_t *> mbr_part_factory_t::detect(storage_dev_base_t *drive)
{
    ext::vector<part_dev_t *> list;

    long sector_size = drive->info(STORAGE_INFO_BLOCKSIZE);

    if (sector_size >= 512) {
        ext::unique_ptr<uint8_t[]> sector(new (ext::nothrow)
                                          uint8_t[sector_size]);
        memset(sector, 0, sector_size);

        if (drive->read_blocks(sector, 1, 0) >= 0) {
            if (sector[510] == 0x55 && sector[511] == 0xAA) {
                partition_tbl_ent_t ptbl[4];
                memcpy(ptbl, sector + 446, sizeof(ptbl));

                for (int i = 0; i < 4; ++i) {
                    ext::unique_ptr<part_dev_t> part;

                    switch (ptbl[i].system_id) {
                    case 0x0B:// fall thru
                    case 0x0C:
                    case 0xEF:
                        // FAT32 LBA filesystem
                        part.reset(new (ext::nothrow) part_dev_t{});
                        part->drive = drive;
                        part->lba_st = ptbl[i].start_lba;
                        part->lba_len = ptbl[i].total_sectors;
                        part->name = "fat32";

                        if (unlikely(!partitions.push_back(part)))
                            panic_oom();

                        if (likely(list.push_back(part.get())))
                            part.release();
                        break;

                    case 0x83:
                        // Linux filesystem
                        ext::unique_ptr<uint8_t> ext_superblock =
                                new (ext::nothrow) uint8_t[sector_size];

                        if (drive->read_blocks(ext_superblock, 1,
                                                ptbl[i].start_lba) < 0) {
                            MBR_TRACE("Unable to read linux partition"
                                      " (at LBA %u)!\n", ptbl[i].start_lba);
                            break;
                        }

                        part.reset(new (ext::nothrow) part_dev_t{});
                        part->drive = drive;
                        part->lba_st = ptbl[i].start_lba;
                        part->lba_len = ptbl[i].total_sectors;
                        part->name = "ext4";

                        if (unlikely(!partitions.push_back(part)))
                            panic_oom();

                        if (unlikely(!list.push_back(part.get())))
                            panic_oom();

                        part.release();
                    }
                }
            }
        }
    }

    return list;
}

static mbr_part_factory_t mbr_part_factory;
