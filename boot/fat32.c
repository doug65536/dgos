#include "fat32.h"
#include "malloc.h"
#include "bootsect.h"
#include "screen.h"
#include "string.h"
#include "cpu.h"
#include "paging.h"
#include "utf.h"
#include "elf64.h"
#include "fs.h"

// Bytes Per Sector	BPB_BytsPerSec	0x0B	16 Bits	Always 512 Bytes
// Sectors Per Cluster	BPB_SecPerClus	0x0D	8 Bits	1,2,4,8,16,32,64,128
// Number of Reserved Sectors	BPB_RsvdSecCnt	0x0E	16 Bits	Usually 0x20
// Number of FATs	BPB_NumFATs	0x10	8 Bits	Always 2
// Sectors Per FAT	BPB_FATSz32	0x24	32 Bits	Depends on disk size
// Root Directory First Cluster	BPB_RootClus	0x2C	32 Bits	Usually 0x00000002
// Signature	(none)	0x1FE	16 Bits	Always 0xAA55
typedef struct bpb_data_t {
    uint32_t root_dir_start;	// 0x2C LBA
    uint32_t sec_per_fat;		// 0x24 1 per 128 clusters
    uint16_t reserved_sectors;	// 0x0E Usually 32
    uint16_t bytes_per_sec;		// 0x0B Always 512
    uint16_t signature;			// 0x1FE Always 0xAA55
    uint8_t sec_per_cluster;	// 0x0D 8=4KB cluster
    uint8_t number_of_fats;		// 0x10 Always 2

    // Inferred from data in on-disk BPB
    uint32_t first_fat_lba;
    uint32_t cluster_begin_lba;
} bpb_data_t;

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
typedef struct dir_entry_t {
    // offset = 0x00
    char name[11];

    // offset = 0x0B
    uint8_t attr;

    // offset = 0x0C (bit 4 = lowercase extension, bit 3 = lowercase filename)
    uint8_t lowercase_flags;

    // offset = 0x0D
    uint8_t creation_tenth;

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
} dir_entry_t;

// DOS date bitfields
//  4:0  Day (1-31)
//  8:5  Month (1-12)
// 15:9  Year relative to 1980

// DOS time bitfields
//  4:0  Seconds divided by 2 (0-29)
// 10:5  Minutes (0-59)
// 15:11 Hours (0-23)

#define FAT_LAST_LFN_ORDINAL 0x40
#define FAT_DELETED_FLAG 0xE5

#define FAT_ATTR_NONE 0
#define FAT_ATTR_RO 1
#define FAT_ATTR_HIDDEN 2
#define FAT_ATTR_SYS 4
#define FAT_ATTR_VOLUME 8
#define FAT_ATTR_DIR 16
#define FAT_ATTR_ARCH 32

#define FAT_ATTR_MASK 0x3F
#define FAT_LONGNAME (FAT_ATTR_RO | FAT_ATTR_HIDDEN | FAT_ATTR_SYS | FAT_ATTR_VOLUME)

// Long filenames are stored in reverse order
// The last fragment of the long filename is stored first,
// with bit 6 set in its ordinal.
// The beginning of the filename has an ordinal of 1
// The 8.3 entry is immediately after the long filename entries
// Each long filename entry stores 13 UTF-16 characters

// https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#VFAT_long_file_names
// encodes 13 UTF-16 codepoints per entry
typedef struct long_dir_entry_t {
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
} long_dir_entry_t;

typedef union dir_union_t {
    dir_entry_t short_entry;
    long_dir_entry_t long_entry;
} dir_union_t;

typedef struct long_name_fragment_t {
    uint8_t ordinal;
    uint16_t fragment[13];
} long_name_fragment_t;

typedef struct filename_info_t {
    uint8_t lowercase_flags;
    uint8_t filename_length;
    uint8_t extension_length;
} filename_info_t;

// ===========================================================================

