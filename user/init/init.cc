#include <sys/module.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

__thread int testtls = 42;

__attribute__((__format__(__printf__, 1, 0), __noreturn__))
void verr(char const *format, va_list ap)
{
    printf("Error:\n");
    vprintf(format, ap);
    exit(1);
}

__attribute__((__format__(__printf__, 1, 2), __noreturn__))
void err(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    verr(format, ap);
    va_end(ap);
}

void load_module(char const *path, char const *parameters = nullptr)
{
    if (!parameters)
        parameters = "";

    int fd = open(path, O_EXCL | O_RDONLY);
    if (fd < 0)
        err("Cannot open %s\n", path);

    off_t sz = lseek(fd, 0, SEEK_END);
    if (sz < 0)
        err("Cannot seek to end of module\n");

    if (lseek(fd, 0, SEEK_SET) != 0)
        err("Cannot seek to start of module\n");

    void *mem = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
    if (mem == MAP_FAILED)
        err("Cannot allocate %zd bytes\n", sz);

    if (sz != read(fd, mem, sz))
        err("Cannot read %zd bytes\n", sz);

    int status = init_module(mem, sz, path, nullptr, parameters);

    if (status < 0)
        err("Module failed to initialize with %d %d\n", status, errno);

    close(fd);
}

int main(int argc, char **argv, char **envp)
{
    // fixme: check ACPI
    load_module("keyb8042.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_SERIAL,
                      PCI_SUBCLASS_SERIAL_USB,
                      PCI_PROGIF_SERIAL_USB_XHCI) > 0)
        load_module("usbxhci.km");

    load_module("fat32.km");
    load_module("iso9660.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_NVM,
                      PCI_PROGIF_STORAGE_NVM_NVME) > 0)
        load_module("nvme.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_SATA,
                      PCI_PROGIF_STORAGE_SATA_AHCI) > 0)
        load_module("ahci.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_ATA, -1))
        load_module("ide.km");

    load_module("gpt.km");
    load_module("mbr.km");

    if (probe_pci_for(0x10EC, 0x8139,
                      PCI_DEV_CLASS_NETWORK,
                      PCI_SUBCLASS_NETWORK_ETHERNET, -1))
        load_module("rtl8139.km");

    load_module("unittest.km");
}
