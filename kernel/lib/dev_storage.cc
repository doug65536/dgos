#include "dev_storage.h"

#include "printk.h"
#include "string.h"
#include "assert.h"
#include "vector.h"
#include "cpu/control_regs.h"

#include "hash_table.h"

#define DEBUG_STORAGE   0
#if DEBUG_STORAGE
#define STORAGE_TRACE(...) printk("storage: " __VA_ARGS__)
#else
#define STORAGE_TRACE(...) ((void)0)
#endif

struct fs_reg_t {
    char const *name;
    fs_factory_t *factory;
};

struct fs_mount_t {
    fs_reg_t *reg;
    fs_base_t *fs;
};

using path_table_t = hashtbl_t<fs_mount_t, fs_reg_t*, &fs_mount_t::reg>;
static path_table_t path_table;

static std::vector<storage_if_factory_t*> storage_if_factories;
static std::vector<storage_if_base_t*> storage_ifs;
static std::vector<storage_dev_base_t*> storage_devs;
static std::vector<part_factory_t*> part_factories;
static std::vector<fs_reg_t*> fs_regs;
static std::vector<fs_mount_t> fs_mounts;

size_t storage_dev_count()
{
    return storage_devs.size();
}

storage_dev_base_t *storage_dev_open(dev_t dev)
{
    assert(size_t(dev) <= storage_devs.size());
    return size_t(dev) < storage_devs.size() ? storage_devs[dev] : nullptr;
}

void storage_dev_close(storage_dev_base_t *dev)
{
    (void)dev;
}

void storage_if_register_factory(char const *name,
                                storage_if_factory_t *factory)
{
    (void)name;

    if (!storage_if_factories.push_back(factory))
        panic_oom();
    STORAGE_TRACE("Registered storage driver %s\n", name);
}

void probe_storage_factory(storage_if_factory_t *factory)
{
    STORAGE_TRACE("Probing for %s interfaces...\n", factory->name);

    // Get a list of storage devices of this type
    std::vector<storage_if_base_t *> list = factory->detect();

    STORAGE_TRACE("Found %zu %s interfaces\n", list.size(), factory->name);

    for (unsigned i = 0; i < list.size(); ++i) {
        // Calculate pointer to storage interface instance
        storage_if_base_t *if_ = list[i];

        // Store interface instance
        if (!storage_ifs.push_back(if_))
            panic_oom();

        STORAGE_TRACE("Probing %s[%u] for drives...\n", factory->name, i);

        // Get a list of storage devices on this interface
        std::vector<storage_dev_base_t*> dev_list = if_->detect_devices();

        STORAGE_TRACE("Found %zu %s[%u] drives\n", dev_list.size(),
                      factory->name, i);

        for (unsigned k = 0; k < dev_list.size(); ++k) {
            // Calculate pointer to storage device instance
            storage_dev_base_t *dev = dev_list[k];
            // Store device instance
            if (!storage_devs.push_back(dev))
                panic_oom();
        }
    }
}

void invoke_storage_factories(void *)
{
    for (storage_if_factory_t* factory : storage_if_factories)
        probe_storage_factory(factory);
}

REGISTER_CALLOUT(invoke_storage_factories, nullptr,
                 callout_type_t::storage_dev, "000");

void fs_register_factory(char const *name, fs_factory_t *fs)
{
    if (!fs_regs.push_back(new fs_reg_t{ name, fs }))
        panic_oom();
    printdbg("%s filesystem registered\n", name);
}

static fs_reg_t *find_fs(char const *name)
{
    for (fs_reg_t *reg : fs_regs) {
        if (strcmp(reg->name, name))
            continue;

        return reg;
    }
    return nullptr;
}

void fs_mount(char const *fs_name, fs_init_info_t *info)
{
    fs_reg_t *fs_reg = find_fs(fs_name);

    if (!fs_reg) {
        STORAGE_TRACE("Could not find %s filesystem implementation\n", fs_name);
        return;
    }

    assert(fs_reg != nullptr);
    assert(fs_reg->factory != nullptr);

    printdbg("Mounting %s filesystem\n", fs_name);

    fs_base_t *mfs = fs_reg->factory->mount(info);

    if (mfs && mfs->is_boot())
        fs_mounts.insert(fs_mounts.begin(), fs_mount_t{ fs_reg, mfs });
    else if (mfs) {
        if (!fs_mounts.push_back(fs_mount_t{ fs_reg, mfs }))
            panic_oom();
    }
}

fs_base_t *fs_from_id(size_t id)
{
    return !fs_mounts.empty()
            ? fs_mounts[id].fs
            : nullptr;
}

void part_register_factory(char const *name, part_factory_t *factory)
{
    if (!part_factories.push_back(factory))
        panic_oom();
    printk("%s partition type registered\n", name);
}

