#define STORAGE_DEV_NAME storage
#include "dev_storage.h"
#undef STORAGE_DEV_NAME

#include "printk.h"

#define MAX_STORAGE_IFS 64
storage_if_base_t *storage_ifs[MAX_STORAGE_IFS];
int storage_if_count;

#define MAX_STORAGE_DEVS 64
storage_dev_base_t *storage_devs[MAX_STORAGE_DEVS];
int storage_dev_count;

void register_storage_if_device(const char *name, storage_if_vtbl_t *vtbl)
{
    // Get a list of storage devices of this type
    storage_if_list_t if_list = vtbl->detect();

    for (unsigned i = 0; i < if_list.count; ++i) {
        // Calculate pointer to storage interface instance
        storage_if_base_t *if_ = (void*)
                ((char*)if_list.base + i * if_list.stride);
        // Store interface instance
        storage_ifs[storage_if_count++] = if_;

        // Get a list of storage devices on this interface
        storage_dev_list_t dev_list;
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
