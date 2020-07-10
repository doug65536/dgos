/// pci driver:
/// V=0x1274 (Ensoniq),
/// D=0x5000 (ES1370)
/// C=0x4 (MULTIMEDIA),
/// S=1 (AUDIO),

#include "kmodule.h"
#include "../pci.h"

PCI_DRIVER(
        es137x,
        0x1274, 0x5000,
        PCI_DEV_CLASS_MULTIMEDIA, PCI_SUBCLASS_MULTIMEDIA_AUDIO, -1);

#include "types.h"

struct es137x_t {
    uint32_t control;
    uint32_t status;
    uint8_t uart_data;
    uint8_t uart_status_ctl;
    uint16_t uart_test_mode;
    uint32_t memory_page;
    uint32_t samp_rate_conv;
    uint32_t code_rw;
    uint32_t legacy;
    uint32_t serial_iface;
    uint32_t pb1fc;
    uint32_t pb2fc;
    uint32_t rfc;
};

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

void set_playback2_sample_rate(unsigned rate)
{
    uint32_t frequency = (uint64_t(rate) << 16) / 3000;

//    SampleRateConverter[0x75] = (frequency >> 6) & 0xfc00;
//    SampleRateConverter[0x77] = (frequency >> 1);
}

