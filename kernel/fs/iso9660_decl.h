#pragma once

typedef struct uint8_both_t {
    uint8_t le;
    uint8_t be;
} uint8_both_t;

typedef struct uint16_both_t {
    uint16_t le;
    uint16_t be;
} uint16_both_t;

typedef struct uint32_both_t {
    uint32_t le;
    uint32_t be;
} uint32_both_t;

typedef struct uint64_both_t {
    uint64_t le;
    uint64_t be;
} uint64_both_t;

typedef struct int8_both_t {
    int8_t le;
    int8_t be;
} int8_both_t;

typedef struct int16_both_t {
    int16_t le;
    int16_t be;
} int16_both_t;

typedef struct int32_both_t {
    int32_t le;
    int32_t be;
} int32_both_t;

typedef struct int64_both_t {
    int64_t le;
    int64_t be;
} int64_both_t;

typedef struct iso9660_pvd_datetime_t {
    char year[4];
    char month[2];
    char day[2];
    char hour[2];
    char min[2];
    char sec[2];
    char hundreths[2];
    int8_t tzofs;
} iso9660_pvd_datetime_t;

typedef struct iso9660_datetime_t {
    // Since 1900
    uint8_t year;

    // 1 to 12
    uint8_t month;

    // 1 to 31
    uint8_t day;

    // 0 to 23
    uint8_t hour;

    // 0 to 59
    uint8_t min;

    // 0 to 59
    uint8_t sec;

    // -48 to +52 (15 minute intervals)
    int8_t tz_ofs;
} iso9660_datetime_t;

// 34 bytes
typedef struct iso9660_dir_ent_t {
    // directory entries cannot span a sector boundary
    // treat 0 len as 1 byte ignored entry
    uint8_t len;
    uint8_t ea_len;

    //
    // ISO, come on, morons. Misaligned fields.

    // Miasligned LBA field
    uint16_t lba_lo_le;
    uint16_t lba_hi_le;
    uint16_t lba_hi_be;
    uint16_t lba_lo_be;

    // Misaligned size field
    uint16_t size_lo_le;
    uint16_t size_hi_le;
    uint16_t size_hi_be;
    uint16_t size_lo_be;

    // 7 bytes
    iso9660_datetime_t date;

    uint8_t flags;

    uint8_t interleave_unit;

    uint8_t interleve_gap;

    int16_both_t disc_number;

    uint8_t filename_len;

    char name[1];

    // ...followed by filename
    // use len field to find next dir entry
} iso9660_dir_ent_t;

// Primary Volume Descriptor
typedef struct iso9660_pvd_t {
    uint8_t type_code;

    // Always "CD001"
    char id[5];

    // Always 1
    uint8_t ver;

    char unused[1];

    // The name of the system that can act upon sectors 0x00-0x0F for the volume.
    char system_id[32];

    char volume_id[32];

    char unused2[8];

    int32_both_t block_count;

    char unused3[32];

    uint16_both_t disc_count;

    uint16_both_t disc_number;

    // Probably 2KB
    uint16_both_t block_size;

    uint32_both_t path_table_bytes;

    uint32_t path_table_le_lba;

    uint32_t opt_path_table_le_lba;

    uint32_t path_table_be_lba;

    uint32_t opt_path_table_be_lba;

    // 34 bytes! watch out!
    iso9660_dir_ent_t root_dirent;
    char root_dirent_name[2];

    char volume_set_id[128];

    char publisher_id[128];

    char preparer_id[128];

    char app_id[128];

    char copyright_file[38];

    char abstract_file[36];

    char bibliography_id[37];

    iso9660_pvd_datetime_t created_datetime;
    iso9660_pvd_datetime_t modified_datetime;
    iso9660_pvd_datetime_t expiry_datetime;
    iso9660_pvd_datetime_t effective_datetime;

    // Always 1
    uint8_t structure_version;

    char unused4[1];

    char application_specific[512];

    char reserved[653];
} iso9660_pvd_t;

typedef struct iso9660_sector_iterator_t {
    uint32_t lba;
    uint32_t size;
    uint16_t err;
} iso9660_sector_iterator_t;
