#include "dev_storage.h"
#include "stdlib.h"
#include "printk.h"
#include "string.h"
#include "mm.h"
#include "bitsearch.h"
#include "pool.h"
#include "fat32_decl.h"
#include "unique_ptr.h"
#include "threadsync.h"
#include "time.h"
#include "vector.h"

#define DEBUG_FAT32 1
#if DEBUG_FAT32
#define FAT32_TRACE(...) printdbg("fat32: " __VA_ARGS__)
#else
#define FAT32_TRACE(...) ((void)0)
#endif

typedef int32_t cluster_t;

struct fat32_fs_t : public fs_base_t {
    FS_BASE_IMPL

    fat32_fs_t();

    friend class fat32_factory_t;

    struct full_lfn_t {
        fat32_dir_union_t fragments[(256 + 12) / 13 + 1];
        uint8_t lfn_entry_count;
    };

    struct file_handle_t : public fs_file_info_t {
        fat32_fs_t *fs;
        fat32_dir_entry_t *dirent;

        off_t cached_offset;
        uint64_t cached_cluster;
        bool dirty;
    };

    fs_base_t *mount(fs_init_info_t *conn);

    static int mm_fault_handler(void *dev, void *addr,
            uint64_t offset, uint64_t length, bool read, bool flush);

    void *lookup_sector(uint64_t lba);

    uint64_t offsetof_cluster(uint64_t cluster);

    void *lookup_cluster(uint64_t cluster);

    static cluster_t dirent_start_cluster(fat32_dir_entry_t const *de);
    static void dirent_start_cluster(fat32_dir_entry_t *de, cluster_t cluster);

    static uint8_t lfn_checksum(char const *fcb_name);

    static void fcbname_from_lfn(
            char *fcbname, uint16_t const *lfn, size_t lfn_len);

    static void dirents_from_name(full_lfn_t *full,
                                  char const *pathname, size_t name_len);

    static size_t name_from_dirents(char *pathname,
                                    full_lfn_t const *full);

    static bool is_eof(cluster_t cluster);
    static bool is_free(cluster_t cluster);
    static bool is_dirent_free(fat32_dir_union_t const *de);

    int32_t extend_fat_chain(cluster_t *clusters,
                             int32_t count, cluster_t cluster);
    cluster_t allocate_near(cluster_t cluster);
    int commit_fat_extend(cluster_t *clusters, int32_t count);

    // Iterate the directory, and if extend_cluster is not null, then
    // store the appended cluster number there if we fall off the end
    // of the fat chain of the directory
    // Callback signature: bool(fat32_dir_union_t* de,
    //                          cluster_t dir_cluster, bool new_cluster)
    template<typename F>
    void iterate_dir(cluster_t cluster, F callback,
                     cluster_t *extend_cluster = nullptr);

    fat32_dir_union_t *search_dir(cluster_t cluster,
            char const *filename, size_t name_len);

    off_t walk_cluster_chain(file_handle_t *file, off_t offset, bool append);

    fat32_dir_union_t *lookup_dirent(char const *pathname,
                                     fat32_dir_union_t **dde);

    fat32_dir_union_t *create_dirent(char const *pathname,
                                     fat32_dir_union_t *dde, mode_t mode);

    fat32_dir_union_t *dirent_create(fat32_dir_union_t *dde,
                                     full_lfn_t const& lfn);

    int change_dirent_start(fat32_dir_union_t *dde, cluster_t start);

    int sync_fat_entry(cluster_t cluster);

    cluster_t transact_cluster(cluster_t prev_cluster,
                              fat32_dir_union_t *dde, cluster_t cluster);

    static void date_encode(uint16_t *date_field, uint16_t *time_field,
                            uint8_t *centisec_field, time_of_day_t tod);

    file_handle_t *create_handle(char const *path, int flags, mode_t mode);

    ssize_t internal_rw(file_handle_t *file,
            void *buf, size_t size, off_t offset, bool read);

    rwlock_t rwlock;

    storage_dev_base_t *drive;

    // Device memory mapping
    char *mm_dev;

    // Partition range
    uint64_t lba_st;
    uint64_t lba_en;

    cluster_t *fat;
    cluster_t *fat2;
    int32_t fat_size;

    cluster_t root_cluster;
    cluster_t cluster_ofs;
    cluster_t end_cluster;

    uint32_t block_size;

    uint32_t sector_size;
    uint8_t sector_shift;
    uint8_t block_shift;
    uint8_t fat_block_shift;

    // Synthetic root directory entry
    // to allow code to refer to root as a
    // directory entry
    fat32_dir_union_t root_dirent;
};

class fat32_factory_t : public fs_factory_t {
public:
    fat32_factory_t() : fs_factory_t("fat32") {}
    fs_base_t *mount(fs_init_info_t *conn);
};

static fat32_factory_t fat32_factory;
STORAGE_REGISTER_FACTORY(fat32);

static vector<fat32_fs_t*> fat32_mounts;

static pool_t fat32_handles;

static constexpr uint8_t dirent_size_shift =
        bit_log2_n(sizeof(fat32_dir_union_t));

