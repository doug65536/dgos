/// pci driver:
/// V=0x1274 (Ensoniq),
/// D=0x5000 (ES1370)
/// C=0x4 (MULTIMEDIA),
/// S=1 (AUDIO),

#include "kmodule.h"
#include "../pci.h"

PCI_DRIVER(
        es1370,
        PCI_VENDOR_ENSONIQ, 0x5000,
        PCI_DEV_CLASS_MULTIMEDIA, PCI_SUBCLASS_MULTIMEDIA_AUDIO, -1);

#include "types.h"
#include "cpu/ioport.h"
#include "es137x.bits.h"

__BEGIN_ANONYMOUS

// Startup:
/// Enable bus mastering
/// Reset by writing 0x20 to status register 0x4
/// reset codec by writing 0xff to code register 0x14
/// program sample rate in 0x10
/// set master volume on codec
/// set memory page register (?)
/// set playback 2 buffer address register to physaddr 0x38
/// set playback 2 buffer definition register 0x3c to the dword count of buffer
/// set playback 2 frame count 0x28 to the number of frames to play
/// set serial interface register 0x20 to 0x0020020C to enable 16-bit
/// stereo, interrupts, looped mode on playback 2.
/// set control register 0x00 to 0x20 to enable playback 2 dac
///
/// set the frame count to play half of the buffer
///
/// clear then set interrupt enable (serial interface register 0x20)
/// to acknowledge IRQ
///
///
/// Internal memory is organized into 4 64 byte blocks
/// 0: dac1 samples
/// 1: dac2 samples
/// 2: adc samples
/// 3: dac1/2 frame info, adc frame info, uart fifo, uart fifo

struct es1370_t {
    unsigned base;

    bool init(pci_dev_iterator_t const& pci_iter);

    pci_bar_accessor_t accessor;
    static constexpr pci_bar_register_t reg_control{
        ES1370_CONTROL, sizeof(uint32_t)
    };
    static constexpr pci_bar_register_t reg_status{
        ES1370_STATUS, sizeof(uint32_t)
    };
    static constexpr pci_bar_register_t reg_uartdata{
        ES1370_UARTDATA, sizeof(uint8_t)
    };
    static constexpr pci_bar_register_t reg_uartstatus{
        ES1370_UARTSTATUS, sizeof(uint8_t)
    };
    static constexpr pci_bar_register_t reg_uartcontrol{
        ES1370_UART_CONTROL, sizeof(uint8_t)
    };
    static constexpr pci_bar_register_t reg_uarttest{
        ES1370_UARTTEST, sizeof(uint8_t)
    };
    static constexpr pci_bar_register_t reg_mempage{
        ES1370_MEMPAGE, sizeof(uint32_t)
    };
    static constexpr pci_bar_register_t reg_codec{
        ES1370_CODEC, sizeof(uint32_t)
    };
    static constexpr pci_bar_register_t reg_serial{
        ES1370_SERIAL, sizeof(uint32_t)
    };
};

static es1370_t es1370_instances[4];
static size_t es1370_count;

void set_playback2_sample_rate(unsigned rate)
{
    uint32_t frequency = (uint64_t(rate) << 16) / 3000;

//    SampleRateConverter[0x75] = (frequency >> 6) & 0xfc00;
//    SampleRateConverter[0x77] = (frequency >> 1);
}

int module_main(int argc, const char * const *argv)
{
    pci_dev_iterator_t pci_iter;

    if (unlikely(!pci_enumerate_begin(
                     &pci_iter, PCI_DEV_CLASS_MULTIMEDIA,
                     PCI_SUBCLASS_MULTIMEDIA_AUDIO,
                     PCI_VENDOR_ENSONIQ, 0x5000)))
        return 0;

    do {
        es1370_t *dev = es1370_instances + es1370_count;

        if (dev->init(pci_iter))
            ++es1370_count;
        else {
            dev->~es1370_t();
            new (dev) es1370_t();
        }
    } while (unlikely(pci_enumerate_next(&pci_iter)));

    return 0;
}

bool es1370_t::init(const pci_dev_iterator_t &pci_iter)
{
    pci_adj_control_bits(pci_iter, PCI_CMD_IOSE | PCI_CMD_BME, PCI_CMD_MSE);

    accessor = pci_bar_accessor_t(pci_iter, 0, ES1370_REG__END);

    accessor.wr_32(reg_control,
         ES1370_CONTROL_ADC_STOP |
         ES1370_CONTROL_WTSRSEL_n(ES1370_CONTROL_WTSRSEL_44K) |
         ES1370_CONTROL_CDC_EN);

    return true;
}

__END_ANONYMOUS
