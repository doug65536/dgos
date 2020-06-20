#pragma once
#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

struct png_image_t {
    int32_t width;
    int32_t height;
    void *reserved;
};

png_image_t *png_load(char const *path);
void png_free(png_image_t *pp);
void png_autofree(png_image_t **pp);

static inline uint32_t const *png_pixels(png_image_t const *img)
{
    return (uint32_t const*)(img + 1);
}

#define autofree_png __attribute__((cleanup(png_free)))

__END_DECLS
