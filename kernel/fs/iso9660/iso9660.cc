#include "dev_storage.h"
#include "iso9660_decl.h"
#include "threadsync.h"
#include "bitsearch.h"
#include "mm.h"
#include "bswap.h"
#include "string.h"
#include "pool.h"
#include "bsearch.h"
#include "printk.h"
#include "vector.h"
#include "inttypes.h"
#include "cxxstring.h"
#include "user_mem.h"
#include "string.h"

struct iso9660_factory_t : public fs_factory_t {
public:
    iso9660_factory_t();
    fs_base_t *mount(fs_init_info_t *conn) override;
};

struct iso9660_fs_t final : public fs_base_ro_t {
    FS_BASE_IMPL

    struct file_handle_t : public fs_file_info_t {
        iso9660_fs_t *fs;
        iso9660_dir_ent_t *dirent;
        char *content;

        ino_t get_inode() const override
        {
            return fs->dirent_lba(dirent);
        }
    };

    struct dir_handle_t : public file_handle_t {
    };

    union handle_t {
        file_handle_t file;
        dir_handle_t dir;

        handle_t(std::true_type)
            : file()
        {
        }

        handle_t(std::false_type)
            : dir()
        {
        }
    };

    static pool_t<handle_t> handles;

    struct path_key_t {
        char const *name;
        size_t len;
        size_t parent;
    };

    enum dirent_flags_t : uint8_t {
        dirent_flag_hide = 0x01,
        dirent_flag_dir = 0x02,
        dirent_flag_associated = 0x04,
        dirent_flag_record_fmt = 0x08,
        dirent_flag_has_rwx = 0x10,
        dirent_flag_multi_extent = 0x80
    };

    _pure
    static uint64_t dirent_size(iso9660_dir_ent_t const *de);

    static uint64_t dirent_lba(iso9660_dir_ent_t const *de);

    static uint64_t pt_rec_lba(iso9660_pt_rec_t const *pt_rec);

    static int name_to_ascii(
            void *ascii_buf,
            char const *utf8);

    static size_t name_len_ascii(
            void *ascii_buf,
            size_t limit);

    static size_t name_len_utf16be(
            void *ascii_buf,
            size_t limit);

    static int name_to_utf16be(
            void *utf16be_buf,
            char const *utf8);

    static char *name_copy_ascii(
            char *out, void *in, size_t len);

    static char *name_copy_utf16be(
            char *out, void *in, size_t len);

    static uint32_t round_up(
            uint32_t n,
            uint8_t log2_size);

    uint32_t walk_pt(
            void (*cb)(uint32_t i,
                       iso9660_pt_rec_t *rec,
                       void *p),
            void *p);

    static void pt_fill(
            uint32_t i,
            iso9660_pt_rec_t *rec,
            void *p);

    static int name_compare_ascii(
            void const *name, size_t name_len,
            char const *find, size_t find_len);

    static int name_compare_utf16be(
            void const *name, size_t name_len,
            char const *find, size_t find_len);

    static int lookup_path_cmp_ascii(
            void const *v, void const *k, void *s);

    static int lookup_path_cmp_utf16be(
            void const *v, void const *k, void *s);

    iso9660_pt_rec_t *lookup_path(char const *path, size_t path_len);

    _pure
    void *lookup_sector(uint64_t lba);

    _pure
    size_t next_dirent(iso9660_dir_ent_t *dir, size_t ofs);

    iso9660_dir_ent_t *lookup_dirent(char const *pathname);

    static int mm_fault_handler(void *dev, void *addr,
            uint64_t offset, uint64_t length, bool read, bool flush);
    int mm_fault_handler(void *addr, uint64_t offset, uint64_t length,
                         bool read, bool flush);

    bool mount(fs_init_info_t *conn);

    storage_dev_base_t *drive = nullptr;

    // Partition range
    uint64_t lba_st = 0;
    uint64_t lba_len = 0;

    // Root
    uint32_t root_lba = 0;
    uint32_t root_bytes = 0;

    // Path table
    uint32_t pt_lba = 0;
    uint32_t pt_bytes = 0;

    ext::string serial;

