#include "dev_storage.h"
#include "string.h"
#include "unique_ptr.h"
#include "printk.h"

#define DEBUG_GPT   1
#if DEBUG_GPT
#define GPT_TRACE(...) printdbg("gpt: " __VA_ARGS__)
#else
#define GPT_TRACE(...) ((void)0)
#endif

struct guid_t {
    uint8_t v[16];

    bool operator==(guid_t const &rhs) const
    {
        return !memcmp(v, rhs.v, sizeof(v));
    }

    bool operator!=(guid_t const &rhs) const
    {
        return !(*this == rhs);
    }
};

static constexpr const guid_t efi_part = {
    0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
    0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B
};

struct part_dev_t;

struct gpt_hdr_t {
    static constexpr const uint64_t sig_expected = UINT64_C(0x5452415020494645);
    uint64_t sig;
    uint32_t rev;
    uint32_t hdr_sz;
    uint32_t hdr_crc32;
    uint32_t reserved1;
    uint64_t cur_lba;
    uint64_t bkp_lba;
    uint64_t lba_st;
    uint64_t lba_en;
    guid_t disk_guid;
    uint64_t part_ent_lba;
    uint32_t part_ent_count;
    uint32_t part_ent_sz;
    uint32_t part_ent_crc32;
};

struct gpt_part_tbl_ent_t {
    guid_t type_guid;
    guid_t uniq_guid;
    uint64_t lba_st;
    uint64_t lba_en;
    uint64_t attr;
    char16_t name[36];

    static constexpr const uint64_t attr_reqd = 1;
    static constexpr const uint64_t attr_ignore = 2;
    static constexpr const uint64_t attr_bootable = 4;
} _packed;

struct gpt_part_factory_t : public part_factory_t {
    constexpr gpt_part_factory_t();
    std::vector<part_dev_t*> detect(storage_dev_base_t *drive) override;
};

static std::vector<part_dev_t*> partitions;

constexpr gpt_part_factory_t::gpt_part_factory_t()
    : part_factory_t("gpt")
{
    part_register_factory(this);
}

std::vector<part_dev_t *> gpt_part_factory_t::detect(storage_dev_base_t *drive)
{
    std::vector<part_dev_t *> list;

    long sector_size = drive->info(STORAGE_INFO_BLOCKSIZE);

    if (unlikely(sector_size < 128))
        return list;

    std::unique_ptr<uint8_t[]> sector(new (std::nothrow)
                                      uint8_t[sector_size]());


    gpt_hdr_t hdr;

    if (unlikely(drive->read_blocks(sector, 1, 1) < 0))
        return list;

    memcpy(&hdr, sector, sizeof(hdr));

    if (unlikely(hdr.sig != hdr.sig_expected))
        return list;

    gpt_part_tbl_ent_t ptent;

    for (uint32_t i = 0; i < hdr.part_ent_count; ++i) {
        std::unique_ptr<part_dev_t> part;

        if (unlikely(drive->read_blocks(sector, 1, hdr.part_ent_lba + i) < 0))
            return list;

        memcpy(&ptent, sector, sizeof(ptent));

        if (ptent.type_guid == efi_part) {
            part.reset(new (std::nothrow) part_dev_t{});
            part->drive = drive;
            part->lba_st = ptent.lba_st;
            part->lba_len = ptent.lba_en - ptent.lba_st + 1;
            part->name = "fat32";

            partitions.push_back(part);
            if (likely(list.push_back(part.get())))
                part.release();
        }
    }

    return list;
}

static gpt_part_factory_t gpt_part_factory;