int fat32_fs_t::mm_fault_handler(void *dev, void *addr,
                                 uint64_t offset, uint64_t length,
                                 bool read, bool flush)
{
    FS_DEV_PTR(fat32_fs_t, dev);

    uint64_t sector_offset = (offset >> self->sector_shift);
    uint64_t lba = self->lba_st + sector_offset;

    if (likely(read)) {
        printdbg("Demand paging LBA %ld at addr %p\n", lba, (void*)addr);

        return self->drive->read_blocks(
                    addr, length >> self->sector_shift, lba);
    }

    printdbg("Writing back LBA %ld at addr %p\n", lba, (void*)addr);
    int result = self->drive->write_blocks(
                addr, length >> self->sector_shift, lba, flush);

    return result;
}

void *fat32_fs_t::lookup_sector(uint64_t lba)
{
    return mm_dev + (lba << sector_shift);
}

uint64_t fat32_fs_t::offsetof_cluster(uint64_t cluster)
{
    assert(cluster >= 2);

    return (cluster_ofs << sector_shift) +
            (cluster << (sector_shift + block_shift));
}

void *fat32_fs_t::lookup_cluster(uint64_t cluster)
{
    return mm_dev + fat32_fs_t::offsetof_cluster(cluster);
}

cluster_t fat32_fs_t::dirent_start_cluster(fat32_dir_entry_t const *de)
{
    return (de->start_hi << 16) | de->start_lo;
}

void fat32_fs_t::dirent_start_cluster(fat32_dir_entry_t *de, cluster_t cluster)
{
    de->start_lo = (cluster >> 0*16) & 0xFFFF;
    de->start_hi = (cluster >> 1*16) & 0xFFFF;
}

// fcb_name is the space padded 11 character name with no dot,
// the representation used in dir_entry_t's name field
uint8_t fat32_fs_t::lfn_checksum(char const *fcb_name)
{
   uint16_t i;
   uint8_t sum = 0;

   for (i = 11; i; i--)
      sum = ((sum & 1) << 7) +
              (sum >> 1) +
              (uint8_t)*fcb_name++;

   return sum;
}

void fat32_fs_t::fcbname_from_lfn(
        char *fcbname, uint16_t const *lfn, size_t lfn_len)
{
    memset(fcbname, ' ', 11);

    size_t last_dot = 0;
    for (size_t i = lfn_len; !last_dot && i > 0; --i)
        if (lfn[i-1] == '.')
            last_dot = i - 1;
    if (!last_dot)
        last_dot = lfn_len;

    size_t i;
    size_t out = 0;
    for (i = 0; i < last_dot && out < 8; ++i) {
        if ((lfn[i] >= 'A' && lfn[i] <= 'Z') ||
                (lfn[i] >= '0' && lfn[i] <= '9')) {
            fcbname[out++] = lfn[i];
        } else if (lfn[i] >= 'a' && lfn[i] <= 'z') {
            fcbname[out++] = lfn[i] + ('A' - 'a');
        } else {
            fcbname[out++] = '_';
        }
    }

    if (i < last_dot) {
        fcbname[6] = '~';
        fcbname[7] = '1';
    }

    out = 8;
    for (i = last_dot + 1; i < lfn_len && out < 11; ++i) {
        if ((lfn[i] >= 'A' && lfn[i] <= 'Z') ||
                (lfn[i] >= '0' && lfn[i] <= '9')) {
            fcbname[out++] = lfn[i];
        } else if (lfn[i] >= 'a' && lfn[i] <= 'z') {
            fcbname[out++] = lfn[i] + ('A' - 'a');
        }
    }
}

void fat32_fs_t::dirents_from_name(
        full_lfn_t *full, char const *pathname, size_t name_len)
{
    memset(full, 0, sizeof(*full));

    uint16_t lfn[256];

    char const *utf8_in = pathname;
    char const *utf8_end = pathname + name_len;
    uint16_t *utf16_out = lfn;
    int codepoint;

    do {
        codepoint = utf8_to_ucs4(utf8_in, &utf8_in);
        utf16_out += ucs4_to_utf16(utf16_out, codepoint);
    } while (utf8_in < utf8_end);
    *utf16_out = 0;

    size_t len = utf16_out - lfn;

    char fcbname[11];

    fcbname_from_lfn(fcbname, lfn, len);

    uint8_t checksum = lfn_checksum(fcbname);

    int lfn_entries = (len + 12) / 13;

    int fragment_ofs = 0;
    fat32_dir_union_t *frag = full->fragments + lfn_entries - 1;
    for (size_t ofs = 0; ofs < len || fragment_ofs != 0; ++ofs) {
        uint8_t *dest;

        if (fragment_ofs == 0) {
            frag->long_entry.attr = FAT_LONGNAME;
            frag->long_entry.checksum = checksum;
        }

        if (fragment_ofs < 5) {
            dest = frag->long_entry.name +
                    (fragment_ofs << 1);
        } else if (fragment_ofs < 11) {
            dest = frag->long_entry.name2 +
                    ((fragment_ofs - 5) << 1);
        } else {
            dest = frag->long_entry.name3 +
                    ((fragment_ofs - 11) << 1);
        }

        // After writing null terminator, fill with 0xFF
        if (ofs <= len) {
            memcpy(dest, lfn + ofs, sizeof(uint16_t));
        } else {
            dest[0] = 0xFF;
            dest[1] = 0xFF;
        }

        if (++fragment_ofs == 13) {
            fragment_ofs = 0;
            --frag;
        }
    }

    memcpy(full->fragments[lfn_entries].short_entry.name,
           fcbname, sizeof(frag->short_entry.name));

    full->lfn_entry_count = lfn_entries;

    for (size_t i = 0; i < full->lfn_entry_count; ++i) {
        full->fragments[i].long_entry.ordinal =
                i == 0
                ? full->lfn_entry_count | FAT_LAST_LFN_ORDINAL
                : full->lfn_entry_count - i;
    }
}

