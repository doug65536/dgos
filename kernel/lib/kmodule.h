#pragma once
#include "types.h"
#include "algorithm.h"

__BEGIN_DECLS

using module_entry_fn_t = int(*)(int __argc, char const **__argv);

int module_main(int __argc, char const * const * __argv);

void __module_register_frame(void const * const *__module_dso_handle,
                             void *__frame);

__END_DECLS

struct pci_ids_t {
    int vendor = -1;
    int device = -1;
    int dev_class = -1;
    int sub_class = -1;
    int prog_if = -1;
    char name[32];
};

#define PCI_DRIVER_BY_CLASS(name, dev_class, sub_class, prog_if) \
    _used _section(".driver") static pci_ids_t __##name##pci_ids = { \
        -1, -1, (dev_class), (sub_class), (prog_if), (#name) \
        }

#define PCI_DRIVER_BY_DEVICE(name, vendor, device) \
    _used _section(".driver") static pci_ids_t __##name##pci_ids = { \
            (vendor), (device), -1, -1, -1, (#name) \
        }

#define PCI_DRIVER(name, vendor, device, dev_class, sub_class, prog_if) \
    _used _section(".driver") static pci_ids_t __##name##pci_ids = { \
            (vendor), (device), (dev_class), (sub_class), (prog_if), (#name) \
        }

