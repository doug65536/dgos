#pragma once

#include "types.h"

// Handles advancing through the sectors of
// a cluster and following the cluster chain
// to the next cluster
typedef struct sector_iterator_t {
    uint32_t start_cluster;

    // Cluster number of current position
    uint32_t cluster;

    // Offset from start_cluster in sectors
    // (This being 32 bit means max file size is 2TB)
    uint32_t position;

    // Sector offset from beginning of cluster
    uint16_t sector_offset;

    uint16_t err;
} sector_iterator_t;

// Handles iterating a directory and advancing
// to the next dir_union_t
typedef struct directory_iterator_t {
    // Directory file iterator
    sector_iterator_t dir_file;

    // dir_entry_t index into sector
    uint16_t sector_index;
} directory_iterator_t;

void boot_partition(uint32_t partition_lba);

// Open/close/read file on the boot drive
int boot_open(char const *filename);
int boot_close(int file);
int boot_pread(int file, void *buf, size_t bytes, off_t ofs);