size_t fat32_fs_t::name_from_dirents(char *pathname,
                                      full_lfn_t const *full)
{
    if (unlikely(full->lfn_entry_count == 0)) {
        // Copy short name
        fat32_dir_entry_t const &short_ent = full->fragments[0].short_entry;

        char const *short_end = (char*)memchr(short_ent.name, ' ', 8);
        if (!short_end)
            short_end = short_ent.name + 8;

        char const *ext_end = (char*)memchr(short_ent.name + 8, ' ', 3);
        if (!ext_end)
            ext_end = short_ent.name + 8 + 3;

        size_t name_len = short_end - short_ent.name;
        size_t ext_len = ext_end - (short_ent.name + 8);

        memcpy(pathname, short_ent.name, name_len);
        pathname[name_len] = 0;

        if (short_ent.lowercase_flags & FAT_LOWERCASE_NAME) {
            for (size_t i = 0; i < name_len; ++i) {
                if (pathname[i] >= 'A' && pathname[i] <= 'Z')
                    pathname[i] += 'a' - 'A';
            }
        }

        if (ext_len) {
            pathname[name_len] = '.';
            memcpy(pathname + name_len + 1, short_ent.name + 8, ext_len);
            pathname[name_len + 1 + ext_len] = 0;

            if (short_ent.lowercase_flags & FAT_LOWERCASE_EXT) {
                for (size_t i = 0; i < ext_len; ++i) {
                    if (pathname[name_len + 1 + i] >= 'A' && pathname[i] <= 'Z')
                        pathname[name_len + 1 + i] += 'a' - 'A';
                }
            }

            return name_len + 1 + ext_len;
        }

        return name_len;
    }

    uint16_t chunk[(sizeof(full->fragments[0].long_entry.name) +
            sizeof(full->fragments[0].long_entry.name2) +
            sizeof(full->fragments[0].long_entry.name3)) /
            sizeof(uint16_t)];

    char *out = pathname;

    fat32_dir_union_t const *frag = full->fragments +
            full->lfn_entry_count - 1;

    for (size_t i = 0; i < full->lfn_entry_count; ++i, --frag) {
        memcpy(chunk,
               frag->long_entry.name,
               sizeof(frag->long_entry.name));

        memcpy(chunk + 5,
               frag->long_entry.name2,
               sizeof(frag->long_entry.name2));

        memcpy(chunk + 5 + 6,
               frag->long_entry.name3,
               sizeof(frag->long_entry.name3));

        uint16_t const *in = chunk;
        while ((in < (chunk + countof(chunk))) &&
               *in && (*in != 0xFFFFU)) {
            int codepoint = utf16_to_ucs4(in, &in);
            out += ucs4_to_utf8(out, codepoint);
        }
    }

    return out - pathname;
}

bool fat32_fs_t::is_eof(cluster_t cluster)
{
    cluster &= 0x0FFFFFFF;
    return cluster < 2 || cluster >= 0x0FFFFFF8;
}

bool fat32_fs_t::is_free(cluster_t cluster)
{
    return (cluster & 0x0FFFFFFF) == 0;
}

bool fat32_fs_t::is_dirent_free(fat32_dir_union_t const *de)
{
    return (de->short_entry.name[0] == 0) ||
            (de->short_entry.name[0] == FAT_DELETED_FLAG);
}

int32_t fat32_fs_t::extend_fat_chain(cluster_t *clusters,
                                    int32_t count, cluster_t cluster)
{
    int32_t allocated;

    for (allocated = 0; allocated < count; ++allocated) {
        cluster = allocate_near(cluster);
        clusters[allocated] = cluster;
    }

    if (unlikely(allocated < count))
        clusters[allocated] = 0;

    return allocated;
}

cluster_t fat32_fs_t::allocate_near(cluster_t cluster)
{
    cluster_t candidate = cluster + 1;

    for (int32_t checked = 0; checked < end_cluster; ++checked, ++candidate) {
        if (unlikely(candidate >= end_cluster || candidate < 2))
            candidate = 2;

        if (unlikely(is_free(fat[candidate]))) {
            void *block = lookup_cluster(candidate);
            memset(block, 0, block_size);
            fat[candidate] = 1;
            return candidate;
        }
    }

    // Disk full
    return 0;
}

int fat32_fs_t::commit_fat_extend(cluster_t *clusters, int32_t count)
{
    int result = 0;

    if (likely(count)) {
        int32_t i;
        cluster_t cluster;
        for (i = 0; i < count - 1; ++i) {
            cluster = clusters[i];

            assert(fat[cluster] == 1);
            fat[cluster] = clusters[i+1];
            fat2[cluster] = clusters[i+1];

            result = sync_fat_entry(cluster);

            if (unlikely(result < 0))
                return result;
        }
        cluster = clusters[i];
        fat[cluster] = 0x0FFFFFFF;
        fat2[cluster] = 0x0FFFFFFF;

        result = sync_fat_entry(cluster);
    }

    return result;
}