static bpb_data_t bpb;
static fat32_sector_iterator_t *file_handles;
#define MAX_HANDLES 5

static char *sector_buffer;

// Initialize bpb data from sector buffer
// Expects first sector of partition
// Returns
static uint16_t read_bpb(uint32_t partition_lba)
{
    // Use extra large sector buffer to accomodate
    // arbitrary sector size, until sector size is known
    sector_buffer = malloc(512);

    uint16_t err = read_lba_sectors(
                sector_buffer,
                boot_drive, partition_lba, 1);
    if (err)
        return err;

    bpb.root_dir_start = *(uint32_t*)(sector_buffer + 0x2C);
    bpb.sec_per_fat = *(uint32_t*)(sector_buffer + 0x24);
    bpb.reserved_sectors = *(uint16_t*)(sector_buffer + 0x0E);
    bpb.bytes_per_sec = *(uint16_t*)(sector_buffer + 0x0B);
    bpb.signature = *(uint16_t*)(sector_buffer + 0x1FE);
    bpb.sec_per_cluster = *(uint8_t*)(sector_buffer + 0x0D);
    bpb.number_of_fats = *(uint8_t*)(sector_buffer + 0x10);

    bpb.first_fat_lba = partition_lba + bpb.reserved_sectors;
    bpb.cluster_begin_lba = bpb.first_fat_lba +
            bpb.number_of_fats * bpb.sec_per_fat -
            2 * bpb.sec_per_cluster;

    return 0;
}

static uint32_t lba_from_cluster(uint32_t cluster)
{
    return bpb.cluster_begin_lba +
            cluster * bpb.sec_per_cluster;
}

static uint16_t is_eof_cluster(uint32_t cluster)
{
    return cluster < 2 || cluster >= 0x0FFFFFF8;
}

// Read the first sector of a cluster chain
// and prepare to iterate sectors
// Returns -1 on error
// Returns 0 on EOF
// Returns 1 on success
static int16_t fat32_sector_iterator_begin(
        fat32_sector_iterator_t *iter,
        char *sector,
        uint32_t cluster)
{
    iter->err = 0;
    iter->start_cluster = cluster;
    iter->cluster = cluster;
    iter->position = 0;
    iter->sector_offset = 0;

    if (!is_eof_cluster(cluster)) {
        int32_t lba = lba_from_cluster(cluster);

        iter->err = read_lba_sectors(
                    sector, boot_drive, lba, 1);

        if (!iter->err)
            return 1;

        return -1;
    }

    return 0;
}

// Reads the FAT, finds next cluster, and reads cluster
// Returns new cluster number, returns 0 at end of file
// Returns 0xFFFFFFFF on error
static uint32_t next_cluster(
        uint32_t current_cluster, char *sector, uint16_t *err_ptr)
{
    uint32_t fat_sector_index = current_cluster >> (9-2);
    uint32_t fat_sector_offset = current_cluster & ((1 << (9-2))-1);
    uint32_t const *fat_array = (uint32_t *)sector;
    uint32_t lba = bpb.first_fat_lba + fat_sector_index;

    uint16_t err = read_lba_sectors(sector, boot_drive, lba, 1);
    if (err_ptr)
        *err_ptr = err;
    if (err)
        return 0xFFFFFFFF;

    current_cluster = fat_array[fat_sector_offset] & 0x0FFFFFFF;

    // Check for end of chain
    if (is_eof_cluster(current_cluster))
        return 0;

    lba = lba_from_cluster(current_cluster);

    err = read_lba_sectors(sector, boot_drive, lba, 1);
    if (err_ptr)
        *err_ptr = err;
    if (err)
        return 0xFFFFFFFF;

    return current_cluster;
}

