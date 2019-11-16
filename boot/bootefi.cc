#include "bootefi.h"
#include "string.h"
#include "screen.h"
#include "debug.h"
#include "assert.h"
#include "fs.h"
#include "elf64.h"
#include "malloc.h"
#include "ctors.h"
#include "cpu.h"
#include "utf.h"
#include "bootmenu.h"
#include "../kernel/lib/bswap.h"
#include "halt.h"

EFI_HANDLE efi_image_handle;
EFI_SYSTEM_TABLE *efi_systab;

void * __dso_handle = &__dso_handle;

extern "C" void __cxa_atexit()
{
}

#if 1
#include "fs.h"

static EFI_BLOCK_IO_PROTOCOL *efi_blk_io;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *efi_simple_filesystem;
static EFI_PXE_BASE_CODE_PROTOCOL *efi_pxe;
static EFI_FILE *efi_root_dir;

static EFI_GUID const efi_simple_file_system_protocol_guid = {
    0x964e5b22, 0x6459, 0x11d2, {
        0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b
    }
};

static EFI_GUID const efi_loaded_image_protocol_guid = {
    0x5B1B31A1, 0x9562, 0x11d2, {
        0x8E,0x3F,0x00,0xA0,0xC9,0x69,0x72,0x3B
    }
};

static EFI_GUID const efi_block_io_protocol_guid = {
    0x964e5b21, 0x6459, 0x11d2, {
        0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b
    }
};

static EFI_GUID const efi_file_system_info_guid = EFI_FILE_SYSTEM_INFO_GUID;

static EFI_GUID const efi_pxe_base_code_protocol =
        EFI_PXE_BASE_CODE_PROTOCOL_GUID;

static EFI_GUID const efi_mtftp4_protocol_guid = EFI_MTFTP4_PROTOCOL_GUID;

class file_handle_base_t;
class efi_fs_file_handle_t;
class efi_pxe_file_handle_t;

class file_handle_base_t {
public:
    static int open(tchar const* filename);
    static off_t filesize(int fd);
    static int close(int fd);
    static ssize_t pread(int fd, void *buf, size_t bytes, off_t ofs);
    static uint64_t boot_drv_serial();

    static constexpr int MAX_OPEN_FILES = 16;
protected:
    static file_handle_base_t *file_handles[];

    virtual ~file_handle_base_t() = 0;
    virtual bool open_impl(tchar const *filename) = 0;
    virtual off_t filesize_impl() = 0;
    virtual bool close_impl() = 0;
    virtual bool pread_impl(void *buf, size_t bytes, off_t ofs) = 0;

private:
    static int find_unused_handle()
    {
        for (size_t i = 0; i < MAX_OPEN_FILES; ++i)
            if (!file_handles[i])
                return i;
        return -1;
    }
};

file_handle_base_t *
file_handle_base_t::file_handles[file_handle_base_t::MAX_OPEN_FILES];

static EFI_GUID efi_file_info_guid = EFI_FILE_INFO_GUID;

struct efi_fs_file_handle_t : public file_handle_base_t {
    EFI_FILE_PROTOCOL *file = nullptr;
    UINT64 file_size = 0;

    bool open_impl(tchar const *filename) override final
    {
        EFI_STATUS status;

        status = efi_root_dir->Open(efi_root_dir, &file,
                                    filename, EFI_FILE_MODE_READ, 0);

        if (unlikely(EFI_ERROR(status)))
            return false;

        UINTN buf_sz = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 256;
        auto info_buffer = (EFI_FILE_INFO*)malloc(buf_sz);

        if (unlikely(!info_buffer))
            return false;

        status = file->GetInfo(file, &efi_file_info_guid,
                               &buf_sz, info_buffer);

        if (unlikely(EFI_ERROR(status)))
            return false;

        file_size = info_buffer->FileSize;

        return true;
    }

