#pragma once
#include "types.h"

// This is included by the bootloader

// Bytes Per Sector	BPB_BytsPerSec	0x0B	16 Bits	Always 512 Bytes
// Sectors Per Cluster	BPB_SecPerClus	0x0D	8 Bits	1,2,4,8,16,32,64,128
// Number of Reserved Sectors	BPB_RsvdSecCnt	0x0E	16 Bits	Usually 0x20
// Number of FATs	BPB_NumFATs	0x10	8 Bits	Always 2
// Sectors Per FAT	BPB_FATSz32	0x24	32 Bits	Depends on disk size
// Root Directory First Cluster	BPB_RootClus	0x2C	32 Bits	Usually 0x00000002
// Signature	(none)	0x1FE	16 Bits	Always 0xAA55
struct fat32_bpb_data_t {
    uint16_t bytes_per_sec;		// 0x0B Always 512
    uint8_t sec_per_cluster;	// 0x0D 8=4KB cluster
    uint16_t reserved_sectors;	// 0x0E Usually 32
    uint8_t number_of_fats;		// 0x10 Always 2
    uint32_t sec_per_fat;		// 0x24 1 per 128 clusters
    uint32_t root_dir_start;	// 0x2C LBA
    uint32_t serial;            // 0x43 serial number
    uint16_t signature;			// 0x1FE Always 0xAA55

    // Inferred from data in on-disk BPB
    uint64_t first_fat_lba;
    uint64_t cluster_begin_lba;
} _packed;

// DOS date bitfields
//  4:0  Day (1-31)
//  8:5  Month (1-12)
// 15:9  Year relative to 1980

// DOS time bitfields
//  4:0  Seconds divided by 2 (0-29)
// 10:5  Minutes (0-59)
// 15:11 Hours (0-23)

#define FAT_LAST_LFN_ORDINAL    char(0x40)
#define FAT_ORDINAL_MASK        char(0x3F)
#define FAT_DELETED_FLAG        char(0xE5)
#define FAT_ESCAPED_0xE5        char(0x05)

#define FAT_LOWERCASE_NAME_BIT  3
#define FAT_LOWERCASE_EXT_BIT   4
#define FAT_LOWERCASE_NAME      (1U<<FAT_LOWERCASE_NAME_BIT)
#define FAT_LOWERCASE_EXT       (1U<<FAT_LOWERCASE_EXT_BIT)

#define FAT_ATTR_NONE   0
#define FAT_ATTR_RO     1
#define FAT_ATTR_HIDDEN 2
#define FAT_ATTR_SYS    4
#define FAT_ATTR_VOLUME 8
#define FAT_ATTR_DIR    16
#define FAT_ATTR_ARCH   32

#define FAT_ATTR_MASK   0x3F
#define FAT_LONGNAME (FAT_ATTR_RO | FAT_ATTR_HIDDEN | \
    FAT_ATTR_SYS | FAT_ATTR_VOLUME)

// 32 bit boundaries are marked with *
// *-+-+-+-*-+-+-+-*-+-+-+-*-+-+-+-*-+-+-+-*-+-+-+-*-+-+-+-*-+-+-+-*
// | Name          |Ext  |A|R|T|CT |CD |AD |STH|MD |MT |STL|Size   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | 0             |     |B|C|D|E  |10 |12 |14 |16 |18 |1A |1C     |
// *-+-+-+-*-+-+-+-*-+-+-+-*-+-+-+-*-+-+-+-*-+-+-+-*-+-+-+-*-+-+-+-*
// .|               |     | | | |   |   |   |   |   |   |   |      .
// .|               |     | | | |   |   |   |   |   |   |   Size   .
// .|               |     | | | |   |   |   |   |   |   Start      .
// .|               |     | | | |   |   |   |   |   |   Cluster    .
// .|               |     | | | |   |   |   |   |   |   Low        .
// .|               |     | | | |   |   |   |   |   Modified time  .
// .|               |     | | | |   |   |   |   Modified date      .
// .|               |     | | | |   |   |   Start cluster high     .
// .|               |     | | | |   |   Last access date           .
// .|               |     | | | |   Creation date                  .
// .|               |     | | | Creation time                      .
// .|               |     | | Creation 10th of a second            .
// .|               |     | Reserved                               .
// .|               |     Attributes                               .
// .|               Extension                                      .
// .Filename (padded with spaces)                                  .
// .................................................................
//
struct fat32_dir_entry_t {
    // offset = 0x00
    char name[11];