// Returns -1 on error
// Returns 0 on EOF
// Returns 1 if successfully advanced to next sector
static int16_t sector_iterator_next(
        fat32_sector_iterator_t *iter,
        char *sector,
        uint16_t read_data)
{
    if (is_eof_cluster(iter->cluster))
        return 0;

    ++iter->position;

    // Advance to the next sector
    if (++iter->sector_offset == bpb.sec_per_cluster) {
        // Reached end of cluster
        iter->sector_offset = 0;

        // Advance to the next cluster
        iter->cluster = next_cluster(
                    iter->cluster, sector, &iter->err);

        if (iter->cluster == 0)
            return 0;

        if (iter->cluster == 0xFFFFFFFF)
            return -1;
    } else if (read_data) {
        uint32_t lba = lba_from_cluster(iter->cluster) +
                iter->sector_offset;

        iter->err = read_lba_sectors(
                    sector, boot_drive, lba, 1);

        if (iter->err)
            return -1;
    }

    return 1;
}

static int16_t sector_iterator_seek(
        fat32_sector_iterator_t *iter,
        uint32_t sector_offset,
        char *sector)
{

    // See if we are already there
    if (iter->position == sector_offset) {
        uint16_t is_eof_now = is_eof_cluster(iter->cluster);
        return is_eof_now ? 0 : 1;
    }

    int16_t status = fat32_sector_iterator_begin(
                iter, sector, iter->start_cluster);

    while (status > 0 && sector_offset--)
        status = sector_iterator_next(iter, sector,
                                      sector_offset == 0);

    return status;
}

// Read the first sector of a directory
// and prepare to iterate it
// Returns -1 on error
// Returns 0 on end of directory
// Returns 1 on success
static int16_t read_directory_begin(
        dir_iterator_t *iter,
        char *sector,
        uint32_t cluster)
{
    int16_t status = fat32_sector_iterator_begin(
                &iter->dir_file, sector, cluster);
    iter->sector_index = 0;

    return status;
}

// fcb_name is the space padded 11 character name with no dot,
// the representation used in dir_entry_t's name field
static uint8_t lfn_checksum(char const *fcb_name)
{
   uint16_t i;
   uint8_t sum = 0;

   for (i = 11; i; i--)
      sum = ((sum & 1) << 7) +
              (sum >> 1) +
              (uint8_t)*fcb_name++;

   return sum;
}

// Returns
static dir_union_t const *read_directory_current(
        dir_iterator_t const *iter,
        char const* sector)
{
    if (iter->dir_file.err == 0)
        return (dir_union_t const*)sector + iter->sector_index;
    return 0;
}

static int16_t read_directory_move_next(
        dir_iterator_t *iter,
        char *sector)
{
    // Advance to next sector_index
    if (++iter->sector_index >=
            bpb.bytes_per_sec / sizeof(dir_entry_t))
        return sector_iterator_next(&iter->dir_file, sector, 1);

    return 1;
}

