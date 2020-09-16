#include "pxemain.h"
#include "pxemain_abstract.h"
#include "screen.h"
#include "assert.h"
#include "string.h"
#include "cpu.h"
#include "gdt_sel_pxe.h"
#include "fs.h"
#include "malloc.h"
#include "physmem.h"
#include "elf64.h"
#include "halt.h"

__BEGIN_ANONYMOUS

class pxe_fragment_t {
    size_t offset;
    size_t length;
    void *data;
};

#if __SIZEOF_POINTER__ == 8
static constexpr size_t log2_radix_slots = 9;
#elif __SIZEOF_POINTER__ == 4
static constexpr size_t log2_radix_slots = 10;
#else
#error Unexpected pointer size
#endif
static constexpr size_t radix_slots = size_t(1) << log2_radix_slots;

class pxe_openfile_t;
#define MAX_OPEN_FILES  48
static pxe_openfile_t *pxe_open_files;
static int pxe_current_file = -1;

static void *bounce_buf;
static int bounce_buf_sz;

void pxe_end_current()
{
    if (pxe_current_file >= 0) {
        pxe_api_tftp_close();
        free(bounce_buf);
        bounce_buf = nullptr;
        bounce_buf_sz = 0;
    }
    pxe_current_file = -1;
}

class radix_tree_t {
public:
    void *lookup(uint64_t addr, bool commit_pages)
    {
        unsigned misalignment = addr & PAGE_MASK;
        addr -= misalignment;

        size_t ai = addr >> (PAGE_SIZE_BIT + (log2_radix_slots * 0));
        size_t bi = addr >> (PAGE_SIZE_BIT + (log2_radix_slots * 1));
        size_t ci = addr >> (PAGE_SIZE_BIT + (log2_radix_slots * 2));
        ai &= (radix_slots - 1);
        bi &= (radix_slots - 1);
        ci &= (radix_slots - 1);

        phys_alloc_t alloc;

        if (!commit_pages) {
            if (unlikely(!radix_tree ||
                         !radix_tree[ci] ||
                         !radix_tree[ci][bi] ||
                         !radix_tree[ci][bi][ai]))
                return nullptr;

            return (char*)radix_tree[ci][bi][ai] + misalignment;
        }

        if (unlikely(!radix_tree)) {
            alloc = alloc_phys(PAGE_SIZE);

            if (unlikely(!alloc.size))
                return nullptr;

            radix_tree = (void****)alloc.base;

            if (unlikely(!radix_tree))
                return nullptr;

            memset(radix_tree, 0, sizeof(*radix_tree) * radix_slots);
        }

        void ***&level1 = radix_tree[ci];

        if (unlikely(!level1)) {
            if (unlikely(!commit(level1)))
                return nullptr;
        }

        void **&level2 = level1[bi];

        if (unlikely(!level2)) {
            if (unlikely(!commit(level2)))
                return nullptr;
        }

        void *&level3 = level2[ai];

        if (!level3) {
            if (unlikely(!commit(level3)))
                return nullptr;
        }

        return (char*)level3 + misalignment;
    }

private:
    template<typename T>
    void *commit(T &p)
    {
        p = (T)alloc_phys(PAGE_SIZE).base;

        if (unlikely(!p))
            return nullptr;

        return memset(p, 0, sizeof(void*) * radix_slots);
    }

    // Points to a single page. 3-level radix tree.
    // Works the same as 3-level PAE x86 paging structure without the flag bits
    // At each level of indirection, the pointer points to a page of 512 or
    // 1024 entries for progressively less significant groups of 9 or 10 bits
    // of the key (64 bit or 32 bit pointers, respectively)
    // Handles key range from 0 to 512GB-1
    void ****radix_tree;
};

class pxe_openfile_t {
public:
    char *filename;

    radix_tree_t file_cache;

    // Size field is not updated until end of file is reached
    int64_t size;
    size_t received;
    int block_size;

    bool set_filename(char const *new_filename)
    {
        char *copy = strdup(new_filename);

        if (unlikely(!copy))
            return false;

        if (unlikely(filename))
            free(filename);

        filename = copy;

        return true;
    }

    static char const *drop_path(char const *filename)
    {
        char const *first_slash;

        while ((first_slash = strchr(filename, '/')) != nullptr)
            filename = first_slash + 1;

        return filename;
    }

    bool open(char const *filename)
    {
        filename = drop_path(filename);

        if (unlikely(!set_filename(filename)))
            return false;

        pxe_end_current();

        size = pxe_api_tftp_get_fsize(filename);

        block_size = pxe_api_tftp_open(filename, 1024);
        if (unlikely(block_size < 0))
            return false;

        return block_size > 0;
    }