template<typename F>
void fat32_fs_t::iterate_dir(cluster_t cluster, F callback,
                             cluster_t *extend_cluster)
{
    size_t de_per_cluster = block_size >> dirent_size_shift;

    if (extend_cluster)
        *extend_cluster = 0;

    while (!is_eof(cluster)) {
        fat32_dir_union_t *de = (fat32_dir_union_t *)lookup_cluster(cluster);

        // Iterate through all of the directory entries in this cluster
        for (size_t i = 0; i < de_per_cluster; ++i, ++de) {
            if (!callback(de, cluster, i == 0))
                return;
        }

        if (extend_cluster && is_eof(fat[cluster])) {
            extend_fat_chain(extend_cluster, 1, cluster);// fixme disk full?
            cluster = *extend_cluster;
        } else {
            cluster = fat[cluster];
        }
    }
}

fat32_dir_union_t *fat32_fs_t::search_dir(
        cluster_t cluster,
        char const *filename, size_t name_len)
{
    full_lfn_t lfn;
    dirents_from_name(&lfn, filename, name_len);

    size_t match_index = 0;

    uint8_t last_checksum = 0;

    fat32_dir_union_t *result = nullptr;

    iterate_dir(cluster,
                [&](fat32_dir_union_t *de, cluster_t, bool) -> bool {
        // If this is a short filename entry
        if (de->long_entry.attr != FAT_LONGNAME) {
            uint8_t checksum = lfn_checksum(de->short_entry.name);
            if (!memcmp(de->short_entry.name,
                        lfn.fragments[lfn.lfn_entry_count]
                        .short_entry.name,
                        sizeof(de->short_entry.name)) &&
                    match_index == lfn.lfn_entry_count &&
                    last_checksum == checksum) {
                // Found
                result = de;
                return false;
            }

            // Start matching from the start of the searched entry
            match_index = 0;
            return true;
        }

        if (match_index < lfn.lfn_entry_count) {
            fat32_long_dir_entry_t const *le;
            le = &lfn.fragments[match_index].long_entry;

            if (le->ordinal == de->long_entry.ordinal &&
                    !memcmp((void*)le->name,
                            (void*)de->long_entry.name,
                               sizeof(le->name)) &&
                    !memcmp((void*)le->name2,
                            (void*)de->long_entry.name2,
                            sizeof(le->name2)) &&
                    !memcmp((void*)le->name3,
                            (void*)de->long_entry.name3,
                            sizeof(le->name3))) {
                last_checksum = de->long_entry.checksum;
                ++match_index;
            } else {
                match_index = 0;
            }
        } else {
            match_index = 0;
        }

        return true;
    });

    return result;
}

off_t fat32_fs_t::walk_cluster_chain(
        file_handle_t *file, off_t offset, bool append)
{
    off_t walked = 0;

    cluster_t cluster = file->cached_cluster;

    vector<cluster_t> sync_pending;

    while (walked + block_size <= offset) {
        if (is_eof(fat[cluster])) {
            if (!append)
                break;

            cluster_t alloc = allocate_near(cluster);
            if (unlikely(alloc == 0))
                break;

            if (dirent_start_cluster(file->dirent) == 0)
                dirent_start_cluster(file->dirent, alloc);

            cluster_t fat_block = cluster >> fat_block_shift;
            if (sync_pending.empty() || sync_pending.back() != fat_block)
                sync_pending.push_back(fat_block);

            fat[cluster] = alloc;
            fat2[cluster] = alloc;

            file->cached_cluster = alloc;
            file->cached_offset += block_size;
        }

        cluster = fat[cluster];
        walked += block_size;
    }

    if (append && file->cached_cluster == 0) {
        // Initial block in empty file (try to reserve first MB to metadata)
        file->cached_cluster = allocate_near(2047);
        file->cached_offset = 0;

        cluster_t fat_block = cluster >> fat_block_shift;
        if (sync_pending.empty() || sync_pending.back() != fat_block)
            sync_pending.push_back(fat_block);
    }

    for (cluster_t block_index : sync_pending) {
        int status = msync(fat + (block_index << fat_block_shift),
                           block_size, MS_SYNC);

        if (status < 0)
            return status;

        if (file->cached_offset == 0) {
            dirent_start_cluster(file->dirent, file->cached_cluster);
            status = msync(file->dirent, sizeof(*file->dirent), MS_SYNC);
            if (status < 0)
                return status;
        }
    }

    return walked;
}

fat32_dir_union_t *fat32_fs_t::lookup_dirent(char const *pathname,
                                             fat32_dir_union_t **dde)
{
    cluster_t cluster = root_cluster;
    fat32_dir_union_t *de = nullptr;

    if (dde)
        *dde = &root_dirent;

    char const *name_st = pathname;
    char const *path_end = pathname + strlen(pathname);

    if (name_st == path_end)
        return &root_dirent;

    char const *name_en;
    for ( ; name_st < path_end; name_st = name_en + 1) {
        name_en = (char*)memchr(name_st, '/', path_end - name_st);

        if (!name_en)
            name_en = path_end;

        size_t name_len = name_en - name_st;

        if (name_len == 0)
            break;

        de = search_dir(cluster, name_st, name_len);

        if (!de)
            return 0;

        if (dde)
            *dde = de;

        cluster = dirent_start_cluster(&de->short_entry);
    }

    return de;
}