    int (*name_convert)(void *encoded_buf,
                        char const *utf8);
    size_t (*name_len)(void *encoded_buf, size_t limit);
    int (*lookup_path_cmp)(void const *v,
                           void const *k,
                           void *s);
    int (*name_compare)(void const *name, size_t name_len,
                        char const *find, size_t find_len);
    char *(*name_copy)(char *out, void *in, size_t len);

    // Path table and path table lookup table
    iso9660_pt_rec_t *pt;
    iso9660_pt_rec_t **pt_ptrs;

    uint32_t pt_alloc_size;
    uint32_t pt_count;

    // From the drive
    uint32_t sector_size;

    // >= 2KB
    uint32_t block_size;

    // Device memory mapping
    char *mm_dev;

    // log2(sector_size)
    uint8_t sector_shift;

    // log2(num_sectors_per_block)
    uint8_t block_shift;
};

pool_t<iso9660_fs_t::handle_t> iso9660_fs_t::handles;
static std::vector<iso9660_fs_t*> iso9660_mounts;

uint64_t iso9660_fs_t::dirent_size(iso9660_dir_ent_t const *de)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (de->size_hi_le << 16) | de->size_lo_le;
#else
    return (de->size_hi_be << 16) | de->size_lo_be;
#endif
}

uint64_t iso9660_fs_t::dirent_lba(iso9660_dir_ent_t const *de)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (de->lba_hi_le << 16) | de->lba_lo_le;
#else
    return (de->lba_hi_be << 16) | de->lba_lo_be;
#endif
}

uint64_t iso9660_fs_t::pt_rec_lba(iso9660_pt_rec_t const *pt_rec)
{
    return (pt_rec->lba_hi << 16) | pt_rec->lba_lo;
}

size_t iso9660_fs_t::name_len_ascii(void *ascii_buf, size_t limit)
{
    char *np = (char*)memchr(ascii_buf, 0, limit);
    return np ? np - (char*)ascii_buf : limit;
}

int iso9660_fs_t::name_to_ascii(
        void *ascii_buf,
        char const *utf8)
{
    char *ascii = (char*)ascii_buf;
    int codepoint;
    int out = 0;
    char *lastdot = nullptr;

    for (int i = 0; *utf8 && i < ISO9660_MAX_NAME; ++i) {
        codepoint = utf8_to_ucs4_upd(utf8);

        if (codepoint == '.') {
            if (lastdot)
                *lastdot = '_';
            lastdot = ascii + out;
        }

        if (codepoint >= 'a' && codepoint <= 'z')
            codepoint += 'A' - 'a';

        if ((codepoint >= '0' && codepoint <= '9') ||
                (codepoint >= 'A' && codepoint <= 'Z') ||
                (codepoint == '.'))
            ascii[out++] = (char)codepoint;
        else
            ascii[out++] = '_';
    }

    return out;
}

int iso9660_fs_t::name_to_utf16be(
        void *utf16be_buf,
        char const *utf8)
{
    char16_t *utf16be = (char16_t*)utf16be_buf;
    int codepoint;
    char16_t utf16buf[3];
    int utf16sz;
    int out = 0;

    for (int i = 0; *utf8 && i < ISO9660_MAX_NAME; ++i) {
        codepoint = utf8_to_ucs4_upd(utf8);

        utf16sz = ucs4_to_utf16(utf16buf, codepoint);

        if (utf16sz > 0)
            utf16be[out++] = htons(utf16buf[0]);
        if (utf16sz > 1)
            utf16be[out++] = htons(utf16buf[1]);
    }

    return out;
}

size_t iso9660_fs_t::name_len_utf16be(void *buf, size_t limit)
{
    char *input = (char*)buf;
    for (size_t i = 0; i < limit; i += 2) {
        if (input[i] == 0 && input[i + 1] == 0)
            return i >> 1;
    }
    return limit;
}

char *iso9660_fs_t::name_copy_ascii(
        char *out, void *in, size_t len)
{
    memcpy(out, in, len);
    out[len] = 0;
    return out + len;
}

char *iso9660_fs_t::name_copy_utf16be(
        char *out, void *in, size_t len)
{
    char16_t const *name = (char16_t const*)in;
    size_t name_len = len >> 1;
    char16_t const *name_end = name + name_len;

    while (name < name_end) {
        int codepoint = utf16be_to_ucs4(name, &name);
        out += ucs4_to_utf8(out, codepoint);
    }
    *out = 0;

    return out;
}

