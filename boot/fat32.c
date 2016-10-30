#include "code16gcc.h"

#include "fat32.h"
#include "malloc.h"
#include "bootsect.h"
#include "screen.h"
#include "string.h"
#include "cpu.h"
#include "paging.h"

bpb_data_t bpb;

char *sector_buffer;

// Initialize bpb data from sector buffer
// Expects first sector of partition
// Returns
static uint16_t read_bpb(uint32_t partition_lba)
{
    // Use extra large sector buffer to accomodate
    // arbitrary sector size, until sector size is known
    sector_buffer = malloc(512);

    uint16_t err = read_lba_sector(
                sector_buffer,
                boot_drive, partition_lba);
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

// Returns 0 on failure due to invalid UTF-8 or truncation due to insufficient buffer
// Returns output count (not including null terminator) on success
static uint16_t utf8_to_utf16(uint16_t *output,
                       uint16_t out_size_words,
                       char const *in)
{
    uint16_t *out = output;
    uint16_t *out_end = out + out_size_words;
    uint16_t len;
    uint32_t ch;

    while (*in) {
        if ((*in & 0xF8) == 0xF0) {
            ch = *in++ & 0x03;
            len = 4;
        } else if ((*in & 0xF0) == 0xE0) {
            ch = *in++ & 0x07;
            len = 3;
        } else if ((*in & 0xE0) == 0xC0) {
            ch = *in++ & 0x0F;
            len = 2;
        } else if ((*in & 0x80) != 0) {
            // Invalid, too long or character begins with 10xxxxxx
            return 0;
        } else if (out < out_end) {
            *out++ = (uint16_t)(uint8_t)*in++;
            continue;
        } else {
            // Output buffer overrun
            return 0;
        }

         while (--len) {
            if ((*in & 0xC0) == 0x80) {
                ch <<= 6;
                ch |= *in++ & 0x3F;
            } else {
                // Invalid, byte isn't 10xxxxxx
                return 0;
            }
        }

        if (ch >= 0xD800 && ch < 0xE000) {
            // Invalid UTF-8 in surrogate range
            return 0;
        }

        if (ch == 0) {
            // Overlong null character not allowed
            return 0;
        }
        if (out >= out_end) {
            // Output buffer would overrun
            return 0;
        }
        if (ch < 0x10000) {
            // Single UTF-16 character
            *out++ = (uint16_t)ch;
        } else if (out + 1 >= out_end) {
            // Surrogate pair would overrun output buffer
            return 0;
        } else {
            ch -= 0x10000;
            *out++ = 0xD800 + ((ch >> 10) & 0x3FF);
            *out++ = 0xDC00 + (ch & 0x3FF);
        }
    }

    if (out >= out_end) {
        // Output buffer would overrun
        return 0;
    }

    *out++ = 0;

    return out - output - 1;
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
static int16_t sector_iterator_begin(
        sector_iterator_t *iter,
        char *sector,
        uint32_t cluster)
{
    iter->err = 0;
    iter->start_cluster = cluster;
    iter->cluster = cluster;
    iter->sector_offset = 0;

    if (!is_eof_cluster(cluster)) {
        int32_t lba = lba_from_cluster(cluster);

        iter->err = read_lba_sector(
                    sector, boot_drive, lba);

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
    uint32_t fat_sector_offset = current_cluster & ~((1 << (9-2))-1);
    uint32_t const *fat_array = (uint32_t *)sector;
    uint32_t lba = bpb.first_fat_lba + fat_sector_index;

    uint16_t err = read_lba_sector(sector, boot_drive, lba);
    if (err_ptr)
        *err_ptr = err;
    if (err)
        return 0xFFFFFFFF;

    current_cluster = fat_array[fat_sector_offset] & 0x0FFFFFFF;

    // Check for end of chain
    if (is_eof_cluster(current_cluster))
        return 0;

    lba = lba_from_cluster(current_cluster);

    err = read_lba_sector(sector, boot_drive, lba);
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
        sector_iterator_t *iter,
        char *sector)
{
    if (is_eof_cluster(iter->cluster))
        return 0;

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
    } else {
        uint32_t lba = lba_from_cluster(iter->cluster) +
                iter->sector_offset;

        iter->err = read_lba_sector(
                    sector, boot_drive, lba);

        if (iter->err)
            return -1;
    }

    return 1;
}

// Read the first sector of a directory
// and prepare to iterate it
// Returns -1 on error
// Returns 0 on end of directory
// Returns 1 on success
static int16_t read_directory_begin(
        directory_iterator_t *iter,
        char *sector,
        uint32_t cluster)
{
    uint16_t status = sector_iterator_begin(
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
        directory_iterator_t const *iter,
        char const* sector)
{
    if (iter->dir_file.err == 0)
        return (dir_union_t*)sector + iter->sector_index;
    return 0;
}

static int16_t read_directory_move_next(
        directory_iterator_t *iter,
        char *sector)
{
    // Advance to next sector_index
    if (++iter->sector_index >= bpb.bytes_per_sec / sizeof(dir_entry_t))
        return sector_iterator_next(&iter->dir_file, sector);

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

        match = malloc(lfn_entries * sizeof(*match));

        memset(match, 0, lfn_entries * sizeof(*match));

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

    directory_iterator_t dir;
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

void boot_partition(uint32_t partition_lba)
{
    //test_malloc();
    paging_init();

    uint64_t addr;

    // Map 4KB at linear address 0xd00d0000 to physaddr 0x100000
    addr = 0xd00d0000;
    paging_map_range(addr, 0x1000, 0x100000, 0x1);
    copy_to_address(addr, "Hello!", 6);

    // Map 64KB at linear address 0x12345000 to physaddr 0x101000
    addr = 0x12345000;
    paging_map_range(addr, 0x10000, 0x101000, 0x1);
    copy_to_address(addr, 0, 0x10000);

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

    uint32_t root = find_file_by_name("long-kernel-name", bpb.root_dir_start);
    print_line("kernel start=%d", root);
}

static void test_utf16()
{
    uint16_t utf16[8], *up;
    char const *utf8 = u8"$¢€𐍈";
    utf8_to_utf16(utf16, sizeof(utf16), utf8);

    for (up = utf16; *up; ++up)
        print_line("%x", *up);
}

