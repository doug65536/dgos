#include "vesa.h"
#include "farptr.h"
#include "string.h"
#include "malloc.h"
#include "screen.h"

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
        : "D" (info),
          "c" (cx)
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

uint32_t vbe_set_mode(uint16_t width, uint16_t height, uint16_t verbose)
{
    vbe_info_t *info;
    vbe_mode_info_t *mode_info;

    info = malloc(sizeof(*info));
    mode_info = malloc(sizeof(*mode_info));

    uint16_t mode;
    uint16_t done = 0;
    if (vbe_detect(info)) {
        for (uint16_t ofs = 0; !done; ofs += sizeof(uint16_t)) {
            // Get mode number
            far_copy_to(&mode,
                        far_adj(info->mode_list_ptr, ofs),
                        sizeof(mode));

            if (mode == 0xFFFF)
                break;

            // Get mode information
            if (vbe_mode_info(mode_info, mode)) {
                if (verbose) {
                    // Ignore palette modes
                    if (!mode_info->mask_size_r &&
                            !mode_info->mask_size_g &&
                            !mode_info->mask_size_b &&
                            !mode_info->mask_size_rsvd)
                        continue;

                    uint16_t aspect_n;
                    uint16_t aspect_d;
                    aspect_ratio(&aspect_n, &aspect_d,
                                 mode_info->res_x,
                                 mode_info->res_y);

                    print_line("vbe mode %u w=%u h=%u"
                               " %d:%d:%d:%d phys_addr=%x %d:%d",
                               mode,
                               mode_info->res_x,
                               mode_info->res_y,
                               mode_info->mask_size_r,
                               mode_info->mask_size_g,
                               mode_info->mask_size_b,
                               mode_info->mask_size_rsvd,
                               mode_info->phys_base_ptr,
                               aspect_n, aspect_d);
                }

                if (mode_info->res_x == width &&
                        mode_info->res_y == height &&
                        mode_info->bpp ==
                        mode_info->mask_size_r +
                        mode_info->mask_size_g +
                        mode_info->mask_size_b +
                        mode_info->mask_size_rsvd) {
                    done = 1;
                    break;
                }
            }
        }
    }

    uint32_t physaddr = done ? mode_info->phys_base_ptr : 0;

    free(mode_info);
    free(info);

    return physaddr;
}
