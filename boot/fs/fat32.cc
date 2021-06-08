#include "fat32.h"
#include "malloc.h"
#include "diskio.h"
#include "paging.h"
#include "screen.h"
#include "string.h"
#include "elf64.h"
#include "fs.h"
#include "utf.h"
#include "assert.h"
#include "log2.h"
#include "halt.h"
#include "physmem.h"

#include "../kernel/fs/fat32/fat32_decl.h"

#define FAT32_DEBUG_DIRENT 0

// ===========================================================================

static fat32_bpb_data_t bpb;
static fat32_sector_iterator_t *file_handles;
#define MAX_HANDLES 5

uint32_t fat32_serial;

static char *sector_buffer;
static char *fat_buffer;
uint32_t fat_buffer_lba;
static uint_least32_t sector_sz;
static uint8_t log2_sector_sz;
static uint8_t log2_sec_per_cluster;

_hot
static uint32_t next_cluster(uint32_t current_cluster, bool *ok_ptr);

// Initialize bpb data from sector buffer
// Expects first sector of partition
_use_result
static bool read_bpb(uint64_t partition_lba)
{
    sector_sz = disk_sector_size();
    log2_sector_sz = bit_log2_n(sector_sz);

    sector_buffer = (char*)malloc(sector_sz);

    if (unlikely(!sector_buffer))
        return false;

    fat_buffer = (char*)malloc(sector_sz);

    if (unlikely(!fat_buffer))
        return false;

    if (unlikely(!disk_read_lba(uint64_t(sector_buffer),
                                partition_lba, log2_sector_sz, 1)))
        return false;

    fat32_parse_bpb(&bpb, partition_lba, sector_buffer);

    log2_sec_per_cluster = bit_log2_n(bpb.sec_per_cluster);

    return true;
}

static uint32_t lba_from_cluster(uint32_t cluster)
{
    return bpb.cluster_begin_lba +
            (cluster << log2_sec_per_cluster);
}

static bool is_eof_cluster(uint32_t cluster)
{
    return cluster < 2 || cluster >= 0x0FFFFFF8;
}

// Read the first sector of a cluster chain
// and prepare to iterate sectors
// Returns -1 on error
// Returns 0 on EOF
// Returns 1 on success
static int fat32_sector_iterator_begin(
        fat32_sector_iterator_t *iter,
        char *sector,
        uint32_t cluster)
{
    iter->ok = true;
    iter->start_cluster = cluster;
    iter->cluster = cluster;
    iter->position = 0;
    iter->sector_offset = 0;

    PRINT("Counting the clusters in the chain");

    // Cache cluster chain
    int cluster_count = 0;
    for (uint32_t walk = cluster; !is_eof_cluster(walk); ++cluster_count) {
        walk = next_cluster(walk, &iter->ok);
        if (unlikely(!iter->ok))
            return -1;
    }

    PRINT("Allocating %d entry 32-bit cluster list", cluster_count);

    iter->cluster_count = cluster_count;

    //if (cluster_count <= 1024)
        iter->clusters = new (ext::nothrow) uint32_t[cluster_count];
    //else
    //    iter->clusters = (uint32_t*)alloc_phys(
    //                sizeof(uint32_t) * cluster_count, true).base;

    if (unlikely(!iter->clusters))
        PANIC_OOM();

    PRINT("Populating %d entry 32-bit cluster list", cluster_count);

    cluster_count = 0;
    for (int walk = cluster; !is_eof_cluster(walk); ++cluster_count) {
        iter->clusters[cluster_count] = walk;
        walk = next_cluster(walk, &iter->ok);
        if (unlikely(!iter->ok))
            return -1;
    }

    if (likely(!is_eof_cluster(cluster))) {
        int32_t lba = lba_from_cluster(cluster);

        iter->ok = disk_read_lba(uint64_t(sector), lba, log2_sector_sz, 1);

        if (likely(iter->ok))
            return 1;

        return -1;
    }

    return 0;
}

static bool read_fat(uint64_t lba)
{
    bool ok = disk_read_lba(uint64_t(fat_buffer), lba, log2_sector_sz, 1);
    // Update buffered lba, and on failure, mark it with impossible lba
    fat_buffer_lba = ok ? lba : -1;
    return ok;
}

static size_t fat_cache_hits;
static size_t fat_cache_misses;

