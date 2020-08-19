#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <fcntl.h>
#include <spawn.h>
#include <dirent.h>
#include <pthread.h>
#include <surface.h>

#include <sys/mman.h>
#include <sys/likely.h>
#include <sys/module.h>

#include "frameserver.h"

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
        needed[0] = 0;
        status = init_module(mem, sz, path, nullptr, parameters, needed);

        if (needed[0] != 0) {
            size_t len = strlen(needed);
            memmove(needed + 5, needed, len + 1);
            memcpy(needed, "boot/", 5);
            load_module(needed);
        }
    } while (needed[0]);
    free(needed);

    if (unlikely(status < 0))
        err("Module failed to initialize with %d %d\n", status, errno);

    close(fd);
}


static void *stress_fs(void *)
{
    DIR *dir = opendir("/");

    dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        printf("%s\n", ent->d_name);
    }

    closedir(dir);

    // Create this many files
    size_t iters = 10000;

    // Keep the number of files that exist less than or equal to this many
    size_t depth = 400;

    int mds = mkdir("stress", 0755);

    if (mds < 0 && errno == EROFS) {
        printf("Cannot run mkdir test on readonly filesystem\n");
        return nullptr;
    }

    if (mds < 0) {
        printf("mkdir failed\n");
        return nullptr;
    }

    for (size_t i = 0; i < iters + depth; ++i) {
        char name[NAME_MAX];
        if (i < iters) {
            int name_len = snprintf(name, sizeof(name), "stress/name%zu", i);
            printf("Creating %s\n", name);
            int fd = open(name, O_CREAT | O_EXCL);
            if (write(fd, name, name_len) != name_len)
                printf("Write error writing \"%s\" to fd %d\n", name, fd);
            close(fd);
        }

        if (i >= depth) {
            snprintf(name, sizeof(name), "stress/name%zu", i - depth);
            printf("Unlinking %s\n", name);
            unlink(name);
        }
    }

    return nullptr;
}

void start_fs_stress()
{
    pthread_t stress_tid{};
    int sts = pthread_create(&stress_tid, nullptr, stress_fs, nullptr);

    if (sts != 0)
        printf("pthread_create failed\n");
}

//void start_mouse_thread()
//{
//    pthread_t mouse_thread{};
//    int err = pthread_create(&mouse_thread, nullptr, mouse_test, nullptr);
//    if (unlikely(err))
//        printf("Error creating mouse thread\n");
//}

int main(int argc, char **argv, char **envp)
{
    printf("init startup complete\n");

    load_module("boot/unittest.km");

    // fixme: check ACPI
    load_module("boot/keyb8042.km");

    load_module("boot/ext4.km");
    load_module("boot/fat32.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_SERIAL,
                      PCI_SUBCLASS_SERIAL_USB,
                      PCI_PROGIF_SERIAL_USB_XHCI) > 0)
        load_module("boot/usbxhci.km");

    load_module("boot/usbmsc.km");

    //mouse_test();

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_NVM,
                      PCI_PROGIF_STORAGE_NVM_NVME) > 0)
        load_module("boot/nvme.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_SATA,
                      PCI_PROGIF_STORAGE_SATA_AHCI) > 0)
        load_module("boot/ahci.km");

    if (probe_pci_for(0x1AF4, -1,
                      PCI_DEV_CLASS_STORAGE,
                      -1,
                      -1) > 0)
        load_module("boot/virtio-blk.km");

    if (probe_pci_for(0x1AF4, -1,
                      PCI_DEV_CLASS_DISPLAY,
                      -1,
                      -1) > 0)
        load_module("boot/virtio-gpu.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_STORAGE,
                      PCI_SUBCLASS_STORAGE_ATA, -1))
        load_module("boot/ide.km");


    load_module("boot/iso9660.km");
    load_module("boot/gpt.km");
    load_module("boot/mbr.km");

    if (probe_pci_for(0x10EC, 0x8139,
                      PCI_DEV_CLASS_NETWORK,
                      PCI_SUBCLASS_NETWORK_ETHERNET, -1))
        load_module("boot/rtl8139.km");

    if (probe_pci_for(-1, -1,
                      PCI_DEV_CLASS_MULTIMEDIA,
                      PCI_SUBCLASS_MULTIMEDIA_AUDIO, -1))
        load_module("boot/ide.km");

    load_module("boot/symsrv.km");

    for (size_t iter = 0; iter < 16; ++iter) {
        uint64_t st = __builtin_ia32_rdtsc();
        for (size_t i = 0; i < 1000000; ++i)
            raise(42);
        uint64_t en = __builtin_ia32_rdtsc();
        uint64_t el = en - st;
        printf("One million syscalls in %lu cycles (%lu/call)\n",
               el, el/1000000);
    }

    //start_fs_stress();

    //start_mouse_thread();

    return start_framebuffer();
}

