#pragma once

#include <sys/cdefs.h>
#include <stdint.h>

struct surface_t {
    int32_t width;
    int32_t height;
    void *reserved;
};

void surface_free(surface_t *pp);
void surface_autofree(surface_t **pp);

static inline uint32_t const *surface_pixels(surface_t const *img)
{
    return (uint32_t const*)(img + 1);
}

#define autofree_png __attribute__((cleanup(png_free)))