// Reads the FAT, finds next cluster, and reads cluster
// Returns new cluster number, returns 0 at end of file
// Returns 0xFFFFFFFF on error
_hot
static uint32_t next_cluster(
        uint32_t current_cluster, bool *ok_ptr)
{
    // minus 2 because 32 bit (2^2 byte) FAT entries
    uint32_t fat_sector_index = current_cluster >> (log2_sector_sz-2);
    uint32_t fat_sector_offset = current_cluster &
            ((1U << (log2_sector_sz-2))-1);
    uint32_t const *fat_array = nullptr;
    uint64_t lba = bpb.first_fat_lba + fat_sector_index;

    bool ok = true;
    if (unlikely(fat_buffer_lba != lba)) {
        ++fat_cache_misses;

        ok = read_fat(lba);

        if (ok_ptr)
            *ok_ptr = ok;

        if (unlikely(!ok))
            return 0xFFFFFFFF;
    } else {
        ++fat_cache_hits;
    }

    fat_array = (uint32_t*)fat_buffer;

    current_cluster = fat_array[fat_sector_offset] & 0x0FFFFFFF;

    // Check for end of chain
    if (unlikely(is_eof_cluster(current_cluster)))
        return 0;

    return current_cluster;
}

// Returns -1 on error
// Returns 0 on EOF
// Returns 1 if successfully advanced to next sector
static int sector_iterator_next(
        fat32_sector_iterator_t *iter,
        char *sector,
        bool read_data)
{
    if (is_eof_cluster(iter->cluster))
        return 0;

    ++iter->position;

    // Advance to the next sector
    if (++iter->sector_offset == bpb.sec_per_cluster) {
        // Reached end of cluster
        iter->sector_offset = 0;

        // Advance to the next cluster
        //iter->cluster = next_cluster(
        //            iter->cluster, &iter->ok);

        if (iter->position >= iter->cluster_count)
            return 0;

        iter->cluster = iter->clusters[iter->position];

        if (iter->cluster == 0)
            return 0;

        if (iter->cluster == 0xFFFFFFFF)
            return -1;
    }

    if (read_data) {
        uint32_t lba = lba_from_cluster(iter->cluster) +
                iter->sector_offset;

        iter->ok = disk_read_lba(uint64_t(sector), lba, log2_sector_sz, 1);

        if (unlikely(!iter->ok))
            return -1;
    }

    return 1;
}

// Read the first sector of a directory
// and prepare to iterate it
// Returns -1 on error
// Returns 0 on end of directory
// Returns 1 on success
static int read_directory_begin(
        dir_iterator_t *iter,
        char *sector,
        uint32_t cluster)
{
    int status = fat32_sector_iterator_begin(&iter->dir_file, sector, cluster);
    iter->sector_index = 0;

    return status;
}

// fcb_name is the space padded 11 character name with no dot,
// the representation used in dir_entry_t's name field
static uint8_t lfn_checksum(char const *fcb_name)
{
   int i;
   uint8_t sum = 0;

   for (i = 11; i; i--)
      sum = ((sum & 1) << 7) + (sum >> 1) + uint8_t(*fcb_name++);

   return sum;
}

static fat32_dir_union_t const *read_directory_current(
        dir_iterator_t const *iter,
        char const* sector)
{
    if (iter->dir_file.ok)
        return (fat32_dir_union_t const*)sector + iter->sector_index;

    return nullptr;
}

static int16_t read_directory_move_next(dir_iterator_t *iter, char *sector)
{
    // Advance to next sector_index
    if (++iter->sector_index >=
            bpb.bytes_per_sec / sizeof(fat32_dir_entry_t)) {
        iter->sector_index = 0;
        return sector_iterator_next(&iter->dir_file, sector, 1);
    }

    return 1;
}

// lowercase_flags:
//  bit 4 = lowercase extension,
//  bit 3 = lowercase filename
//  0 if long filename needed
//  0xFF if invalid
static filename_info_t get_filename_info(tchar const *filename)
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
            // Only lowercase filename
            info.lowercase_flags |= (!f_upper) << 3;

            // Lowercase extension
            info.lowercase_flags |= (!e_upper) << 4;
        }
    } else {
        info.lowercase_flags = 0;
    }

    return info;
}

// Returns an appropriate replacement for a short name
// Returns 0 if the character is not allowed in a short name
static char shortname_char(tchar codeunit)
{
    // Allowed
    if ((codeunit >= 'A' && codeunit <= 'Z') ||
            (codeunit >= '0' && codeunit <= '9'))
        return codeunit;

    // Uppercase only
    if (codeunit >= 'a' && codeunit <= 'z')
        return codeunit + 'A' - 'a';

    // Everything else not allowed
    return '_';
}

