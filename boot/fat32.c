#include "code16gcc.h"

#include "malloc.h"
#include "fat32.h"
#include "bootsect.h"
#include "screen.h"

bpb_data_t bpb;

char *sector_buffer;

// Initialize bpb data from sector buffer
// Expects first sector of partition
// Returns
uint16_t read_bpb(uint32_t partition_lba)
{
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
// Returns 1 on success
uint16_t utf8_to_utf16(uint16_t *out, uint16_t out_size, char const *in)
{
    uint16_t *out_end = out + out_size;
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

    return 1;
}

uint32_t lba_from_cluster(uint32_t cluster)
{
    return bpb.cluster_begin_lba +
            cluster * bpb.sec_per_cluster;
}

// Read the first sector of a directory
// and prepare to iterate it
// Returns error code, or 0 on success
uint16_t read_directory_begin(
        directory_iterator_t *iter,
        char *sector,
        uint32_t cluster)
{
    iter->cluster = cluster;
    iter->sector_offset = 0;
    iter->sector_index = 0;

    cluster = lba_from_cluster(cluster);

    return read_lba_sector(sector,
                           boot_drive,
                           cluster);
}

// fcb_name is the space padded 11 character name with no dot,
// the representation used in dir_entry_t's name field
uint8_t lfn_checksum(const unsigned char *fcb_name)
{
   uint16_t i;
   uint8_t sum = 0;

   for (i = 11; i; i--)
      sum = ((sum & 1) << 7) + (sum >> 1) + *fcb_name++;

   return sum;
}

//uint32_t next_cluster_from_fat(
//        char *sector,
//        uint32_t current_cluster)
//{
//
//}

dir_entry_t const *read_directory_current(
        directory_iterator_t const *iter,
        char const* sector)
{
    return (dir_entry_t*)sector + iter->sector_index;
}

uint16_t read_directory_move_next(
        directory_iterator_t *iter,
        char *sector)
{
    // If we reached the end of the sector
    if (++iter->sector_index == 512 / sizeof(dir_entry_t)) {
        uint32_t lba;
        // If we reached the end of the cluster
        if (++iter->sector_offset >= bpb.sec_per_cluster)
        {
        }

        lba = lba_from_cluster(iter->cluster) +
                iter->sector_offset;
        uint16_t err = read_lba_sector(sector, boot_drive, lba);
        if (err)
            return err;

    }

    return 0;
}

void boot_partition(uint32_t partition_lba, uint16_t drive)
{
    test_malloc();

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

    // Read first sector of root directory
    uint32_t root_dir_cluster = bpb.root_dir_start * bpb.sec_per_cluster +
            bpb.cluster_begin_lba;
    err = read_lba_sector(sector_buffer, drive, root_dir_cluster);
    if (err) {
        print_line("Error reading root directory!");
        return;
    }

    //dir_entry_t const *entry = (dir_entry_t const *)sector_buffer;
    print_line("root dir at %x", root_dir_cluster * bpb.bytes_per_sec);

    //"really long filename with spaces.txt"
}

static void test_utf16()
{
    uint16_t utf16[8], *up;
    char const *utf8 = u8"$¢€𐍈";
    utf8_to_utf16(utf16, sizeof(utf16), utf8);

    for (up = utf16; *up; ++up)
        print_line("%x", *up);
}