fat32_dir_union_t *fat32_fs_t::create_dirent(
        char const *pathname, fat32_dir_union_t *dde, mode_t mode)
{
    // Find the end of the pathname
    char const *name_en = pathname + strlen(pathname);

    // Find the end of the path
    char const *path_en = (char*)memrchr(pathname, '/', name_en - pathname);

    // If there is no separator, the path is empty
    if (unlikely(!path_en))
        path_en = pathname;

    // The name starts after the separator, or the whole pathname is the name
    char const *name_st = path_en ? path_en + (*path_en == '/') : pathname;

    size_t name_len = name_en - name_st;

    full_lfn_t lfn;

    dirents_from_name(&lfn, name_st, name_len);

    fat32_dir_entry_t &sfn = lfn.fragments[lfn.lfn_entry_count].short_entry;

    // Set creation timestamp
    time_of_day_t now = time_ofday();
    date_encode(&sfn.creation_date, &sfn.creation_time,
                &sfn.creation_centisec, now);
    sfn.modified_date = sfn.creation_date;
    sfn.modified_time = sfn.creation_time;
    sfn.access_date = sfn.creation_date;

    // Archive bit is set in new files
    sfn.attr = FAT_ATTR_ARCH;

    // If owner doesn't have write permission, then set read only attribute
    if (!(mode & S_IWUSR))
        sfn.attr |= FAT_ATTR_RO;

    fat32_dir_union_t *entry_ptr;
    entry_ptr = dirent_create(dde, lfn);

    return entry_ptr;
}

fat32_dir_union_t *fat32_fs_t::dirent_create(
        fat32_dir_union_t *dde, full_lfn_t const& lfn)
{
    fat32_dir_union_t *ins_point = nullptr;

    int count = lfn.lfn_entry_count + 1;

    // If the directory was expanded, this is the newly allocated cluster
    cluster_t extend_cluster = 0;

    cluster_t result_cluster = 0;

    cluster_t prev_cluster = 0;
    cluster_t last_cluster = 0;
    int consecutive_free = 0;

    cluster_t dir_start = dirent_start_cluster(&dde->short_entry);
    iterate_dir(dir_start, [&](fat32_dir_union_t *de,
                cluster_t cluster, bool new_cluster) -> bool {
        // If we transitioned to a different cluster, reset
        if (new_cluster) {
            prev_cluster = last_cluster;
            consecutive_free = 0;
            ins_point = nullptr;
        }

        last_cluster = cluster;

        if (!is_dirent_free(de)) {
            // We ran into an allocated entry, restart search
            consecutive_free = 0;
            ins_point = nullptr;
            return true;
        }

        // If this is the first empty entry, remember start cluster
        if (++consecutive_free == 1) {
            result_cluster = cluster;
            ins_point = de;
        }

        // Done when we have found enough consecutive free entries
        return consecutive_free < count;
    }, &extend_cluster);

    cluster_t backup = 0;

    if (extend_cluster) {
        // No need for transaction, it is in a new cluster
        for (int i = 0; i < count; ++i)
            ins_point[i] = lfn.fragments[i];

        // Write EOF
        assert(fat[extend_cluster] == 1);
        //assert(fat2[extend_cluster] == 1);
        fat[extend_cluster] = 0x0FFFFFF8;
        fat2[extend_cluster] = 0x0FFFFFF8;
        if (sync_fat_entry(extend_cluster) < 0)
            return nullptr;
    } else {
        // Perform transacted update
        backup = transact_cluster(prev_cluster, dde, result_cluster);

        // Copy directory entries into page cache
        for (int i = 0; i < count; ++i)
            ins_point[i] = lfn.fragments[i];

        // Write new directory entries to disk
        if (msync(ins_point, sizeof(*ins_point) * count, MS_SYNC) < 0)
            return nullptr;
    }

    // Commit transaction
    if (prev_cluster) {
        fat[prev_cluster] = result_cluster;
        fat2[prev_cluster] = result_cluster;

        if (sync_fat_entry(prev_cluster) < 0)
            return nullptr;
    } else {
        if (change_dirent_start(dde, result_cluster) < 0)
            return nullptr;
    }

    if (!extend_cluster) {
        assert(backup != 0);
        fat[backup] = 0;
        fat2[backup] = 0;
    }

    return ins_point + lfn.lfn_entry_count;
}

int fat32_fs_t::change_dirent_start(fat32_dir_union_t *dde, cluster_t start)
{
    if (dde != &root_dirent) {
        dde->short_entry.start_lo = (start >> (0*16)) & 0xFFFF;
        dde->short_entry.start_hi = (start >> (1*16)) & 0xFFFF;

        return msync(dde, sizeof(*dde), MS_SYNC);
    }

    // Update synthetic root directory directory entry
    dde->short_entry.start_lo = (start >> (0*16)) & 0xFFFF;
    dde->short_entry.start_hi = (start >> (1*16)) & 0xFFFF;

    // Update boot parameter block root directory location
    cluster_t *root_start = (cluster_t*)(mm_dev + 0x2C);
    *root_start = start;

    root_cluster = start;

    return msync(root_start, sizeof(cluster_t), MS_SYNC);
}

int fat32_fs_t::sync_fat_entry(cluster_t cluster)
{
    int result = msync(fat + cluster, sizeof(cluster_t), MS_SYNC);

    if (likely(result >= 0))
        return msync(fat2 + cluster, sizeof(cluster_t), MS_SYNC);

    return result;
}

