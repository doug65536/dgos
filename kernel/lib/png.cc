#include "png.h"
#include "zlib_helper.h"
#include "fileio.h"
#include "stdlib.h"
#include "bswap.h"
#include "string.h"
#include "likely.h"
#include "assert.h"
#include "printk.h"

#define PNG_DEBUG 0
#if PNG_DEBUG
#define PNG_TRACE(...) printdbg("png: " __VA_ARGS__)
#else
#define PNG_TRACE(...) ((void)0)
#endif

static uint8_t png_sig[] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

typedef enum png_type_t {
    PNG_IHDR,
    PNG_PLTE,
    PNG_IDAT,
    PNG_IEND,
    PNG_OTHER
} png_type_t;

static char png_blk_types[][5] = {
    "IHDR",
    "PLTE",
    "IDAT",
    "IEND"
};

#define PNG_BUFSIZE (64<<10)

typedef struct png_chunk_hdr_t {
    uint32_t len;
    char type[4];
} __attribute__((packed)) png_chunk_hdr_t;

typedef uint32_t png_crc_t;

typedef struct png_hdr_ihdr_t {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compr_method;
    uint8_t filter_method;
    uint8_t interlace_method;
} __attribute__((packed)) png_hdr_ihdr_t;

#define PNG_IHDR_COLORTYPE_P_BIT    0
#define PNG_IHDR_COLORTYPE_C_BIT    1
#define PNG_IHDR_COLORTYPE_A_BIT    2

// Pixel format has a palette
#define PNG_IHDR_COLORTYPE_P        (1U<<PNG_IHDR_COLORTYPE_P_BIT)

// Pixel format has color
#define PNG_IHDR_COLORTYPE_C        (1U<<PNG_IHDR_COLORTYPE_C_BIT)

// Pixel format has alpha
#define PNG_IHDR_COLORTYPE_A        (1U<<PNG_IHDR_COLORTYPE_A_BIT)

// Luminance, no color, no palette, no alpha
#define PNG_IHDR_COLORTYPE_Y    0

// Color, no palette, no alpha
#define PNG_IHDR_COLOR_RGB  (PNG_IHDR_COLORTYPE_C)

// Palette, no alpha
#define PNG_IHDR_COLOR_I    (PNG_IHDR_COLORTYPE_C|PNG_IHDR_COLORTYPE_P)

// Luminance and alpha, no palette
#define PNG_IHDR_COLOR_YA   (PNG_IHDR_COLORTYPE_A)

// Color and alpha, no palette
#define PNG_IHDR_COLOR_RGBA (PNG_IHDR_COLORTYPE_C|PNG_IHDR_COLORTYPE_A)

C_ASSERT((sizeof(png_image_t) & 0xF) == 0);

typedef enum png_filter_type_t {
    PNG_FILTER_NONE,
    PNG_FILTER_SUB,
    PNG_FILTER_UP,
    PNG_FILTER_AVG,
    PNG_FILTER_PAETH,
    PNG_FILTER_UNSET,
} png_filter_type_t;

typedef struct png_read_state_t {
    png_image_t const *img;
    int32_t cur_row;
    png_filter_type_t cur_filter;
    intptr_t pixel_bytes;
    intptr_t scanline_bytes;
} png_read_state_t;

static png_type_t png_hdr_type(png_chunk_hdr_t *hdr)
{
    for (size_t i = 0; i < countof(png_blk_types); ++i)
        if (!memcmp(png_blk_types[i], &hdr->type, sizeof(hdr->type)))
            return (png_type_t)i;
    return PNG_OTHER;
}

static inline uint8_t png_paeth_predict(uint8_t a, uint8_t b, uint8_t c)
{
    // a = left, b = above, c = upper left
    int p = a + b - c;
    int pa = p <= a ? a - p : p - a;
    int pb = p <= b ? b - p : p - b;
    int pc = p <= c ? c - p : p - c;
    return pa <= pb && pa <= pc
            ? a : pb <= pc ? b : c;
}

static void png_fixup_bgra(void *p, size_t count)
{
    uint32_t *p32 = p;
    while (count--) {
        *p32 = (bswap_32(*p32) >> 8);
        ++p32;
    }
}

