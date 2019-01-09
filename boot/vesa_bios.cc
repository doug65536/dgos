#include "vesainfo.h"
#include "vesa.h"

#include "likely.h"
#include "bioscall.h"
#include "screen.h"
#include "malloc.h"
#include "paging.h"
#include "cpuid.h"

#define VESA_DEBUG 1
#if VESA_DEBUG
#define VESA_TRACE(...) PRINT("vesa: " __VA_ARGS__)
#else
#define VESA_TRACE(...) ((void)0)
#endif

vbe_mode_list_t mode_list{};

// All VBE BIOS calls have AH=4Fh
// Successful VBE calls return with AX=4F00h
// Errors have nonzero error code in AL

static uint16_t vbe_get_info(void *info, uint16_t ax, uint16_t cx)
{
    bios_regs_t regs{};
    far_ptr_t info_fp = info;

    regs.eax = ax;
    regs.ecx = cx;
    regs.es = info_fp.segment;
    regs.edi = info_fp.offset;

    bioscall(&regs, 0x10);

    return regs.eax & 0xFFFF;
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

bool vbe_set_mode(int mode)
{
    mode = mode_list.modes[mode].mode_num;

    bios_regs_t regs;

    regs.eax = 0x4F02;
    // 0x4000 means use linear framebuffer
    regs.ebx = 0x4000 | mode;

    bioscall(&regs, 0x10);

    return (regs.eax & 0xFFFF) == 0x4F;
}

void selected_from_vbeinfo(vbe_mode_info_t * restrict mode_info,
                           vbe_info_t * restrict info,
                           vbe_selected_mode_t * restrict sel,
                           uint16_t mode_num)
{
    sel->mode_num = mode_num;
    sel->width = mode_info->res_x;
    sel->height = mode_info->res_y;
    sel->pitch = mode_info->bytes_scanline;
    sel->framebuffer_addr = mode_info->phys_base_ptr;
    sel->framebuffer_bytes = uint64_t(info->mem_size_64k) << 16;
    sel->mask_size_r = mode_info->mask_size_r;
    sel->mask_size_g = mode_info->mask_size_g;
    sel->mask_size_b = mode_info->mask_size_b;
    sel->mask_size_a = mode_info->mask_size_rsvd;
    sel->mask_pos_r = mode_info->mask_pos_r;
    sel->mask_pos_g = mode_info->mask_pos_g;
    sel->mask_pos_b = mode_info->mask_pos_b;
    sel->mask_pos_a = mode_info->mask_pos_rsvd;
    memset(sel->reserved, 0, sizeof(sel->reserved));
}

bool vbe_is_mode_usable(vbe_mode_info_t const *mode)
{
    return mode->planes == 1 &&
            (mode->phys_base_ptr ||
             (mode->win_a_seg && !mode->win_b_seg)) &&
            mode->mask_size_r &&
            mode->mask_size_g &&
            mode->mask_size_b;
}

vbe_mode_list_t const& vbe_enumerate_modes()
{
    if (mode_list.count != 0)
        return mode_list;

    vbe_info_t *info = new vbe_info_t{};

    if (vbe_detect(info)) {
        vbe_mode_info_t *mode_info = new vbe_mode_info_t{};

        mode_list.count = 0;

        // Mode list
        for (uint16_t *mi = info->mode_list_ptr; *mi != 0xFFFF; ++mi) {
            uint16_t mode = *mi;

            if (unlikely(!vbe_mode_info(mode_info, mode)))
                continue;

            if (!vbe_is_mode_usable(mode_info))
                continue;

            ++mode_list.count;
        }

        mode_list.modes = new vbe_selected_mode_t[mode_list.count] {};

        size_t out = 0;

        for (uint16_t *mi = info->mode_list_ptr; *mi != 0xFFFF; ++mi) {
            uint16_t mode = *mi;

            if (unlikely(!vbe_mode_info(mode_info, mode)))
                continue;

            if (!vbe_is_mode_usable(mode_info))
                continue;

            selected_from_vbeinfo(mode_info, info,
                                  &mode_list.modes[out++], mode);
        }

        delete mode_info;
    }

    return mode_list;
}

vbe_selected_mode_t *vbe_select_mode(
        uint32_t width, uint32_t height, bool verbose)
{
    vbe_info_t *info = new vbe_info_t;
    vbe_mode_info_t *mode_info = new vbe_mode_info_t;

    vbe_selected_mode_t *sel = new vbe_selected_mode_t{};

    uint16_t mode;
    uint16_t done = 0;
    if (vbe_detect(info)) {
        VESA_TRACE("VBE Memory %dMB\n", info->mem_size_64k >> 4);

        uint16_t *modes = info->mode_list_ptr;

        while (!done) {
            // Get mode number
            mode = *modes++;

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

                aspect_ratio(&sel->aspect_n, &sel->aspect_d,
                             mode_info->res_x,
                             mode_info->res_y);

                if (verbose) {
                    VESA_TRACE("vbe mode %u w=%u h=%u"
                               " %d:%d:%d:%d phys_addr=%" PRIx32 " %d:%d",
                               mode,
                               mode_info->res_x,
                               mode_info->res_y,
                               mode_info->mask_size_r,
                               mode_info->mask_size_g,
                               mode_info->mask_size_b,
                               mode_info->mask_size_rsvd,
                               mode_info->phys_base_ptr,
                               sel->aspect_n, sel->aspect_d);
                }

                if (mode_info->res_x == width &&
                        mode_info->res_y == height &&
                        mode_info->bpp == 32) {
                    selected_from_vbeinfo(mode_info, info, sel, mode);
                    done = 1;
                    vbe_set_mode(mode);
                    break;
                }
            }
        }
    }

    void *kernel_data = nullptr;

    if (done) {
        kernel_data = malloc(sizeof(sel));
        memcpy(kernel_data, &sel, sizeof(sel));

        uint64_t pte_flags = PTE_PRESENT | PTE_WRITABLE |
                (-cpu_has_global_pages() & PTE_GLOBAL);

        paging_map_physical(sel->framebuffer_addr, sel->framebuffer_addr,
                            sel->framebuffer_bytes, pte_flags);
    }

    delete mode_info;
    delete info;

    return (vbe_selected_mode_t*)kernel_data;
}