    off_t filesize_impl() override final
    {
        return file_size;
    }

    bool close_impl() override final
    {
        EFI_STATUS status;

        status = file->Close(file);
        file = nullptr;

        return !EFI_ERROR(status);
    }

    _use_result
    bool pread_impl(void *buf, size_t bytes, off_t ofs) override final
    {
        EFI_STATUS status;

        status = file->SetPosition(file, ofs);

        if (unlikely(EFI_ERROR(status)))
            return -1;

        UINTN transferred = bytes;
        status = file->Read(file, &transferred, buf);

        if (unlikely(EFI_ERROR(status)))
            return false;

        assert(transferred <= bytes);

        return true;
    }
};

class efi_pxe_file_handle_t : public file_handle_base_t {
public:
    efi_pxe_file_handle_t()
        : data(nullptr)
        , file_size(0)
    {
    }

    ~efi_pxe_file_handle_t()
    {
        if (data)
            efi_systab->BootServices->FreePool(data);
    }

private:
    friend void register_efi_fs();
    static void initialize()
    {

        EFI_PXE_BASE_CODE_PACKET const *dhcp_packet = nullptr;

        static_assert(sizeof(server_addr.ipv4) ==
                      sizeof(efi_pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr),
                      "Unexpected size mismatch");

        if (!efi_pxe->Mode->UsingIpv6) {
            if (!efi_pxe->Mode->ProxyOfferReceived) {
                // Use the primary DHCP discover data, no proxy server replied
                dhcp_packet = &efi_pxe->Mode->DhcpAck;
            } else {
                dhcp_packet = &efi_pxe->Mode->ProxyOffer;
            }
        } else {
            PANIC(TSTR "Don't know how to handle IPv6 PXE");
        }

        if (unlikely(!dhcp_packet))
            PANIC(TSTR "Unable to process DHCP packet");

        memcpy(&server_addr.ipv4, &dhcp_packet->Dhcpv4.BootpSiAddr,
               sizeof(server_addr.ipv4));

        // Seriously? I have to parse the DHCP option bytes to get
        // the TFTP client and server ports? Come on!

        // 69 is the standard TFTP server port
        // 57344 is in the center of the IANA ephemeral port range
        // Assumption until optionally overridden by BOOTP vendor options
        mtftp_info.SPort = htons(69);
        mtftp_info.CPort = htons(57344);
        mtftp_info.ListenTimeout = 2;
        mtftp_info.TransmitTimeout = 2;

        // Find vendor options option
        for (UINT8 const *dhcp_option =
             efi_pxe->Mode->DhcpAck.Dhcpv4.DhcpOptions,
             *dhcp_options_end = dhcp_option + countof(
                 efi_pxe->Mode->DhcpAck.Dhcpv4.DhcpOptions);
             dhcp_option < dhcp_options_end && dhcp_option[0] != 255;
             dhcp_option += dhcp_option[1] + 2) {
            switch (dhcp_option[0]) {
            case 54:    // server identifier
                memcpy(server_addr.ipv4.addr, dhcp_option + 2,
                       sizeof(server_addr.ipv4.addr));
                break;

            case 43:
                for (UINT8 const *vendor_option = dhcp_option + 2,
                     *vendor_options_end = vendor_option + vendor_option[1];
                     vendor_option < vendor_options_end &&
                     dhcp_option[0] != 255;
                     dhcp_option += dhcp_option[1] + 2) {
                    switch (vendor_option[0]) {
                    case 2:     // TFTP client port
                        mtftp_info.CPort = (dhcp_option[2] << 8) |
                                dhcp_option[3];
                        break;
                    case 3:     // TFTP server port
                        mtftp_info.SPort = (dhcp_option[2] << 8) |
                                dhcp_option[3];
                        break;

                    }
                }
                break;

            }
        }
    }

