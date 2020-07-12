#include "dev_storage.h"

#include "printk.h"
#include "string.h"
#include "assert.h"
#include "vector.h"

#include "hash_table.h"

#define DEBUG_STORAGE   1
#if DEBUG_STORAGE
#define STORAGE_TRACE(...) printk("storage: " __VA_ARGS__)
#else
#define STORAGE_TRACE(...) ((void)0)
#endif

dev_base_t::major_map_t dev_base_t::dev_lookup;

struct fs_mount_t {
    fs_factory_t *reg;
    fs_base_t *fs;
};

using path_table_t = hashtbl_t<fs_mount_t, fs_factory_t*, &fs_mount_t::reg>;
static path_table_t path_table;

using lock_type = ext::noirq_lock<ext::spinlock>;
using scoped_lock = std::unique_lock<lock_type>;
static lock_type storage_lock;

static std::vector<storage_if_factory_t*> storage_if_factories;
static std::vector<storage_if_base_t*> storage_ifs;
static std::vector<storage_dev_base_t*> storage_devs;
static std::vector<part_factory_t*> part_factories;
static std::vector<fs_factory_t*> fs_regs;
static std::vector<fs_mount_t> fs_mounts;

size_t storage_dev_count()
{
    scoped_lock lock(storage_lock);
    return storage_devs.size();
}

storage_dev_base_t *storage_dev_open(dev_t dev)
{
    scoped_lock lock(storage_lock);
    assert(size_t(dev) <= storage_devs.size());
    return size_t(dev) < storage_devs.size() ? storage_devs[dev] : nullptr;
}

void storage_dev_close(storage_dev_base_t *dev)
{
    (void)dev;
}

EXPORT bool storage_if_unregister_factory(storage_if_factory_t *factory)
{
    scoped_lock lock(storage_lock);
    return unregister_factory(storage_if_factories, factory);
}

EXPORT void storage_if_register_factory(storage_if_factory_t *factory)
{
    scoped_lock lock(storage_lock);

    if (unlikely(!storage_if_factories.push_back(factory)))
        panic_oom();

    lock.unlock();

    STORAGE_TRACE("Registered storage driver %s\n", factory->name);
    probe_storage_factory(factory);
}

void probe_storage_factory(storage_if_factory_t *factory)
{
    STORAGE_TRACE("Probing for %s interfaces...\n", factory->name);

    // Get a list of storage devices of this type
    std::vector<storage_if_base_t *> list = factory->detect();

    STORAGE_TRACE("Found %zu %s interfaces\n", list.size(), factory->name);

    for (size_t i = 0; i < list.size(); ++i) {
        // Calculate pointer to storage interface instance
        storage_if_base_t *if_ = list[i];

        // Store interface instance
        if (unlikely(!storage_ifs.push_back(if_)))
            panic_oom();

        STORAGE_TRACE("Probing %s[%zu] for drives...\n", factory->name, i);

        // Get a list of storage devices on this interface
        std::vector<storage_dev_base_t*> dev_list = if_->detect_devices();

        STORAGE_TRACE("Found %zu %s[%zu] drives\n", dev_list.size(),
                      factory->name, i);

        for (unsigned k = 0; k < dev_list.size(); ++k) {
            // Calculate pointer to storage device instance
            storage_dev_base_t *dev = dev_list[k];
            // Store device instance
            scoped_lock lock(storage_lock);
            if (unlikely(!storage_devs.push_back(dev)))
                panic_oom();
        }
    }
}

EXPORT bool fs_unregister_factory(fs_factory_t *fs)
{
    scoped_lock lock(storage_lock);
    return unregister_factory(fs_regs, fs);
}

EXPORT void fs_register_factory(fs_factory_t *fs)
{
    scoped_lock lock(storage_lock);
    if (unlikely(!fs_regs.push_back(fs)))
        panic_oom();
    printdbg("%s filesystem registered\n", fs->name);
}

static fs_factory_t *find_fs(char const *name)
{
    scoped_lock lock(storage_lock);
    for (fs_factory_t *reg : fs_regs) {
        if (strcmp(reg->name, name))
            continue;

        return reg;
    }
    return nullptr;
}

EXPORT void fs_add(fs_factory_t *fs_reg, fs_base_t *fs)
{
    if (unlikely(!fs))
        return;

    scoped_lock lock(storage_lock);
    if (unlikely(fs_mounts.insert(fs->is_boot()
                     ? fs_mounts.begin()
                     : fs_mounts.end(),
                     fs_mount_t{ fs_reg, fs }) ==
                 std::vector<fs_mount_t>::iterator()))
        panic_oom();
}