// lowercase_flags:
//  bit 4 = lowercase extension,
//  bit 3 = lowercase filename
//  0 if long filename needed
//  0xFF if invalid
static filename_info_t get_filename_info(char const *filename)
{
    size_t ni;
    size_t ei;
    size_t f_upper = 0;
    size_t f_lower = 0;
    size_t e_upper = 0;
    size_t e_lower = 0;
    size_t is_long = 0;
    char c;
    filename_info_t info;

    info.filename_length = 0;
    info.extension_length = 0;

    static char const ignored[] = "0123456789!#$%&'()-@^_`{}~";
    static char const long_only[] = " +,.;=[]";
    static char const invalid[] = "\"*/:<>?\\|\x7f";

    // Process name part
    for (ni = 0; filename[ni]; ++ni) {
        c = filename[ni];

        if (c < ' ' || strchr(invalid, c)) {
            info.lowercase_flags = 0xFF;
            return info;
        }

        if (c == '.') {
            // Start processing extension
            ++ni;
            break;
        }

        ++info.filename_length;

        if (strchr(ignored, c))
            continue;

        if (ni == 8) {
            is_long = 1;
        }

        if (strchr(long_only, c)) {
            is_long = 1;
        } else if (c >= 'A' && c <= 'Z') {
            ++f_upper;
        } else if (c >= 'a' && c <= 'z') {
            ++f_lower;
        }
    }

    for (ei = 0; filename[ni]; ++ei, ++ni) {
        c = filename[ni];

        if (c < ' ' || strchr(invalid, c)) {
            info.lowercase_flags = 0xFF;
            return info;
        }

        if (ei == 3) {
            is_long = 1;
        }

        ++info.extension_length;

        if (strchr(ignored, c))
            continue;

        if (c == '.') {
            is_long = 1;
        } else if (c >= 'A' && c <= 'Z') {
            ++e_upper;
        } else if (c >= 'a' && c <= 'z') {
            ++e_lower;
        }
    }

    if (info.filename_length > 8 || info.extension_length > 3)
        is_long = 1;

    info.lowercase_flags = 0;

    if (!is_long) {
        if ((f_lower && f_upper) || (e_lower && e_upper)) {
            // Mixed case filename, or
            // mixed case extension, it is long
            info.lowercase_flags = 0;
        } else {
            if (!f_upper) {
                // Only lowercase filename
                info.lowercase_flags |= (1 << 3);
            }

            if (!e_upper) {
                // Lowercase extension
                info.lowercase_flags |= (1 << 4);
            }
        }
    } else {
        info.lowercase_flags = 0;
    }

    return info;
}

// Returns an appropriate replacement for a short name
// Returns 0 if the character is not allowed in a short name
static char shortname_char(char ch)
{
    // Allowed
    if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
        return ch;

    // Uppercase only
    if (ch >= 'a' && ch <= 'z')
        return ch + 'A' - 'a';

    // Everything else not allowed
    return '_';
}

static void fill_short_filename(dir_union_t *match, char const *filename)
{
    char *fill_name = match->short_entry.name;
    char const *src = filename;
    char ch;

    // Take up to 8 characters, not including first '.'
    while (*src && *src != '.' &&
           fill_name < match->short_entry.name + 8) {
        ch = shortname_char(*src++);
        if (ch)
            *fill_name++ = ch;
    }

    // Pad with ' ' until name is 8 characters
    while (fill_name < match->short_entry.name + 8)
        *fill_name++ = ' ';

    // Discard input until '.'
    while (*src && *src != '.')
        ++src;

    // Discard '.'
    if (*src == '.')
        ++src;

    // Copy up to 3 characters of extension
    while (*src && fill_name < match->short_entry.name + 11) {
        ch = shortname_char(*src++);
        if (ch)
            *fill_name++ = ch;
    }

    // Pad extension with spaces until it is 3 characters long
    while (fill_name < match->short_entry.name + 11)
        *fill_name++ = ' ';
}

// Fill in one of the name fragments in a long_dir_entry_t
// Returns new value of encoded_src caller should use
// Uses/updates done_name flag which the caller needs to
// carry across fragment calls
static uint16_t const *encode_lfn_name_fragment(
        uint8_t *lfn_fragment,
        uint16_t fragment_size,
        uint16_t const *encoded_src,
        uint16_t *done_name)
{
    for (uint16_t i = 0; i < fragment_size; ++i) {
        if (*done_name) {
            // Everything after null terminator is 0xFFFF
            lfn_fragment[i*2] = 0xFF;
            lfn_fragment[i*2+1] = 0xFF;
        } else if (*encoded_src != 0) {
            // Encode (sometimes) misaligned UTF-16 codepoints
            lfn_fragment[i*2] =
                    *encoded_src & 0xFF;
            lfn_fragment[i*2+1] =
                    (*encoded_src >> 8) & 0xFF;
            ++encoded_src;
        } else {
            // Encode null terminator
            *done_name = 1;
            lfn_fragment[i*2] = 0x00;
            lfn_fragment[i*2+1] = 0x00;
        }
    }
    return encoded_src;
}