    bool open_impl(tchar const *filename) override final
    {
        EFI_STATUS status;

        //uint64_t file_size_size = sizeof(file_size);

        char *utf8_filename = (char*)malloc(strlen(filename) * 4 + 1);

        if (unlikely(!utf8_filename))
            return false;

        char *out = utf8_filename;
        tchar const *in = filename;
        int codepoint;
        do {
            codepoint = utf16_to_ucs4(in, &in);
            out += ucs4_to_utf8(out, codepoint);
        } while (codepoint);

        status = efi_pxe->Mtftp(
                    efi_pxe, EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
                    nullptr, false, &file_size, nullptr,
                    &server_addr, (UINT8*)utf8_filename,
                    &mtftp_info, false);

        if (unlikely(EFI_ERROR(status)))
            return false;

        status = efi_systab->BootServices->AllocatePool(
                    EfiRuntimeServicesData, file_size, &data);

        if (unlikely(EFI_ERROR(status)))
            return false;

        UINTN block_size = 4096;

        status = efi_pxe->Mtftp(
                    efi_pxe, EFI_PXE_BASE_CODE_TFTP_READ_FILE,
                    data, false, &file_size, &block_size,
                    &server_addr, (UINT8*)utf8_filename,
                    &mtftp_info, false);

        free(utf8_filename);

        if (unlikely(EFI_ERROR(status)))
            return false;

        return true;
    }

    off_t filesize_impl() override final
    {
        return file_size;
    }

    bool close_impl() override final
    {
        return true;
    }

    bool pread_impl(void *buf, size_t bytes, off_t ofs) override final
    {
        if (bytes + ofs > file_size)
            return false;

        memcpy(buf, (char*)data + ofs, bytes);

        return true;
    }

    void *data;
    uint64_t file_size;

    static EFI_PXE_BASE_CODE_MTFTP_INFO mtftp_info;
    static EFI_IP_ADDRESS server_addr;
};

EFI_PXE_BASE_CODE_MTFTP_INFO efi_pxe_file_handle_t::mtftp_info;
EFI_IP_ADDRESS efi_pxe_file_handle_t::server_addr;

extern "C" void halt(tchar const *s)
{
    efi_systab->BootServices->Exit(efi_image_handle, 1, 0, nullptr);
}

// Register EFI filesystem shim
_constructor(ctor_fs) void register_efi_fs()
{
    EFI_STATUS status;

    EFI_LOADED_IMAGE_PROTOCOL *efi_loaded_image = nullptr;

    status = efi_systab->BootServices->HandleProtocol(
                efi_image_handle,
                &efi_loaded_image_protocol_guid,
                (VOID**)&efi_loaded_image);

    if (unlikely(EFI_ERROR(status)))
        PANIC(TSTR "HandleProtocol LOADED_IMAGE_PROTOCOL failed");

    // Get the vtbl for the simple_filesystem_protocol of this executable
    status = efi_systab->BootServices->HandleProtocol(
                efi_loaded_image->DeviceHandle,
                &efi_simple_file_system_protocol_guid,
                (VOID**)&efi_simple_filesystem);

    // If we were unable to get an EFI_SIMPLE_FILE_SYSTEM_PROTOCOL for the
    // image, fall back to PXE TFTP
    if (!EFI_ERROR(status)) {
        // Open the root directory of the volume containing this executable
        status = efi_simple_filesystem->OpenVolume(
                    efi_simple_filesystem, &efi_root_dir);

        if (unlikely(EFI_ERROR(status)))
            PANIC(TSTR "OpenVolume for boot partition failed");

        status = efi_systab->BootServices->HandleProtocol(
                efi_loaded_image->DeviceHandle,
                &efi_block_io_protocol_guid,
                (VOID**)&efi_blk_io);

        if (unlikely(EFI_ERROR(status)))
            PANIC(TSTR "HandleProtocol for block_io_protocol failed");
    } else {
        status = efi_systab->BootServices->HandleProtocol(
                    efi_loaded_image->DeviceHandle,
                    &efi_pxe_base_code_protocol,
                    (VOID**)&efi_pxe);

        if (unlikely(EFI_ERROR(status)))
            PANIC(TSTR "HandleProtocol LOADED_IMAGE_PROTOCOL failed");

        efi_pxe_file_handle_t::initialize();
        PRINT("EFI PXE API initialized");
    }

    fs_api.boot_open = file_handle_base_t::open;
    fs_api.boot_filesize = file_handle_base_t::filesize;
    fs_api.boot_pread = file_handle_base_t::pread;
    fs_api.boot_close = file_handle_base_t::close;
    fs_api.boot_drv_serial = file_handle_base_t::boot_drv_serial;

    PRINT("EFI FS API initialized");
}
#endif