EXPORT void fs_mount(char const *fs_name, fs_init_info_t *info)
{
    fs_factory_t *fs_reg = find_fs(fs_name);

    if (!fs_reg) {
        STORAGE_TRACE("Could not find %s filesystem implementation\n", fs_name);
        return;
    }

    assert(fs_reg != nullptr);

    printdbg("Mounting %s filesystem\n", fs_name);

    fs_base_t *mfs = fs_reg->mount(info);

    if (mfs && mfs->is_boot()) {
        scoped_lock lock(storage_lock);
        fs_mounts.insert(fs_mounts.begin(), fs_mount_t{ fs_reg, mfs });

    } else if (mfs) {
        scoped_lock lock(storage_lock);
        if (unlikely(!fs_mounts.push_back(fs_mount_t{ fs_reg, mfs })))
            panic_oom();
    }
}

fs_base_t *fs_from_id(size_t id)
{
    scoped_lock lock(storage_lock);
    return !fs_mounts.empty()
            ? fs_mounts[id].fs
            : nullptr;
}

static void probe_part_factory_on_drive(
        part_factory_t *factory, storage_dev_base_t *drive)
{
    if (drive) {
        STORAGE_TRACE("Probing %s for %s partitions...\n",
                      (char const *)drive->info(STORAGE_INFO_NAME),
                      factory->name);

        std::vector<part_dev_t*> part_list = factory->detect(drive);

        // Mount partitions
        for (size_t i = 0; i < part_list.size(); ++i) {
            part_dev_t *part = part_list[i];
            fs_init_info_t info;
            info.drive = drive;
            info.part_st = part->lba_st;
            info.part_len = part->lba_len;
            fs_mount(part->name, &info);
        }
        storage_dev_close(drive);

        STORAGE_TRACE("Found %zu %s partitions\n", part_list.size(),
                      factory->name);
    }
}

// For each storage device
static void probe_part_factory(part_factory_t *factory)
{
    for (storage_dev_base_t *drive : storage_devs)
        probe_part_factory_on_drive(factory, drive);
}

bool part_unregister_factory(part_factory_t *factory)
{
    scoped_lock lock(storage_lock);
    return unregister_factory(part_factories, factory);
}

EXPORT void part_register_factory(part_factory_t *factory)
{
    scoped_lock lock(storage_lock);

    if (unlikely(!part_factories.push_back(factory)))
        panic_oom();

    lock.unlock();

    printk("%s partition type registered\n", factory->name);
    probe_part_factory(factory);
}

EXPORT void fs_factory_t::register_factory(void *p)
{
    fs_factory_t *instance = (fs_factory_t*)p;
    fs_register_factory(instance);
}

EXPORT void part_factory_t::register_factory(void *p)
{
    part_factory_t *instance = (part_factory_t*)p;
    part_register_factory(instance);
}

EXPORT int storage_dev_base_t::read_blocks(
        void *data, int64_t count, uint64_t lba)
{
    blocking_iocp_t block;
    errno_t err = read_async(data, count, lba, &block);
    if (unlikely(err != errno_t::OK))
        return -int64_t(err);
    auto result = block.wait();
    if (unlikely(result.first != errno_t::OK))
        return -int64_t(result.first);
    return result.second;
}

EXPORT int storage_dev_base_t::write_blocks(
        void const *data, int64_t count, uint64_t lba, bool fua)
{
    blocking_iocp_t block;
    errno_t err = write_async(data, count, lba, fua, &block);
    if (unlikely(err != errno_t::OK))
        return -int64_t(err);
    auto result = block.wait();
    if (unlikely(result.first != errno_t::OK))
        return -int64_t(result.first);
    return result.second;
}

EXPORT int64_t storage_dev_base_t::trim_blocks(int64_t count, uint64_t lba)
{
    blocking_iocp_t block;
    errno_t err = trim_async(count, lba, &block);
    if (unlikely(err != errno_t::OK))
        return -int64_t(err);
    auto result = block.wait();
    if (unlikely(result.first != errno_t::OK))
        return -int64_t(result.second);
    return count;
}

EXPORT int storage_dev_base_t::flush()
{
    blocking_iocp_t block;
    errno_t err = flush_async(&block);
    if (unlikely(err != errno_t::OK))
        return -int64_t(err);
    auto result = block.wait();
    if (unlikely(result.first != errno_t::OK))
        return -int64_t(result.first);
    return result.second;
}

//
// Modify directories

