#pragma once

#include <sys/cdefs.h>
#include <stdint.h>

struct surface_t {
    int32_t width;
    int32_t height;
    void *reserved[1];
};

void surface_free(surface_t *pp);
void surface_autofree(surface_t **pp);

static inline uint32_t const *surface_pixels(surface_t const *s)
{
    return (uint32_t const*)(s + 1);
}

#define autofree_png __attribute__((__cleanup__(png_free)))
