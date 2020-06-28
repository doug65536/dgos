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

    char const *drive_name = (char const *)drive->info(STORAGE_INFO_NAME);

    long sector_size = drive->info(STORAGE_INFO_BLOCKSIZE);

    if (unlikely(sector_size < 128))
        return list;

    // Create sector-sized buffer to look at partition header
    std::unique_ptr<uint8_t[]> buffer(new (ext::nothrow)
                                      uint8_t[sector_size]());

    if (unlikely(!buffer))
        panic_oom();

    GPT_TRACE("Reading 1 %lu byte block at LBA 1 from %s\n",
              sector_size, drive_name);

    if (unlikely(drive->read_blocks(buffer, 1, 1) < 0))
        return list;

    gpt_hdr_t hdr;
    memcpy(&hdr, buffer, sizeof(hdr));

    if (unlikely(hdr.sig != hdr.sig_expected))
        return list;

    gpt_part_tbl_ent_t ptent;

    buffer.reset();
    buffer.reset(new (ext::nothrow) uint8_t[sector_size * hdr.part_ent_count]);

    if (unlikely(!buffer))
        panic_oom();

    GPT_TRACE("Reading %u %ld byte blocks at LBA %" PRIu64 " from %s\n",
              hdr.part_ent_count, sector_size, hdr.part_ent_lba, drive_name);

    if (unlikely(drive->read_blocks(buffer, hdr.part_ent_count,
                                    hdr.part_ent_lba)) < 0)
        return list;

    for (uint32_t i = 0; i < hdr.part_ent_count; ++i) {
        std::unique_ptr<part_dev_t> part;

        // Copy into aligned object
        memcpy(&ptent, &buffer.get()[i * sector_size], sizeof(ptent));

        if (ptent.type_guid == efi_part) {
            part.reset(new (ext::nothrow) part_dev_t{});
            part->drive = drive;
            part->lba_st = ptent.lba_st;
            part->lba_len = ptent.lba_en - ptent.lba_st + 1;
            part->name = "fat32";
            GPT_TRACE("Found %" PRIu64 " block partition at LBA"
                      " %" PRIu64 "\n", part->lba_len, part->lba_st);

            if (unlikely(!partitions.push_back(part)))
                panic_oom();

            if (likely(list.push_back(part.get())))
                part.release();
            else
                panic_oom();
        }
    }

    GPT_TRACE("Found %zu partitions on %s\n", list.size(), drive_name);
    return list;
}

static gpt_part_factory_t gpt_part_factory;