static void fill_short_filename(fat32_dir_union_t *match,
        tchar const *filename)
{
    char *fill_name = match->short_entry.name;
    tchar const *src = filename;
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
static char16_t const *encode_lfn_name_fragment(
        uint8_t *lfn_fragment,
        size_t fragment_size,
        char16_t const *encoded_src,
        uint_fast16_t *done_name)
{
    for (size_t i = 0; i < fragment_size; ++i) {
        if (*done_name) {
            // Everything after null terminator is 0xFFFF
            lfn_fragment[i*2] = 0xFF;
            lfn_fragment[i*2+1] = 0xFF;
        } else if (*encoded_src != 0) {
            // Encode (sometimes) misaligned UTF-16 codepoints
            memcpy(lfn_fragment + i * 2, encoded_src, sizeof(char16_t));
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

static void dir_extract_lfn_name(char16_t * restrict out,
        fat32_dir_union_t const * restrict entry)
{
    char16_t *piece1 = out;
    char16_t *piece2 = out + sizeof(entry->long_entry.name) / sizeof(char16_t);
    char16_t *piece3 = out + sizeof(entry->long_entry.name) / sizeof(char16_t) +
        sizeof(entry->long_entry.name2) / sizeof(char16_t);

    memcpy(piece1, entry->long_entry.name,
        sizeof(entry->long_entry.name));

    memcpy(piece2, entry->long_entry.name2,
        sizeof(entry->long_entry.name2));

    memcpy(piece3, entry->long_entry.name3,
        sizeof(entry->long_entry.name3));
}

static bool dir_entry_match(fat32_dir_union_t const *entry,
                          fat32_dir_union_t const *match)
{
    uint_fast16_t long_entry = (entry->long_entry.attr == FAT_LONGNAME);
    uint_fast16_t long_match = (match->long_entry.attr == FAT_LONGNAME);

    if (long_entry != long_match)
        return false;

    if (long_entry) {
        // Compare long entry

        if (entry->long_entry.ordinal != match->long_entry.ordinal)
            return false;

        char16_t ent_name[13];
        char16_t mat_name[13];

        dir_extract_lfn_name(ent_name, entry);
        dir_extract_lfn_name(mat_name, match);

        if (strncmp(ent_name, mat_name, countof(ent_name)))
            return false;

        return true;
    } else {
        // Compare short entry
        if (memcmp(entry->short_entry.name,
                   match->short_entry.name,
                   sizeof(entry->short_entry.name)))
            return false;

        return true;
    }
}

#if FAT32_DEBUG_DIRENT
static void print_dirent(fat32_dir_union_t const *entry)
{
    char name[5 + 6 + 2 + 1];
    if (entry->short_entry.attr == FAT_LONGNAME) {
        for (int i = 0; i < 5; ++i)
            name[i] = (char)entry->long_entry.name[i*2];
        for (int i = 0; i < 6; ++i)
            name[i+5] = (char)entry->long_entry.name2[i*2];
        for (int i = 0; i < 2; ++i)
            name[i+5+6] = (char)entry->long_entry.name3[i*2];
        name[5+6+2] = 0;
        PRINT(" long 0x%x: %s\n", entry->long_entry.ordinal, name);
    } else {
        memcpy(name, entry->short_entry.name,
               sizeof(entry->short_entry.name));
        name[sizeof(entry->short_entry.name)] = 0;
        PRINT("  short: %s\n", name);
    }
}
#endif

// Returns 0 on failure
static uint32_t find_file_by_name(tchar const *filename, uint32_t dir_cluster,
                                  uint32_t *out_file_size)
{
    if (likely(out_file_size))
        *out_file_size = 0;

    fat32_dir_union_t *match;

    filename_info_t info = get_filename_info(filename);

    if (info.lowercase_flags == 0xFF)
        return 0;

    uint_fast16_t lfn_entries = 0;

    if (info.lowercase_flags == 0) {
        // Needs long filename
        char16_t encoded_name[255];
        size_pair_t encoded_len = tchar_to_utf16(
                    encoded_name, 255,
                    filename, info.filename_length);
        char16_t const *encoded_src;

        // Check for bad UTF-8
        if (unlikely(encoded_len.output_produced == 0))
            return 0;

        lfn_entries = (encoded_len.output_produced + 12) / 13;

        match = (fat32_dir_union_t*)calloc(lfn_entries, sizeof(*match));

        // Fill in reverse order
        fat32_dir_union_t *match_fill = match + (lfn_entries - 1);

        uint_fast16_t done_name = 0;
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

        for (size_t i = 0; i < lfn_entries; ++i) {
            match[i].long_entry.ordinal =
                ((i == 0) << 6) + (lfn_entries - i);
        }
    } else {
        // Short filename optimization
        match = (fat32_dir_union_t*)malloc(sizeof(*match));

        if (unlikely(!match))
            return 0;

        fill_short_filename(match, filename);

        lfn_entries = 0;
    }

    dir_iterator_t dir;
    uint16_t match_index = 0;
    uint8_t checksum = 0;
    for (int status = read_directory_begin(&dir, sector_buffer, dir_cluster);
         status > 0; status = read_directory_move_next(&dir, sector_buffer)) {
        fat32_dir_union_t const *entry =
                read_directory_current(&dir, sector_buffer);

#if FAT32_DEBUG_DIRENT
        print_dirent(entry);
#endif

        if (entry->short_entry.attr == FAT_LONGNAME)
            checksum = entry->long_entry.checksum;

        // If there are no lfn entries, or
        // there are lfn entries and this is not the short entry
        if (lfn_entries == 0 || match_index < lfn_entries) {
            if (dir_entry_match(match + match_index, entry)) {
                // Entries match
                ++match_index;

                if (lfn_entries == 0) {
                    if (likely(out_file_size))
                        *out_file_size = entry->short_entry.size;

                    return entry->short_entry.get_start();
                }
            } else {
                match_index = 0;
            }
        } else {
            if (lfn_checksum(entry->short_entry.name) == checksum) {
                // Found
                //PRINT("Found\n");

                if (likely(out_file_size))
                    *out_file_size = entry->short_entry.size;

                return (uint32_t)entry->short_entry.get_start();
            } else {
                match_index = 0;
            }
        }
    }

    return 0;
}

static uint32_t find_file_by_pathname(tchar const *pathname,
                                      uint32_t dir_cluster,
                                      uint32_t *out_file_size)
{
    tchar name[256];

    size_t pathname_len = strlen(pathname);

    for (; pathname_len; ) {

        tchar const *filename_end = strnchr(pathname, '/', pathname_len);

        filename_end = filename_end
                ? filename_end
                : (pathname + pathname_len);

        size_t filename_len = filename_end - pathname;

        if (unlikely(filename_len >= sizeof(name)))
            PANIC("Implausible filename length");

        strncpy(name, pathname, filename_len);
        name[filename_len] = 0;

        uint32_t file_size = 0;
        uint32_t cluster = find_file_by_name(name, dir_cluster, &file_size);

        if (cluster == 0)
            return 0;

        if (*filename_end == 0) {
            *out_file_size = file_size;
            return cluster;
        }

        dir_cluster = cluster;

        // Include slash
        ++filename_len;

        assert(pathname_len >= filename_len);

        pathname += filename_len;
        assert(pathname_len >= filename_len);
        pathname_len -= filename_len;
    }

    return 0;
}

static int fat32_find_available_file_handle()
{
    for (size_t i = 0; i < MAX_HANDLES; ++i) {
        if (file_handles[i].start_cluster == 0)
            return i;
    }
    return -1;
}

static uint64_t fat32_boot_serial()
{
    return fat32_serial;
}

static int fat32_boot_open(tchar const *filename)
{
    uint32_t cluster;
    uint32_t file_size = 0;

    PRINT("Finding file by pathname: %" TFMT, filename);

    // Find the start of the file
    cluster = find_file_by_pathname(filename, bpb.root_dir_start, &file_size);
    if (cluster == 0)
        return -1;

    PRINT("Finding available file handle");

    // Allocate a file handle
    int file = fat32_find_available_file_handle();
    if (file < 0)
        return -1;

    // Get ready to read the file
    file_handles[file].reset();

    PRINT("Storing file size: %" PRIu32, file_size);

    // Stash start cluster until cluster chain gets preloaded
    file_handles[file].cluster = cluster;
    file_handles[file].file_size = file_size;

    PRINT("Resetting iterator");

    PRINT("Open complete");

    // Return file handle
    return file;
}

static bool check_fd(int file)
{
    return file >= 0 && file < MAX_HANDLES;
}

static off_t fat32_boot_filesize(int file)
{
    if (unlikely(!check_fd(file)))
        return -1;

    return file_handles[file].file_size;
}

static int fat32_boot_close(int file)
{
    if (unlikely(!check_fd(file)))
        return -1;

    return file_handles[file].close();
}

static ssize_t fat32_boot_pread(int file, void *buf, size_t bytes, off_t ofs)
{
    assert(file >= 0 && file < MAX_HANDLES);
    assert(buf != nullptr);

    if (unlikely(!check_fd(file)))
        return -1;

    fat32_sector_iterator_t &desc = file_handles[file];

    if (!desc.clusters) {
        int status = fat32_sector_iterator_begin(
                    file_handles + file, sector_buffer, desc.cluster);
        if (unlikely(status < 0))
            return -1;
    }

    disk_io_plan_t plan(buf, log2_sector_sz);

    while (bytes) {
        // Calculate how many sectors into the file to start the read
        uint32_t sector_offset = ofs >> log2_sector_sz;

        // Calculate how far into the sector to start the read
        uint16_t byte_offset = ofs & (sector_sz - 1);

        // Calculate the maximum we could read from this sector
        uint16_t sector_read = sector_sz - byte_offset;

        // Cap it to read the remainder if we don't need that much
        if (sector_read > bytes)
            sector_read = bytes;

        // Calculate the cluster offset in the file
        uint32_t cluster_ofs = sector_offset >> log2_sec_per_cluster;
        uint32_t cluster_idx = sector_offset & (bpb.sec_per_cluster-1);

        if (unlikely(!plan.add(lba_from_cluster(desc.clusters[cluster_ofs]) +
                      cluster_idx, 1, byte_offset, sector_read)))
            return false;

        ofs += sector_read;
        bytes -= sector_read;
    }

    int total_read = 0;

    for (size_t i = 0; i < plan.count; ++i) {
        disk_vec_t const &item = plan.vec[i];
        if (item.sector_ofs == 0 && item.byte_count == sector_sz) {
            // Direct read
            if (unlikely(!disk_read_lba(uint64_t(plan.dest), item.lba,
                                        log2_sector_sz, item.count)))
                return false;
        } else {
            // Read sector into buffer
            if (unlikely(!disk_read_lba(uint64_t(sector_buffer), item.lba,
                                        log2_sector_sz, 1)))
                return false;
            memcpy(plan.dest, sector_buffer + item.sector_ofs, item.byte_count);
        }
        uint32_t xfer = item.byte_count * item.count;
        plan.dest = (char*)plan.dest + xfer;
        total_read += xfer;
    }

    return total_read;
}

void fat32_use_fs(uint64_t partition_lba)
{
    file_handles = (fat32_sector_iterator_t *)calloc(
                MAX_HANDLES, sizeof(*file_handles));

    PRINT("Booting partition at LBA %llu", partition_lba);

    if (unlikely(!read_bpb(partition_lba))) {
        PRINT("Error reading BPB!");
        return;
    }

    fat32_serial = bpb.serial;

#if 0
    // 0x2C LBA
    PRINT("root_dir_start:	  %" PRIu32, bpb.root_dir_start);

    // 0x24 1 per 128 clusters
    PRINT("sec_per_fat:      %" PRIu32, bpb.sec_per_fat);

    // 0x0E Usually 32
    PRINT("reserved_sectors: %d", bpb.reserved_sectors);

    // 0x0B Always 512 (yeah, sure it is)
    PRINT("bytes_per_sec:	  %d", bpb.bytes_per_sec);

    // 0x1FE Always 0xAA55
    PRINT("signature:		  %x", bpb.signature);

    // 0x0D 8=4KB cluster
    PRINT("sec_per_cluster:  %d", bpb.sec_per_cluster);

    // 0x10 Always 2
    PRINT("number_of_fats:	  %d", bpb.number_of_fats);
#endif

    fs_api.name = TSTR "direct_fat32";
    fs_api.boot_open = fat32_boot_open;
    fs_api.boot_filesize = fat32_boot_filesize;
    fs_api.boot_close = fat32_boot_close;
    fs_api.boot_pread = fat32_boot_pread;
    fs_api.boot_drv_serial = fat32_boot_serial;
}

void fat32_boot_partition(uint64_t partition_lba)
{
    fat32_use_fs(partition_lba);


    elf64_boot();
}

void fat32_sector_iterator_t::reset()
{
    start_cluster = 0;
    cluster = 0;
    position = 0;
    sector_offset = 0;
    ok = false;
    clusters = nullptr;
    cluster_count = 0;
    file_size = 0;
}

int fat32_sector_iterator_t::close()
{
    int result = ok ? 0 : 1;

    //if (cluster_count <= 1024)
        delete[] clusters;
    //else
    //    free_phys({(uint64_t)clusters, cluster_count * sizeof(uint32_t)});

    reset();

    return result;
}
