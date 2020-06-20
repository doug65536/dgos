#include <sys/module.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/likely.h>
#include <spawn.h>
#include <dirent.h>
#include <string.h>
#include <inttypes.h>
#include <sys/framebuffer.h>
#include <pthread.h>
#include <png.h>

template<typename T, size_t hw_vec_sz = 64>
struct aosoa_t {
    typedef T vector_t __attribute__((__vector_size__(hw_vec_sz / sizeof(T)),
                                      __aligned__(16)));
    static constexpr const size_t row_count = hw_vec_sz / sizeof(vector_t);
};

using aosoa_int_t = aosoa_t<int>;
using aosoa_float_t = aosoa_t<float>;

extern "C"
__attribute__((__target_clones__("default,avx2"),
               __visibility__("default"),
               __used__,
               __optimize__("-O3")))
void do_aosoa(aosoa_int_t::vector_t * restrict c,
              aosoa_int_t::vector_t const * restrict a,
              aosoa_int_t::vector_t const * restrict b,
              size_t n)
{
    for (size_t i = 0; i < n; i += aosoa_float_t::row_count) {
        for (size_t vec = 0; vec < aosoa_float_t::row_count; ++vec)
            c[i+vec] = a[i+vec] + b[i+vec];
    }
}

__BEGIN_DECLS
#include <stddef.h>
__attribute__((__format__(__printf__, 1, 0), __noreturn__))
void verr(char const *format, va_list ap)
{
    printf("Error:\n");
    vprintf(format, ap);
    exit(1);
}

__attribute__((__format__(__printf__, 1, 2), __noreturn__))
void err(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    verr(format, ap);
    va_end(ap);
}

void load_module(char const *path, char const *parameters = nullptr)
{
    if (!parameters)
        parameters = "";

    int fd = open(path, O_EXCL | O_RDONLY);
    if (unlikely(fd < 0))
        err("Cannot open %s\n", path);

    off_t sz = lseek(fd, 0, SEEK_END);
    if (unlikely(sz < 0))
        err("Cannot seek to end of module\n");

    if (unlikely(lseek(fd, 0, SEEK_SET) != 0))
        err("Cannot seek to start of module\n");

    void *mem = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
    if (unlikely(mem == MAP_FAILED))
        err("Cannot allocate %" PRIu64 "d bytes\n", sz);

    if (unlikely(sz != read(fd, mem, sz)))
        err("Cannot read %" PRIu64 " bytes\n", sz);

    int status;
    char *needed = (char*)malloc(NAME_MAX);
    do {
        strcpy(needed, "what the fuck");
        needed[0] = 0;
        status = init_module(mem, sz, path, nullptr, parameters, needed);

        if (needed[0] != 0) {
            load_module(needed);
        }
    } while (needed[0]);
    free(needed);

    if (unlikely(status < 0))
        err("Module failed to initialize with %d %d\n", status, errno);

    close(fd);
}

void test_aosoa_ifunc()
{
    aosoa_int_t::vector_t *c = new aosoa_int_t::vector_t[
            64 / sizeof(aosoa_int_t::vector_t)];
    aosoa_int_t::vector_t *a = new aosoa_int_t::vector_t[
            64 / sizeof(aosoa_int_t::vector_t)];
    aosoa_int_t::vector_t *b = new aosoa_int_t::vector_t[
            64 / sizeof(aosoa_int_t::vector_t)];
    __asm__ __volatile__ ("" : : : "memory");
    do_aosoa(c, a, b, 64 / sizeof(aosoa_int_t::vector_t));
}

static uint32_t translate_pixel(fb_info_t *info, uint32_t pixel)
{
    uint32_t r = (pixel >> 16) & 0xFF;
    uint32_t g = (pixel >> 8) & 0xFF;
    uint32_t b = pixel & 0xFF;
    uint32_t a = 0xFF;// (pixel >> 24) & 0xFF;

    r >>= 8 - info->fmt.mask_size_r;
    g >>= 8 - info->fmt.mask_size_g;
    b >>= 8 - info->fmt.mask_size_b;
    a >>= 8 - info->fmt.mask_size_a;

    r <<= info->fmt.mask_pos_r;
    g <<= info->fmt.mask_pos_g;
    b <<= info->fmt.mask_pos_b;
    a <<= info->fmt.mask_pos_a;

    r |= g;
    b |= a;
    r |= b;

    return r;
}

