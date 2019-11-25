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

bool vbe_set_mode(vbe_selected_mode_t& mode)
{
    bios_regs_t regs;

    regs.eax = 0x4F02;
    // 0x4000 means use linear framebuffer
    regs.ebx = 0x4000 | mode.mode_num;

    bioscall(&regs, 0x10);

    return (regs.eax & 0xFFFF) == 0x4F;
}

static void selected_from_vbeinfo(vbe_mode_info_t * restrict mode_info,
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
    sel->bpp = mode_info->bpp;
    sel->byte_pp = (mode_info->bpp + 7) >> 3;
    memset(sel->reserved, 0, sizeof(sel->reserved));
}

static bool vbe_is_mode_usable(vbe_mode_info_t const *mode)
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

    vbe_info_t *info = new (std::nothrow) vbe_info_t();

    if (vbe_detect(info)) {
        vbe_mode_info_t *mode_info = new (std::nothrow) vbe_mode_info_t();

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

        mode_list.modes = new (std::nothrow)
                vbe_selected_mode_t[mode_list.count]();

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