    // offset = 0x0B
    uint8_t attr;

    // offset = 0x0C (bit 4 = lowercase extension, bit 3 = lowercase filename)
    uint8_t lowercase_flags;

    // offset = 0x0D
    uint8_t creation_centisec;

    // offset = 0x0E
    uint16_t creation_time;

    // offset = 0x10
    uint16_t creation_date;

    // offset = 0x12
    uint16_t access_date;

    // offset = 0x14
    uint16_t start_hi;

    // offset = 0x16
    uint16_t modified_date;

    // offset = 0x18
    uint16_t modified_time;

    // offset = 0x1A
    uint16_t start_lo;

    // offset = 0x1C
    uint32_t size;

    bool is_directory() const
    {
        return (attr & FAT_ATTR_DIR) != 0;
    }

    bool is_readonly() const
    {
        return (attr & FAT_ATTR_RO);
    }

    bool is_within_size(off_t ofs) const
    {
        return ofs < size || is_directory();
    }
};
// Long filenames are stored in reverse order
// The last fragment of the long filename is stored first,
// with bit 6 set in its ordinal.
// The beginning of the filename has an ordinal of 1
// The 8.3 entry is immediately after the long filename entries
// Each long filename entry stores 13 UTF-16 characters

// https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#VFAT_long_file_names
// encodes 13 UTF-16 codepoints per entry
struct fat32_long_dir_entry_t {
    // offset = 0x00
    uint8_t ordinal;

    // offset = 0x01 (5 UTF-16 characters) (misaligned!)
    uint8_t name[10];

    // offset = 0x0B (FAT_LONGNAME (0x0F))
    uint8_t attr;

    // offset = 0x0C
    uint8_t zero1;

    // offset = 0x0D
    uint8_t checksum;

    // offset = 0x0E (6 UTF-16 characters)
    uint8_t name2[12];

    // offset = 0x1A
    uint16_t zero2;

    // offset = 0x1C (2 UTF-16 characters)
    uint8_t name3[4];
};

union fat32_dir_union_t {
    fat32_dir_entry_t short_entry;
    fat32_long_dir_entry_t long_entry;
};

struct fat32_lfn_fragment_t {
    uint8_t ordinal;
    uint16_t fragment[13];
};

struct filename_info_t {
    uint8_t lowercase_flags;
    uint8_t filename_length;
    uint8_t extension_length;
};

static _always_inline void fat32_parse_bpb(fat32_bpb_data_t *bpb,
                                   uint64_t partition_lba,
                                   char const *sector_buffer)
{
    bpb->bytes_per_sec = *(uint16_t*)(sector_buffer + 0x0B);
    bpb->sec_per_cluster = *(uint8_t*)(sector_buffer + 0x0D);
    bpb->reserved_sectors = *(uint16_t*)(sector_buffer + 0x0E);
    bpb->number_of_fats = *(uint8_t*)(sector_buffer + 0x10);
    bpb->sec_per_fat = *(uint32_t*)(sector_buffer + 0x24);
    bpb->root_dir_start = *(uint32_t*)(sector_buffer + 0x2C);
    bpb->serial = *(uint32_t*)(sector_buffer + 0x43);
    bpb->signature = *(uint16_t*)(sector_buffer + 0x1FE);

    bpb->first_fat_lba = partition_lba + bpb->reserved_sectors;
    bpb->cluster_begin_lba = bpb->first_fat_lba +
            bpb->number_of_fats * bpb->sec_per_fat -
            2 * bpb->sec_per_cluster;
}
