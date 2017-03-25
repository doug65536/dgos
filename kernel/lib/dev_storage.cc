#define STORAGE_DEV_NAME storage
#include "dev_storage.h"
#undef STORAGE_DEV_NAME

#include "printk.h"
#include "string.h"
#include "assert.h"

#define MAX_STORAGE_IFS 64
static storage_if_base_t *storage_ifs[MAX_STORAGE_IFS];
static unsigned storage_if_count;

#define MAX_STORAGE_DEVS 64
static storage_dev_base_t *storage_devs[MAX_STORAGE_DEVS];
static unsigned storage_dev_count;

#define MAX_PART_FACTORIES   4
static part_factory_t *part_factories[MAX_PART_FACTORIES];
static int part_factory_count;

typedef struct fs_reg_t {
    char const *name;
    fs_factory_t *factory;
} fs_reg_t;

#define MAX_FS_REGS     16
static fs_reg_t fs_regs[MAX_FS_REGS];
static int fs_reg_count;

typedef struct fs_mount_t {
    fs_reg_t *reg;
    fs_base_t *fs;
} fs_mount_t;

#define MAX_MOUNTS      16
static fs_mount_t fs_mounts[MAX_MOUNTS];
static unsigned fs_mount_count;

storage_dev_base_t *open_storage_dev(dev_t dev)
{
    assert(dev >= 0 && (unsigned)dev <= storage_dev_count);
    return (unsigned)dev < storage_dev_count ? storage_devs[dev] : 0;
}

void close_storage_dev(storage_dev_base_t *dev)
{
    (void)dev;
}

void register_storage_if_device(char const *name,
                                storage_if_factory_t *factory)
{
    // Get a list of storage devices of this type
    if_list_t if_list = factory->detect();

    for (unsigned i = 0; i < if_list.count; ++i) {
        // Calculate pointer to storage interface instance
        storage_if_base_t *if_ = (storage_if_base_t *)
                ((char*)if_list.base + i * if_list.stride);

        assert(storage_if_count < countof(storage_ifs));

        // Store interface instance
        storage_ifs[storage_if_count++] = if_;

        // Get a list of storage devices on this interface
        if_list_t dev_list;
        dev_list = if_->detect_devices();

        for (unsigned k = 0; k < dev_list.count; ++k) {
            // Calculate pointer to storage device instance
            storage_dev_base_t *dev = (storage_dev_base_t*)
                    ((char*)dev_list.base +
                    k * dev_list.stride);
            assert(storage_dev_count < countof(storage_devs));

            // Store device instance
            storage_devs[storage_dev_count++] = dev;
        }
    }
    printdbg("%s device registered\n", name);
}

void register_fs_device(const char *name, fs_factory_t *fs)
{
    if (fs_reg_count < MAX_FS_REGS) {
        fs_regs[fs_reg_count].name = name;
        fs_regs[fs_reg_count].factory = fs;
        ++fs_reg_count;
        printdbg("%s filesystem registered\n", name);
    }
}

static fs_reg_t *find_fs(char const *name)
{
    for (int i = 0; i < fs_reg_count; ++i) {
        if (strcmp(fs_regs[i].name, name))
            continue;

        return fs_regs + i;
    }
    return 0;
}

void fs_mount(char const *fs_name, fs_init_info_t *info)
{
    fs_reg_t *fs_reg = find_fs(fs_name);

    // FIXME: why is this needed
    if (fs_reg == 0)
        return;

    assert(fs_reg != 0);

    fs_base_t *mfs = fs_reg->factory->mount(info);
    if (mfs && fs_mount_count < countof(fs_mounts)) {
        fs_mounts[fs_mount_count].fs = mfs;
        fs_mounts[fs_mount_count].reg = fs_reg;
        ++fs_mount_count;
    }
}

fs_base_t *fs_from_id(size_t id)
{
    return fs_mounts[id].fs;
}

void register_part_factory(const char *name, part_factory_t *factory)
{
    if (part_factory_count < MAX_PART_FACTORIES) {
        part_factories[part_factory_count++] = factory;

        for (unsigned dev = 0; dev < storage_dev_count; ++dev) {
            storage_dev_base_t *drive = open_storage_dev(dev);
            if (drive) {
                if_list_t part_list = factory->detect(drive);

                // Mount partitions
                for (unsigned i = 0; i < part_list.count; ++i) {
                    part_dev_t *part = (part_dev_t*)((char*)part_list.base +
                                               part_list.stride * i);
                    fs_init_info_t info;
                    info.drive = drive;
                    info.part_st = part->lba_st;
                    info.part_len = part->lba_len;
                    fs_mount(part->name, &info);
                }
                close_storage_dev(drive);
            }
        }
    }
    printk("%s partition type registered\n", name);
}

fs_factory_t::fs_factory_t(char const *name)
{
    register_fs_device(name, this);
}

storage_if_factory_t::storage_if_factory_t(const char *name)
{
    register_storage_if_device(name, this);
}