void fat32_fs_t::date_encode(uint16_t *date_field, uint16_t *time_field,
                             uint8_t *centisec_field, time_of_day_t tod)
{
    assert(tod.month >= 1 && tod.month <= 12);
    assert(tod.day >= 1 && tod.day <= 31);
    assert(tod.hour >= 0 && tod.hour <= 23);
    assert(tod.minute >= 0 && tod.minute <= 59);
    assert(tod.second >= 0 && tod.second <= 59);

    int yr = tod.century * 100 + tod.year;

    // 7 bit year : 4 bit month : 5 bit day
    *date_field = ((yr - 1980) << 9) |
            (tod.month << 5) | tod.day;

    // 5 bit hour : 6 bit minute : 4 bit second
    *time_field = (tod.hour << 11) |
            (tod.minute << 5) | (tod.second >> 1);

    if (centisec_field)
        *centisec_field = tod.centisec + (100 * (tod.second & 1));
}

// Move a cluster to another location and link FAT chain to moved cluster
// flushing the data, then the fat sectors
int32_t fat32_fs_t::transact_cluster(
        cluster_t prev_cluster, fat32_dir_union_t *dde, cluster_t cluster)
{
    int result;

    cluster_t backup = allocate_near(cluster);

    if (unlikely(!backup))
        return 0;

    void *destination = lookup_cluster(backup);
    void *source = lookup_cluster(cluster);
    memcpy(destination, source, block_size);
    result = msync(destination, block_size, MS_SYNC);
    if (unlikely(result < 0))
        return result;

    fat[backup] = fat[cluster];
    fat2[backup] = fat[cluster];
    result = sync_fat_entry(backup);
    if (unlikely(result < 0))
        return result;

    if (prev_cluster == 0) {
        // First cluster, adjust start cluster in directory entry
        result = change_dirent_start(dde, backup);
    } else {
        // Not the first cluster, adjust FAT entry
        fat[prev_cluster] = backup;
        fat2[prev_cluster] = backup;
        result = sync_fat_entry(prev_cluster);
    }

    return likely(result >= 0) ? backup : result;
}

fat32_fs_t::file_handle_t *fat32_fs_t::create_handle(
        char const *path, int flags, mode_t mode)
{
    fat32_dir_union_t *dde;
    fat32_dir_union_t *fde = lookup_dirent(path, &dde);

    if (!(flags & O_CREAT)) {
        // Opening existing file
        if (unlikely(!fde)) {
            // File not found
            return 0;
        }
    } else {
        // Creating file
        if (unlikely(!dde)) {
            // Path not found
            return 0;
        }

        if ((flags & O_EXCL) && fde) {
            // File already exists
            return 0;
        }

        if (!fde) {
            fde = create_dirent(path, dde, mode);
            if (!fde)
                return 0;
        }
    }

    file_handle_t *file = (file_handle_t*)pool_alloc(&fat32_handles);

    memzero(*file);

    file->fs = this;
    file->dirent = &fde->short_entry;

    file->cached_cluster = dirent_start_cluster(&fde->short_entry);
    file->cached_offset = 0;

    return file;
}

ssize_t fat32_fs_t::internal_rw(file_handle_t *file,
        void *buf, size_t size, off_t offset, bool read)
{
    char *io = (char*)buf;
    ssize_t result = 0;

    off_t cached_end = file->cached_offset + block_size;
    while (size > 0) {
        if (file->cached_cluster &&
                offset < file->dirent->size &&
                (offset >= cached_end) &&
                (offset < cached_end + block_size)) {
            // Move to next cluster
            file->cached_offset += block_size;
            file->cached_cluster = fat[file->cached_cluster];
            cached_end += block_size;
        } else if (!file->cached_cluster ||
                   (offset < file->cached_offset) ||
                   (offset >= cached_end)) {
            // Offset cache miss
            file->cached_cluster = dirent_start_cluster(file->dirent);
            file->cached_offset = 0;
            walk_cluster_chain(file, offset, !read);
            if (file->dirent->size > 0 || !read)
                cached_end = file->cached_offset + block_size;
        }

        size_t avail;

        if (offset < cached_end)
            avail = cached_end - offset;
        else
            avail = 0;

        assert(avail <= block_size);

        if (avail > size)
            avail = size;

        size_t cluster_data_ofs = offsetof_cluster(file->cached_cluster);

        if (read) {
            memcpy(io, mm_dev + cluster_data_ofs +
                   (offset - file->cached_offset), avail);
        } else {
            void *output = mm_dev + cluster_data_ofs +
                    (offset - file->cached_offset);
            memcpy(output, io, avail);
            int status = msync(output, avail, MS_ASYNC);
            if (status < 0)
                return status;

            // Update size
            if (file->dirent->size < offset + avail)
                file->dirent->size = offset + avail;

            // Update last-modified
            time_of_day_t now = time_ofday();
            date_encode(&file->dirent->modified_date,
                        &file->dirent->modified_time, nullptr, now);

            // Set archive bit
            file->dirent->attr |= FAT_ATTR_ARCH;

            // Mark for writeback
            file->dirty = true;
        }

        offset += avail;
        size -= avail;
        io += avail;
        result += avail;
    }

    return result;
}

//
// Startup and shutdown

fat32_fs_t::fat32_fs_t()
{
    rwlock_init(&rwlock);
}

