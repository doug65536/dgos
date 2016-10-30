#include "types.h"

// Handles advancing through the sectors of
// a cluster and following the cluster chain
// to the next cluster
typedef struct {
    uint32_t start_cluster;

    // Cluster number of current position
    uint32_t cluster;

    // Sector offset from beginning of cluster
    uint16_t sector_offset;

    uint16_t err;
} sector_iterator_t;

// Handles iterating a directory and advancing
// to the next dir_union_t
typedef struct {
    // Directory file iterator
    sector_iterator_t dir_file;

    // dir_entry_t index into sector
    uint16_t sector_index;
} directory_iterator_t;

void boot_partition(uint32_t partition_lba);
