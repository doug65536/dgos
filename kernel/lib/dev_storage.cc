#include "dev_storage.h"

#include "printk.h"
#include "string.h"
#include "assert.h"
#include "vector.h"
#include "cpu/control_regs.h"

#define DEBUG_STORAGE   1
#if DEBUG_STORAGE
#define STORAGE_TRACE(...) printdbg("storage: " __VA_ARGS__)
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

static vector<storage_if_factory_t*> storage_factories;
static vector<storage_if_base_t*> storage_ifs;
static vector<storage_dev_base_t*> storage_devs;
static vector<part_factory_t*> part_factories;
static vector<fs_reg_t*> fs_regs;
static vector<fs_mount_t> fs_mounts;

storage_dev_base_t *open_storage_dev(dev_t dev)
{
    assert(size_t(dev) <= storage_devs.size());
    return size_t(dev) < storage_devs.size() ? storage_devs[dev] : 0;
}

void close_storage_dev(storage_dev_base_t *dev)
{
    (void)dev;
}

void storage_if_register_factory(char const *name,
                                storage_if_factory_t *factory)
{
    (void)name;

    storage_factories.push_back(factory);
    STORAGE_TRACE("Registered storage driver %s\n", name);
}

void probe_storage_factory(storage_if_factory_t *factory)
{
    STORAGE_TRACE("Probing for %s interfaces...\n", factory->name);

    // Get a list of storage devices of this type
    if_list_t if_list = factory->detect();

    STORAGE_TRACE("Found %u %s interfaces\n", if_list.count, factory->name);

    for (unsigned i = 0; i < if_list.count; ++i) {
        // Calculate pointer to storage interface instance
        storage_if_base_t *if_ = (storage_if_base_t *)
                ((char*)if_list.base + i * if_list.stride);

        // Store interface instance
        storage_ifs.push_back(if_);

        STORAGE_TRACE("Probing %s[%u] for drives...\n", factory->name, i);

        // Get a list of storage devices on this interface
        if_list_t dev_list;
        dev_list = if_->detect_devices();

        STORAGE_TRACE("Found %u %s[%u] drives\n", dev_list.count,
                      factory->name, i);

        for (unsigned k = 0; k < dev_list.count; ++k) {
            // Calculate pointer to storage device instance
            storage_dev_base_t *dev = (storage_dev_base_t*)
                    ((char*)dev_list.base +
                    k * dev_list.stride);
            // Store device instance
            storage_devs.push_back(dev);
        }
    }
}

void invoke_storage_factories(void *)
{
    for (storage_if_factory_t* factory : storage_factories)
        probe_storage_factory(factory);
}

REGISTER_CALLOUT(invoke_storage_factories, 0,
                 callout_type_t::storage_dev, "000");

void fs_register_factory(char const *name, fs_factory_t *fs)
{
    fs_regs.push_back(new fs_reg_t{ name, fs });
    printdbg("%s filesystem registered\n", name);
}

static fs_reg_t *find_fs(char const *name)
{
    for (fs_reg_t *reg : fs_regs) {
        if (strcmp(reg->name, name))
            continue;

        return reg;
    }
    return 0;
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
    if (mfs)
        fs_mounts.push_back(fs_mount_t{ fs_reg, mfs });
}

fs_base_t *fs_from_id(size_t id)
{
    return !fs_mounts.empty()
            ? fs_mounts[id].fs
            : nullptr;
}

void part_register_factory(char const *name, part_factory_t *factory)
{
    part_factories.push_back(factory);
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
                STORAGE_TRACE("Probing for %s partitions...\n", factory->name);

                if_list_t part_list = factory->detect(drive);

                // Mount partitions
                for (unsigned i = 0; i < part_list.count; ++i) {
                    part_dev_t *part = (part_dev_t*)
                            ((char*)part_list.base + part_list.stride * i);
                    fs_init_info_t info;
                    info.drive = drive;
                    info.part_st = part->lba_st;
                    info.part_len = part->lba_len;
                    fs_mount(part->name, &info);
                }
                close_storage_dev(drive);

                STORAGE_TRACE("Found %u %s partitions\n", part_list.count,
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

part_factory_t::part_factory_t(const char *factory_name)
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
    cpu_scoped_irq_disable intr_were_enabled;

    blocking_iocp_t block;
    errno_t err = read_async(data, count, lba, &block);
    if (unlikely(err != errno_t::OK))
        return -int64_t(err);
    err = block.wait();
    if (unlikely(err != errno_t::OK))
        return -int64_t(err);
    return count;
}

int storage_dev_base_t::write_blocks(
        const void *data, int64_t count, uint64_t lba, bool fua)
{
    blocking_iocp_t block;
    errno_t err = write_async(data, count, lba, fua, &block);
    err = block.wait();
    if (unlikely(err != errno_t::OK))
        return -int64_t(err);
    return count;
}
int64_t storage_dev_base_t::trim_blocks(int64_t count, uint64_t lba)
{
    blocking_iocp_t block;
    errno_t err = trim_async(count, lba, &block);
    err = block.wait();
    if (unlikely(err != errno_t::OK))
        return -int64_t(err);
    return count;
}

int storage_dev_base_t::flush()
{
    blocking_iocp_t block;
    errno_t err = flush_async(&block);
    err = block.wait();
    if (unlikely(err != errno_t::OK))
        return -int64_t(err);
    return 0;
}
