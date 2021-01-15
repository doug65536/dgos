#include "modelist.h"
#include "bootefi.h"
#include "ctors.h"
#include "likely.h"
#include "../kernel/lib/bitsearch.h"
#include "malloc.h"
#include "halt.h"

static constexpr EFI_GUID efi_graphics_output_protocol_guid =
        EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

static EFI_GRAPHICS_OUTPUT_PROTOCOL *efi_graphics_output;

vbe_mode_list_t mode_list;

static inline uint8_t lsb_set(uint32_t n)
{
    return n ? bit_lsb_set(n) : 0;
}

static inline uint8_t bits_width(uint32_t n, uint8_t lsb_set)
{
    return n ? 1 + lsb_set - bit_msb_set(n) : 0;
}

static vbe_selected_mode_t *selected_mode_from_efi_mode(
        vbe_selected_mode_t *result,
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *efi_mode,
        uint32_t mode_num)
{
    result->mode_num = mode_num;

    result->height = uint16_t(efi_mode->VerticalResolution);
    result->width = uint16_t(efi_mode->HorizontalResolution);

    switch (efi_mode->PixelFormat) {
    case PixelRedGreenBlueReserved8BitPerColor:
        result->mask_pos_r = 0;
        result->mask_pos_g = 8;
        result->mask_pos_b = 16;
        result->mask_pos_a = 24;
        result->mask_size_r = 8;
        result->mask_size_g = 8;
        result->mask_size_b = 8;
        result->mask_size_a = 8;
        result->bpp = 32;
        result->byte_pp = 4;
        break;

    case PixelBlueGreenRedReserved8BitPerColor:
        result->mask_pos_r = 16;
        result->mask_pos_g = 8;
        result->mask_pos_b = 0;
        result->mask_pos_a = 24;
        result->mask_size_r = 8;
        result->mask_size_g = 8;
        result->mask_size_b = 8;
        result->mask_size_a = 8;
        result->bpp = 32;
        result->byte_pp = 4;
        break;

    case PixelBitMask:
    {
        auto const& pi = efi_mode->PixelInformation;

        result->mask_pos_r = lsb_set(pi.RedMask);
        result->mask_size_r = bits_width(pi.RedMask, result->mask_pos_r);

        result->mask_pos_g = bit_lsb_set(pi.GreenMask);
        result->mask_size_g = bits_width(pi.GreenMask, result->mask_pos_g);

        result->mask_pos_b = bit_lsb_set(pi.BlueMask);
        result->mask_size_b = bits_width(pi.BlueMask, result->mask_pos_b);

        result->mask_pos_a = bit_lsb_set(pi.ReservedMask);
        result->mask_size_a = bits_width(pi.ReservedMask, result->mask_pos_a);

        result->bpp = result->mask_size_r + result->mask_size_g +
                result->mask_pos_b + result->mask_size_a;

        result->byte_pp = (result->bpp + 7) >> 3;

        break;
    }

    case PixelBltOnly:
    default:
        result->mask_pos_r = 0;
        result->mask_pos_g = 0;
        result->mask_pos_b = 0;
        result->mask_pos_a = 0;
        result->mask_size_r = 0;
        result->mask_size_g = 0;
        result->mask_size_b = 0;
        result->mask_size_a = 0;
        break;
    }

    result->pitch = efi_mode->PixelsPerScanLine * result->byte_pp;

    aspect_ratio(&result->aspect_n, &result->aspect_d,
                 result->width, result->height);

    return result;
}


bool vbe_set_mode(vbe_selected_mode_t& mode)
{
    EFI_STATUS status = 0;

    if (mode.mode_num != uint16_t(-1)) {
        status = efi_graphics_output->SetMode(
                    efi_graphics_output, mode.mode_num);
    }

    mode.framebuffer_addr = efi_graphics_output->Mode->FrameBufferBase;
    mode.framebuffer_bytes = efi_graphics_output->Mode->FrameBufferSize;

    return !EFI_ERROR(status);
}

vbe_mode_list_t const& vbe_enumerate_modes()
{
    EFI_STATUS status;

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = nullptr;

    mode_list.count = efi_graphics_output->Mode->MaxMode;
    mode_list.modes = new (ext::nothrow)
            vbe_selected_mode_t[mode_list.count]();

    for (uint32_t i = 0; i < mode_list.count; ++i) {
        UINTN info_sz = sizeof(*info);

        status = efi_graphics_output->QueryMode(
                    efi_graphics_output, i, &info_sz, &info);

        if (likely(!EFI_ERROR(status)))
            selected_mode_from_efi_mode(&mode_list.modes[i], info, i);
        else
            mode_list.modes[i] = {};
    }

    return mode_list;
}

_constructor(ctor_graphics) static void vbe_init()
{
    EFI_STATUS status;

    EFI_HANDLE *efi_graphics_output_handles = nullptr;
    UINTN efi_num_graphics_output_handles = 0;

    status = efi_systab->BootServices->LocateHandleBuffer(
                ByProtocol,
                &efi_graphics_output_protocol_guid,
                nullptr,
                &efi_num_graphics_output_handles,
                &efi_graphics_output_handles);

    if (unlikely(EFI_ERROR(status) || !efi_graphics_output_handles))
        PANIC(TSTR "Unable to query graphics output handle");

    status = efi_systab->BootServices->HandleProtocol(
                efi_graphics_output_handles[0],
            &efi_graphics_output_protocol_guid,
            (VOID**)&efi_graphics_output);

    if (unlikely(EFI_ERROR(status)))
        PANIC(TSTR "Unable to query graphics output interface");

    //efi_graphics_output->SetMode(efi_graphics_output, 0);
}