uint32_t iso9660_fs_t::round_up(
        uint32_t n,
        uint8_t log2_size)
{
    uint32_t size = 1U << log2_size;
    uint32_t mask = size - 1;
    return (n + mask) & ~mask;
}

uint32_t iso9660_fs_t::walk_pt(
        void (*cb)(uint32_t i,
                   iso9660_pt_rec_t *rec,
                   void *p),
        void *p)
{
    uint32_t i = 0;
    for (iso9660_pt_rec_t *pt_rec = pt,
         *pt_end = (iso9660_pt_rec_t*)((char*)pt + pt_bytes);
         pt_rec < pt_end;
         ++i,
         pt_rec = (iso9660_pt_rec_t*)((char*)pt_rec +
                          ((offsetof(iso9660_pt_rec_t, name) +
                          pt_rec->di_len + 1) & -2))) {
        if (cb)
            cb(i, pt_rec, p);
    }
    return i;
}

void iso9660_fs_t::pt_fill(
        uint32_t i,
        iso9660_pt_rec_t *rec,
        void *p)
{
    iso9660_fs_t *self = (iso9660_fs_t *)p;

    if (self)
        self->pt_ptrs[i] = rec;
}

int iso9660_fs_t::name_compare_ascii(
        void const *name, size_t name_len,
        char const *find, size_t find_len)
{
    char const *chk = (char const *)name;
    char const *find_limit = (char const *)memrchr(find, '.', find_len);
    char const *chk_limit = (char const *)memrchr(chk, '.', name_len);
    char const *find_end = find + find_len;
    char const *chk_end = (char const *)memrchr(chk, ';', name_len);

    // ISO9660 uses a goofy DOS-like algorithm where the
    // shorter name is space-padded to the length of the
    // longer name, and the extension is space-padded
    // to the length of the longer extension

    int cmp = 0;

    for (int pass = 0; pass < 2; ++pass) {
        while (find < find_limit || chk < chk_limit) {
            char32_t key_codepoint = find < find_limit
                    ? utf8_to_ucs4_upd(find)
                    : ' ';
            char32_t chk_codepoint = chk < chk_limit
                    ? *chk
                    : ' ';

//            printdbg("utf8: compare %c (0x%02x) %c (0x%02x)\n",
//                     !key_codepoint
//                     ? '0'
//                     : key_codepoint >= ' ' && key_codepoint < 126
//                     ? key_codepoint
//                     : '.',
//                     key_codepoint,
//                     !chk_codepoint
//                     ? '0'
//                     : chk_codepoint >= ' ' && chk_codepoint < 126
//                     ? chk_codepoint
//                     : '.',
//                     chk_codepoint);

            cmp = chk_codepoint - key_codepoint;

            if (cmp)
                return cmp;
        }

        find_limit = find_end;
        chk_limit = chk_end;
    }

    return cmp;
}

int iso9660_fs_t::name_compare_utf16be(
        void const *name, size_t name_len,
        char const *find, size_t find_len)
{
    char const *find_end = find + find_len;
    char16_t const *chk = (char16_t const *)name;
    char16_t const *chk_end = chk + (name_len >> 1);

    int cmp = 0;

    while ((cmp == 0) && (find < find_end || chk < chk_end)) {
        char32_t key_codepoint =
                find < find_end
                ? utf8_to_ucs4_upd(find)
                : 0;

        char32_t chk_codepoint =
                chk < chk_end
                ? utf16be_to_ucs4(chk, &chk)
                : 0;

//        printdbg("utf16be: compare %c (0x%02x) %c (0x%02x)\n",
//                 !key_codepoint
//                 ? '0'
//                 : key_codepoint >= ' ' && key_codepoint < 126
//                 ? key_codepoint
//                 : '.',
//                 key_codepoint,
//                 !chk_codepoint
//                 ? '0'
//                 : chk_codepoint >= ' ' && chk_codepoint < 126
//                 ? chk_codepoint
//                 : '.',
//                 chk_codepoint);

        cmp = key_codepoint - chk_codepoint;
    }

    return cmp;
}