static uint32_t png_process_idata(
        png_read_state_t *state, uint8_t *data, uint32_t avail)
{
    size_t ofs = 0;

    uint8_t prev[4];
    uint8_t curr[4];

    PNG_TRACE("Processing %u bytes IDAT\n", avail);

    memset(prev, 0, sizeof(prev));

    uint8_t *out = (uint8_t*)(state->img + 1) +
            state->cur_row * state->scanline_bytes;

    for ( ; state->cur_row < state->img->height; ++state->cur_row) {
        // Done if there isn't a whole scanline
        if (ofs + 1 + state->scanline_bytes > avail)
            break;

        state->cur_filter = (png_filter_type_t)data[ofs++];

        if (state->cur_row == 0) {
            if (state->cur_filter == PNG_FILTER_PAETH ||
                 state->cur_filter == PNG_FILTER_AVG)
                state->cur_filter = PNG_FILTER_SUB;
            else if (state->cur_filter == PNG_FILTER_UP)
                state->cur_filter = PNG_FILTER_NONE;
        }

        PNG_TRACE("Filter type is %d\n", (int)state->cur_filter);

        switch (state->cur_filter) {
        case PNG_FILTER_NONE:
            for (int32_t i = 0; i < state->img->width; ++i) {
                for (int32_t c = 0; c < state->pixel_bytes; ++c)
                    out[c] = data[ofs + c];
                ofs += state->pixel_bytes;
                out += sizeof(curr);
            }
            break;

        case PNG_FILTER_SUB:
            for (int32_t i = 0; i < state->img->width; ++i) {
                for (int32_t c = 0; c < state->pixel_bytes; ++c)
                    out[c] = prev[c] = prev[c] + data[ofs + c];
                ofs += state->pixel_bytes;
                out += sizeof(curr);
            }
            break;

        case PNG_FILTER_UP:
            for (int32_t i = 0; i < state->img->width; ++i) {
                for (int32_t c = 0; c < state->pixel_bytes; ++c) {
                    uint8_t p = out[-state->scanline_bytes + c];
                    out[c] = p + data[ofs + c];
                }
                ofs += state->pixel_bytes;
                out += sizeof(curr);
            }
            break;

        case PNG_FILTER_AVG:
            for (int32_t c = 0; c < state->pixel_bytes; ++c) {
                uint8_t p = out[-state->scanline_bytes + c] >> 1;
                out[c] = p + data[ofs + c];
            }
            ofs += state->pixel_bytes;
            out += sizeof(curr);

            for (int32_t i = 1; i < state->img->width; ++i) {
                for (int32_t c = 0; c < state->pixel_bytes; ++c) {
                    uint8_t p = ((unsigned)out[-state->pixel_bytes + c] +
                            out[-state->scanline_bytes + c]) >> 1;
                    out[c] = p + data[ofs + c];
                }
                ofs += state->pixel_bytes;
                out += sizeof(curr);
            }
            break;

        case PNG_FILTER_PAETH:
            for (int32_t c = 0; c < state->pixel_bytes; ++c) {
                uint8_t p = png_paeth_predict(
                        0, out[-state->scanline_bytes + c], 0);
                out[c] = p + data[ofs + c];
            }
            ofs += state->pixel_bytes;
            out += sizeof(curr);

            for (int32_t i = 1; i < state->img->width; ++i) {
                for (int32_t c = 0; c < state->pixel_bytes; ++c) {
                    uint8_t p = png_paeth_predict(
                            out[-state->pixel_bytes + c],
                            out[-state->scanline_bytes + c],
                            out[-state->scanline_bytes -
                            state->pixel_bytes + c]);
                    out[c] = p + data[ofs + c];
                }
                ofs += state->pixel_bytes;
                out += sizeof(curr);
            }
            break;

        default:
        case PNG_FILTER_UNSET:
            assert(0);
            return ~0;
        }
    }

    assert(ofs <= avail);

    PNG_TRACE("Processed %zu bytes IDAT, %zu remain\n", ofs, avail - ofs);

    return ofs;
}