int file_handle_base_t::open(tchar const *filename)
{
    int fd = find_unused_handle();

    if (fd < 0)
        return fd;

    if (!efi_pxe) {
        file_handles[fd] = new (std::nothrow) efi_fs_file_handle_t;
    } else {
        file_handles[fd] = new (std::nothrow) efi_pxe_file_handle_t;
    }

    if (!file_handles[fd]->open_impl(filename)) {
        delete file_handles[fd];
        file_handles[fd] = nullptr;
        return -1;
    }

    return fd;
}

static bool check_fd(int fd)
{
    bool ok = fd >= 0 && fd < file_handle_base_t::MAX_OPEN_FILES;
    assert(fd >= 0 && fd < file_handle_base_t::MAX_OPEN_FILES);
    return ok;
}

off_t file_handle_base_t::filesize(int fd)
{
    if (!check_fd(fd))
        return -1;

    off_t result = file_handles[fd]->filesize_impl();

    return result;
}

int file_handle_base_t::close(int fd)
{
    if (!check_fd(fd))
        return -1;

    bool result = file_handles[fd]->close_impl();
    delete file_handles[fd];
    file_handles[fd] = nullptr;
    return result ? 0 : -1;
}

ssize_t file_handle_base_t::pread(int fd, void *buf, size_t bytes, off_t ofs)
{
    if (!check_fd(fd))
        return -1;

    if (!file_handles[fd]->pread_impl(buf, bytes, ofs))
        return -1;

    return bytes;
}

uint64_t file_handle_base_t::boot_drv_serial()
{
    if (!efi_blk_io)
        return -1;

    EFI_STATUS status;

    {
        size_t constexpr const sz = 512;
        void *mem = calloc(1, sz);
        if (unlikely(mem == nullptr))
            PANIC_OOM();
        EFI_FILE_SYSTEM_INFO *info = new (mem) EFI_FILE_SYSTEM_INFO;

        UINTN info_buffer_size = sz;
        status = efi_root_dir->GetInfo(
                    efi_root_dir, &efi_file_system_info_guid,
                    &info_buffer_size, info);

        info->~EFI_FILE_SYSTEM_INFO();
        free(info);
    }

    char *buffer = (char*)malloc(efi_blk_io->Media->BlockSize);

    if (unlikely(!buffer))
        return -1;

    status = efi_blk_io->ReadBlocks(efi_blk_io, efi_blk_io->Media->MediaId,
                                    0, efi_blk_io->Media->BlockSize, buffer);

    uint64_t serial = 0;
    if (likely(!EFI_ERROR(status))) {
        // BPB serial is 32 bit
        memcpy(&serial, buffer + 0x43, sizeof(uint32_t));
    } else {
        serial = -1;
    }

    free(buffer);

    return serial;
}

file_handle_base_t::~file_handle_base_t()
{
}

extern char __text_st[];

extern "C" _noreturn
EFI_STATUS efi_main()
{
    PRINT("choosing kernel");
    tchar const *kernel_name = cpu_choose_kernel();

    PRINT("running kernel");
    elf64_run(kernel_name);
}
