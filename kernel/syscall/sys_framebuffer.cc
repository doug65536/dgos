#include "sys_framebuffer.h"
#include "errno.h"
#include "bootinfo.h"
#include "mm.h"
#include "user_mem.h"

int sys_framebuffer_enum(size_t index, size_t count, fb_info_t *result_ptr)
{
    vbe_selected_mode_t *mode = reinterpret_cast<vbe_selected_mode_t *>(
                bootinfo_parameter(bootparam_t::vbe_mode_info));

    fb_info_t result;
    memset(&result, 0, sizeof(result));

    result.fmt = pix_fmt_t{
        mode->mask_size_r,
        mode->mask_size_g,
        mode->mask_size_b,
        mode->mask_size_a,
        mode->mask_pos_r,
        mode->mask_pos_g,
        mode->mask_pos_b,
        mode->mask_pos_a
    };

    result.w = mode->width;
    result.h = mode->height;
    result.pitch = mode->pitch;
    result.pixel_sz = mode->byte_pp;

    result.vmem = mmap((void*)mode->framebuffer_addr,
                       mode->framebuffer_bytes,
                       PROT_READ | PROT_WRITE,
                       MAP_PHYSICAL | MAP_WEAKORDER | MAP_USER);

    result.vmem_size = mode->framebuffer_bytes;
    result.x = 0;
    result.y = 0;

    if (unlikely(!mm_is_user_range(result_ptr, sizeof(*result_ptr))))
        return int(errno_t::EFAULT);

    if (unlikely(!mm_copy_user(result_ptr, &result, sizeof(*result_ptr))))
        return int(errno_t::EFAULT);

    return 0;
}

int sys_framebuffer_map(size_t index, size_t count)
{
    return 0;
}
