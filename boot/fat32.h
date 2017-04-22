#pragma once

#include "types.h"
#include "farptr.h"

// Handles advancing through the sectors of
// a cluster and following the cluster chain
// to the next cluster
struct fat32_sector_iterator_t {
    uint32_t start_cluster;

    // Cluster number of current position
    uint32_t cluster;

    // Offset from start_cluster in sectors
    // (This being 32 bit means max file size is 2TB)
    uint32_t position;

    // Sector offset from beginning of cluster
    uint16_t sector_offset;

    uint16_t err;

    far_ptr_t clusters;
    uint32_t cluster_count;
};

// Handles iterating a directory and advancing
// to the next dir_union_t
struct dir_iterator_t {
    // Directory file iterator
    fat32_sector_iterator_t dir_file;

    // dir_entry_t index into sector
    uint16_t sector_index;
};

void fat32_boot_partition(uint32_t partition_lba);