static void invoke_part_factories(void *arg)
{
    (void)arg;

    // For each partition factory
    for (part_factory_t *factory : part_factories) {
        // For each storage device
        for (storage_dev_base_t *drive : storage_devs) {
            if (drive) {
                STORAGE_TRACE("Probing %s for %s partitions...\n",
                              (char const *)drive->info(STORAGE_INFO_NAME),
                              factory->name);

                std::vector<part_dev_t*> part_list = factory->detect(drive);

                // Mount partitions
                for (unsigned i = 0; i < part_list.size(); ++i) {
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
    }

    STORAGE_TRACE("Partition probe complete\n");
}

REGISTER_CALLOUT(invoke_part_factories, nullptr,
                 callout_type_t::partition_probe, "000");

fs_factory_t::fs_factory_t(char const *factory_name)
    : name(factory_name)
{
}

void fs_factory_t::register_factory(void *p)
{
    fs_factory_t *instance = (fs_factory_t*)p;
    fs_register_factory(instance->name, instance);
}

storage_if_factory_t::storage_if_factory_t(char const *factory_name)
    : name(factory_name)
{
}

void storage_if_factory_t::register_factory(void *p)
{
    storage_if_factory_t *instance = (storage_if_factory_t*)p;
    storage_if_register_factory(instance->name, instance);
}

part_factory_t::part_factory_t(char const *factory_name)
    : name(factory_name)
{
}

void part_factory_t::register_factory(void *p)
{
    part_factory_t *instance = (part_factory_t*)p;
    part_register_factory(instance->name, instance);
}

int storage_dev_base_t::read_blocks(void *data, int64_t count, uint64_t lba)
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

int storage_dev_base_t::write_blocks(
        const void *data, int64_t count, uint64_t lba, bool fua)
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

int64_t storage_dev_base_t::trim_blocks(int64_t count, uint64_t lba)
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

int storage_dev_base_t::flush()
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

int fs_base_ro_t::mknod(fs_cpath_t path,
                         fs_mode_t mode,
                         fs_dev_t rdev)
{
    (void)path;
    (void)mode;
    (void)rdev;
    // Fail, read only
    return -int(errno_t::EROFS);
}

int fs_base_ro_t::mkdir(fs_cpath_t path,
                         fs_mode_t mode)
{
    (void)path;
    (void)mode;
    // Fail, read only
    return -int(errno_t::EROFS);
}

int fs_base_ro_t::rmdir(fs_cpath_t path)
{
    (void)path;
    // Fail, read only
    return -int(errno_t::EROFS);
}

int fs_base_ro_t::symlink(
        fs_cpath_t to,
        fs_cpath_t from)
{
    (void)to;
    (void)from;
    // Fail, read only
    return -int(errno_t::EROFS);
}

int fs_base_ro_t::rename(
        fs_cpath_t from,
        fs_cpath_t to)
{
    (void)from;
    (void)to;
    // Fail, read only
    return -int(errno_t::EROFS);
}

int fs_base_ro_t::link(
        fs_cpath_t from,
        fs_cpath_t to)
{
    (void)from;
    (void)to;
    // Fail, read only
    return -int(errno_t::EROFS);
}

int fs_base_ro_t::unlink(
        fs_cpath_t path)
{
    (void)path;
    // Fail, read only
    return -int(errno_t::EROFS);
}

//
// Modify directory entries

int fs_base_ro_t::chmod(
        fs_cpath_t path,
        fs_mode_t mode)
{
    (void)path;
    (void)mode;
    // Fail, read only
    return -int(errno_t::EROFS);
}

int fs_base_ro_t::chown(
        fs_cpath_t path,
        fs_uid_t uid,
        fs_gid_t gid)
{
    (void)path;
    (void)uid;
    (void)gid;
    // Fail, read only
    return -int(errno_t::EROFS);
}

int fs_base_ro_t::truncate(
        fs_cpath_t path,
        off_t size)
{
    (void)path;
    (void)size;
    // Fail, read only
    return -int(errno_t::EROFS);
}

int fs_base_ro_t::utimens(
        fs_cpath_t path,
        const fs_timespec_t *ts)
{
    (void)path;
    (void)ts;
    // Fail, read only
    return -int(errno_t::EROFS);
}

ssize_t fs_base_ro_t::write(fs_file_info_t *fi,
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

int fs_base_ro_t::ftruncate(fs_file_info_t *fi,
                             off_t offset)
{
    (void)offset;
    (void)fi;
    // Fail, read only
    return -int(errno_t::EROFS);
}

//
// Sync files and directories and flush buffers

int fs_base_ro_t::fsync(fs_file_info_t *fi,
                         int isdatasync)
{
    (void)isdatasync;
    (void)fi;
    // Read only device, do nothing
    return 0;
}

int fs_base_ro_t::fsyncdir(fs_file_info_t *fi,
                            int isdatasync)
{
    (void)isdatasync;
    (void)fi;
    // Ignore, read only
    return 0;
}

int fs_base_ro_t::flush(fs_file_info_t *fi)
{
    (void)fi;
    // Do nothing, read only
    return 0;
}

int fs_base_ro_t::setxattr(
        fs_cpath_t path,
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
