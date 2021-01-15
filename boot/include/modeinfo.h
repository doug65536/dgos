#pragma once
#include "farptr.h"

// 512 bytes
struct vbe_info_t {
    char sig[4];            // "VESA"
    uint16_t version;       // 0x200
    far_ptr_t oem_str_ptr;
    uint8_t capabilities[4];
    far_ptr_t mode_list_ptr;
    uint16_t mem_size_64k;
    uint16_t rev;
    far_ptr_t vendor_name_str;
    far_ptr_t product_name_str;
    far_ptr_t product_rev_str;
    char reserved[222];
    char oemdata[256];
} _packed;

// 256 bytes
struct vbe_mode_info_t {
    // VBE 1.0
    uint16_t mode_attrib;
    uint8_t win_a_attrib;
    uint8_t win_b_attrib;
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t win_a_seg;
    uint16_t win_b_seg;
    far_ptr_t win_fn;
    uint16_t bytes_scanline;

    // VBE 1.2
    uint16_t res_x;
    uint16_t res_y;
    uint8_t char_size_x;
    uint8_t char_size_y;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t mem_model;
    uint8_t bank_size_kb;
    uint8_t image_pages;
    uint8_t reserved;

    uint8_t mask_size_r;
    uint8_t mask_pos_r;
    uint8_t mask_size_g;
    uint8_t mask_pos_g;
    uint8_t mask_size_b;
    uint8_t mask_pos_b;
    uint8_t mask_size_rsvd;
    uint8_t mask_pos_rsvd;
    uint8_t dc_mode_info;

    // VBE 2.0
    uint32_t phys_base_ptr;
    uint32_t offscreen_mem_offset;
    uint16_t offscreen_mem_size_kb;
    char reserved2[206];
} _packed;

struct vbe_selected_mode_t {
    uint64_t framebuffer_addr;
    uint64_t framebuffer_bytes;

    uint32_t mode_num;

    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint16_t aspect_n;
    uint16_t aspect_d;
    uint8_t bpp;
    uint8_t byte_pp;

    uint8_t mask_size_r;
    uint8_t mask_size_g;
    uint8_t mask_size_b;
    uint8_t mask_size_a;
    uint8_t mask_pos_r;
    uint8_t mask_pos_g;
    uint8_t mask_pos_b;
    uint8_t mask_pos_a;

    char reserved[2];
};