int iso9660_fs_t::lookup_path_cmp_ascii(
        void const *v, void const *k, void *s)
{
    (void)s;
    iso9660_pt_rec_t const *rec = *(iso9660_pt_rec_t const **)v;
    path_key_t const *key = (path_key_t*)k;

    if (rec->parent_dn != key->parent)
        return key->parent - rec->parent_dn;

    size_t name_len = (rec->di_len - offsetof(
                iso9660_pt_rec_t, name));

    return name_compare_ascii(
                rec->name, name_len,
                key->name, key->len);
}

int iso9660_fs_t::lookup_path_cmp_utf16be(
        void const *v, void const *k, void *s)
{
    (void)s;
    iso9660_pt_rec_t const *rec = *(iso9660_pt_rec_t const**)v;
    path_key_t const *key = (path_key_t*)k;

    if (rec->parent_dn != key->parent)
        return key->parent - rec->parent_dn;

    return name_compare_utf16be(
                rec->name, rec->di_len,
                key->name, key->len);
}

iso9660_pt_rec_t *iso9660_fs_t::lookup_path(char const *path, size_t path_len)
{
    path_key_t key;

    if (path_len == -size_t(1))
        path_len = strlen(path);

    key.parent = 1;
    key.name = path;

    intptr_t match = -1;
    int done;
    do {
        char *sep = (char*)memchr(key.name, '/', path_len);

        done = !sep;

        if (!done)
            key.len = sep - key.name;
        else
            break;

        intptr_t next_match = binary_search(
                    pt_ptrs, pt_count,
                    sizeof(*pt_ptrs), &key,
                    lookup_path_cmp, this, 1);

        if (next_match < 0)
            break;

        match = next_match;

        key.parent = match + 1;
        key.name += key.len + 1;
    } while(!done);

    return match >= 0 ? pt_ptrs[match] : nullptr;
}

void *iso9660_fs_t::lookup_sector(uint64_t lba)
{
    return mm_dev + (lba << sector_shift);
}

size_t iso9660_fs_t::next_dirent(iso9660_dir_ent_t *dir, size_t ofs)
{
    iso9660_dir_ent_t *de = (iso9660_dir_ent_t*)((char*)dir + ofs);
    if (de->len != 0)
        return ofs + ((de->len + 1) & -2);
    return (ofs + (sector_size - 1)) & -(int)sector_size;
}

iso9660_dir_ent_t *iso9660_fs_t::lookup_dirent(char const *pathname)
{
    // Get length of whole path
    size_t pathname_len = strlen(pathname);

    // Find last path separator
    char const *sep = (char*)memrchr(pathname, '/', pathname_len);

    size_t path_len;
    size_t name_len;
    char const *name;

    if (sep) {
        path_len = sep - pathname;
        name = sep + 1;
        name_len = pathname_len - path_len - 1;
    } else {
        path_len = 0;
        name = pathname;
        name_len = pathname_len;
    }

    iso9660_pt_rec_t *pt_rec;

    if (path_len > 0) {
        pt_rec = lookup_path(pathname, pathname_len);
    } else {
        pt_rec = pt_ptrs[0];
    }

    if (unlikely(!pt_rec))
        return nullptr;

    iso9660_dir_ent_t *dir = (iso9660_dir_ent_t *)
            lookup_sector(pt_rec_lba(pt_rec));

    if (unlikely(!dir))
        return nullptr;

    size_t dir_len = dirent_size(dir);

    iso9660_dir_ent_t *result = nullptr;
    for (size_t ofs = 0; ofs < dir_len; ofs = next_dirent(dir, ofs)) {
        iso9660_dir_ent_t *de = (iso9660_dir_ent_t*)((char*)dir + ofs);

        int cmp = name_compare(de->name, de->filename_len, name, name_len);
        if (cmp == 0) {
            result = de;
            break;
        }
    }

    return result;
}

int iso9660_fs_t::mm_fault_handler(
        void *dev, void *addr, uint64_t offset, uint64_t length,
        bool read, bool)
{
    FS_DEV_PTR(iso9660_fs_t, dev);
    return self->mm_fault_handler(addr, offset, length, read, false);
}

