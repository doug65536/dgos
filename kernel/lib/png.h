#pragma once
#include "types.h"

struct png_image_t {
    int32_t width;
    int32_t height;
    void *reserved;
};

png_image_t *png_load(char const *path);
void png_free(png_image_t *pp);
void png_autofree(png_image_t **pp);
uint32_t const *png_pixels(png_image_t const *img);

#define autofree_png __attribute__((cleanup(png_free)))
