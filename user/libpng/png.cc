#include <png.h>
#include <byteswap.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <zlib.h>

#define C_ASSERT(expr) static_assert((expr), #expr)

#include <string.h>
#include <sys/likely.h>
#include <assert.h>

#define PNG_DEBUG 0
#if PNG_DEBUG
#define PNG_TRACE(...) printdbg("png: " __VA_ARGS__)
#else
#define PNG_TRACE(...) ((void)0)
#endif

#ifndef _packed
#define _packed __attribute__((__packed__))
#endif

static uint8_t png_sig[] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

typedef enum png_type_t {
    PNG_IHDR,
    PNG_PLTE,
    PNG_IDAT,
    PNG_IEND,
    PNG_OTHER
} png_type_t;

static char constexpr png_blk_types[][5] = {
    "IHDR",
    "PLTE",
    "IDAT",
    "IEND"
};

#define PNG_BUFSIZE ((64<<10)-_MALLOC_OVERHEAD)

struct png_chunk_hdr_t {
    uint32_t len;
    char type[4];
} _packed;

typedef uint32_t png_crc_t;

struct png_hdr_ihdr_t {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compr_method;
    uint8_t filter_method;
    uint8_t interlace_method;
} _packed;

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

//C_ASSERT((sizeof(png_image_t) & 0xF) == 0);

enum struct png_filter_type_t {
    NONE,
    SUB,
    UP,
    AVG,
    PAETH,
    UNSET,
};

struct png_read_state_t {
    surface_t const *img;
    int32_t cur_row;
    png_filter_type_t cur_filter;
    intptr_t pixel_bytes;
    intptr_t scanline_bytes;
};

template<typename T, size_t N> inline
constexpr size_t countof(T const (&)[N]) noexcept
{
    return N;
}

class file_t {
public:
    file_t() = default;
    file_t(int fd) : fd(fd) {}
    file_t(file_t&& rhs) { close(); fd = rhs.fd; rhs.fd = -1; }
    file_t(file_t const&) = delete;
    ~file_t() { close(); }
    file_t& operator=(file_t const&) = delete;
    file_t& operator=(int new_fd) { close(); fd = new_fd; return *this; }
    void close() { if (fd >= 0) ::close(fd); fd = -1; }
    int detach() noexcept { int r = fd; fd = -1; return r; }
    operator int() const noexcept { return fd; }
    operator bool() const noexcept { return is_open(); }
    bool is_open() const noexcept { return fd >= 0; }

    int fd = -1;
};

static png_type_t png_hdr_type(png_chunk_hdr_t *hdr)
{
    for (size_t i = 0; i < countof(png_blk_types); ++i)
        if (!memcmp(png_blk_types[i], &hdr->type, sizeof(hdr->type)))
            return (png_type_t)i;
    return PNG_OTHER;
}

static _always_inline uint8_t png_paeth_predict(
        uint8_t a, uint8_t b, uint8_t c)
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
    uint32_t *p32 = (uint32_t*)p;
    while (count--) {
        *p32 = (bswap_32(*p32) >> 8) | (*p32 & 0xFF000000U);
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

        state->cur_filter = png_filter_type_t(data[ofs++]);

        if (state->cur_row == 0) {
            if (state->cur_filter == png_filter_type_t::PAETH ||
                 state->cur_filter == png_filter_type_t::AVG)
                state->cur_filter = png_filter_type_t::SUB;
            else if (state->cur_filter == png_filter_type_t::UP)
                state->cur_filter = png_filter_type_t::NONE;
        }

        PNG_TRACE("Filter type is %d\n", (int)state->cur_filter);

        switch (state->cur_filter) {
        case png_filter_type_t::NONE:
            for (int32_t i = 0; i < state->img->width; ++i) {
                for (int32_t c = 0; c < state->pixel_bytes; ++c)
                    out[c] = data[ofs + c];
                ofs += state->pixel_bytes;
                out += sizeof(curr);
            }
            break;

        case png_filter_type_t::SUB:
            for (int32_t i = 0; i < state->img->width; ++i) {
                for (int32_t c = 0; c < state->pixel_bytes; ++c)
                    out[c] = prev[c] = prev[c] + data[ofs + c];
                ofs += state->pixel_bytes;
                out += sizeof(curr);
            }
            break;

        case png_filter_type_t::UP:
            for (int32_t i = 0; i < state->img->width; ++i) {
                for (int32_t c = 0; c < state->pixel_bytes; ++c) {
                    uint8_t p = out[-state->scanline_bytes + c];
                    out[c] = p + data[ofs + c];
                }
                ofs += state->pixel_bytes;
                out += sizeof(curr);
            }
            break;

        case png_filter_type_t::AVG:
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

        case png_filter_type_t::PAETH:
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
        case png_filter_type_t::UNSET:
            assert(0);
            return ~0;
        }
    }

    assert(ofs <= avail);

    PNG_TRACE("Processed %zu bytes IDAT, %zu remain\n", ofs, avail - ofs);

    return ofs;
}