png_image_t *png_load(char const *path)
{
    // Allocate read buffer
    autofree uint8_t *buf = malloc(PNG_BUFSIZE);
    if (!buf)
        return 0;

    uint8_t * const read_buf = buf;
    uint8_t * const decomp_buf = buf + (PNG_BUFSIZE >> 1);

    // Open PNG file
    autoclose int fd = file_open(path);
    if (fd < 0)
        return 0;

    z_stream inf;
    zlib_init(&inf);
    inflateInit2(&inf, 15);

    // Read header
    if (sizeof(png_sig) != file_read(fd, buf, sizeof(png_sig)))
        return 0;

    // Check signature
    if (memcmp(buf, png_sig, sizeof(png_sig)))
        return 0;

    uint32_t input_level = 0;
    uint32_t decomp_level = 0;
    uint32_t consumed;
    uint32_t read_size;

    autofree png_image_t *img = 0;
    png_read_state_t state;
    state.cur_row = 0;
    state.cur_filter = PNG_FILTER_UNSET;
    state.scanline_bytes = 0;
    state.pixel_bytes = 0;
    state.img = 0;

    for (int done = 0; !done; ) {
        png_chunk_hdr_t hdr;

        if (sizeof(hdr) != file_read(fd, &hdr, sizeof(hdr)))
            return 0;

        PNG_TRACE("Encountered %4.4s chunk\n", hdr.type);

        hdr.len = htonl(hdr.len);

        png_hdr_ihdr_t ihdr;

        png_type_t type = png_hdr_type(&hdr);

        switch (type) {
        case PNG_IHDR:
            if (sizeof(ihdr) != file_read(fd, &ihdr, sizeof(ihdr)))
                return 0;

            ihdr.width = htonl(ihdr.width);
            ihdr.height = htonl(ihdr.height);

            file_seek(fd, sizeof(png_crc_t), SEEK_CUR);

            // Only method 0 is defined
            if (unlikely(ihdr.compr_method != 0))
                return 0;

            // Only filter 0 is defined
            if (unlikely(ihdr.filter_method != 0))
                return 0;

            // Only uninterlaced is supported
            if (unlikely(ihdr.interlace_method != 0))
                return 0;

            // Palette not supported (yet)
            if (unlikely(ihdr.color_type & PNG_IHDR_COLORTYPE_P))
                return 0;

            // Only 8, 16, 24, and 32 bit Y, YA, RGB, RGBA supported
            if (unlikely(ihdr.bit_depth != 8))
                return 0;

            state.pixel_bytes = ((3 & -!!(ihdr.color_type & PNG_IHDR_COLORTYPE_C)) +
                    !!(ihdr.color_type & PNG_IHDR_COLORTYPE_A)) *
                    (ihdr.bit_depth >> 3);
            state.scanline_bytes = state.pixel_bytes * ihdr.width;

            img = malloc(sizeof(*img) +
                         ihdr.width * ihdr.height * sizeof(uint32_t));

            img->width = ihdr.width;
            img->height = ihdr.height;
            state.img = img;

            PNG_TRACE("Loading %zdKB image \"%s\"\n",
                      (ihdr.width * ihdr.height * state.pixel_bytes) >> 10,
                      path);

            break;

        case PNG_IDAT:
            for (uint32_t ofs = 0;
                 ofs < hdr.len || input_level;
                 ofs += read_size) {
                // Try to refill the input buffer
                read_size = (PNG_BUFSIZE >> 1) - input_level;

                // Don't read past end of block
                if (ofs + read_size > hdr.len)
                    read_size = hdr.len - ofs;

                if (read_size) {
                    PNG_TRACE("Reading %u bytes from disk"
                              " to buffer offset %u\n",
                              read_size, input_level);
                    if (file_read(fd, read_buf + input_level,
                                  read_size) != read_size)
                        return 0;

                    input_level += read_size;
                }

                if (input_level) {
                    inf.avail_in = input_level;
                    inf.next_in = read_buf;
                    inf.next_out = decomp_buf + decomp_level;
                    inf.avail_out = (PNG_BUFSIZE >> 1) - decomp_level;
                    inf.total_out = 0;

                    int inf_status = inflate(&inf, Z_NO_FLUSH);

                    if (inf_status == Z_DATA_ERROR)
                        return 0;

                    // Slide input buffer
                    if (inf.avail_in > 0)
                        memmove(read_buf, inf.next_in, inf.avail_in);
                    input_level = inf.avail_in;

                    decomp_level += inf.total_out;
                }

                // Process some data
                consumed = png_process_idata(
                            &state, decomp_buf, decomp_level);

                if (consumed == ~0U)
                    return 0;

                decomp_level -= consumed;
                if (decomp_level) {
                    memmove(decomp_buf, decomp_buf + consumed,
                            decomp_level);
                }
            }

            file_seek(fd, sizeof(png_crc_t), SEEK_CUR);

            break;

        case PNG_IEND:
            assert(input_level == 0);
            assert(decomp_level == 0);
            done = 1;
            break;

        case PNG_OTHER:
        default:
            PNG_TRACE("Ignoring %4.4s chunk\n", hdr.type);
            file_seek(fd, hdr.len + sizeof(png_crc_t), SEEK_CUR);
            break;
        }
    }

    png_fixup_bgra(img + 1, img->width * img->height);

    // Release autofree
    png_image_t *result = img;
    img = 0;

    return result;
}

uint32_t const *png_pixels(png_image_t const *img)
{
    return (uint32_t const*)(img + 1);
}