int iso9660_fs_t::mm_fault_handler(void *addr, uint64_t offset, uint64_t length,
                                   bool read, bool)
{
    if (unlikely(!read))
        return -int(errno_t::EROFS);

    uint32_t sector_offset = (offset >> sector_shift);
    uint64_t lba = lba_st + sector_offset;

    printdbg("Demand paging LBA %" PRId64 " at addr %p\n", lba, addr);

    return drive->read_blocks(addr, length >> sector_shift, lba);
}

//
// Startup and shutdown

iso9660_factory_t::iso9660_factory_t()
    : fs_factory_t("iso9660")
{
    fs_register_factory(this);
}

fs_base_t *iso9660_factory_t::mount(fs_init_info_t *conn)
{
    if (iso9660_mounts.empty())
        iso9660_fs_t::handles.create(512);

    std::unique_ptr<iso9660_fs_t> self(new (ext::nothrow) iso9660_fs_t);
    if (self->mount(conn)) {
        if (unlikely(!iso9660_mounts.push_back(self)))
            return nullptr;
        return self.release();
    }

    return nullptr;
}

bool iso9660_fs_t::mount(fs_init_info_t *conn)
{
    drive = conn->drive;

    sector_size = drive->info(STORAGE_INFO_BLOCKSIZE);

    sector_shift = bit_log2(sector_size);
    block_shift = 11 - sector_shift;
    block_size = sector_size << block_shift;

    iso9660_pvd_t pvd;
    uint32_t best_ofs = 0;

    name_convert = name_to_ascii;
    name_len = name_len_ascii;
    lookup_path_cmp = lookup_path_cmp_ascii;
    name_compare = name_compare_ascii;
    name_copy = name_copy_ascii;

    for (uint32_t ofs = 0; ofs < 4; ++ofs) {
        // Read logical block 16
        int err = drive->read_blocks(&pvd,
                    1 << block_shift,
                    (16 + ofs) << block_shift);

        if (err < 0)
            continue;

        if (pvd.type_code == 2) {
            // Prefer joliet pvd
            name_convert = name_to_utf16be;
            name_len = name_len_utf16be;
            lookup_path_cmp = lookup_path_cmp_utf16be;
            name_compare = name_compare_utf16be;
            name_copy = name_copy_utf16be;
            best_ofs = ofs;
            break;
        }
    }

    if (best_ofs == 0) {
        // We didn't find Joliet PVD, reread first one
        int err = drive->read_blocks(&pvd, 1 << block_shift,
                           16 << block_shift);

        if (err < 0)
            return false;
    }

    serial.resize(sizeof(pvd.app_id));

    char *serial_end = name_copy(
                serial.data(), pvd.app_id, sizeof(pvd.app_id));

    serial.resize(serial_end - serial.data());

    while (!serial.empty() && serial.back() == ' ')
        serial.pop_back();

    root_lba = dirent_lba(&pvd.root_dirent);

    root_bytes = dirent_size(&pvd.root_dirent);

    pt_lba = pvd.path_table_le_lba;
    pt_bytes = pvd.path_table_bytes.le;

    pt_alloc_size = round_up(pt_bytes, sector_shift);

    pt = (iso9660_pt_rec_t *)mmap(
                nullptr, pt_alloc_size, PROT_READ | PROT_WRITE,
                MAP_POPULATE);
    if (pt == MAP_FAILED)
        return false;

    int err = drive->read_blocks(pt, pt_alloc_size >> sector_shift, pt_lba);
    if (err < 0)
        return false;

    // Count the path table entries
    pt_count = walk_pt(nullptr, nullptr);

    // Allocate path table entry pointer array
    pt_ptrs = (iso9660_pt_rec_t **)mmap(
                nullptr, sizeof(*pt_ptrs) * pt_count,
                PROT_READ | PROT_WRITE, 0);
    if (pt_ptrs == MAP_FAILED)
        return false;

    // Populate path table entry pointer array
    walk_pt(&iso9660_fs_t::pt_fill, this);

    lba_st = conn->part_st;
    lba_len = conn->part_len;

    mm_dev = (char*)mmap_register_device(
                this, block_size, conn->part_len,
                PROT_READ, mm_fault_handler);
    if (!mm_dev)
        return false;

    return true;
}

char const *iso9660_fs_t::name() const noexcept
{
    return "iso9660";
}

