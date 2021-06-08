#include "serial.h"
#include "ctors.h"
#include "bioscall.h"

__BEGIN_ANONYMOUS

class serial_logger_bios_t : public serial_logger_t {
public:
    constexpr serial_logger_bios_t() = default;

    // serial_logger_t interface
    bool init(int port) override final;
    bool write(int port, char const *data, size_t size) override final;
};

bool serial_logger_bios_t::init(int port)
{
    bios_regs_t regs{};

    uint8_t params = 0;
    params |= (7 << 5); // 9600 baud (max!)
    // 0=no parity
    // 0=1 stop bit
    params |= 3;        // 8 data bits

    regs.eax = params;
    regs.edx = port;
    bioscall(&regs, 0x14);

    return !regs.flags_CF();
}

bool serial_logger_bios_t::write(int port, char const *data, size_t size)
{
    bios_regs_t regs{};

    for (size_t i = 0; i < size; ++i) {
        // ah=1, al=character
        regs.eax = (0x01 << 8) | uint8_t(data[i]);
        regs.edx = port;

        bioscall(&regs, 0x14);

        if (regs.flags_CF())
            return false;
    }

    return true;
}

__END_ANONYMOUS

static serial_logger_bios_t serial_logger_bios_instance;

void arch_serial_init(int port)
{
    serial_logger_t::instance = &serial_logger_bios_instance;
}