EXPORT int fs_base_ro_t::mknodat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        fs_mode_t mode, fs_dev_t rdev)
{
    (void)path;
    (void)mode;
    (void)rdev;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT int fs_base_ro_t::mkdirat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        fs_mode_t mode)
{
    (void)path;
    (void)mode;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT int fs_base_ro_t::rmdirat(
        fs_file_info_t *dirfi, fs_cpath_t path)
{
    (void)path;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT int fs_base_ro_t::symlinkat(
        fs_file_info_t *dirtofi, fs_cpath_t to,
        fs_file_info_t *dirfromfi, fs_cpath_t from)
{
    (void)to;
    (void)from;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT int fs_base_ro_t::renameat(
        fs_file_info_t *dirfromfi, fs_cpath_t from,
        fs_file_info_t *dirtofi, fs_cpath_t to)
{
    (void)from;
    (void)to;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT int fs_base_ro_t::linkat(
        fs_file_info_t *dirfromfi, fs_cpath_t from,
        fs_file_info_t *dirtofi, fs_cpath_t to)
{
    (void)from;
    (void)to;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT int fs_base_ro_t::unlinkat(
        fs_file_info_t *dirfi, fs_cpath_t path)
{
    (void)path;
    // Fail, read only
    return -int(errno_t::EROFS);
}

//
// Modify directory entries

EXPORT int fs_base_ro_t::fchmod(
        fs_file_info_t *fi,
        fs_mode_t mode)
{
    (void)fi;
    (void)mode;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT int fs_base_ro_t::fchown(
        fs_file_info_t *fi,
        fs_uid_t uid,
        fs_gid_t gid)
{
    (void)fi;
    (void)uid;
    (void)gid;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT int fs_base_ro_t::truncateat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        off_t size)
{
    (void)path;
    (void)size;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT int fs_base_ro_t::utimensat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        fs_timespec_t const *ts)
{
    (void)path;
    (void)ts;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT ssize_t fs_base_ro_t::write(fs_file_info_t *fi,
                             char const *buf,
                             size_t size,
                             off_t offset)
{
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT int fs_base_ro_t::ftruncate(fs_file_info_t *fi,
                             off_t offset)
{
    (void)offset;
    (void)fi;
    // Fail, read only
    return -int(errno_t::EROFS);
}

//
// Sync files and directories and flush buffers

EXPORT int fs_base_ro_t::fsync(fs_file_info_t *fi,
                               int isdatasync)
{
    (void)isdatasync;
    (void)fi;
    // Read only device, do nothing
    return 0;
}

EXPORT int fs_base_ro_t::fsyncdir(fs_file_info_t *fi,
                                  int isdatasync)
{
    (void)isdatasync;
    (void)fi;
    // Ignore, read only
    return 0;
}

EXPORT int fs_base_ro_t::flush(fs_file_info_t *fi)
{
    (void)fi;
    // Do nothing, read only
    return 0;
}

EXPORT int fs_base_ro_t::setxattrat(
        fs_file_info_t *dirfi, fs_cpath_t path,
        char const* name,
        char const* value,
        size_t size,
        int flags)
{
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    // Fail, read only
    return -int(errno_t::EROFS);
}

EXPORT storage_if_factory_t::storage_if_factory_t(char const *factory_name)
    : name(factory_name)
{
}

void storage_if_factory_t::register_factory()
{
    storage_if_register_factory(this);
}

EXPORT disk_io_plan_t::disk_io_plan_t(void *dest, uint8_t log2_sector_size)
    : dest(dest)
    , vec(nullptr)
    , count(0)
    , capacity(0)
    , log2_sector_size(log2_sector_size)
{
}

EXPORT disk_io_plan_t::~disk_io_plan_t()
{
    free(vec);
    vec = nullptr;
    count = 0;
    capacity = 0;
}

EXPORT bool disk_io_plan_t::add(
        uint32_t lba, uint16_t sector_count,
        uint16_t sector_ofs, uint16_t byte_count)
{
    if (count > 0) {
        // See if we can coalesce with previous entry

        disk_vec_t &prev = vec[count - 1];

        uint32_t sector_size = UINT32_C(1) << log2_sector_size;;

        if (prev.lba + prev.count == lba &&
                prev.sector_ofs == 0 &&
                sector_ofs == 0 &&
                prev.byte_count == sector_size &&
                byte_count == sector_size &&
                0xFFFFFFFFU - count > prev.count) {
            // Added entry is a sequential run of full sector-aligned sector
            // which is contiguous with previous run of full sector-aligned
            // sectors and the sector count won't overflow
            prev.count += sector_count;
            return true;
        }
    }

    if (count + 1 > capacity) {
        size_t new_capacity = capacity >= 16 ? capacity * 2 : 16;
        disk_vec_t *new_vec = (disk_vec_t*)realloc(
                    vec, new_capacity * sizeof(*vec));
        if (unlikely(!new_vec))
            return false;
        vec = new_vec;
        capacity = new_capacity;
    }

    disk_vec_t &item = vec[count++];

    item.lba = lba;
    item.count = sector_count;
    item.sector_ofs = sector_ofs;
    item.byte_count = byte_count;

    return true;
}

EXPORT storage_dev_base_t::~storage_dev_base_t()
{
}

EXPORT storage_if_base_t::~storage_if_base_t()
{
}

//std::unique_ptr<storage_if_base_t> dummy;

storage_if_factory_t::~storage_if_factory_t()
{
    storage_if_unregister_factory(this);
}

fs_factory_t::~fs_factory_t()
{
    fs_unregister_factory(this);
}

part_factory_t::~part_factory_t()
{
    part_unregister_factory(this);
}

fs_factory_t::fs_factory_t(const char *factory_name)
    : name(factory_name)
{
}

fs_base_t::~fs_base_t()
{
}

//==

void fs_nosys_t::unmount()
{
}

bool fs_nosys_t::is_boot() const
{
    return false;
}

int fs_nosys_t::resolve(fs_file_info_t *dirfi,
                        fs_cpath_t path, size_t &consumed)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::opendirat(fs_file_info_t **fi,
                          fs_file_info_t *dirfi, fs_cpath_t path)
{
    return -int(errno_t::ENOSYS);
}

ssize_t fs_nosys_t::readdir(fs_file_info_t *fi, dirent_t *buf, off_t offset)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::releasedir(fs_file_info_t *fi)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::getattrat(fs_file_info_t *dirfi,
                          fs_cpath_t path, fs_stat_t *stbuf)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::accessat(fs_file_info_t *dirfi, fs_cpath_t path, int mask)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::readlinkat(fs_file_info_t *dirfi,
                           fs_cpath_t path, char *buf, size_t size)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::mknodat(fs_file_info_t *dirfi,
                        fs_cpath_t path, fs_mode_t mode, fs_dev_t rdev)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::mkdirat(fs_file_info_t *dirfi,
                        fs_cpath_t path, fs_mode_t mode)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::rmdirat(fs_file_info_t *dirfi, fs_cpath_t path)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::symlinkat(fs_file_info_t *dirtofi, fs_cpath_t to,
                          fs_file_info_t *dirfromfi, fs_cpath_t from)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::renameat(fs_file_info_t *dirfromfi, fs_cpath_t from,
                         fs_file_info_t *dirtofi, fs_cpath_t to)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::linkat(fs_file_info_t *dirfromfi, fs_cpath_t from,
                       fs_file_info_t *dirtofi, fs_cpath_t to)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::unlinkat(fs_file_info_t *dirfi, fs_cpath_t path)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::fchmod(fs_file_info_t *fi, fs_mode_t mode)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::fchown(fs_file_info_t *fi, fs_uid_t uid, fs_gid_t gid)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::truncateat(fs_file_info_t *dirfi, fs_cpath_t path, off_t size)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::utimensat(fs_file_info_t *dirfi, fs_cpath_t path,
                          fs_timespec_t const *ts)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::openat(fs_file_info_t **fi, fs_file_info_t *dirfi,
                       fs_cpath_t path, int flags, mode_t mode)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::release(fs_file_info_t *fi)
{
    return -int(errno_t::ENOSYS);
}

ssize_t fs_nosys_t::read(fs_file_info_t *fi,
                         char *buf, size_t size, off_t offset)
{
    return -int(errno_t::ENOSYS);
}

ssize_t fs_nosys_t::write(fs_file_info_t *fi,
                          const char *buf, size_t size, off_t offset)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::ftruncate(fs_file_info_t *fi, off_t offset)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::fstat(fs_file_info_t *fi, fs_stat_t *st)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::fsync(fs_file_info_t *fi, int isdatasync)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::fsyncdir(fs_file_info_t *fi, int isdatasync)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::flush(fs_file_info_t *fi)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::lock(fs_file_info_t *fi, int cmd, fs_flock_t *locks)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::bmapat(fs_file_info_t *dirfi,
                       fs_cpath_t path, size_t blocksize, uint64_t *blockno)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::statfs(fs_statvfs_t *stbuf)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::setxattrat(fs_file_info_t *dirfi, fs_cpath_t path,
                           const char *name, const char *value,
                           size_t size, int flags)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::getxattrat(fs_file_info_t *dirfi, fs_cpath_t path,
                           const char *name, char *value, size_t size)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::listxattrat(fs_file_info_t *dirfi, fs_cpath_t path,
                            const char *list, size_t size)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::ioctl(fs_file_info_t *fi, int cmd, void *arg,
                      unsigned int flags, void *data)
{
    return -int(errno_t::ENOSYS);
}

int fs_nosys_t::poll(fs_file_info_t *fi,
                     fs_pollhandle_t *ph, unsigned *reventsp)
{
    return -int(errno_t::ENOSYS);
}