void iso9660_fs_t::unmount()
{
    munmap(pt, pt_bytes);
    pt = nullptr;

    munmap(pt_ptrs, sizeof(*pt_ptrs) * pt_count);
    pt_ptrs = nullptr;
}

bool iso9660_fs_t::is_boot() const
{
    return serial && !strcmp(serial.c_str(),
                             "ea870ef2-2483-11e8-9bba-3f1a71a07f83");
}

int iso9660_fs_t::resolve(fs_file_info_t *dirfi, fs_cpath_t path,
                          size_t &consumed)
{
    return -1;
}

//
// Read directory entry information

int iso9660_fs_t::getattrat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        fs_stat_t* stbuf)
{
    iso9660_dir_ent_t *de = lookup_dirent(path);

    // Device ID of device containing file.
    stbuf->st_dev = 0;

    // File serial number.
    stbuf->st_ino = 0;

    // Mode of file
    stbuf->st_mode = 0;

    // Number of hard links to the file
    stbuf->st_nlink = 0;

    // User ID of file
    stbuf->st_uid = 0;

    // Group ID of file
    stbuf->st_gid = 0;

    // Device ID (if file is character or block special)
    stbuf->st_rdev = 0;

    // For regular files, the file size in bytes.
    // For symbolic links, the length in bytes of the
    // pathname contained in the symbolic link.
    // For other file types, the use of this field is
    // unspecified.
    stbuf->st_size = dirent_size(de);

    // Last data modification timestamp
    stbuf->st_mtime = 0;
    // Last file status change timestamp
    stbuf->st_ctime = 0;

    // A file system-specific preferred I/O block size
    // for this object. In some file system types, this
    // may vary from file to file.
    stbuf->st_blksize = 0;

    // Number of blocks allocated for this object.
    stbuf->st_blocks = dirent_size(de) >> (sector_shift + block_shift);

    return 0;
}

int iso9660_fs_t::accessat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        int mask)
{
    (void)path;
    (void)mask;
    return 0;
}

int iso9660_fs_t::readlinkat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        char* buf,
        size_t size)
{
    (void)path;
    (void)buf;
    (void)size;
    return 0;
}

//
// Scan directories

int iso9660_fs_t::opendirat(fs_file_info_t **fi,
                            fs_file_info_t *dirfi, fs_cpath_t path)
{
    iso9660_pt_rec_t *ptrec = lookup_path(path, -1);

    if (unlikely(!ptrec))
        return -int(errno_t::ENOENT);

    dir_handle_t *dir = (dir_handle_t*)handles.alloc(std::false_type());

    if (unlikely(!dir))
        return -int(errno_t::EMFILE);

    dir->fs = this;
    dir->dirent = (iso9660_dir_ent_t *)lookup_sector(pt_rec_lba(ptrec));

    if (unlikely(!(dir->dirent->flags & dirent_flag_dir))) {
        handles.free(dir);
        return -int(errno_t::ENOTDIR);
    }

    dir->content = (char *)dir->dirent;
    *fi = dir;

    return 0;
}

ssize_t iso9660_fs_t::readdir(
        fs_file_info_t *fi,
        dirent_t *buf, off_t offset)
{
    dir_handle_t *dir = (dir_handle_t*)fi;

    ssize_t dir_size = (ssize_t)dirent_size(dir->dirent);

    if (offset >= dir_size)
        return 0;

    size_t orig_offset = offset;

    iso9660_dir_ent_t *fe = (iso9660_dir_ent_t *)dir->content;
    iso9660_dir_ent_t *de = (iso9660_dir_ent_t *)(dir->content + offset);

    if (de->len == 0) {
        // Next entry would cross a sector boundary
        offset = next_dirent(fe, offset);
        de = (iso9660_dir_ent_t*)(dir->content + offset);

        if (offset >= dir_size)
            return 0;
    }

    name_copy(buf->d_name, de->name, de->filename_len);

    offset = next_dirent(fe, offset);

    return offset - orig_offset;
}

int iso9660_fs_t::releasedir(fs_file_info_t *fi)
{
    handles.free(&((handle_t*)fi)->dir);
    return 0;
}

//
// Open/close files

