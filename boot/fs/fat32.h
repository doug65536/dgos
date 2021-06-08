#pragma once

#include "types.h"
#include "farptr.h"

// Handles advancing through the sectors of
// a cluster and following the cluster chain
// to the next cluster
struct fat32_sector_iterator_t {
    uint32_t start_cluster;

    // Cluster number of current position
    uint64_t cluster;

    // Offset from start_cluster in sectors
    // (This being 32 bit means max file size is 2TB)
    uint32_t position;

    // Sector offset from beginning of cluster
    uint16_t sector_offset;

    // Set to false when a disk error occurs
    bool ok;

    off_t file_size;

    uint32_t *clusters;
    uint32_t cluster_count;

    fat32_sector_iterator_t()
    {
        reset();
    }

    ~fat32_sector_iterator_t()
    {
        close();
    }

    void reset();

    int close();
};

// Handles iterating a directory and advancing
// to the next dir_union_t
struct dir_iterator_t {
    // Directory file iterator
    fat32_sector_iterator_t dir_file;

    // dir_entry_t index into sector
    size_t sector_index;
};

__BEGIN_DECLS

void fat32_use_fs(uint64_t partition_lba);
void fat32_boot_partition(uint64_t partition_lba);

__END_DECLS

