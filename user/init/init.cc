#include <sys/module.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>

__thread int testtls = 42;

void verr(char const *format, va_list ap)
{
    printf("Error:\n");
    vprintf(format, ap);
}

void err(char const *format, ...)
{
    va_list ap;
    va_start(ap, format);
    verr(format, ap);
    va_end(ap);
}

void load_module(char const *path)
{
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

    int status = init_module(mem, sz, path, nullptr);

    if (status < 0)
        err("Module failed to initialize with %d %d\n", status, errno);

    close(fd);
}

int main(int argc, char **argv, char **envp)
{
    load_module("keyb8042.km");
    load_module("usbxhci.km");
    load_module("fat32.km");
    load_module("iso9660.km");
    load_module("nvme.km");
    load_module("ahci.km");
    load_module("ide.km");
    load_module("gpt.km");
    load_module("mbr.km");
    load_module("rtl8139.km");
}
