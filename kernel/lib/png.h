#pragma once
#include "types.h"

typedef struct png_image_t png_image_t;

png_image_t *png_load(char const *path);
void png_free(png_image_t *pp);
void png_autofree(png_image_t **pp);
uint32_t *png_pixels(png_image_t *img);

#define autofree_png __attribute__((cleanup(png_free)))
