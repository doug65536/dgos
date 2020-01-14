#include "uleb.h"
#include "printk.h"

uintptr_t decode_uleb128(const uint8_t *&in)
{
    uintptr_t result = 0;
    uint8_t shift = 0;
    for (;;) {
        uint8_t byte = *in++;
        result |= uintptr_t(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0)
            break;
        shift += 7;
    }
    return result;
}

intptr_t decode_sleb128(const uint8_t *&in)
{
    intptr_t result = 0;
    uint8_t shift = 0;
    for (;;) {
        uint8_t byte = *in++;
        result |= uintptr_t(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0 && (byte & 0x40)) {
            if (shift < sizeof(intptr_t)*8)
                result |= ~uintptr_t(0) << shift;
            break;
        }
        shift += 7;
    }
    return result;
}

uint64_t read_enc_val(const uint8_t *&input, uint8_t encoding)
{
    switch (encoding & 0xF) {
    case DW_EH_PE_omit:
        return 0;

    case DW_EH_PE_uleb128:  // 0x01
        return decode_uleb128(input);

    case DW_EH_PE_udata2:   // 0x02
        return fetch_enc_val<uint16_t>(input);

    case DW_EH_PE_udata4:   // 0x03
        return fetch_enc_val<uint32_t>(input);

    case DW_EH_PE_udata8:   // 0x04
        return fetch_enc_val<uint64_t>(input);

    case DW_EH_PE_sleb128:  // 0x09
        return decode_sleb128(input);

    case DW_EH_PE_sdata2:   // 0x0A
        return fetch_enc_val<int16_t>(input);

    case DW_EH_PE_sdata4:   // 0x0B
        return fetch_enc_val<int32_t>(input);

    case DW_EH_PE_sdata8:   // 0x0C
        return fetch_enc_val<int64_t>(input);

    }

    panic("Unhandled dwarf frame information encoding\n");
}
