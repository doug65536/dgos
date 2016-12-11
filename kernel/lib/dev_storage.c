#include "dev_storage.h"
#include "printk.h"

static storage_if_base_t *storage_ifs;
int storage_if_count;

#define MAX_STORAGE_DEVS 64
storage_dev_base_t *storage_devs[MAX_STORAGE_DEVS];

void register_storage_if_device(const char *name, storage_if_vtbl_t *vtbl)
{
    storage_if_count = vtbl->detect(&storage_ifs);

    for (int i = 0; i < storage_if_count; ++i) {
        storage_dev_base_t *devs;

        int storage_devs;
        storage_devs = storage_ifs[i].vtbl->detect_devices(&devs);

        (void)storage_devs;
    }
    printk("%s device registered\n", name);
}