static uint16_t dir_entry_match(dir_union_t const *entry,
                          dir_union_t const *match)
{
    uint16_t long_entry = (entry->long_entry.attr == FAT_LONGNAME);
    uint16_t long_match = (match->long_entry.attr == FAT_LONGNAME);

    if (long_entry != long_match)
        return 0;

    if (long_entry) {
        // Compare long entry

        if (entry->long_entry.ordinal != match->long_entry.ordinal)
            return 0;

        if (memcmp(entry->long_entry.name,
                   match->long_entry.name,
                   sizeof(entry->long_entry.name)))
            return 0;

        if (memcmp(entry->long_entry.name2,
                   match->long_entry.name2,
                   sizeof(entry->long_entry.name2)))
            return 0;

        if (memcmp(entry->long_entry.name3,
                   match->long_entry.name3,
                   sizeof(entry->long_entry.name3)))
            return 0;

        return 1;
    } else {
        // Compare short entry
        if (memcmp(entry->short_entry.name, match->short_entry.name, sizeof(entry->short_entry.name)))
            return 0;

        return 1;
    }
}

// Returns 0 on failure
static uint32_t find_file_by_name(char const *filename,
                                  uint32_t dir_cluster)
{
    dir_union_t *match;

    filename_info_t info = get_filename_info(filename);

    if (info.lowercase_flags == 0xFF)
        return 0;

    uint16_t lfn_entries = 0;

    if (info.lowercase_flags == 0) {
        // Needs long filename
        uint16_t encoded_name[255];
        uint16_t encoded_len = utf8_to_utf16(
                    encoded_name, 255, filename);
        uint16_t const *encoded_src;

        // Check for bad UTF-8
        if (encoded_len == 0)
            return 0;

        lfn_entries = (encoded_len + 12) / 13;

        match = calloc(lfn_entries, sizeof(*match));

        // Fill in reverse order
        dir_union_t *match_fill = match + (lfn_entries - 1);

        uint16_t done_name = 0;
        encoded_src = encoded_name;
        do {
            encoded_src = encode_lfn_name_fragment(
                        match_fill->long_entry.name,
                        sizeof(match_fill->long_entry.name) >> 1,
                        encoded_src,
                        &done_name);

            encoded_src = encode_lfn_name_fragment(
                        match_fill->long_entry.name2,
                        sizeof(match_fill->long_entry.name2) >> 1,
                        encoded_src,
                        &done_name);

            encoded_src = encode_lfn_name_fragment(
                        match_fill->long_entry.name3,
                        sizeof(match_fill->long_entry.name3) >> 1,
                        encoded_src,
                        &done_name);

            match_fill->long_entry.attr = FAT_LONGNAME;
        } while (match_fill-- != match);

        for (uint16_t i = 0; i < lfn_entries; ++i) {
            match[i].long_entry.ordinal =
                ((i == 0) << 6) + (lfn_entries - i);
        }
    } else {
        // Short filename optimization
        match = malloc(sizeof(*match));

        fill_short_filename(match, filename);

        lfn_entries = 0;
    }

    dir_iterator_t dir;
    uint16_t match_index = 0;
    uint8_t checksum = 0;
    for (int16_t status = read_directory_begin(
             &dir, sector_buffer, dir_cluster);
         status > 0;
         status = read_directory_move_next(
             &dir, sector_buffer)) {
        dir_union_t const *entry =
                read_directory_current(&dir, sector_buffer);

        if (entry->short_entry.attr == FAT_LONGNAME)
            checksum = entry->long_entry.checksum;

        // If there are no lfn entries, or
        // there are lfn entries and this is not the short entry
        if (lfn_entries == 0 || match_index < lfn_entries) {
            if (dir_entry_match(match + match_index, entry)) {
                // Entries match
                ++match_index;
            } else {
                match_index = 0;
            }
        } else {
            if (lfn_checksum(entry->short_entry.name) == checksum) {
                // Found
                return ((uint32_t)entry->short_entry.start_hi << 16) |
                        entry->short_entry.start_lo;
            } else {
                match_index = 0;
            }
        }
    }

    return 0;
}

