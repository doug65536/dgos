#pragma once
#include "types.h"
#include "export.h"
#include "algorithm.h"

__BEGIN_DECLS

HIDDEN extern uint8_t * const ___rodata_fixup_insn_st[];
HIDDEN extern uint8_t * const ___rodata_fixup_insn_en[];

extern "C"
KERNEL_API void cpu_apply_fixups(uint8_t * const *rodata_st,
                                 uint8_t * const *rodata_en);

using module_entry_fn_t = int(*)(int __argc, char const **__argv);

int module_main(int __argc, char const * const * __argv) _used;

KERNEL_API void __module_register_frame(
        void const * const *__module_dso_handle,
        void *__frame, size_t size);
KERNEL_API void __module_unregister_frame(
        void const * const *__module_dso_handle,
        void *__frame);

void __register_frame(void *addr, size_t size);
void __deregister_frame(void *addr);

__END_DECLS

struct pci_ids_t {
    int vendor = -1;
    int device = -1;
    int dev_class = -1;
    int sub_class = -1;
    int prog_if = -1;
    char name[32];
};

#define PCI_DRIVER(name, vendor, device, dev_class, sub_class, prog_if) \
     static _used _section(".driver") pci_ids_t __##name##pci_ids = { \
            (vendor), (device), (dev_class), (sub_class), (prog_if), (#name) \
        }

#define PCI_DRIVER_BY_CLASS(name, dev_class, sub_class, prog_if) \
    PCI_DRIVER(name, -1, -1, dev_class, sub_class, prog_if)

#define PCI_DRIVER_BY_DEVICE(name, vendor, device) \
    PCI_DRIVER(name, vendor, device, -1, -1, -1)

#define USB_DRIVER(name, vendor, device, dev_class, sub_class) \
    PCI_DRIVER(name, vendor, device, dev_class, sub_class, -2)
