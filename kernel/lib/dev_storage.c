#define STORAGE_DEV_NAME storage
#include "dev_storage.h"
#undef STORAGE_DEV_NAME

#include "printk.h"

#define MAX_STORAGE_IFS 64
static storage_if_base_t *storage_ifs[MAX_STORAGE_IFS];
static int storage_if_count;

#define MAX_STORAGE_DEVS 64
static storage_dev_base_t *storage_devs[MAX_STORAGE_DEVS];
static int storage_dev_count;

#define MAX_PART_DEVS   4
static part_vtbl_t *part_devs[MAX_PART_DEVS];
static int part_dev_count;

storage_dev_base_t *open_storage_dev(dev_t dev)
{
    return storage_devs[dev];
}

void close_storage_dev(storage_dev_base_t *dev)
{
    (void)dev;
}

void register_storage_if_device(char const *name,
                                storage_if_vtbl_t *vtbl)
{
    // Get a list of storage devices of this type
    if_list_t if_list = vtbl->detect();

    for (unsigned i = 0; i < if_list.count; ++i) {
        // Calculate pointer to storage interface instance
        storage_if_base_t *if_ = (void*)
                ((char*)if_list.base + i * if_list.stride);
        // Store interface instance
        storage_ifs[storage_if_count++] = if_;

        // Get a list of storage devices on this interface
        if_list_t dev_list;
        dev_list = if_->vtbl->detect_devices(if_);

        for (unsigned i = 0; i < dev_list.count; ++i) {
            // Calculate pointer to storage device instance
            storage_dev_base_t *dev = (void*)
                    ((char*)dev_list.base +
                    i * dev_list.count);
            // Store device instance
            storage_devs[storage_dev_count++] = dev;
        }
    }
    printk("%s device registered\n", name);
}

void register_part_device(const char *name, part_vtbl_t *vtbl)
{
    if (part_dev_count < MAX_PART_DEVS) {
        part_devs[part_dev_count++] = vtbl;

        for (int dev = 0; dev < storage_dev_count; ++dev) {
            storage_dev_base_t *drive = open_storage_dev(dev);
            if (drive)
                vtbl->detect(drive);
            close_storage_dev(drive);
        }
    }
    printk("%s device registered\n", name);
}