fs_base_t *fat32_fs_t::mount(fs_init_info_t *conn)
{
    drive = conn->drive;

    sector_size = drive->info(STORAGE_INFO_BLOCKSIZE);

    unique_ptr<char> sector_buffer = new char[sector_size];
    fat32_bpb_data_t bpb;

    lba_st = conn->part_st;
    lba_en = conn->part_st + conn->part_len;

    if (drive->read_blocks(sector_buffer, 1, lba_st) < 0)
        return nullptr;

    // Pass 0 partition cluster to get partition relative values
    fat32_parse_bpb(&bpb, 0, sector_buffer);

    root_cluster = bpb.root_dir_start;

    memset(&root_dirent, 0, sizeof(root_dirent));
    root_dirent.short_entry.start_lo =
            (root_cluster) & 0xFFFF;
    root_dirent.short_entry.start_hi =
            (root_cluster >> 16) & 0xFFFF;

    // Sector offset of cluster 0
    cluster_ofs = bpb.cluster_begin_lba;

    sector_shift = bit_log2_n(sector_size);
    block_shift = bit_log2_n(bpb.sec_per_cluster);
    block_size = sector_size << block_shift;

    // Right shift a cluster number this much to find cluster index within FAT
    fat_block_shift = bit_log2_n(block_size >> bit_log2_n(sizeof(cluster_t)));

    mm_dev = (char*)mmap_register_device(
                this, block_size, conn->part_len,
                PROT_READ | PROT_WRITE, &fat32_fs_t::mm_fault_handler);

    fat_size = bpb.sec_per_fat << sector_shift;
    fat = (cluster_t*)lookup_sector(bpb.first_fat_lba);
    fat2 = (cluster_t*)lookup_sector(bpb.first_fat_lba + bpb.sec_per_fat);
    end_cluster = (lba_en - cluster_ofs) >> bit_log2_n(bpb.sec_per_cluster);

    int fat_mismatches = 0;
    for (int i = 0, e = fat_size >> bit_log2_n(sizeof(cluster_t)); i < e; ++i)
        fat_mismatches += fat[i] != fat2[i];

    if (fat_mismatches != 0) {
        FAT32_TRACE("%d FAT mismatches", fat_mismatches);
    }

    return this;
}

fs_base_t *fat32_factory_t::mount(fs_init_info_t *conn)
{
    if (fat32_mounts.empty())
        pool_create(&fat32_handles, sizeof(fat32_fs_t::file_handle_t), 512);

    fat32_fs_t *self = new fat32_fs_t;
    fat32_mounts.push_back(self);

    return self->mount(conn);
}

void fat32_fs_t::unmount()
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::exclusive);

    munmap(mm_dev, (uint64_t)(lba_en - lba_st) << sector_shift);
}

//
// Scan directories

int fat32_fs_t::opendir(fs_file_info_t **fi, fs_cpath_t path)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    file_handle_t *file = create_handle(path, O_RDONLY | O_DIRECTORY, 0);

    if (!file)
        return -1;

    *fi = file;

    return 0;
}

ssize_t fat32_fs_t::readdir(fs_file_info_t *fi,
                             dirent_t *buf,
                             off_t offset)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    full_lfn_t lfn;
    size_t index;
    size_t distance;

    memset(&lfn, 0, sizeof(lfn));

    char expected_fragments = 0;

    for (index = 0, distance = 0;
         sizeof(lfn.fragments[index]) == internal_rw(
             (file_handle_t*)fi, lfn.fragments + index,
             sizeof(lfn.fragments[index]),
             offset + sizeof(lfn.fragments[index]) * index, true);
         ++distance, ++index) {
        fat32_dir_union_t& fragment = lfn.fragments[index];

        if (unlikely(fragment.short_entry.name[0] == 0))
            break;

        // If there are too many fragments, restart
        if (unlikely(index + 1 >= countof(lfn.fragments))) {
            // Invalid
            index = 0;
            continue;
        }

        // Accept long name entry
        if (likely(fragment.short_entry.attr == FAT_LONGNAME)) {
            char ordinal = fragment.short_entry.name[0];
            char lfn_index = ordinal & FAT_ORDINAL_MASK;

            // Detect the beginning of a run of LFN entries
            if (unlikely(ordinal & FAT_LAST_LFN_ORDINAL)) {
                index = 0;
                expected_fragments = lfn_index;
                continue;
            }

            // Validate LFN sequence number
            if (unlikely(index != size_t(expected_fragments - lfn_index))) {
                // Encountered LFN entry with wrong ordinal index
                index = 0;
                expected_fragments = 0;
            }

            continue;
        }

        // If first LFN entry had wrong fragment count, drop
        if (index != size_t(expected_fragments)) {
            index = 0;
            expected_fragments = 0;
            continue;
        }

        // Drop deleted files
        if (unlikely(fragment.short_entry.name[0] == FAT_DELETED_FLAG)) {
            index = 0;
            continue;
        }

        // 0xE5 is represented as 0x05 in short names
        if (unlikely(fragment.short_entry.name[0] == FAT_ESCAPED_0xE5))
            fragment.short_entry.name[0] = FAT_DELETED_FLAG;

        lfn.lfn_entry_count = index;
        ++distance;
        break;
    }

    if (distance > 0)
        name_from_dirents(buf->d_name, &lfn);
    else
        memset(buf, 0, sizeof(*buf));

    return distance * sizeof(fat32_dir_entry_t);
}

int fat32_fs_t::releasedir(fs_file_info_t *fi)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    pool_free(&fat32_handles, fi);
    return 0;
}

//
// Read directory entry information

int fat32_fs_t::getattr(fs_cpath_t path, fs_stat_t* stbuf)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    (void)path;
    (void)stbuf;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::access(fs_cpath_t path, int mask)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    (void)path;
    (void)mask;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::readlink(fs_cpath_t path, char* buf, size_t size)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    (void)path;
    (void)buf;
    (void)size;
    return -int(errno_t::ENOSYS);
}

