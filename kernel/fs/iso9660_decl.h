#pragma once

#include "types.h"
#include "assert.h"

struct uint8_both_t {
    uint8_t le;
    uint8_t be;
};

struct uint16_both_t {
    uint16_t le;
    uint16_t be;
};

struct uint32_both_t {
    uint32_t le;
    uint32_t be;
};

struct uint64_both_t {
    uint64_t le;
    uint64_t be;
};

struct int8_both_t {
    int8_t le;
    int8_t be;
};

struct int16_both_t {
    int16_t le;
    int16_t be;
};

struct int32_both_t {
    int32_t le;
    int32_t be;
};

struct int64_both_t {
    int64_t le;
    int64_t be;
};

// 17 bytes
struct iso9660_pvd_datetime_t {
    char year[4];
    char month[2];
    char day[2];
    char hour[2];
    char min[2];
    char sec[2];
    char hundreths[2];
    int8_t tzofs;
};

// 7 bytes
struct iso9660_datetime_t {
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
};

// 34 bytes
struct iso9660_dir_ent_t {
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
};

C_ASSERT(sizeof(iso9660_dir_ent_t) == 34);

// Primary Volume Descriptor
struct iso9660_pvd_t {
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
};

C_ASSERT(sizeof(iso9660_pvd_t) == 2048);

// Path table record. Always begins at a 16-bit boundary
struct iso9660_pt_rec_t {
    uint8_t di_len;
    uint8_t ea_len;
    uint16_t lba_lo;
    uint16_t lba_hi;
    uint16_t parent_dn;
    char name[2];
};

// Extended attribute record
struct iso9660_ea_rec_t {
    uint32_t owner_id;
    uint32_t group_id;
    uint16_t permissions;

    iso9660_pvd_datetime_t created_datetime;
    iso9660_pvd_datetime_t modified_datetime;
    iso9660_pvd_datetime_t expiry_datetime;
    iso9660_pvd_datetime_t effective_datetime;
    uint8_t record_format;
    uint8_t record_attr;
    uint32_t record_len;

    // ofs=84
    char system_id[32];
    char system_used[64];

    // ofs=180
    uint8_t ea_rec_ver;
    uint8_t esc_len;
    char reserved[64];

    // ofs=246
    uint16_t app_use_len_lo;
    uint16_t app_use_len_hi;

    // App use data and escape sequence data
    char data[1798];
};

#define ISO9660_EA_PERM_SYS_NO_OR_BIT   0
#define ISO9660_EA_PERM_SYS_NO_OX_BIT   2
#define ISO9660_EA_PERM_NO_OR_BIT       4
#define ISO9660_EA_PERM_NO_OX_BIT       6
#define ISO9660_EA_PERM_NO_GR_BIT       8
#define ISO9660_EA_PERM_NO_GX_BIT       10
#define ISO9660_EA_PERM_NO_WR_BIT       12
#define ISO9660_EA_PERM_NO_WX_BIT       14

#define ISO9660_EA_PERM_SYS_NO_OR (1<<ISO9660_EA_PERM_SYS_NO_OR_BIT)
#define ISO9660_EA_PERM_SYS_NO_OX (1<<ISO9660_EA_PERM_SYS_NO_OX_BIT)
#define ISO9660_EA_PERM_NO_OR     (1<<ISO9660_EA_PERM_NO_OR_BIT)
#define ISO9660_EA_PERM_NO_OX     (1<<ISO9660_EA_PERM_NO_OX_BIT)
#define ISO9660_EA_PERM_NO_GR     (1<<ISO9660_EA_PERM_NO_GR_BIT)
#define ISO9660_EA_PERM_NO_GX     (1<<ISO9660_EA_PERM_NO_GX_BIT)
#define ISO9660_EA_PERM_NO_WR     (1<<ISO9660_EA_PERM_NO_WR_BIT)
#define ISO9660_EA_PERM_NO_WX     (1<<ISO9660_EA_PERM_NO_WX_BIT)

#define ISO9660_EA_PERM_ONES      0xAA

C_ASSERT(sizeof(iso9660_ea_rec_t) == 2048);
C_ASSERT(offsetof(iso9660_ea_rec_t, system_id) == 84);
C_ASSERT(offsetof(iso9660_ea_rec_t, ea_rec_ver) == 180);
C_ASSERT(offsetof(iso9660_ea_rec_t, app_use_len_lo) == 246);

//
// RockRidge Extension

struct iso9660_rr_hdr_t {
    char sig[2];
    uint8_t len;
    uint8_t ver;
} __attribute__((packed));

C_ASSERT(sizeof(iso9660_rr_hdr_t) == 4);

// POSIX file attributes
struct iso9660_rr_px_t {
    iso9660_rr_hdr_t hdr;
    uint32_both_t st_mode;
    uint32_both_t st_nlink;
    uint32_both_t st_uid;
    uint32_both_t st_gid;
    uint32_both_t st_ino;
} __attribute__((packed));

C_ASSERT(sizeof(iso9660_rr_px_t) == 44);

// POSIX device number
struct iso9660_rr_pn_t {
    iso9660_rr_hdr_t hdr;
    uint32_both_t dev_hi;
    uint32_both_t dev_lo;
} __attribute__((packed));

C_ASSERT(sizeof(iso9660_rr_pn_t) == 20);

// Symbolic link
struct iso9660_rr_sl_t {
    iso9660_rr_hdr_t hdr;
    uint8_t flags;
    // followed by variable length component
} __attribute__((packed));

C_ASSERT(sizeof(iso9660_rr_sl_t) == 5);

// Alternate name
struct iso9660_rr_nm_t {
    iso9660_rr_hdr_t hdr;
    uint8_t flags;
    // followed by variable length name
} __attribute__((packed));

// Child link
struct iso9660_rr_cl_t {
    iso9660_rr_hdr_t hdr;
    uint32_both_t lba;
} __attribute__((packed));

C_ASSERT(sizeof(iso9660_rr_cl_t) == 12);

// Parent link
struct iso9660_rr_pl_t {
    iso9660_rr_hdr_t hdr;
    uint32_both_t lba;
} __attribute__((packed));

C_ASSERT(sizeof(iso9660_rr_pl_t) == 12);

// Relocated directory
struct iso9660_rr_re_t {
    iso9660_rr_hdr_t hdr;
} __attribute__((packed));

C_ASSERT(sizeof(iso9660_rr_re_t) == 4);

// Timestamps
struct iso9660_rr_tf_t {
    iso9660_rr_hdr_t hdr;
    uint8_t flags;
    // followed by timestamps
} __attribute__((packed));

//C_ASSERT(sizeof(iso9660_tf_re_t) == 12);

// Sparse file
struct iso9660_rr_sf_t {
    iso9660_rr_hdr_t hdr;
    uint32_both_t virtual_size_hi;
    uint32_both_t virtual_size_lo;
    uint8_t table_depth;
} __attribute__((packed));

C_ASSERT(sizeof(iso9660_rr_sf_t) == 21);

struct iso9660_sector_iterator_t {
    uint32_t lba;
    uint32_t size;
    uint16_t err;
};

// Maximum length of a name, not including null, if any
#define ISO9660_MAX_NAME    110