int iso9660_fs_t::openat(fs_file_info_t **fi,
                         fs_file_info_t *dirfi, fs_cpath_t path,
                         int flags, mode_t)
{
    if (unlikely((flags & O_CREAT) | ((flags & O_RDWR) == O_WRONLY)))
        return -int(errno_t::EROFS);

    file_handle_t *file = (file_handle_t *)handles.alloc(std::true_type());
    *fi = file;

    file->fs = this;

    file->dirent = lookup_dirent(path + (path[0] == '/'));

    if (!file->dirent)
        return -int(errno_t::ENOENT);

    uint64_t lba = dirent_lba(file->dirent);

    file->content = (char*)lookup_sector(lba);

    return 0;
}

int iso9660_fs_t::release(fs_file_info_t *fi)
{
    handles.free(&((handle_t*)fi)->file);
    return 0;
}


//
// Read/write files

ssize_t iso9660_fs_t::read(fs_file_info_t *fi,
                            char *buf,
                            size_t size,
                            off_t offset)
{
    file_handle_t *file = (file_handle_t *)fi;

    size_t file_size = dirent_size(file->dirent);

    if (offset < 0)
        return -1;

    if ((size_t)offset >= file_size)
        return 0;

    if (offset + size > file_size)
        size = file_size - offset;

    if (mm_is_user_range(buf, size)) {
        if (unlikely(!mm_copy_user(buf, file->content + offset, size)))
            return -int(errno_t::EFAULT);
    } else
        memcpy(buf, file->content + offset, size);

    return size;
}

//
// Query open file

int iso9660_fs_t::fstat(fs_file_info_t *fi,
                         fs_stat_t *st)
{
    file_handle_t *file = (file_handle_t *)fi;

    size_t file_size = dirent_size(file->dirent);

    memset(st, 0, sizeof(*st));
    // FIXME: fill in more fields
    st->st_size = file_size;

    return 0;
}

//
// Get filesystem information

int iso9660_fs_t::statfs(fs_statvfs_t* stbuf)
{
    // Filesystem block size
    stbuf->f_bsize = 1UL << (sector_shift + block_shift);

    // Fragment size
    stbuf->f_frsize = stbuf->f_bsize;

    // Size of filesystem in f_frsize units
    stbuf->f_blocks = lba_len;

    // free blocks
    stbuf->f_bfree = 0;

    // free blocks for unprivileged users
    stbuf->f_bavail = 0;

    // inodes
    stbuf->f_files = 0;

    // Free inodes
    stbuf->f_ffree = 0;

    // Free inodes for unprivileged users
    stbuf->f_favail = 0;

    // Filesystem ID
    stbuf->f_fsid = 0xCD;

    // Mount flags
    stbuf->f_flag = 1;//ST_RDONLY;

    // Maximum filename length
    stbuf->f_namemax = ISO9660_MAX_NAME;

    return 0;
}

//
// lock/unlock file

int iso9660_fs_t::lock(fs_file_info_t *fi,
                        int cmd,
                        fs_flock_t* locks)
{
    (void)fi;
    (void)cmd;
    (void)locks;
    return 0;
}

//
// Get block map

int iso9660_fs_t::bmapat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        size_t blocksize,
        uint64_t* blockno)
{
    (void)path;
    (void)blocksize;
    (void)blockno;
    // No idea what this does
    return -1;
}

//
// Read/Write/Enumerate extended attributes

int iso9660_fs_t::getxattrat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        char const* name,
        char* value,
        size_t size)
{
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    return 0;
}

int iso9660_fs_t::listxattrat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        char const* list,
        size_t size)
{
    (void)path;
    (void)list;
    (void)size;
    return 0;
}

//
// ioctl API

int iso9660_fs_t::ioctl(fs_file_info_t *fi,
                         int cmd,
                         void* arg,
                         unsigned int flags,
                         void* data)
{
    (void)cmd;
    (void)arg;
    (void)fi;
    (void)flags;
    (void)data;
    return 0;
}

//
//

int iso9660_fs_t::poll(fs_file_info_t *fi,
                        fs_pollhandle_t* ph,
                        unsigned* reventsp)
{
    (void)fi;
    (void)ph;
    (void)reventsp;
    return 0;
}

static iso9660_factory_t iso9660_factory;