//
// Modify directories

int fat32_fs_t::mknod(fs_cpath_t path,
                       fs_mode_t mode, fs_dev_t rdev)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)path;
    (void)mode;
    (void)rdev;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::mkdir(fs_cpath_t path, fs_mode_t mode)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)path;
    (void)mode;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::rmdir(fs_cpath_t path)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)path;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::symlink(fs_cpath_t to, fs_cpath_t from)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)to;
    (void)from;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::rename(fs_cpath_t from, fs_cpath_t to)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)from;
    (void)to;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::link(fs_cpath_t from, fs_cpath_t to)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)from;
    (void)to;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::unlink(fs_cpath_t path)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)path;
    return -int(errno_t::ENOSYS);
}

//
// Modify directory entries

int fat32_fs_t::chmod(fs_cpath_t path, fs_mode_t mode)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)path;
    (void)mode;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::chown(fs_cpath_t path, fs_uid_t uid, fs_gid_t gid)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)path;
    (void)uid;
    (void)gid;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::truncate(fs_cpath_t path, off_t size)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)path;
    (void)size;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::utimens(fs_cpath_t path,
                         const fs_timespec_t *ts)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)path;
    (void)ts;
    return -int(errno_t::ENOSYS);
}

//
// Open/close files

int fat32_fs_t::open(fs_file_info_t **fi,
                      fs_cpath_t path, int flags, mode_t mode)
{
    scoped_rwlock_t lock(rwlock, (flags & O_CREAT)
                         ? scoped_rwlock_t::writer
                         : scoped_rwlock_t::reader);

    file_handle_t *file = create_handle(path, flags, mode);

    if (!file)
        return -1;

    *fi = file;

    return 0;
}

int fat32_fs_t::release(fs_file_info_t *fi)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    file_handle_t *file = (file_handle_t*)fi;
    int status = 0;

    if (file->dirty)
        status = msync(file->dirent, sizeof(*file->dirent), MS_SYNC);

    pool_free(&fat32_handles, fi);
    return status;
}

//
// Read/write files

ssize_t fat32_fs_t::read(fs_file_info_t *fi,
                          char *buf,
                          size_t size,
                          off_t offset)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    return internal_rw((file_handle_t*)fi, (char*)buf, size, offset, true);
}

ssize_t fat32_fs_t::write(fs_file_info_t *fi,
                           char const *buf,
                           size_t size,
                           off_t offset)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    return internal_rw((file_handle_t*)fi, (char*)buf, size, offset, false);
}

int fat32_fs_t::ftruncate(fs_file_info_t *fi,
                           off_t offset)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)offset;
    (void)fi;
    return -int(errno_t::ENOSYS);
}

//
// Query open file

int fat32_fs_t::fstat(fs_file_info_t *fi,
                       fs_stat_t *st)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    (void)fi;
    (void)st;
    return -int(errno_t::ENOSYS);
}

//
// Sync files and directories and flush buffers

int fat32_fs_t::fsync(fs_file_info_t *fi,
                       int isdatasync)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)isdatasync;
    (void)fi;
    return 0;
}

int fat32_fs_t::fsyncdir(fs_file_info_t *fi,
                          int isdatasync)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)isdatasync;
    (void)fi;
    return 0;
}

int fat32_fs_t::flush(fs_file_info_t *fi)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)fi;
    return 0;
}

//
// lock/unlock file

int fat32_fs_t::lock(fs_file_info_t *fi,
                      int cmd,
                      fs_flock_t* locks)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)fi;
    (void)cmd;
    (void)locks;
    return -int(errno_t::ENOSYS);
}

//
// Get block map

int fat32_fs_t::bmap(fs_cpath_t path,
                      size_t blocksize,
                      uint64_t* blockno)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    (void)path;
    (void)blocksize;
    (void)blockno;
    return -int(errno_t::ENOSYS);
}

//
// Get filesystem information

int fat32_fs_t::statfs(fs_statvfs_t* stbuf)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    (void)stbuf;
    return -int(errno_t::ENOSYS);
}

//
// Read/Write/Enumerate extended attributes

int fat32_fs_t::setxattr(fs_cpath_t path,
                          char const* name,
                          char const* value,
                          size_t size, int flags)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::getxattr(fs_cpath_t path,
                          char const* name,
                          char* value,
                          size_t size)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    (void)path;
    (void)name;
    (void)value;
    (void)size;
    return -int(errno_t::ENOSYS);
}

int fat32_fs_t::listxattr(fs_cpath_t path,
                           char const* list,
                           size_t size)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    (void)path;
    (void)list;
    (void)size;
    return -int(errno_t::ENOSYS);
}

//
// ioctl API

int fat32_fs_t::ioctl(fs_file_info_t *fi,
                       int cmd,
                       void* arg,
                       unsigned int flags,
                       void* data)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::writer);

    (void)cmd;
    (void)arg;
    (void)fi;
    (void)flags;
    (void)data;
    return -int(errno_t::ENOSYS);
}

//
//

int fat32_fs_t::poll(fs_file_info_t *fi,
                      fs_pollhandle_t* ph,
                      unsigned* reventsp)
{
    scoped_rwlock_t lock(rwlock, scoped_rwlock_t::reader);

    (void)fi;
    (void)ph;
    (void)reventsp;
    return -int(errno_t::ENOSYS);
}
