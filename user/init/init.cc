#include <sys/module.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/likely.h>
#include <spawn.h>
#include <dirent.h>
#include <string.h>
#include <inttypes.h>

template<typename T, size_t hw_vec_sz = 64>
struct aosoa_t {
    typedef T vector_t __attribute__((__vector_size__(hw_vec_sz / sizeof(T)),
                                      __aligned__(16)));
    static constexpr const size_t row_count = hw_vec_sz / sizeof(vector_t);
};

using aosoa_int_t = aosoa_t<int>;
using aosoa_float_t = aosoa_t<float>;

extern "C"
__attribute__((__target_clones__("default,avx2"),
               __visibility__("default"),
               __used__,
               __optimize__("-O3")))
void do_aosoa(aosoa_int_t::vector_t * restrict c,
              aosoa_int_t::vector_t const * restrict a,
              aosoa_int_t::vector_t const * restrict b,
              size_t n)
{
    for (size_t i = 0; i < n; i += aosoa_float_t::row_count) {
        for (size_t vec = 0; vec < aosoa_float_t::row_count; ++vec)
            c[i+vec] = a[i+vec] + b[i+vec];
    }
}

__BEGIN_DECLS
#include <stddef.h>
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
    if (unlikely(fd < 0))
        err("Cannot open %s\n", path);

    off_t sz = lseek(fd, 0, SEEK_END);
    if (unlikely(sz < 0))
        err("Cannot seek to end of module\n");

    if (unlikely(lseek(fd, 0, SEEK_SET) != 0))
        err("Cannot seek to start of module\n");

    void *mem = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
    if (unlikely(mem == MAP_FAILED))
        err("Cannot allocate %" PRIu64 "d bytes\n", sz);

    if (unlikely(sz != read(fd, mem, sz)))
        err("Cannot read %" PRIu64 " bytes\n", sz);

    int status;
    char *needed = (char*)malloc(NAME_MAX);
    do {
        strcpy(needed, "what the fuck");
        needed[0] = 0;
        status = init_module(mem, sz, path, nullptr, parameters, needed);

        if (needed[0] != 0) {
            load_module(needed);
        }
    } while (needed[0]);
    free(needed);

    if (unlikely(status < 0))
        err("Module failed to initialize with %d %d\n", status, errno);

    close(fd);
}

int main(int argc, char **argv, char **envp)
{
    aosoa_int_t::vector_t *c = new aosoa_int_t::vector_t[
            64 / sizeof(aosoa_int_t::vector_t)];
    aosoa_int_t::vector_t *a = new aosoa_int_t::vector_t[
            64 / sizeof(aosoa_int_t::vector_t)];
    aosoa_int_t::vector_t *b = new aosoa_int_t::vector_t[
            64 / sizeof(aosoa_int_t::vector_t)];
    __asm__ __volatile__ ("" : : : "memory");
    do_aosoa(c, a, b, 64 / sizeof(aosoa_int_t::vector_t));

    DIR *dir = opendir("/");

    dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        printf("%s\n", ent->d_name);
    }

    closedir(dir);

//    dir = opendir("/dev/something");

    load_module("symsrv.km");

    load_module("unittest.km");

    // fixme: check ACPI
    load_module("keyb8042.km");

    load_module("ext4.km");
    load_module("fat32.km");
    load_module("iso9660.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_SERIAL,
                      PCI_SUBCLASS_SERIAL_USB,
                      PCI_PROGIF_SERIAL_USB_XHCI) > 0)
        load_module("usbxhci.km");

    load_module("usbmsc.km");

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

    if (probe_pci_for(0x1AF4, -1,
                      PCI_DEV_CLASS_STORAGE,
                      -1,
                      -1) > 0)
        load_module("virtio-blk.km");

    if (probe_pci_for(0x1AF4, -1,
                      PCI_DEV_CLASS_DISPLAY,
                      -1,
                      -1) > 0)
        load_module("virtio-gpu.km");

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

}
__END_DECLS
