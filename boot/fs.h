#pragma once
#include "types.h"

struct fs_api_t {
    int (*boot_open)(tchar const *filename);
    int (*boot_close)(int file);
    int (*boot_pread)(int file, void *buf, size_t bytes, off_t ofs);
    uint64_t (*boot_drv_serial)();
};

extern fs_api_t fs_api;

// Open/close/read file on the boot drive
int boot_open(tchar const *filename);
int boot_close(int file);
ssize_t boot_pread(int file, void *buf, size_t bytes, off_t ofs);
uint64_t boot_serial();

// Filesystem I/O code builds a list of these to do burst I/O
struct disk_vec_t {
    // Start LBA of range
    uint64_t lba;
    
    // Number of contiguous sectors
    uint32_t count;
    
    // Offset into first sector to transfer
    // If this is nonzero, count is guaranteed to be 1
    uint16_t sector_ofs;
    
    // Number of bytes to transfer.
    // If count is nonzero, this field is ignored, and count is the number
    // of whole sectors to transfer.
    uint16_t byte_count;
};

struct disk_io_plan_t {
    void *dest;
    disk_vec_t *vec;
    uint16_t count;
    uint16_t capacity;
    uint8_t log2_sector_size;
    
    disk_io_plan_t(void *dest, uint8_t log2_sector_size);
    ~disk_io_plan_t();
    
    bool add(uint32_t lba, uint16_t sector_count, 
             uint16_t sector_ofs, uint16_t byte_count);
};
