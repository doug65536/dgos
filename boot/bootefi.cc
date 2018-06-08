#include "bootefi.h"
#include "fs.h"

int x __used;

static EFI_HANDLE image_handle;
static EFI_SYSTEM_TABLE *systab;

UINTN efi_num_filesys_handles;
EFI_HANDLE *efi_filesys_handles;

static EFI_GUID efi_simple_file_system_protocol_guid = {
    0x964e5b22, 0x6459, 0x11d2, {
        0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b
    }
};

static EFI_GUID efi_loaded_image_proto = {
    0x5B1B31A1, 0x9562, 0x11d2, {
        0x8E,0x3F,0x00,0xA0,0xC9,0x69,0x72,0x3B
    }
};

static EFI_GUID efi_block_io_protocol_guid = {
    0x964e5b21, 0x6459, 0x11d2, {
        0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b
    }
};

static EFI_GUID efi_file_guid;

static int efi_open(char const *filename)
{
}

static int efi_close(int file)
{

}

static int efi_pread(int file, void *buf, size_t bytes, off_t ofs)
{

}

static uint64_t efi_boot_drv_serial()
{

}

// Register EFI filesystem shim
bool register_efi_fs()
{
    EFI_STATUS status;
    EFI_BLOCK_IO_PROTOCOL *blk_io;

    status = systab->BootServices->LocateHandleBuffer(
                ByProtocol,
                &efi_simple_file_system_protocol_guid,
                nullptr,
                &efi_num_filesys_handles,
                (VOID***)&efi_filesys_handles);

    if (EFI_ERROR(status))
        return false;

    status = systab->BootServices->HandleProtocol(
            efi_filesys_handles[0],
            &efi_block_io_protocol_guid,
            (VOID**)&blk_io);

    if (EFI_ERROR(status))
        return false;

    fs_api.boot_open = efi_open;
    fs_api.boot_pread = efi_pread;
    fs_api.boot_close = efi_close;
    fs_api.boot_drv_serial = efi_boot_drv_serial;

    return true;
}

extern "C" void halt(tchar const *s)
{
    systab->BootServices->Exit(image_handle, 1, 0, nullptr);
}

extern "C"
EFIAPI EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
    ::image_handle = image_handle;
    ::systab = systab;
    register_efi_fs();

    systab->ConOut->ClearScreen(systab->ConOut);
    uint64_t volatile delay = 0x10000000;
    while (--delay);
    x = 42;
    systab->BootServices->Exit(image_handle, 0, 0, 0);
    return EFI_SUCCESS;
}
