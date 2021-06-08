#include "serial.h"
#include "ctors.h"
#include "bootefi.h"
#include "likely.h"
#include "string.h"
#include "debug.h"
#include "halt.h"

__BEGIN_ANONYMOUS

static EFI_GUID efi_serial_io_guid = SERIAL_IO_PROTOCOL;

class serial_logger_efi_t : public serial_logger_t {
public:
    constexpr serial_logger_efi_t() = default;

    // serial_logger_t interface
    bool init(int port) override final;
    bool write(int port, char const *data, size_t size) override final;

private:
    EFI_HANDLE opened_port_handle = nullptr;
    EFI_SERIAL_IO_PROTOCOL *opened_port = nullptr;
};

bool serial_logger_efi_t::init(int port)
{
    EFI_STATUS status;

    UINTN serial_port_count = 0;
    EFI_HANDLE *serial_ports = nullptr;

    status = efi_systab->BootServices->LocateHandleBuffer(
                ByProtocol,
                &efi_serial_io_guid,
                nullptr,
                &serial_port_count,
                &serial_ports);

    if (unlikely(EFI_ERROR(status))) {
        DEBUG(TSTR "LocateHandleBuffer SERIAL_IO_PROTOCOL failed");
        return false;
    }

    if (unlikely(port < 0 || size_t(port) >= serial_port_count)) {
        DEBUG("Invalid serial port %d", port);
        return false;
    }

    EFI_SERIAL_IO_PROTOCOL *serial_handle;

    status = efi_systab->BootServices->HandleProtocol(
            serial_ports[port],
            &efi_serial_io_guid,
            (VOID**)&serial_handle);

    if (!EFI_ERROR(status)) {
        opened_port_handle = serial_ports[port];
        opened_port = serial_handle;
    } else {
        DEBUG(TSTR "HandleProtocol SERIAL_IO_PROTOCOL failed");
    }

    return true;
}

bool serial_logger_efi_t::write(int port, char const *data, size_t size)
{
    if (likely(opened_port)) {
        UINTN len = size;
        EFI_STATUS status;
        status = opened_port->Write(opened_port, &len, data);
        return !EFI_ERROR(status);
    }
    return true;
}

__END_ANONYMOUS

static serial_logger_efi_t serial_logger_efi_instance;

void arch_serial_init(int port)
{
    serial_logger_t::instance = &serial_logger_efi_instance;
    if (serial_logger_efi_instance.init(port)) {
        char const *welcome = "\x1b" "c" "dgos bootloader log started\n";
        serial_logger_t::instance->write(port, welcome, strlen(welcome));
    }
}