    int close()
    {
        free(filename);
        filename = nullptr;

        return 0;
    }

    int pread(void *buf, size_t bytes, off_t ofs)
    {
        int64_t end = ofs + bytes;

        if (unlikely(!read_until(end)))
            return -1;

        int total = 0;

        while (bytes > 0) {
            void *cache = file_cache.lookup(ofs, false);

            // Should never happen
            if (unlikely(!cache))
                return -1;

            uintptr_t page_end = (uintptr_t(cache) + PAGE_SIZE) & -PAGE_SIZE;
            size_t xfer = page_end - uintptr_t(cache);

            if (unlikely(xfer > bytes))
                xfer = bytes;

            memcpy(buf, cache, xfer);

            buf = (char*)buf + xfer;
            bytes -= xfer;
            ofs += xfer;
            total += xfer;
        }

        return total;
    }

    bool read_until(int64_t offset)
    {
        if (unlikely(size >= 0 && offset > size))
            offset = size;

        while (received < offset) {
            if (unlikely(!bounce_buf)) {
                bounce_buf = malloc(block_size);
                if (!bounce_buf)
                    return false;
                bounce_buf_sz = block_size;
            }

            int got = pxe_api_tftp_read(bounce_buf, received / block_size + 1,
                                        block_size);
            if (unlikely(got < 0))
                return false;

            // Transfer bounce buffer into cache pages
            // without crossing page boundaries
            int64_t cached = 0;
            do {
                void *buffer = file_cache.lookup(received + cached, true);
                if (unlikely(!buffer))
                    return false;
                uintptr_t page_end = (uintptr_t(buffer) +
                                      PAGE_SIZE) & -PAGE_SIZE;
                size_t xfer = page_end - uintptr_t(buffer);
                if (unlikely(cached + xfer > got))
                    xfer = got - cached;

                memcpy(buffer, (char*)bounce_buf + cached, xfer);

                cached += xfer;
            } while (cached < got);

            received += got;

            if (likely(got < block_size)) {
                size = received;
                break;
            }
        }

        return true;
    }

    static pxe_openfile_t *lookup_file(int file)
    {
        if (likely(pxe_open_files &&
                     file >= 0 &&
                     file < MAX_OPEN_FILES &&
                     pxe_open_files[file].filename))
            return pxe_open_files + file;

        return nullptr;
    }

    static int alloc_file()
    {
        if (unlikely(!pxe_open_files))
            pxe_open_files = (pxe_openfile_t*)
                    calloc(MAX_OPEN_FILES, sizeof(*pxe_open_files));

        if (unlikely(!pxe_open_files))
            return -1;

        int file;
        for (file = 0; file < MAX_OPEN_FILES; ++file) {
            if (!pxe_open_files[file].filename)
                break;
        }

        return file < MAX_OPEN_FILES ? file : -1;
    }
};

static int pxe_boot_open(tchar const *filename)
{
    int file = pxe_openfile_t::alloc_file();

    if (unlikely(file < 0))
        return file;

    pxe_openfile_t &desc = pxe_open_files[file];

    if (unlikely(!desc.open(filename))) {
        if (pxe_current_file == file)
            pxe_end_current();

        desc.close();
        return -1;
    }

    pxe_current_file = file;

    return file;
}

static int pxe_boot_close(int file)
{
    if (unlikely(!pxe_open_files || file < 0 || file >= MAX_OPEN_FILES))
        return -1;

    pxe_openfile_t &desc = pxe_open_files[file];

    if (file == pxe_current_file)
        pxe_end_current();

    return desc.close();
}

static ssize_t pxe_boot_pread(int file, void *buf, size_t bytes, off_t ofs)
{
    pxe_openfile_t *desc = pxe_openfile_t::lookup_file(file);

    if (unlikely(!desc))
        return -1;

    return desc->pread(buf, bytes, ofs);
}

static uint64_t pxe_boot_drv_serial()
{
    return 0;
}

static off_t pxe_boot_filesize(int file)
{
    pxe_openfile_t *desc = pxe_openfile_t::lookup_file(file);

    if (unlikely(!desc))
        return -1;

    return desc->size;
}

__END_ANONYMOUS

void pxe_init_fs()
{
    pxe_init_tftp();

    fs_api.name = "direct_pxe_fs";
    fs_api.boot_open = pxe_boot_open;
    fs_api.boot_pread = pxe_boot_pread;
    fs_api.boot_close = pxe_boot_close;
    fs_api.boot_filesize = pxe_boot_filesize;
    fs_api.boot_drv_serial = pxe_boot_drv_serial;
}