static int fat32_find_available_file_handle(void)
{
    for (size_t i = 0; i < MAX_HANDLES; ++i) {
        if (file_handles[i].start_cluster == 0)
            return i;
    }
    return -1;
}

static int fat32_boot_open(char const *filename)
{
    uint32_t cluster;

    // Find the start of the file
    cluster = find_file_by_name(filename, bpb.root_dir_start);
    if (cluster == 0)
        return -1;

    // Allocate a file handle
    int file = fat32_find_available_file_handle();
    if (file < 0)
        return -1;

    // Get ready to read the file
    int16_t status = fat32_sector_iterator_begin(
                file_handles + file, sector_buffer, cluster);
    if (status < 0)
        return -1;

    // Return file handle
    return file;
}

static int fat32_boot_close(int file)
{
    if (file < 0 || file >= MAX_HANDLES)
        return -1;

    int result = 0;

    // Fail the close if the file is in error state
    if (file_handles[file].err)
        result = -1;

    // Mark as available
    memset(file_handles + file, 0, sizeof(*file_handles));

    return result;
}

static int fat32_boot_pread(int file, void *buf, size_t bytes, off_t ofs)
{
    if (file < 0 || file >= MAX_HANDLES)
        return -1;

    uint32_t sector_offset = ofs >> 9;
    uint16_t byte_offset = ofs & ((1 << 9)-1);

    int16_t status = sector_iterator_seek(
                file_handles + file,
                sector_offset,
                sector_buffer);

    char *output = buf;

    int total = 0;
    for (;;) {
        // Error?
        if (status < 0)
            return -1;

        // EOF?
        if (status == 0)
            return 0;

        uint16_t limit = 512 - byte_offset;
        if (limit > bytes)
            limit = bytes;

        if (limit == 0)
            break;

        // Copy data from sector buffer
        memcpy(output, sector_buffer + byte_offset, limit);

        bytes -= limit;
        output += limit;
        total += limit;

        byte_offset = 0;

        if (bytes > 0) {
            status = sector_iterator_next(
                        file_handles + file, sector_buffer, 1);
        }
    }

    return total;
}

void fat32_boot_partition(uint32_t partition_lba)
{
    file_handles = calloc(MAX_HANDLES, sizeof(*file_handles));

    paging_init();

    print_line("Booting partition at LBA %u", partition_lba);

    uint16_t err = read_bpb(partition_lba);
    if (err) {
        print_line("Error reading BPB!");
        return;
    }

    print_line("root_dir_start:	  %d", bpb.root_dir_start);     // 0x2C LBA
    print_line("sec_per_fat:      %d", bpb.sec_per_fat);        // 0x24 1 per 128 clusters
    print_line("reserved_sectors: %d", bpb.reserved_sectors);	// 0x0E Usually 32
    print_line("bytes_per_sec:	  %d", bpb.bytes_per_sec);		// 0x0B Always 512
    print_line("signature:		  %x", bpb.signature);			// 0x1FE Always 0xAA55
    print_line("sec_per_cluster:  %d", bpb.sec_per_cluster);    // 0x0D 8=4KB cluster
    print_line("number_of_fats:	  %d", bpb.number_of_fats);		// 0x10 Always 2

    //uint32_t root = find_file_by_name("long-kernel-name", bpb.root_dir_start);
    //print_line("kernel start=%d", root);

    fs_api.boot_open = fat32_boot_open;
    fs_api.boot_close = fat32_boot_close;
    fs_api.boot_pread = fat32_boot_pread;

    elf64_run("dgos_kernel");
}
