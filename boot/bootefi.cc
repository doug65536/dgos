#include "bootefi.h"
#include "screen.h"
#include "debug.h"
#include "assert.h"
#include "fs.h"
#include "elf64.h"
#include "malloc.h"
#include "ctors.h"

EFI_HANDLE efi_image_handle;
EFI_SYSTEM_TABLE *efi_systab;

#if 1
#include "fs.h"

int x __used;

static UINTN efi_num_filesys_handles;
static EFI_HANDLE *efi_filesys_handles;

static EFI_BLOCK_IO_PROTOCOL *efi_blk_io;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *efi_simple_filesystem;
static EFI_FILE *efi_root_dir;

static EFI_GUID const efi_simple_file_system_protocol_guid = {
    0x964e5b22, 0x6459, 0x11d2, {
        0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b
    }
};

//static EFI_GUID const efi_loaded_image_proto = {
//    0x5B1B31A1, 0x9562, 0x11d2, {
//        0x8E,0x3F,0x00,0xA0,0xC9,0x69,0x72,0x3B
//    }
//};

static EFI_GUID const efi_block_io_protocol_guid = {
    0x964e5b21, 0x6459, 0x11d2, {
        0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b
    }
};

//static EFI_GUID const efi_file_guid;

struct file_handle_t {
    EFI_FILE_PROTOCOL *file;

    operator bool() const
    {
        return file != nullptr;
    }

    bool open(tchar const *filename)
    {
        EFI_STATUS status;

        status = efi_root_dir->Open(efi_root_dir, &file,
                                    filename, EFI_FILE_MODE_READ, 0);

        if (EFI_ERROR(status))
            return false;

        return true;
    }

    bool close()
    {
        EFI_STATUS status;

        status = file->Close(file);
        file = nullptr;

        return !EFI_ERROR(status);
    }

    int pread(void *buf, size_t bytes, off_t ofs)
    {
        EFI_STATUS status;

        status = file->SetPosition(file, ofs);

        if (EFI_ERROR(status))
            return -1;

        UINTN transferred = bytes;
        status = file->Read(file, &transferred, buf);

        if (EFI_ERROR(status))
            return -1;

        assert(transferred <= bytes);

        return int(transferred);
    }
};

#define MAX_OPEN_FILES 16
static file_handle_t file_handles[MAX_OPEN_FILES];

static int efi_find_unused_handle()
{
    for (size_t i = 0; i < MAX_OPEN_FILES; ++i)
        if (!file_handles[i].file)
            return i;
    return -1;
}

static int efi_open(tchar const *filename)
{
    int file = efi_find_unused_handle();

    if (file >= 0) {
        if (!file_handles[file].open(filename))
            return false;
    } else {
        return false;
    }

    return file_handles[file] ? file : -1;
}

static int efi_close(int file)
{
    assert(file >= 0 && file < MAX_OPEN_FILES);
    if (file < 0 || file >= MAX_OPEN_FILES || !file_handles[file])
        return -1;
    file_handles[file].close();
    return 0;
}

static int efi_pread(int file, void *buf, size_t bytes, off_t ofs)
{
    assert(file >= 0 && file < MAX_OPEN_FILES);
    if (file < 0 || file >= MAX_OPEN_FILES || !file_handles[file])
        return -1;
    return file_handles[file].pread(buf, bytes, ofs);
}

static uint64_t efi_boot_drv_serial()
{
    return 0;
}

extern "C" void halt(tchar const *s)
{
    efi_systab->BootServices->Exit(efi_image_handle, 1, 0, nullptr);
}

// Register EFI filesystem shim
__constructor(ctor_fs) void register_efi_fs()
{
    EFI_STATUS status;

    // Get a handle to this executable, and get the
    // simple_file_system_protocol_guid handle to this executable
    status = efi_systab->BootServices->LocateHandleBuffer(
                ByProtocol,
                &efi_simple_file_system_protocol_guid,
                nullptr,
                &efi_num_filesys_handles,
                &efi_filesys_handles);

    if (EFI_ERROR(status))
        halt(TSTR "LocateHandleBuffer failed");

    // Get the vtbl for the simple_filesystem_protocol of this executable
    status = efi_systab->BootServices->HandleProtocol(
                efi_filesys_handles[0],
            &efi_simple_file_system_protocol_guid,
            (VOID**)&efi_simple_filesystem);

    if (EFI_ERROR(status))
        halt(TSTR "HandleProtocol for simple_file_system_protocol failed");

    // Open the root directory of the volume containing this executable
    status = efi_simple_filesystem->OpenVolume(
                efi_simple_filesystem, &efi_root_dir);

    if (EFI_ERROR(status))
        halt(TSTR "OpenVolume for boot partition failed");

    status = efi_systab->BootServices->HandleProtocol(
            efi_filesys_handles[0],
            &efi_block_io_protocol_guid,
            (VOID**)&efi_blk_io);

    if (EFI_ERROR(status))
        halt(TSTR "HandleProtocol for block_io_protocol failed");

    fs_api.boot_open = efi_open;
    fs_api.boot_pread = efi_pread;
    fs_api.boot_close = efi_close;
    fs_api.boot_drv_serial = efi_boot_drv_serial;

    PRINT(TSTR "EFI FS API initialized");
}
#endif

extern "C"
EFIAPI EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
    ::efi_image_handle = image_handle;
    ::efi_systab = systab;

    PRINT(TSTR "efi_main = %" PRIxPTR, uintptr_t(efi_main));

    ctors_invoke();

    elf64_run(TSTR "dgos-kernel-generic");

    ctors_invoke();

    systab->BootServices->Exit(image_handle, 0, 0, 0);
    return EFI_SUCCESS;
}