void png_draw_noclip(int dx, int dy,
                     int dw, int dh,
                     int sx, int sy,
                     png_image_t *img, fb_info_t *info)
{
    // Calculate a pointer to the first image pixel
    uint32_t const * restrict src = png_pixels(img) + img->width * sy + sx;

    if (info->pixel_sz == 4) {
        uint32_t * restrict dst;
        dst = (uint32_t*)(uintptr_t(info->vmem) + dy * info->pitch) + dx;
        for (size_t y = 0; y < unsigned(dh); ++y) {
            for (size_t x = 0; x < unsigned(dw); ++x) {
                uint32_t pixel = translate_pixel(info, src[x]);
                dst[x] = pixel;
            }
            src += img->width;
            dst = (uint32_t*)(uintptr_t(dst) + info->pitch);
        }
    } else if (info->pixel_sz == 3) {
        uint8_t * restrict dst;
        dst = (uint8_t*)(uintptr_t(info->vmem) + dy * info->pitch) + dx * 3;
        for (size_t y = 0; y < unsigned(dh); ++y) {
            for (size_t x = 0; x < unsigned(dw); ++x) {
                uint32_t pixel = translate_pixel(info, src[x]);
                memcpy(dst + x * 3, &pixel, 3);
            }
            src += img->width;
            dst = (uint8_t*)(uintptr_t(dst) + info->pitch);
        }
    } else if (info->pixel_sz == 2) {
        uint16_t * restrict dst;
        dst = (uint16_t*)(uintptr_t(info->vmem) + dy * info->pitch) + dx;
        for (size_t y = 0; y < unsigned(dh); ++y) {
            for (size_t x = 0; x < unsigned(dw); ++x) {
                uint32_t pixel = translate_pixel(info, src[x]);
                dst[x] = pixel;
            }
            src += img->width;
            dst = (uint16_t*)(uintptr_t(dst) + info->pitch);
        }
    } else {
        // Palette mode not supported
    }
}


void png_draw(int dx, int dy,
              int dw, int dh,
              int sx, int sy,
              png_image_t *img, fb_info_t *info)
{
    int hclip, vclip;

    // Calculate amount clipped off left and top
    hclip = -dx;
    vclip = -dy;

    // Zero out negative clip amount
    hclip &= -(hclip >= 0);
    vclip &= -(vclip >= 0);

    // Adjust left and width and source left
    dx += hclip;
    dw -= hclip;
    sx += hclip;

    // Adjust top and height and source top
    dy += vclip;
    dh -= vclip;
    sy += vclip;

    // Calculate amount clipped off right and bottom
    hclip = (dx + dw) - info->w;
    vclip = (dy + dh) - info->h;

    // Zero out negative clip amount
    hclip &= -(hclip >= 0);
    vclip &= -(vclip >= 0);

    // Adjust bottom of destination to handle clip
    dw -= hclip;
    dh -= vclip;

    // Clamp right and bottom of destination
    // to right and bottom edge of source
    hclip = (sx + dw) - img->width;
    vclip = (sy + dh) - img->height;

    // Zero out negative clip amount
    hclip &= -(hclip >= 0);
    vclip &= -(vclip >= 0);

    // Adjust bottom of destination to handle clip
    dw -= hclip;
    dh -= vclip;

    if (unlikely((dw <= 0) | (dh <= 0)))
        return;

    png_draw_noclip(dx, dy, dw, dh, sx, sy, img, info);
}

int main(int argc, char **argv, char **envp)
{
    test_aosoa_ifunc();

    DIR *dir = opendir("/");

    dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        printf("%s\n", ent->d_name);
    }

    closedir(dir);

//    dir = opendir("/dev/something");

    load_module("symsrv.km");

    load_module("unittest.km");

    // fixme: check ACPI
    load_module("keyb8042.km");

    load_module("ext4.km");
    load_module("fat32.km");
    load_module("iso9660.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_SERIAL,
                      PCI_SUBCLASS_SERIAL_USB,
                      PCI_PROGIF_SERIAL_USB_XHCI) > 0)
        load_module("usbxhci.km");

    load_module("usbmsc.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_NVM,
                      PCI_PROGIF_STORAGE_NVM_NVME) > 0)
        load_module("nvme.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_SATA,
                      PCI_PROGIF_STORAGE_SATA_AHCI) > 0)
        load_module("ahci.km");

    if (probe_pci_for(0x1AF4, -1,
                      PCI_DEV_CLASS_STORAGE,
                      -1,
                      -1) > 0)
        load_module("virtio-blk.km");

    if (probe_pci_for(0x1AF4, -1,
                      PCI_DEV_CLASS_DISPLAY,
                      -1,
                      -1) > 0)
        load_module("virtio-gpu.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_ATA, -1))
        load_module("ide.km");

    load_module("gpt.km");
    load_module("mbr.km");

    if (probe_pci_for(0x10EC, 0x8139,
                      PCI_DEV_CLASS_NETWORK,
                      PCI_SUBCLASS_NETWORK_ETHERNET, -1))
        load_module("rtl8139.km");

    fb_info_t info;
    int err = framebuffer_enum(0, 0, &info);

    if (err < 0) {
        errno_t save_errno = errno;
        printf("Unable to map framebuffer!\n");
        errno = save_errno;
        return -1;
    }

    png_image_t *img = png_load("background.png");
    int x = 0;
    int direction = 1;
    for (;;) {
        x += direction;
        if (x == 0)
            direction = 1;
        if (x == 500)
            direction = -1;

        png_draw(0, 0, img->width, img->height, x, 0, img, &info);
    }
    png_free(img);

    return 0;
}
__END_DECLS
