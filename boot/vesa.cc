#include "vesa.h"
#include "vesainfo.h"
#include "farptr.h"
#include "string.h"
#include "malloc.h"
#include "screen.h"
#include "paging.h"
#include "cpu.h"
#define VESA_DEBUG 1
#if VESA_DEBUG
#define VESA_TRACE(...) print_line("vesa: " __VA_ARGS__)
#else
#define VESA_TRACE(...) ((void)0)
#endif

// All VBE BIOS calls have AH=4Fh
// Successful VBE calls return with AX=4F00h
// Errors have nonzero error code in AL

// 512 bytes
typedef struct vbe_info_t {
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
} __attribute__((packed)) vbe_info_t;

// 256 bytes
typedef struct vbe_mode_info_t {
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
} __attribute__((packed)) vbe_mode_info_t;

static uint16_t vbe_get_info(void *info, uint16_t ax, uint16_t cx)
{
    __asm__ __volatile__ (
        "int $0x10\n\t"
        : "+a" (ax)
        : "D" (info)
        , "c" (cx)
        : "memory"
    );
    return ax;
}

static int vbe_detect(vbe_info_t *info)
{
    info->sig[0] = 'V';
    info->sig[1] = 'B';
    info->sig[2] = 'E';
    info->sig[3] = '2';
    return vbe_get_info(info, 0x4F00, 0) == 0x4F;
}

static int vbe_mode_info(vbe_mode_info_t *info, uint16_t mode)
{
    return vbe_get_info(info, 0x4F01, mode) == 0x4F;
}

static uint16_t vbe_set_mode(uint16_t mode)
{
    uint16_t ax = 0x4F02;
    // 0x4000 means use linear framebuffer
    uint16_t bx = 0x4000 | mode;
    __asm__ __volatile__ (
        "int $0x10\n\t"
        : "+a" (ax)
        : "b" (bx)
    );
    return ax == 0x4F;
}

static uint16_t gcd(uint16_t a, uint16_t b)
{
    while (a != b) {
        if (a > b)
            a -= b;
        else
            b -= a;
    }

    return a;
}

static void aspect_ratio(uint16_t *n, uint16_t *d, uint16_t w, uint16_t h)
{
    uint16_t div = gcd(w, h);
    *n = w / div;
    *d = h / div;
}

uint16_t vbe_select_mode(uint16_t width, uint16_t height, uint16_t verbose)
{
    vbe_info_t *info;
    vbe_mode_info_t *mode_info;

    info = (vbe_info_t *)malloc(sizeof(*info));
    mode_info = (vbe_mode_info_t *)malloc(sizeof(*mode_info));

    vbe_selected_mode_t sel;

    uint16_t mode;
    uint16_t done = 0;
    if (vbe_detect(info)) {
        VESA_TRACE("VBE Memory %dMB\n", info->mem_size_64k >> 4);

        for (uint16_t ofs = 0; !done; ofs += sizeof(uint16_t)) {
            // Get mode number
            far_copy_to(&mode,
                        far_adj(info->mode_list_ptr, ofs),
                        sizeof(mode));

            if (mode == 0xFFFF)
                break;

            // Get mode information
            if (vbe_mode_info(mode_info, mode)) {
                // Ignore palette modes
                if (!mode_info->mask_size_r &&
                        !mode_info->mask_size_g &&
                        !mode_info->mask_size_b &&
                        !mode_info->mask_size_rsvd)
                    continue;

                aspect_ratio(&sel.aspect_n, &sel.aspect_d,
                             mode_info->res_x,
                             mode_info->res_y);

                if (verbose) {
                    VESA_TRACE("vbe mode %u w=%u h=%u"
                               " %d:%d:%d:%d phys_addr=%x %d:%d",
                               mode,
                               mode_info->res_x,
                               mode_info->res_y,
                               mode_info->mask_size_r,
                               mode_info->mask_size_g,
                               mode_info->mask_size_b,
                               mode_info->mask_size_rsvd,
                               mode_info->phys_base_ptr,
                               sel.aspect_n, sel.aspect_d);
                }

                if (mode_info->res_x == width &&
                        mode_info->res_y == height &&
                        mode_info->bpp == 32) {
                    sel.width = mode_info->res_x;
                    sel.height = mode_info->res_y;
                    sel.pitch = mode_info->bytes_scanline;
                    sel.framebuffer_addr = mode_info->phys_base_ptr;
                    sel.framebuffer_bytes = info->mem_size_64k << 16;
                    sel.mask_size_r = mode_info->mask_size_r;
                    sel.mask_size_g = mode_info->mask_size_g;
                    sel.mask_size_b = mode_info->mask_size_b;
                    sel.mask_size_a = mode_info->mask_size_rsvd;
                    sel.mask_pos_r = mode_info->mask_pos_r;
                    sel.mask_pos_g = mode_info->mask_pos_g;
                    sel.mask_pos_b = mode_info->mask_pos_b;
                    sel.mask_pos_a = mode_info->mask_pos_rsvd;
                    memset(sel.reserved, 0, sizeof(sel.reserved));
                    done = 1;
                    vbe_set_mode(mode);
                    break;
                }
            }
        }
    }

    far_ptr_t kernel_data;
    kernel_data.segment = 0;
    kernel_data.offset = 0;

    if (done) {
        kernel_data.segment = far_malloc(sizeof(sel));
        kernel_data.offset = 0;
        far_copy_from(kernel_data, &sel, sizeof(sel));

        paging_map_range(kernel_data.segment << 4, sizeof(sel),
                         kernel_data.segment << 4,
                         PTE_PRESENT | PTE_WRITABLE, 2);

        paging_map_range(sel.framebuffer_addr, sel.framebuffer_bytes,
                         sel.framebuffer_addr,
                         PTE_PRESENT | PTE_WRITABLE |
                         (-cpu_has_global_pages() & PTE_GLOBAL), 2);
    }

    free(mode_info);
    free(info);

    return kernel_data.segment;
}
