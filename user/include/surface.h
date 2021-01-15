#pragma once

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/likely.h>
#include <sys/framebuffer.h>

struct surface_t {
    int32_t width;
    int32_t height;
    uint32_t *pixels;

    surface_t(int32_t width, int32_t height, uint32_t *pixels)
        : width(width)
        , height(height)
        , pixels(pixels)
    {
    }

    surface_t(int32_t width, int32_t height)
        : surface_t(width, height, (uint32_t*)(this + 1))
    {
    }

    void *operator new(size_t type_size, int32_t width, int32_t height) throw()
    {
        size_t sz = type_size + width * height * sizeof(uint32_t);

        void *mem = malloc(sz);

        if (unlikely(!mem))
            return nullptr;

        return mem;
    }

    void operator delete(void *p)
    {
        free(p);
    }

    void fill(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey,
              uint32_t color);
};

surface_t *surface_create(int32_t width, int32_t height);
void surface_free(surface_t *pp);
void surface_autofree(surface_t **pp);

static inline uint32_t const *surface_pixels(surface_t const *s)
{
    return s->pixels;
}

#define autofree_png __attribute__((__cleanup__(png_free)))

class vga_console_t : public surface_t {
public:
    vga_console_t(int32_t width, int32_t height, uint32_t *pixels)
        : surface_t(width, height, pixels)
    {
    }

    vga_console_t(int32_t width, int32_t height)
        : surface_t(width, height)
    {
    }

    virtual void reset() = 0;
    virtual void new_line(uint32_t color) = 0;
    virtual void write(char const *data, size_t size,
                       uint32_t bg, uint32_t fg) = 0;
    virtual void render(fb_info_t *fb, int dx, int dy, int dw, int dh) = 0;
};

class vga_console_factory_t {
public:
    static vga_console_t *create(int32_t w, int32_t h);
};