static void *zlib_malloc(void *opaque, unsigned items, unsigned size)
{
    (void)opaque;
    return malloc(items * size);
}

static void zlib_free(void *opaque, void *p)
{
    (void)opaque;
    free(p);
}

surface_t *surface_from_png(char const *path)
{
    // Allocate read buffer
    // fixme, was unique_ptr
    uint8_t *buf = new uint8_t[PNG_BUFSIZE]();
    if (unlikely(!buf))
        return nullptr;

    uint8_t * const read_buf = buf;
    uint8_t * const decomp_buf = buf + (PNG_BUFSIZE >> 1);

    // Open PNG file
    file_t fd = openat(AT_FDCWD, path, O_RDONLY);

    if (unlikely(!fd.is_open()))
        return nullptr;

    z_stream inf;
    memset(&inf, 0, sizeof(inf));
    inf.zalloc = zlib_malloc;
    inf.zfree = zlib_free;

    inflateInit2(&inf, 15);

    // Read header
    if (unlikely(sizeof(png_sig) != read(fd, buf, sizeof(png_sig))))
        return nullptr;

    // Check signature
    if (unlikely(memcmp(buf, png_sig, sizeof(png_sig))))
        return nullptr;

    uint32_t input_level = 0;
    uint32_t decomp_level = 0;
    uint32_t consumed;
    int32_t read_size;

    // fixme, was unique_ptr
    surface_t *img;
    png_read_state_t state;
    state.cur_row = 0;
    state.cur_filter = png_filter_type_t::UNSET;
    state.scanline_bytes = 0;
    state.pixel_bytes = 0;
    state.img = nullptr;

    for (int done = 0; !done; ) {
        png_chunk_hdr_t hdr;

        if (sizeof(hdr) != read(fd, &hdr, sizeof(hdr)))
            return nullptr;

        PNG_TRACE("Encountered %4.4s chunk\n", hdr.type);

        hdr.len = htonl(hdr.len);

        png_hdr_ihdr_t ihdr;

        png_type_t type = png_hdr_type(&hdr);

        switch (type) {
        case PNG_IHDR:
            if (unlikely(sizeof(ihdr) != read(fd, &ihdr, sizeof(ihdr))))
                return nullptr;

            ihdr.width = htonl(ihdr.width);
            ihdr.height = htonl(ihdr.height);

            lseek(fd, sizeof(png_crc_t), SEEK_CUR);

            // Only method 0 is defined
            if (unlikely(ihdr.compr_method != 0))
                return nullptr;

            // Only filter 0 is defined
            if (unlikely(ihdr.filter_method != 0))
                return nullptr;

            // Only uninterlaced is supported
            if (unlikely(ihdr.interlace_method != 0))
                return nullptr;

            // Palette not supported (yet)
            if (unlikely(ihdr.color_type & PNG_IHDR_COLORTYPE_P))
                return nullptr;

            // Only 8, 16, 24, and 32 bit Y, YA, RGB, RGBA supported
            if (unlikely(ihdr.bit_depth != 8))
                return nullptr;

            state.pixel_bytes = ((3 & -!!(ihdr.color_type &
                                          PNG_IHDR_COLORTYPE_C)) +
                    !!(ihdr.color_type & PNG_IHDR_COLORTYPE_A)) *
                    (ihdr.bit_depth >> 3);
            state.scanline_bytes = state.pixel_bytes * ihdr.width;

            img = /*.reset*/((surface_t*)malloc(sizeof(*img) +
                         ihdr.width * ihdr.height * sizeof(uint32_t)));

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
                    if (read(fd, read_buf + input_level,
                             read_size) != read_size)
                        return nullptr;

                    input_level += read_size;
                }

                if (input_level) {
                    inf.avail_in = input_level;
                    inf.next_in = read_buf;
                    inf.next_out = decomp_buf + decomp_level;
                    inf.avail_out = (PNG_BUFSIZE >> 1) - decomp_level;
                    inf.total_out = 0;

                    int inf_status = inflate(&inf, Z_NO_FLUSH);

                    assert(inf_status != Z_DATA_ERROR);
                    if (inf_status == Z_DATA_ERROR)
                        return nullptr;

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
                    return nullptr;

                decomp_level -= consumed;
                if (decomp_level) {
                    memmove(decomp_buf, decomp_buf + consumed,
                            decomp_level);
                }
            }

            lseek(fd, sizeof(png_crc_t), SEEK_CUR);

            break;

        case PNG_IEND:
            assert(input_level == 0);
            assert(decomp_level == 0);
            done = 1;
            break;

        case PNG_OTHER:
        default:
            PNG_TRACE("Ignoring %4.4s chunk\n", hdr.type);
            lseek(fd, hdr.len + sizeof(png_crc_t), SEEK_CUR);
            break;
        }
    }

    png_fixup_bgra(img + 1, img->width * img->height);

    return img;//.release();
}

